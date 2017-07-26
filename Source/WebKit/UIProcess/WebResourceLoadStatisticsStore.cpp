/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebResourceLoadStatisticsStore.h"

#include "Logging.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include "WebResourceLoadStatisticsTelemetry.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataStore.h"
#include <WebCore/KeyedCoding.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

constexpr unsigned operatingDatesWindow { 30 };
constexpr unsigned statisticsModelVersion { 9 };
constexpr unsigned maxImportance { 3 };

template<typename T> static inline String isolatedPrimaryDomain(const T& value)
{
    return ResourceLoadStatistics::primaryDomain(value).isolatedCopy();
}

const OptionSet<WebsiteDataType>& WebResourceLoadStatisticsStore::monitoredDataTypes()
{
    static NeverDestroyed<OptionSet<WebsiteDataType>> dataTypes(std::initializer_list<WebsiteDataType>({
        WebsiteDataType::Cookies,
        WebsiteDataType::IndexedDBDatabases,
        WebsiteDataType::LocalStorage,
        WebsiteDataType::MediaKeys,
        WebsiteDataType::OfflineWebApplicationCache,
#if ENABLE(NETSCAPE_PLUGIN_API)
        WebsiteDataType::PlugInData,
#endif
        WebsiteDataType::SearchFieldRecentSearches,
        WebsiteDataType::SessionStorage,
        WebsiteDataType::WebSQLDatabases,
    }));

    ASSERT(RunLoop::isMain());

    return dataTypes;
}

class OperatingDate {
public:
    OperatingDate() = default;

    static OperatingDate fromWallTime(WallTime time)
    {
        double ms = time.secondsSinceEpoch().milliseconds();
        int year = msToYear(ms);
        int yearDay = dayInYear(ms, year);
        int month = monthFromDayInYear(yearDay, isLeapYear(year));
        int monthDay = dayInMonthFromDayInYear(yearDay, isLeapYear(year));

        return OperatingDate { year, month, monthDay };
    }

    static OperatingDate today()
    {
        return OperatingDate::fromWallTime(WallTime::now());
    }

    Seconds secondsSinceEpoch() const
    {
        return Seconds { dateToDaysFrom1970(m_year, m_month, m_monthDay) * secondsPerDay };
    }

    bool operator==(const OperatingDate& other) const
    {
        return m_monthDay == other.m_monthDay && m_month == other.m_month && m_year == other.m_year;
    }

    bool operator<(const OperatingDate& other) const
    {
        return secondsSinceEpoch() < other.secondsSinceEpoch();
    }

    bool operator<=(const OperatingDate& other) const
    {
        return secondsSinceEpoch() <= other.secondsSinceEpoch();
    }

private:
    OperatingDate(int year, int month, int monthDay)
        : m_year(year)
        , m_month(month)
        , m_monthDay(monthDay)
    { }

    int m_year { 0 };
    int m_month { 0 }; // [0, 11].
    int m_monthDay { 0 }; // [1, 31].
};

static Vector<OperatingDate> mergeOperatingDates(const Vector<OperatingDate>& existingDates, Vector<OperatingDate>&& newDates)
{
    if (existingDates.isEmpty())
        return WTFMove(newDates);

    Vector<OperatingDate> mergedDates(existingDates.size() + newDates.size());

    // Merge the two sorted vectors of dates.
    std::merge(existingDates.begin(), existingDates.end(), newDates.begin(), newDates.end(), mergedDates.begin());
    // Remove duplicate dates.
    removeRepeatedElements(mergedDates);

    // Drop old dates until the Vector size reaches operatingDatesWindow.
    while (mergedDates.size() > operatingDatesWindow)
        mergedDates.remove(0);

    return mergedDates;
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(const String& resourceLoadStatisticsDirectory, Function<void (const String&)>&& testingCallback, UpdateCookiePartitioningForDomainsHandler&& updateCookiePartitioningForDomainsHandler)
    : m_statisticsQueue(WorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue", WorkQueue::Type::Serial, WorkQueue::QOS::Utility))
    , m_persistentStorage(*this, resourceLoadStatisticsDirectory)
    , m_updateCookiePartitioningForDomainsHandler(WTFMove(updateCookiePartitioningForDomainsHandler))
    , m_dailyTasksTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::performDailyTasks)
    , m_statisticsTestingCallback(WTFMove(testingCallback))
{
    ASSERT(RunLoop::isMain());

#if PLATFORM(COCOA)
    registerUserDefaultsIfNeeded();
#endif

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        m_persistentStorage.initialize();
        includeTodayAsOperatingDateIfNecessary();
    });

    m_statisticsQueue->dispatchAfter(5_s, [this, protectedThis = makeRef(*this)] {
        if (m_parameters.shouldSubmitTelemetry)
            WebResourceLoadStatisticsTelemetry::calculateAndSubmit(*this);
    });

    m_dailyTasksTimer.startRepeating(24_h);
}

