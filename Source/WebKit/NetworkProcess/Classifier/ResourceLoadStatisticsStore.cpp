/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "ResourceLoadStatisticsStore.h"

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "Logging.h"
#include "NetworkSession.h"
#include "PluginProcessManager.h"
#include "PluginProcessProxy.h"
#include "ResourceLoadStatisticsPersistentStorage.h"
#include "StorageAccessStatus.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsTelemetry.h"
#include "WebsiteDataStore.h"
#include <WebCore/KeyedCoding.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

constexpr Seconds minimumStatisticsProcessingInterval { 5_s };
constexpr unsigned operatingDatesWindow { 30 };

#if !RELEASE_LOG_DISABLED
static String domainsToString(const Vector<RegistrableDomain>& domains)
{
    StringBuilder builder;
    for (auto& domain : domains) {
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.append(domain.string());
    }
    return builder.toString();
}
#endif

OperatingDate OperatingDate::fromWallTime(WallTime time)
{
    double ms = time.secondsSinceEpoch().milliseconds();
    int year = msToYear(ms);
    int yearDay = dayInYear(ms, year);
    int month = monthFromDayInYear(yearDay, isLeapYear(year));
    int monthDay = dayInMonthFromDayInYear(yearDay, isLeapYear(year));

    return OperatingDate { year, month, monthDay };
}

OperatingDate OperatingDate::today()
{
    return OperatingDate::fromWallTime(WallTime::now());
}

Seconds OperatingDate::secondsSinceEpoch() const
{
    return Seconds { dateToDaysFrom1970(m_year, m_month, m_monthDay) * secondsPerDay };
}

bool OperatingDate::operator==(const OperatingDate& other) const
{
    return m_monthDay == other.m_monthDay && m_month == other.m_month && m_year == other.m_year;
}

bool OperatingDate::operator<(const OperatingDate& other) const
{
    return secondsSinceEpoch() < other.secondsSinceEpoch();
}

bool OperatingDate::operator<=(const OperatingDate& other) const
{
    return secondsSinceEpoch() <= other.secondsSinceEpoch();
}

ResourceLoadStatisticsStore::ResourceLoadStatisticsStore(WebResourceLoadStatisticsStore& store, WorkQueue& workQueue)
    : m_store(store)
    , m_workQueue(workQueue)
{
    ASSERT(!RunLoop::isMain());

    includeTodayAsOperatingDateIfNecessary();
}

ResourceLoadStatisticsStore::~ResourceLoadStatisticsStore()
{
    ASSERT(!RunLoop::isMain());
}

unsigned ResourceLoadStatisticsStore::computeImportance(const ResourceLoadStatistics& resourceStatistic)
{
    unsigned importance = ResourceLoadStatisticsStore::maxImportance;
    if (!resourceStatistic.isPrevalentResource)
        importance -= 1;
    if (!resourceStatistic.hadUserInteraction)
        importance -= 2;
    return importance;
}

void ResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned = value;
}

void ResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldClassifyResourcesBeforeDataRecordsRemoval = value;
}

void ResourceLoadStatisticsStore::setShouldSubmitTelemetry(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldSubmitTelemetry = value;
}

void ResourceLoadStatisticsStore::removeDataRecords(CompletionHandler<void()>&& callback)
{
    ASSERT(!RunLoop::isMain());

    if (!shouldRemoveDataRecords()) {
        callback();
        return;
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_activePluginTokens.clear();
    for (const auto& plugin : PluginProcessManager::singleton().pluginProcesses())
        m_activePluginTokens.add(plugin->pluginProcessToken());
#endif

    auto prevalentResourceDomains = registrableDomainsToRemoveWebsiteDataFor();
    if (prevalentResourceDomains.isEmpty()) {
        callback();
        return;
    }

#if !RELEASE_LOG_DISABLED
    RELEASE_LOG_INFO_IF(m_debugLoggingEnabled, ResourceLoadStatisticsDebug, "About to remove data records for %{public}s.", domainsToString(prevalentResourceDomains).utf8().data());
#endif

    setDataRecordsBeingRemoved(true);

    RunLoop::main().dispatch([prevalentResourceDomains = crossThreadCopy(prevalentResourceDomains), callback = WTFMove(callback), weakThis = makeWeakPtr(*this), shouldNotifyPagesWhenDataRecordsWereScanned = m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned, workQueue = m_workQueue.copyRef()] () mutable {
        if (!weakThis) {
            callback();
            return;
        }

        weakThis->m_store.deleteWebsiteDataForRegistrableDomainsInAllPersistentDataStores(WebResourceLoadStatisticsStore::monitoredDataTypes(), WTFMove(prevalentResourceDomains), shouldNotifyPagesWhenDataRecordsWereScanned, [callback = WTFMove(callback), weakThis = WTFMove(weakThis), workQueue = workQueue.copyRef()](const HashSet<RegistrableDomain>& domainsWithDeletedWebsiteData) mutable {
            workQueue->dispatch([domainsWithDeletedWebsiteData = crossThreadCopy(domainsWithDeletedWebsiteData), callback = WTFMove(callback), weakThis = WTFMove(weakThis)] () mutable {
                if (!weakThis) {
                    callback();
                    return;
                }
                weakThis->incrementRecordsDeletedCountForDomains(WTFMove(domainsWithDeletedWebsiteData));
                weakThis->setDataRecordsBeingRemoved(false);
                callback();
#if !RELEASE_LOG_DISABLED
                RELEASE_LOG_INFO_IF(weakThis->m_debugLoggingEnabled, ResourceLoadStatisticsDebug, "Done removing data records.");
#endif
            });
        });
    });
}

void ResourceLoadStatisticsStore::processStatisticsAndDataRecords()
{
    ASSERT(!RunLoop::isMain());

    if (m_parameters.shouldClassifyResourcesBeforeDataRecordsRemoval)
        classifyPrevalentResources();
    
    removeDataRecords([this, weakThis = makeWeakPtr(*this)] () mutable {
        ASSERT(!RunLoop::isMain());
        if (!weakThis)
            return;

        pruneStatisticsIfNeeded();
        syncStorageIfNeeded();

        if (!m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned)
            return;

        RunLoop::main().dispatch([this, weakThis = WTFMove(weakThis)] {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;

            m_store.notifyResourceLoadStatisticsProcessed();
        });
    });
}

void ResourceLoadStatisticsStore::grandfatherExistingWebsiteData(CompletionHandler<void()>&& callback)
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), callback = WTFMove(callback), shouldNotifyPagesWhenDataRecordsWereScanned = m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned, workQueue = m_workQueue.copyRef(), store = makeRef(m_store)] () mutable {
        store->registrableDomainsWithWebsiteData(WebResourceLoadStatisticsStore::monitoredDataTypes(), shouldNotifyPagesWhenDataRecordsWereScanned, [weakThis = WTFMove(weakThis), callback = WTFMove(callback), workQueue = workQueue.copyRef()] (HashSet<RegistrableDomain>&& domainsWithWebsiteData) mutable {
            workQueue->dispatch([weakThis = WTFMove(weakThis), domainsWithWebsiteData = crossThreadCopy(domainsWithWebsiteData), callback = WTFMove(callback)] () mutable {
                if (!weakThis) {
                    callback();
                    return;
                }

                weakThis->grandfatherDataForDomains(domainsWithWebsiteData);
                weakThis->m_endOfGrandfatheringTimestamp = WallTime::now() + weakThis->m_parameters.grandfatheringTime;
                weakThis->syncStorageImmediately();
                callback();
                weakThis->logTestingEvent("Grandfathered"_s);
            });
        });
    });
}

void ResourceLoadStatisticsStore::setResourceLoadStatisticsDebugMode(bool enable)
{
    ASSERT(!RunLoop::isMain());

#if !RELEASE_LOG_DISABLED
    if (enable)
        RELEASE_LOG_INFO(ResourceLoadStatisticsDebug, "Turned ITP Debug Mode on.");
#endif

    m_debugModeEnabled = enable;
    m_debugLoggingEnabled = enable;

    ensurePrevalentResourcesForDebugMode();
    // This will log the current cookie blocking state.
    if (enable)
        updateCookieBlocking([]() { });
}

void ResourceLoadStatisticsStore::setPrevalentResourceForDebugMode(const RegistrableDomain& domain)
{
    m_debugManualPrevalentResource = domain;
}

void ResourceLoadStatisticsStore::scheduleStatisticsProcessingRequestIfNecessary()
{
    ASSERT(!RunLoop::isMain());

    m_pendingStatisticsProcessingRequestIdentifier = ++m_lastStatisticsProcessingRequestIdentifier;
    m_workQueue->dispatchAfter(minimumStatisticsProcessingInterval, [this, weakThis = makeWeakPtr(*this), statisticsProcessingRequestIdentifier = *m_pendingStatisticsProcessingRequestIdentifier] {
        if (!weakThis)
            return;

        if (!m_pendingStatisticsProcessingRequestIdentifier || *m_pendingStatisticsProcessingRequestIdentifier != statisticsProcessingRequestIdentifier) {
            // This request has been canceled.
            return;
        }

        updateCookieBlocking([]() { });
        processStatisticsAndDataRecords();
    });
}