WebResourceLoadStatisticsStore::~WebResourceLoadStatisticsStore()
{
}
    
void WebResourceLoadStatisticsStore::removeDataRecords()
{
    ASSERT(!RunLoop::isMain());
    
    if (!shouldRemoveDataRecords())
        return;

    auto prevalentResourceDomains = topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    if (prevalentResourceDomains.isEmpty())
        return;
    
    setDataRecordsBeingRemoved(true);

    RunLoop::main().dispatch([prevalentResourceDomains = CrossThreadCopier<Vector<String>>::copy(prevalentResourceDomains), this, protectedThis = makeRef(*this)] () mutable {
        WebProcessProxy::deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(WebResourceLoadStatisticsStore::monitoredDataTypes(), WTFMove(prevalentResourceDomains), m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned, [this, protectedThis = WTFMove(protectedThis)](const HashSet<String>& domainsWithDeletedWebsiteData) mutable {
            m_statisticsQueue->dispatch([this, protectedThis = WTFMove(protectedThis), topDomains = CrossThreadCopier<HashSet<String>>::copy(domainsWithDeletedWebsiteData)] () mutable {
                for (auto& prevalentResourceDomain : topDomains) {
                    auto& statistic = ensureResourceStatisticsForPrimaryDomain(prevalentResourceDomain);
                    ++statistic.dataRecordsRemoved;
                }
                setDataRecordsBeingRemoved(false);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleStatisticsAndDataRecordsProcessing()
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        processStatisticsAndDataRecords();
    });
}

void WebResourceLoadStatisticsStore::processStatisticsAndDataRecords()
{
    ASSERT(!RunLoop::isMain());

    if (m_parameters.shouldClassifyResourcesBeforeDataRecordsRemoval) {
        for (auto& resourceStatistic : m_resourceStatisticsMap.values()) {
            if (!resourceStatistic.isPrevalentResource && m_resourceLoadStatisticsClassifier.hasPrevalentResourceCharacteristics(resourceStatistic))
                resourceStatistic.isPrevalentResource = true;
        }
    }
    removeDataRecords();

    pruneStatisticsIfNeeded();

    if (m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned) {
        RunLoop::main().dispatch([] {
            WebProcessProxy::notifyPageStatisticsAndDataRecordsProcessed();
        });
    }

    m_persistentStorage.scheduleOrWriteMemoryStore(ResourceLoadStatisticsPersistentStorage::ForceImmediateWrite::No);
}

void WebResourceLoadStatisticsStore::resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&& origins)
{
    ASSERT(!RunLoop::isMain());

    mergeStatistics(WTFMove(origins));
    // Fire before processing statistics to propagate user interaction as fast as possible to the network process.
    updateCookiePartitioning();
    processStatisticsAndDataRecords();
}

void WebResourceLoadStatisticsStore::grandfatherExistingWebsiteData()
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this)] () mutable {
        // FIXME: This method being a static call on WebProcessProxy is wrong.
        // It should be on the data store that this object belongs to.
        WebProcessProxy::topPrivatelyControlledDomainsWithWebsiteData(WebResourceLoadStatisticsStore::monitoredDataTypes(), m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned, [this, protectedThis = WTFMove(protectedThis)] (HashSet<String>&& topPrivatelyControlledDomainsWithWebsiteData) mutable {
            m_statisticsQueue->dispatch([this, protectedThis = WTFMove(protectedThis), topDomains = CrossThreadCopier<HashSet<String>>::copy(topPrivatelyControlledDomainsWithWebsiteData)] () mutable {
                for (auto& topPrivatelyControlledDomain : topDomains) {
                    auto& statistic = ensureResourceStatisticsForPrimaryDomain(topPrivatelyControlledDomain);
                    statistic.grandfathered = true;
                }
                m_endOfGrandfatheringTimestamp = WallTime::now() + m_parameters.grandfatheringTime;
                m_persistentStorage.scheduleOrWriteMemoryStore(ResourceLoadStatisticsPersistentStorage::ForceImmediateWrite::Yes);
            });

            logTestingEvent(ASCIILiteral("Grandfathered"));
        });
    });
}
    
void WebResourceLoadStatisticsStore::processWillOpenConnection(WebProcessProxy&, IPC::Connection& connection)
{
    connection.addWorkQueueMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName(), m_statisticsQueue.get(), this);
}

void WebResourceLoadStatisticsStore::processDidCloseConnection(WebProcessProxy&, IPC::Connection& connection)
{
    connection.removeWorkQueueMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName());
}

void WebResourceLoadStatisticsStore::applicationWillTerminate()
{
    m_persistentStorage.finishAllPendingWorkSynchronously();
}

void WebResourceLoadStatisticsStore::performDailyTasks()
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        includeTodayAsOperatingDateIfNecessary();
    });
    if (m_parameters.shouldSubmitTelemetry)
        submitTelemetry();
}

void WebResourceLoadStatisticsStore::submitTelemetry()
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        WebResourceLoadStatisticsTelemetry::calculateAndSubmit(*this);
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.hadUserInteraction = true;
        statistics.mostRecentUserInteractionTime = WallTime::now();

        updateCookiePartitioningForDomains({ primaryDomain }, { }, ShouldClearFirst::No);
    });
}

void WebResourceLoadStatisticsStore::clearUserInteraction(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.hadUserInteraction = false;
        statistics.mostRecentUserInteractionTime = { };
    });
}

void WebResourceLoadStatisticsStore::hasHadUserInteraction(const URL& url, WTF::Function<void (bool)>&& completionHandler)
{
    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)] () mutable {
        auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
        bool hadUserInteraction = mapEntry == m_resourceStatisticsMap.end() ? false: hasHadUnexpiredRecentUserInteraction(mapEntry->value);
        RunLoop::main().dispatch([hadUserInteraction, completionHandler = WTFMove(completionHandler)] {
            completionHandler(hadUserInteraction);
        });
    });
}

void WebResourceLoadStatisticsStore::setLastSeen(const URL& url, Seconds seconds)
{
    if (url.isBlankURL() || url.isEmpty())
        return;
    
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), seconds] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.lastSeen = WallTime::fromRawSeconds(seconds.seconds());
    });
}
    
void WebResourceLoadStatisticsStore::setPrevalentResource(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.isPrevalentResource = true;
    });
}

void WebResourceLoadStatisticsStore::isPrevalentResource(const URL& url, WTF::Function<void (bool)>&& completionHandler)
{
    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)] () mutable {
        auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
        bool isPrevalentResource = mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.isPrevalentResource;
        RunLoop::main().dispatch([isPrevalentResource, completionHandler = WTFMove(completionHandler)] {
            completionHandler(isPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::clearPrevalentResource(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.isPrevalentResource = false;
    });
}

void WebResourceLoadStatisticsStore::setGrandfathered(const URL& url, bool value)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), value] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
        statistics.grandfathered = value;
    });
}

void WebResourceLoadStatisticsStore::isGrandfathered(const URL& url, WTF::Function<void (bool)>&& completionHandler)
{
    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler), primaryDomain = isolatedPrimaryDomain(url)] () mutable {
        auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
        bool isGrandFathered = mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.grandfathered;
        RunLoop::main().dispatch([isGrandFathered, completionHandler = WTFMove(completionHandler)] {
            completionHandler(isGrandFathered);
        });
    });
}