void ResourceLoadStatisticsStore::cancelPendingStatisticsProcessingRequest()
{
    ASSERT(!RunLoop::isMain());

    m_pendingStatisticsProcessingRequestIdentifier = WTF::nullopt;
}

void ResourceLoadStatisticsStore::setTimeToLiveUserInteraction(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.timeToLiveUserInteraction = seconds;
}

void ResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.minimumTimeBetweenDataRecordsRemoval = seconds;
}

void ResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.grandfatheringTime = seconds;
}

void ResourceLoadStatisticsStore::setCacheMaxAgeCap(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.cacheMaxAgeCapTime = seconds;
    updateCacheMaxAgeCap();
}

void ResourceLoadStatisticsStore::updateCacheMaxAgeCap()
{
    ASSERT(!RunLoop::isMain());
    
    RunLoop::main().dispatch([store = makeRef(m_store), seconds = m_parameters.cacheMaxAgeCapTime] () {
        store->setCacheMaxAgeCap(seconds, [] { });
    });
}

void ResourceLoadStatisticsStore::setAgeCapForClientSideCookies(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);
    
    m_parameters.clientSideCookiesAgeCapTime = seconds;
    updateClientSideCookiesAgeCap();
}

void ResourceLoadStatisticsStore::updateClientSideCookiesAgeCap()
{
    ASSERT(!RunLoop::isMain());

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    RunLoop::main().dispatch([store = makeRef(m_store), seconds = m_parameters.clientSideCookiesAgeCapTime] () {
        if (auto* networkSession = store->networkSession())
            networkSession->networkStorageSession().setAgeCapForClientSideCookies(seconds);
    });
#endif
}

bool ResourceLoadStatisticsStore::shouldRemoveDataRecords() const
{
    ASSERT(!RunLoop::isMain());

    if (m_dataRecordsBeingRemoved)
        return false;

#if ENABLE(NETSCAPE_PLUGIN_API)
    for (const auto& plugin : PluginProcessManager::singleton().pluginProcesses()) {
        if (!m_activePluginTokens.contains(plugin->pluginProcessToken()))
            return true;
    }
#endif

    return !m_lastTimeDataRecordsWereRemoved || MonotonicTime::now() >= (m_lastTimeDataRecordsWereRemoved + m_parameters.minimumTimeBetweenDataRecordsRemoval);
}

void ResourceLoadStatisticsStore::setDataRecordsBeingRemoved(bool value)
{
    ASSERT(!RunLoop::isMain());

    m_dataRecordsBeingRemoved = value;
    if (m_dataRecordsBeingRemoved)
        m_lastTimeDataRecordsWereRemoved = MonotonicTime::now();
}

void ResourceLoadStatisticsStore::updateCookieBlockingForDomains(const Vector<RegistrableDomain>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    
    RunLoop::main().dispatch([store = makeRef(m_store), domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)] () mutable {
        store->callUpdatePrevalentDomainsToBlockCookiesForHandler(domainsToBlock, [store = store.copyRef(), completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
            });
        });
    });
}
    

void ResourceLoadStatisticsStore::clearBlockingStateForDomains(const Vector<RegistrableDomain>& domains, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (domains.isEmpty()) {
        completionHandler();
        return;
    }

    RunLoop::main().dispatch([store = makeRef(m_store), domains = crossThreadCopy(domains)] {
        store->callRemoveDomainsHandler(domains);
    });

    completionHandler();
}

Optional<Seconds> ResourceLoadStatisticsStore::statisticsEpirationTime() const
{
    if (m_parameters.timeToLiveUserInteraction)
        return WallTime::now().secondsSinceEpoch() - m_parameters.timeToLiveUserInteraction.value();
    
    if (m_operatingDates.size() >= operatingDatesWindow)
        return m_operatingDates.first().secondsSinceEpoch();
    
    return WTF::nullopt;
}

Vector<OperatingDate> ResourceLoadStatisticsStore::mergeOperatingDates(const Vector<OperatingDate>& existingDates, Vector<OperatingDate>&& newDates)
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

void ResourceLoadStatisticsStore::mergeOperatingDates(Vector<OperatingDate>&& newDates)
{
    m_operatingDates = mergeOperatingDates(m_operatingDates, WTFMove(newDates));
}

void ResourceLoadStatisticsStore::includeTodayAsOperatingDateIfNecessary()
{
    ASSERT(!RunLoop::isMain());

    auto today = OperatingDate::today();
    if (!m_operatingDates.isEmpty() && today <= m_operatingDates.last())
        return;

    while (m_operatingDates.size() >= operatingDatesWindow)
        m_operatingDates.remove(0);

    m_operatingDates.append(today);
}

bool ResourceLoadStatisticsStore::hasStatisticsExpired(WallTime mostRecentUserInteractionTime) const
{
    ASSERT(!RunLoop::isMain());
    
    if (m_operatingDates.size() >= operatingDatesWindow) {
        if (OperatingDate::fromWallTime(mostRecentUserInteractionTime) < m_operatingDates.first())
            return true;
    }
    
    // If we don't meet the real criteria for an expired statistic, check the user setting for a tighter restriction (mainly for testing).
    if (m_parameters.timeToLiveUserInteraction) {
        if (WallTime::now() > mostRecentUserInteractionTime + m_parameters.timeToLiveUserInteraction.value())
            return true;
    }
    
    return false;
}

bool ResourceLoadStatisticsStore::hasStatisticsExpired(const ResourceLoadStatistics& resourceStatistic) const
{
    return hasStatisticsExpired(resourceStatistic.mostRecentUserInteractionTime);
}

void ResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount)
{
    ASSERT(!RunLoop::isMain());

    m_parameters.maxStatisticsEntries = maximumEntryCount;
}

void ResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount)
{
    ASSERT(!RunLoop::isMain());

    m_parameters.pruneEntriesDownTo = pruneTargetCount;
}

void ResourceLoadStatisticsStore::resetParametersToDefaultValues()
{
    ASSERT(!RunLoop::isMain());

    m_parameters = { };
}

void ResourceLoadStatisticsStore::logTestingEvent(const String& event)
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([store = makeRef(m_store), event = event.isolatedCopy()] {
        store->logTestingEvent(event);
    });
}

void ResourceLoadStatisticsStore::removeAllStorageAccess(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch([store = makeRef(m_store), completionHandler = WTFMove(completionHandler)]() mutable {
        store->removeAllStorageAccess([store = store.copyRef(), completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
            });
        });
    });
}

void ResourceLoadStatisticsStore::didCreateNetworkProcess()
{
    ASSERT(!RunLoop::isMain());

    updateCookieBlocking([]() { });
    updateCacheMaxAgeCap();
    updateClientSideCookiesAgeCap();
}

void ResourceLoadStatisticsStore::debugLogDomainsInBatches(const char* action, const Vector<RegistrableDomain>& domains)
{
#if !RELEASE_LOG_DISABLED
    static const auto maxNumberOfDomainsInOneLogStatement = 50;
    if (domains.isEmpty())
        return;
    
    if (domains.size() <= maxNumberOfDomainsInOneLogStatement) {
        RELEASE_LOG_INFO(ResourceLoadStatisticsDebug, "About to %{public}s cookies in third-party contexts for: %{public}s.", action, domainsToString(domains).utf8().data());
        return;
    }
    
    Vector<RegistrableDomain> batch;
    batch.reserveInitialCapacity(maxNumberOfDomainsInOneLogStatement);
    auto batchNumber = 1;
    unsigned numberOfBatches = std::ceil(domains.size() / static_cast<float>(maxNumberOfDomainsInOneLogStatement));
    
    for (auto& domain : domains) {
        if (batch.size() == maxNumberOfDomainsInOneLogStatement) {
            RELEASE_LOG_INFO(ResourceLoadStatisticsDebug, "About to %{public}s cookies in third-party contexts for (%{public}d of %u): %{public}s.", action, batchNumber, numberOfBatches, domainsToString(batch).utf8().data());
            batch.shrink(0);
            ++batchNumber;
        }
        batch.append(domain);
    }
    if (!batch.isEmpty())
        RELEASE_LOG_INFO(ResourceLoadStatisticsDebug, "About to %{public}s cookies in third-party contexts for (%{public}d of %u): %{public}s.", action, batchNumber, numberOfBatches, domainsToString(batch).utf8().data());
#else
    UNUSED_PARAM(action);
    UNUSED_PARAM(domains);
#endif
}

} // namespace WebKit

#endif