void WebResourceLoadStatisticsStore::setSubframeUnderTopFrameOrigin(const URL& subframe, const URL& topFrame)
{
    if (subframe.isBlankURL() || subframe.isEmpty() || topFrame.isBlankURL() || topFrame.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryTopFrameDomain = isolatedPrimaryDomain(topFrame), primarySubFrameDomain = isolatedPrimaryDomain(subframe)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primarySubFrameDomain);
        statistics.subframeUnderTopFrameOrigins.add(primaryTopFrameDomain);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUnderTopFrameOrigin(const URL& subresource, const URL& topFrame)
{
    if (subresource.isBlankURL() || subresource.isEmpty() || topFrame.isBlankURL() || topFrame.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryTopFrameDomain = isolatedPrimaryDomain(topFrame), primarySubresourceDomain = isolatedPrimaryDomain(subresource)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomain);
        statistics.subresourceUnderTopFrameOrigins.add(primaryTopFrameDomain);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(const URL& subresource, const URL& hostNameRedirectedTo)
{
    if (subresource.isBlankURL() || subresource.isEmpty() || hostNameRedirectedTo.isBlankURL() || hostNameRedirectedTo.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedTo), primarySubresourceDomain = isolatedPrimaryDomain(subresource)] {
        auto& statistics = ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomain);
        statistics.subresourceUniqueRedirectsTo.add(primaryRedirectDomain);
    });
}

void WebResourceLoadStatisticsStore::scheduleCookiePartitioningUpdate()
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        updateCookiePartitioning();
    });
}

void WebResourceLoadStatisticsStore::scheduleCookiePartitioningUpdateForDomains(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, ShouldClearFirst shouldClearFirst)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd), shouldClearFirst] {
        updateCookiePartitioningForDomains(domainsToRemove, domainsToAdd, shouldClearFirst);
    });
}

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
void WebResourceLoadStatisticsStore::scheduleCookiePartitioningStateReset()
{
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        resetCookiePartitioningState();
    });
}
#endif

void WebResourceLoadStatisticsStore::scheduleClearInMemory()
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        clearInMemory();
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(ShouldGrandfather shouldGrandfather)
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), shouldGrandfather] {
        clearInMemory();
        m_persistentStorage.clear();
        
        if (shouldGrandfather == ShouldGrandfather::Yes)
            grandfatherExistingWebsiteData();
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(std::chrono::system_clock::time_point modifiedSince, ShouldGrandfather shouldGrandfather)
{
    // For now, be conservative and clear everything regardless of modifiedSince.
    UNUSED_PARAM(modifiedSince);
    scheduleClearInMemoryAndPersistent(shouldGrandfather);
}

void WebResourceLoadStatisticsStore::setTimeToLiveUserInteraction(Seconds seconds)
{
    ASSERT(seconds >= 0_s);
    m_parameters.timeToLiveUserInteraction = seconds;
}

void WebResourceLoadStatisticsStore::setTimeToLiveCookiePartitionFree(Seconds seconds)
{
    ASSERT(seconds >= 0_s);
    m_parameters.timeToLiveCookiePartitionFree = seconds;
}

void WebResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds)
{
    ASSERT(seconds >= 0_s);
    m_parameters.minimumTimeBetweenDataRecordsRemoval = seconds;
}

void WebResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds)
{
    ASSERT(seconds >= 0_s);
    m_parameters.grandfatheringTime = seconds;
}

bool WebResourceLoadStatisticsStore::shouldRemoveDataRecords() const
{
    ASSERT(!RunLoop::isMain());
    if (m_dataRecordsBeingRemoved)
        return false;

    return !m_lastTimeDataRecordsWereRemoved || MonotonicTime::now() >= (m_lastTimeDataRecordsWereRemoved + m_parameters.minimumTimeBetweenDataRecordsRemoval);
}

void WebResourceLoadStatisticsStore::setDataRecordsBeingRemoved(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_dataRecordsBeingRemoved = value;
    if (m_dataRecordsBeingRemoved)
        m_lastTimeDataRecordsWereRemoved = MonotonicTime::now();
}

ResourceLoadStatistics& WebResourceLoadStatisticsStore::ensureResourceStatisticsForPrimaryDomain(const String& primaryDomain)
{
    ASSERT(!RunLoop::isMain());
    return m_resourceStatisticsMap.ensure(primaryDomain, [&primaryDomain] {
        return ResourceLoadStatistics(primaryDomain);
    }).iterator->value;
}

std::unique_ptr<KeyedEncoder> WebResourceLoadStatisticsStore::createEncoderFromData() const
{
    ASSERT(!RunLoop::isMain());
    auto encoder = KeyedEncoder::encoder();
    encoder->encodeUInt32("version", statisticsModelVersion);
    encoder->encodeDouble("endOfGrandfatheringTimestamp", m_endOfGrandfatheringTimestamp.secondsSinceEpoch().value());

    encoder->encodeObjects("browsingStatistics", m_resourceStatisticsMap.begin(), m_resourceStatisticsMap.end(), [](KeyedEncoder& encoderInner, const auto& origin) {
        origin.value.encode(encoderInner);
    });

    encoder->encodeObjects("operatingDates", m_operatingDates.begin(), m_operatingDates.end(), [](KeyedEncoder& encoderInner, OperatingDate date) {
        encoderInner.encodeDouble("date", date.secondsSinceEpoch().value());
    });

    return encoder;
}

void WebResourceLoadStatisticsStore::mergeWithDataFromDecoder(KeyedDecoder& decoder)
{
    ASSERT(!RunLoop::isMain());

    unsigned versionOnDisk;
    if (!decoder.decodeUInt32("version", versionOnDisk))
        return;

    if (versionOnDisk != statisticsModelVersion)
        return;

    double endOfGrandfatheringTimestamp;
    if (decoder.decodeDouble("endOfGrandfatheringTimestamp", endOfGrandfatheringTimestamp))
        m_endOfGrandfatheringTimestamp = WallTime::fromRawSeconds(endOfGrandfatheringTimestamp);
    else
        m_endOfGrandfatheringTimestamp = { };

    Vector<ResourceLoadStatistics> loadedStatistics;
    bool succeeded = decoder.decodeObjects("browsingStatistics", loadedStatistics, [](KeyedDecoder& decoderInner, ResourceLoadStatistics& statistics) {
        return statistics.decode(decoderInner);
    });

    if (!succeeded)
        return;

    mergeStatistics(WTFMove(loadedStatistics));
    updateCookiePartitioning();

    Vector<OperatingDate> operatingDates;
    succeeded = decoder.decodeObjects("operatingDates", operatingDates, [](KeyedDecoder& decoder, OperatingDate& date) {
        double value;
        if (!decoder.decodeDouble("date", value))
            return false;

        date = OperatingDate::fromWallTime(WallTime::fromRawSeconds(value));
        return true;
    });

    if (!succeeded)
        return;

    m_operatingDates = mergeOperatingDates(m_operatingDates, WTFMove(operatingDates));
}

void WebResourceLoadStatisticsStore::clearInMemory()
{
    ASSERT(!RunLoop::isMain());
    m_resourceStatisticsMap.clear();
    m_operatingDates.clear();

    updateCookiePartitioningForDomains({ }, { }, ShouldClearFirst::Yes);
}

void WebResourceLoadStatisticsStore::mergeStatistics(Vector<ResourceLoadStatistics>&& statistics)
{
    ASSERT(!RunLoop::isMain());
    for (auto& statistic : statistics) {
        auto result = m_resourceStatisticsMap.ensure(statistic.highLevelDomain, [&statistic] {
            return WTFMove(statistic);
        });
        if (!result.isNewEntry)
            result.iterator->value.merge(statistic);
    }
}

inline bool WebResourceLoadStatisticsStore::shouldPartitionCookies(const ResourceLoadStatistics& statistic) const
{
    return statistic.isPrevalentResource && (!statistic.hadUserInteraction || WallTime::now() > statistic.mostRecentUserInteractionTime + m_parameters.timeToLiveCookiePartitionFree);
}

void WebResourceLoadStatisticsStore::updateCookiePartitioning()
{
    ASSERT(!RunLoop::isMain());

    Vector<String> domainsToRemove;
    Vector<String> domainsToAdd;
    for (auto& resourceStatistic : m_resourceStatisticsMap.values()) {
        bool shouldPartition = shouldPartitionCookies(resourceStatistic);
        if (resourceStatistic.isMarkedForCookiePartitioning && !shouldPartition) {
            resourceStatistic.isMarkedForCookiePartitioning = false;
            domainsToRemove.append(resourceStatistic.highLevelDomain);
        } else if (!resourceStatistic.isMarkedForCookiePartitioning && shouldPartition) {
            resourceStatistic.isMarkedForCookiePartitioning = true;
            domainsToAdd.append(resourceStatistic.highLevelDomain);
        }
    }

    if (domainsToRemove.isEmpty() && domainsToAdd.isEmpty())
        return;

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd)] () {
        m_updateCookiePartitioningForDomainsHandler(domainsToRemove, domainsToAdd, ShouldClearFirst::No);
    });
}

void WebResourceLoadStatisticsStore::updateCookiePartitioningForDomains(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, ShouldClearFirst shouldClearFirst)
{
    ASSERT(!RunLoop::isMain());
    if (domainsToRemove.isEmpty() && domainsToAdd.isEmpty())
        return;

    RunLoop::main().dispatch([this, shouldClearFirst, protectedThis = makeRef(*this), domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd)] () {
        m_updateCookiePartitioningForDomainsHandler(domainsToRemove, domainsToAdd, shouldClearFirst);
    });

    if (shouldClearFirst == ShouldClearFirst::Yes)
        resetCookiePartitioningState();
    else {
        for (auto& domain : domainsToRemove)
            ensureResourceStatisticsForPrimaryDomain(domain).isMarkedForCookiePartitioning = false;
    }

    for (auto& domain : domainsToAdd)
        ensureResourceStatisticsForPrimaryDomain(domain).isMarkedForCookiePartitioning = true;
}

void WebResourceLoadStatisticsStore::resetCookiePartitioningState()
{
    ASSERT(!RunLoop::isMain());
    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        resourceStatistic.isMarkedForCookiePartitioning = false;
}

void WebResourceLoadStatisticsStore::processStatistics(const WTF::Function<void (const ResourceLoadStatistics&)>& processFunction) const
{
    ASSERT(!RunLoop::isMain());
    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        processFunction(resourceStatistic);
}

bool WebResourceLoadStatisticsStore::hasHadUnexpiredRecentUserInteraction(ResourceLoadStatistics& resourceStatistic) const
{
    if (resourceStatistic.hadUserInteraction && hasStatisticsExpired(resourceStatistic)) {
        // Drop privacy sensitive data because we no longer need it.
        // Set timestamp to 0 so that statistics merge will know
        // it has been reset as opposed to its default -1.
        resourceStatistic.mostRecentUserInteractionTime = { };
        resourceStatistic.hadUserInteraction = false;
    }

    return resourceStatistic.hadUserInteraction;
}

Vector<String> WebResourceLoadStatisticsStore::topPrivatelyControlledDomainsToRemoveWebsiteDataFor()
{
    ASSERT(!RunLoop::isMain());

    bool shouldCheckForGrandfathering = m_endOfGrandfatheringTimestamp > WallTime::now();
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && m_endOfGrandfatheringTimestamp;

    if (shouldClearGrandfathering)
        m_endOfGrandfatheringTimestamp = { };

    Vector<String> prevalentResources;
    for (auto& statistic : m_resourceStatisticsMap.values()) {
        if (statistic.isPrevalentResource && !hasHadUnexpiredRecentUserInteraction(statistic) && (!shouldCheckForGrandfathering || !statistic.grandfathered))
            prevalentResources.append(statistic.highLevelDomain);

        if (shouldClearGrandfathering && statistic.grandfathered)
            statistic.grandfathered = false;
    }

    return prevalentResources;
}

void WebResourceLoadStatisticsStore::includeTodayAsOperatingDateIfNecessary()
{
    ASSERT(!RunLoop::isMain());

    auto today = OperatingDate::today();
    if (!m_operatingDates.isEmpty() && today <= m_operatingDates.last())
        return;

    while (m_operatingDates.size() >= operatingDatesWindow)
        m_operatingDates.remove(0);

    m_operatingDates.append(today);
}

bool WebResourceLoadStatisticsStore::hasStatisticsExpired(const ResourceLoadStatistics& resourceStatistic) const
{
    if (m_operatingDates.size() >= operatingDatesWindow) {
        if (OperatingDate::fromWallTime(resourceStatistic.mostRecentUserInteractionTime) < m_operatingDates.first())
            return true;
    }

    // If we don't meet the real criteria for an expired statistic, check the user setting for a tighter restriction (mainly for testing).
    if (m_parameters.timeToLiveUserInteraction) {
        if (WallTime::now() > resourceStatistic.mostRecentUserInteractionTime + m_parameters.timeToLiveUserInteraction.value())
            return true;
    }

    return false;
}
    
void WebResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount)
{
    m_parameters.maxStatisticsEntries = maximumEntryCount;
}
    
void WebResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount)
{
    m_parameters.pruneEntriesDownTo = pruneTargetCount;
}
    
struct StatisticsLastSeen {
    String topPrivatelyOwnedDomain;
    WallTime lastSeen;
};
    
static void pruneResources(HashMap<String, WebCore::ResourceLoadStatistics>& statisticsMap, Vector<StatisticsLastSeen>& statisticsToPrune, size_t& numberOfEntriesToPrune)
{
    if (statisticsToPrune.size() > numberOfEntriesToPrune) {
        std::sort(statisticsToPrune.begin(), statisticsToPrune.end(), [](const StatisticsLastSeen& a, const StatisticsLastSeen& b) {
            return a.lastSeen < b.lastSeen;
        });
    }

    for (size_t i = 0, end = std::min(numberOfEntriesToPrune, statisticsToPrune.size()); i != end; ++i, --numberOfEntriesToPrune)
        statisticsMap.remove(statisticsToPrune[i].topPrivatelyOwnedDomain);
}
    
static unsigned computeImportance(const ResourceLoadStatistics& resourceStatistic)
{
    unsigned importance = maxImportance;
    if (!resourceStatistic.isPrevalentResource)
        importance -= 1;
    if (!resourceStatistic.hadUserInteraction)
        importance -= 2;
    return importance;
}
    
void WebResourceLoadStatisticsStore::pruneStatisticsIfNeeded()
{
    ASSERT(!RunLoop::isMain());
    if (m_resourceStatisticsMap.size() <= m_parameters.maxStatisticsEntries)
        return;

    ASSERT(m_parameters.pruneEntriesDownTo <= m_parameters.maxStatisticsEntries);

    size_t numberOfEntriesLeftToPrune = m_resourceStatisticsMap.size() - m_parameters.pruneEntriesDownTo;
    ASSERT(numberOfEntriesLeftToPrune);
    
    Vector<StatisticsLastSeen> resourcesToPrunePerImportance[maxImportance + 1];
    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        resourcesToPrunePerImportance[computeImportance(resourceStatistic)].append({ resourceStatistic.highLevelDomain, resourceStatistic.lastSeen });
    
    for (unsigned importance = 0; numberOfEntriesLeftToPrune && importance <= maxImportance; ++importance)
        pruneResources(m_resourceStatisticsMap, resourcesToPrunePerImportance[importance], numberOfEntriesLeftToPrune);

    ASSERT(!numberOfEntriesLeftToPrune);
}

void WebResourceLoadStatisticsStore::resetParametersToDefaultValues()
{
    m_parameters = { };
}

void WebResourceLoadStatisticsStore::logTestingEvent(const String& event)
{
    if (!m_statisticsTestingCallback)
        return;

    if (RunLoop::isMain())
        m_statisticsTestingCallback(event);
    else {
        RunLoop::main().dispatch([this, protectedThis = makeRef(*this), event = event.isolatedCopy()] {
            if (m_statisticsTestingCallback)
                m_statisticsTestingCallback(event);
        });
    }
}

} // namespace WebKit
