/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "ResourceLoadStatisticsMemoryStore.h"

#include "Logging.h"
#include "PluginProcessManager.h"
#include "PluginProcessProxy.h"
#include "ResourceLoadStatisticsPersistentStorage.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsTelemetry.h"
#include "WebsiteDataStore.h"
#include <WebCore/KeyedCoding.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>

#if !RELEASE_LOG_DISABLED
#include <wtf/text/StringBuilder.h>
#endif

namespace WebKit {
using namespace WebCore;

constexpr unsigned statisticsModelVersion { 14 };
constexpr unsigned maxNumberOfRecursiveCallsInRedirectTraceBack { 50 };
constexpr Seconds minimumStatisticsProcessingInterval { 5_s };
constexpr unsigned operatingDatesWindow { 30 };
constexpr unsigned maxImportance { 3 };
static const char* debugStaticPrevalentResource { "3rdpartytestwebkit.org" };

#if !RELEASE_LOG_DISABLED
static String domainsToString(const Vector<String>& domains)
{
    StringBuilder builder;
    for (auto& domain : domains) {
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.append(domain);
    }
    return builder.toString();
}
#endif

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

ResourceLoadStatisticsMemoryStore::ResourceLoadStatisticsMemoryStore(WebResourceLoadStatisticsStore& store, WorkQueue& workQueue)
    : m_store(store)
    , m_workQueue(workQueue)
{
    ASSERT(!RunLoop::isMain());

#if PLATFORM(COCOA)
    registerUserDefaultsIfNeeded();
#endif
    includeTodayAsOperatingDateIfNecessary();

    m_workQueue->dispatchAfter(5_s, [weakThis = makeWeakPtr(*this)] {
        if (weakThis)
            weakThis->calculateAndSubmitTelemetry();
    });
}

ResourceLoadStatisticsMemoryStore::~ResourceLoadStatisticsMemoryStore()
{
    ASSERT(!RunLoop::isMain());
}

void ResourceLoadStatisticsMemoryStore::setPersistentStorage(ResourceLoadStatisticsPersistentStorage& persistentStorage)
{
    m_persistentStorage = makeWeakPtr(persistentStorage);
}

void ResourceLoadStatisticsMemoryStore::calculateAndSubmitTelemetry() const
{
    ASSERT(!RunLoop::isMain());

    if (m_parameters.shouldSubmitTelemetry)
        WebResourceLoadStatisticsTelemetry::calculateAndSubmit(*this);
}

void ResourceLoadStatisticsMemoryStore::setNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned = value;
}

void ResourceLoadStatisticsMemoryStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldClassifyResourcesBeforeDataRecordsRemoval = value;
}

void ResourceLoadStatisticsMemoryStore::setShouldSubmitTelemetry(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldSubmitTelemetry = value;
}

void ResourceLoadStatisticsMemoryStore::removeDataRecords(CompletionHandler<void()>&& callback)
{
    ASSERT(!RunLoop::isMain());

    if (!shouldRemoveDataRecords()) {
        callback();
        return;
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_activePluginTokens.clear();
    for (auto plugin : PluginProcessManager::singleton().pluginProcesses())
        m_activePluginTokens.add(plugin->pluginProcessToken());
#endif

    auto prevalentResourceDomains = topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    if (prevalentResourceDomains.isEmpty()) {
        callback();
        return;
    }

#if !RELEASE_LOG_DISABLED
    RELEASE_LOG_INFO_IF(m_debugLoggingEnabled, ResourceLoadStatisticsDebug, "About to remove data records for %{public}s.", domainsToString(prevalentResourceDomains).utf8().data());
#endif

    setDataRecordsBeingRemoved(true);

    RunLoop::main().dispatch([prevalentResourceDomains = crossThreadCopy(prevalentResourceDomains), callback = WTFMove(callback), weakThis = makeWeakPtr(*this), shouldNotifyPagesWhenDataRecordsWereScanned = m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned, workQueue = m_workQueue.copyRef()] () mutable {
        WebProcessProxy::deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(WebResourceLoadStatisticsStore::monitoredDataTypes(), WTFMove(prevalentResourceDomains), shouldNotifyPagesWhenDataRecordsWereScanned, [callback = WTFMove(callback), weakThis = WTFMove(weakThis), workQueue = workQueue.copyRef()](const HashSet<String>& domainsWithDeletedWebsiteData) mutable {
            workQueue->dispatch([topDomains = crossThreadCopy(domainsWithDeletedWebsiteData), callback = WTFMove(callback), weakThis = WTFMove(weakThis)] () mutable {
                if (!weakThis) {
                    callback();
                    return;
                }
                for (auto& prevalentResourceDomain : topDomains) {
                    auto& statistic = weakThis->ensureResourceStatisticsForPrimaryDomain(prevalentResourceDomain);
                    ++statistic.dataRecordsRemoved;
                }
                weakThis->setDataRecordsBeingRemoved(false);
                callback();
#if !RELEASE_LOG_DISABLED
                RELEASE_LOG_INFO_IF(weakThis->m_debugLoggingEnabled, ResourceLoadStatisticsDebug, "Done removing data records.");
#endif
            });
        });
    });
}

unsigned ResourceLoadStatisticsMemoryStore::recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(const WebCore::ResourceLoadStatistics& resourceStatistic, HashSet<String>& domainsThatHaveRedirectedTo, unsigned numberOfRecursiveCalls) const
{
    ASSERT(!RunLoop::isMain());

    if (numberOfRecursiveCalls >= maxNumberOfRecursiveCallsInRedirectTraceBack) {
        // Model version 14 invokes a deliberate re-classification of the whole set.
        if (statisticsModelVersion != 14)
            ASSERT_NOT_REACHED();
        RELEASE_LOG(ResourceLoadStatistics, "Hit %u recursive calls in redirect backtrace. Returning early.", maxNumberOfRecursiveCallsInRedirectTraceBack);
        return numberOfRecursiveCalls;
    }

    numberOfRecursiveCalls++;

    for (auto& subresourceUniqueRedirectFromDomain : resourceStatistic.subresourceUniqueRedirectsFrom.values()) {
        auto mapEntry = m_resourceStatisticsMap.find(subresourceUniqueRedirectFromDomain);
        if (mapEntry == m_resourceStatisticsMap.end() || mapEntry->value.isPrevalentResource)
            continue;
        if (domainsThatHaveRedirectedTo.add(mapEntry->value.highLevelDomain).isNewEntry)
            numberOfRecursiveCalls = recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(mapEntry->value, domainsThatHaveRedirectedTo, numberOfRecursiveCalls);
    }
    for (auto& topFrameUniqueRedirectFromDomain : resourceStatistic.topFrameUniqueRedirectsFrom.values()) {
        auto mapEntry = m_resourceStatisticsMap.find(topFrameUniqueRedirectFromDomain);
        if (mapEntry == m_resourceStatisticsMap.end() || mapEntry->value.isPrevalentResource)
            continue;
        if (domainsThatHaveRedirectedTo.add(mapEntry->value.highLevelDomain).isNewEntry)
            numberOfRecursiveCalls = recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(mapEntry->value, domainsThatHaveRedirectedTo, numberOfRecursiveCalls);
    }

    return numberOfRecursiveCalls;
}

void ResourceLoadStatisticsMemoryStore::markAsPrevalentIfHasRedirectedToPrevalent(WebCore::ResourceLoadStatistics& resourceStatistic)
{
    ASSERT(!RunLoop::isMain());

    if (resourceStatistic.isPrevalentResource)
        return;

    for (auto& subresourceDomainRedirectedTo : resourceStatistic.subresourceUniqueRedirectsTo.values()) {
        auto mapEntry = m_resourceStatisticsMap.find(subresourceDomainRedirectedTo);
        if (mapEntry != m_resourceStatisticsMap.end() && mapEntry->value.isPrevalentResource) {
            setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::High);
            return;
        }
    }

    for (auto& topFrameDomainRedirectedTo : resourceStatistic.topFrameUniqueRedirectsTo.values()) {
        auto mapEntry = m_resourceStatisticsMap.find(topFrameDomainRedirectedTo);
        if (mapEntry != m_resourceStatisticsMap.end() && mapEntry->value.isPrevalentResource) {
            setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::High);
            return;
        }
    }
}

bool ResourceLoadStatisticsMemoryStore::isPrevalentDueToDebugMode(ResourceLoadStatistics& resourceStatistic)
{
    if (!m_debugModeEnabled)
        return false;

    return resourceStatistic.highLevelDomain == debugStaticPrevalentResource || resourceStatistic.highLevelDomain == m_debugManualPrevalentResource;
}

void ResourceLoadStatisticsMemoryStore::processStatisticsAndDataRecords()
{
    ASSERT(!RunLoop::isMain());

    if (m_parameters.shouldClassifyResourcesBeforeDataRecordsRemoval) {
        for (auto& resourceStatistic : m_resourceStatisticsMap.values()) {
            if (isPrevalentDueToDebugMode(resourceStatistic))
                setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::High);
            else if (!resourceStatistic.isVeryPrevalentResource) {
                markAsPrevalentIfHasRedirectedToPrevalent(resourceStatistic);
                auto currentPrevalence = resourceStatistic.isPrevalentResource ? ResourceLoadPrevalence::High : ResourceLoadPrevalence::Low;
                auto newPrevalence = m_resourceLoadStatisticsClassifier.calculateResourcePrevalence(resourceStatistic, currentPrevalence);
                if (newPrevalence != currentPrevalence)
                    setPrevalentResource(resourceStatistic, newPrevalence);
            }
        }
    }

    removeDataRecords([this, weakThis = makeWeakPtr(*this)] {
        ASSERT(!RunLoop::isMain());
        if (!weakThis)
            return;

        pruneStatisticsIfNeeded();
        if (m_persistentStorage)
            m_persistentStorage->scheduleOrWriteMemoryStore(ResourceLoadStatisticsPersistentStorage::ForceImmediateWrite::No);

        if (!m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned)
            return;

        RunLoop::main().dispatch([] {
            WebProcessProxy::notifyPageStatisticsAndDataRecordsProcessed();
        });
    });
}

void ResourceLoadStatisticsMemoryStore::hasStorageAccess(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto& subFrameStatistic = ensureResourceStatisticsForPrimaryDomain(subFramePrimaryDomain);
    if (shouldBlockAndPurgeCookies(subFrameStatistic)) {
        completionHandler(false);
        return;
    }

    if (!shouldBlockAndKeepCookies(subFrameStatistic)) {
        completionHandler(true);
        return;
    }

    RunLoop::main().dispatch([store = makeRef(m_store), subFramePrimaryDomain = subFramePrimaryDomain.isolatedCopy(), topFramePrimaryDomain = topFramePrimaryDomain.isolatedCopy(), frameID, pageID, completionHandler = WTFMove(completionHandler)]() mutable {
        store->callHasStorageAccessForFrameHandler(subFramePrimaryDomain, topFramePrimaryDomain, frameID, pageID, [store = store.copyRef(), completionHandler = WTFMove(completionHandler)](bool result) mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler), result] () mutable {
                completionHandler(result);
            });
        });
    });
}

void ResourceLoadStatisticsMemoryStore::requestStorageAccess(String&& subFramePrimaryDomain, String&& topFramePrimaryDomain, uint64_t frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto& subFrameStatistic = ensureResourceStatisticsForPrimaryDomain(subFramePrimaryDomain);
    if (shouldBlockAndPurgeCookies(subFrameStatistic)) {
#if !RELEASE_LOG_DISABLED
        RELEASE_LOG_INFO_IF(m_debugLoggingEnabled, ResourceLoadStatisticsDebug, "Cannot grant storage access to %{public}s since its cookies are blocked in third-party contexts and it has not received user interaction as first-party.", subFramePrimaryDomain.utf8().data());
#endif
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;
    }

    if (!shouldBlockAndKeepCookies(subFrameStatistic)) {
#if !RELEASE_LOG_DISABLED
        RELEASE_LOG_INFO_IF(m_debugLoggingEnabled, ResourceLoadStatisticsDebug, "No need to grant storage access to %{public}s since its cookies are not blocked in third-party contexts.", subFramePrimaryDomain.utf8().data());
#endif
        completionHandler(StorageAccessStatus::HasAccess);
        return;
    }

    auto userWasPromptedEarlier = promptEnabled && hasUserGrantedStorageAccessThroughPrompt(subFrameStatistic, topFramePrimaryDomain);
    if (promptEnabled && !userWasPromptedEarlier) {
#if !RELEASE_LOG_DISABLED
        RELEASE_LOG_INFO_IF(m_debugLoggingEnabled, ResourceLoadStatisticsDebug, "About to ask the user whether they want to grant storage access to %{public}s under %{public}s or not.", subFramePrimaryDomain.utf8().data(), topFramePrimaryDomain.utf8().data());
#endif
        completionHandler(StorageAccessStatus::RequiresUserPrompt);
        return;
    } else if (userWasPromptedEarlier) {
#if !RELEASE_LOG_DISABLED
        RELEASE_LOG_INFO_IF(m_debugLoggingEnabled, ResourceLoadStatisticsDebug, "Storage access was granted to %{public}s under %{public}s.", subFramePrimaryDomain.utf8().data(), topFramePrimaryDomain.utf8().data());
#endif
    }

    subFrameStatistic.timesAccessedAsFirstPartyDueToStorageAccessAPI++;

    grantStorageAccessInternal(WTFMove(subFramePrimaryDomain), WTFMove(topFramePrimaryDomain), frameID, pageID, userWasPromptedEarlier, [completionHandler = WTFMove(completionHandler)] (bool wasGrantedAccess) mutable {
        completionHandler(wasGrantedAccess ? StorageAccessStatus::HasAccess : StorageAccessStatus::CannotRequestAccess);
    });
}

void ResourceLoadStatisticsMemoryStore::requestStorageAccessUnderOpener(String&& primaryDomainInNeedOfStorageAccess, uint64_t openerPageID, String&& openerPrimaryDomain)
{
    ASSERT(primaryDomainInNeedOfStorageAccess != openerPrimaryDomain);
    ASSERT(!RunLoop::isMain());

    if (primaryDomainInNeedOfStorageAccess == openerPrimaryDomain)
        return;

    auto& domainInNeedOfStorageAccessStatistic = ensureResourceStatisticsForPrimaryDomain(primaryDomainInNeedOfStorageAccess);
    auto cookiesBlockedAndPurged = shouldBlockAndPurgeCookies(domainInNeedOfStorageAccessStatistic);

    // The domain already has access if its cookies are not blocked.
    if (!cookiesBlockedAndPurged && !shouldBlockAndKeepCookies(domainInNeedOfStorageAccessStatistic))
        return;

#if !RELEASE_LOG_DISABLED
    RELEASE_LOG_INFO_IF(m_debugLoggingEnabled, ResourceLoadStatisticsDebug, "[Temporary combatibility fix] Storage access was granted for %{public}s under opener page from %{public}s, with user interaction in the opened window.", primaryDomainInNeedOfStorageAccess.utf8().data(), openerPrimaryDomain.utf8().data());
#endif
    grantStorageAccessInternal(WTFMove(primaryDomainInNeedOfStorageAccess), WTFMove(openerPrimaryDomain), std::nullopt, openerPageID, false, [](bool) { });
}

void ResourceLoadStatisticsMemoryStore::grantStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto subFramePrimaryDomain = ResourceLoadStatistics::primaryDomain(subFrameHost);
    auto topFramePrimaryDomain = ResourceLoadStatistics::primaryDomain(topFrameHost);
    if (userWasPromptedNow) {
        auto& subFrameStatistic = ensureResourceStatisticsForPrimaryDomain(subFramePrimaryDomain);
        ASSERT(subFrameStatistic.hadUserInteraction);
        subFrameStatistic.storageAccessUnderTopFrameOrigins.add(topFramePrimaryDomain);
    }
    grantStorageAccessInternal(WTFMove(subFramePrimaryDomain), WTFMove(topFramePrimaryDomain), frameID, pageID, userWasPromptedNow, WTFMove(completionHandler));
}

void ResourceLoadStatisticsMemoryStore::grantStorageAccessInternal(String&& subFramePrimaryDomain, String&& topFramePrimaryDomain, std::optional<uint64_t> frameID, uint64_t pageID, bool userWasPromptedNowOrEarlier, CompletionHandler<void(bool)>&& callback)
{
    ASSERT(!RunLoop::isMain());

    if (subFramePrimaryDomain == topFramePrimaryDomain) {
        callback(true);
        return;
    }

    // FIXME: Remove m_storageAccessPromptsEnabled check if prompting is no longer experimental.
    if (userWasPromptedNowOrEarlier && m_storageAccessPromptsEnabled) {
        auto& subFrameStatistic = ensureResourceStatisticsForPrimaryDomain(subFramePrimaryDomain);
        ASSERT(subFrameStatistic.hadUserInteraction);
        ASSERT(subFrameStatistic.storageAccessUnderTopFrameOrigins.contains(topFramePrimaryDomain));
        subFrameStatistic.mostRecentUserInteractionTime = WallTime::now();
    }

    RunLoop::main().dispatch([subFramePrimaryDomain = subFramePrimaryDomain.isolatedCopy(), topFramePrimaryDomain = topFramePrimaryDomain.isolatedCopy(), frameID, pageID, store = makeRef(m_store), callback = WTFMove(callback)]() mutable {
        store->callGrantStorageAccessHandler(subFramePrimaryDomain, topFramePrimaryDomain, frameID, pageID, [callback = WTFMove(callback), store = store.copyRef()](bool value) mutable {
            store->statisticsQueue().dispatch([callback = WTFMove(callback), value] () mutable {
                callback(value);
            });
        });
    });
}

void ResourceLoadStatisticsMemoryStore::grandfatherExistingWebsiteData(CompletionHandler<void()>&& callback)
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), callback = WTFMove(callback), shouldNotifyPagesWhenDataRecordsWereScanned = m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned, workQueue = m_workQueue.copyRef()] () mutable {
        // FIXME: This method being a static call on WebProcessProxy is wrong.
        // It should be on the data store that this object belongs to.
        WebProcessProxy::topPrivatelyControlledDomainsWithWebsiteData(WebResourceLoadStatisticsStore::monitoredDataTypes(), shouldNotifyPagesWhenDataRecordsWereScanned, [weakThis = WTFMove(weakThis), callback = WTFMove(callback), workQueue = workQueue.copyRef()] (HashSet<String>&& topPrivatelyControlledDomainsWithWebsiteData) mutable {
            workQueue->dispatch([weakThis = WTFMove(weakThis), topDomains = crossThreadCopy(topPrivatelyControlledDomainsWithWebsiteData), callback = WTFMove(callback)] () mutable {
                if (!weakThis) {
                    callback();
                    return;
                }

                for (auto& topPrivatelyControlledDomain : topDomains) {
                    auto& statistic = weakThis->ensureResourceStatisticsForPrimaryDomain(topPrivatelyControlledDomain);
                    statistic.grandfathered = true;
                }
                weakThis->m_endOfGrandfatheringTimestamp = WallTime::now() + weakThis->m_parameters.grandfatheringTime;
                if (weakThis->m_persistentStorage)
                    weakThis->m_persistentStorage->scheduleOrWriteMemoryStore(ResourceLoadStatisticsPersistentStorage::ForceImmediateWrite::Yes);
                callback();
                weakThis->logTestingEvent("Grandfathered"_s);
            });
        });
    });
}

Vector<String> ResourceLoadStatisticsMemoryStore::ensurePrevalentResourcesForDebugMode()
{
    if (!m_debugModeEnabled)
        return { };

    Vector<String> primaryDomainsToBlock;
    primaryDomainsToBlock.reserveInitialCapacity(2);

    auto& staticSesourceStatistic = ensureResourceStatisticsForPrimaryDomain(debugStaticPrevalentResource);
    setPrevalentResource(staticSesourceStatistic, ResourceLoadPrevalence::High);
    primaryDomainsToBlock.uncheckedAppend(debugStaticPrevalentResource);

    if (!m_debugManualPrevalentResource.isEmpty()) {
        auto& manualResourceStatistic = ensureResourceStatisticsForPrimaryDomain(m_debugManualPrevalentResource);
        setPrevalentResource(manualResourceStatistic, ResourceLoadPrevalence::High);
        primaryDomainsToBlock.uncheckedAppend(m_debugManualPrevalentResource);
    }
    
    return primaryDomainsToBlock;
}

void ResourceLoadStatisticsMemoryStore::setResourceLoadStatisticsDebugMode(bool enable)
{
    ASSERT(!RunLoop::isMain());

    m_debugModeEnabled = enable;
    m_debugLoggingEnabled = enable;

    ensurePrevalentResourcesForDebugMode();
}

void ResourceLoadStatisticsMemoryStore::setPrevalentResourceForDebugMode(const String& domain)
{
    if (!m_debugModeEnabled)
        return;

    m_debugManualPrevalentResource = domain;
    auto& resourceStatistic = ensureResourceStatisticsForPrimaryDomain(domain);
    setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::High);

#if !RELEASE_LOG_DISABLED
    RELEASE_LOG_INFO(ResourceLoadStatisticsDebug, "Did set %{public}s as prevalent resource for the purposes of ITP Debug Mode.", domain.utf8().data());
#endif
}

void ResourceLoadStatisticsMemoryStore::scheduleStatisticsProcessingRequestIfNecessary()
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

void ResourceLoadStatisticsMemoryStore::cancelPendingStatisticsProcessingRequest()
{
    ASSERT(!RunLoop::isMain());

    m_pendingStatisticsProcessingRequestIdentifier = std::nullopt;
}

void ResourceLoadStatisticsMemoryStore::logFrameNavigation(const String& targetPrimaryDomain, const String& mainFramePrimaryDomain, const String& sourcePrimaryDomain, const String& targetHost, const String& mainFrameHost, bool isRedirect, bool isMainFrame)
{
    ASSERT(!RunLoop::isMain());

    bool areTargetAndMainFrameDomainsAssociated = targetPrimaryDomain == mainFramePrimaryDomain;
    bool areTargetAndSourceDomainsAssociated = targetPrimaryDomain == sourcePrimaryDomain;

    bool statisticsWereUpdated = false;
    if (!isMainFrame && targetHost != mainFrameHost && !(areTargetAndMainFrameDomainsAssociated || areTargetAndSourceDomainsAssociated)) {
        auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
        targetStatistics.lastSeen = ResourceLoadStatistics::reduceTimeResolution(WallTime::now());
        if (targetStatistics.subframeUnderTopFrameOrigins.add(mainFramePrimaryDomain).isNewEntry)
            statisticsWereUpdated = true;
    }

    if (isRedirect && !areTargetAndSourceDomainsAssociated) {
        if (isMainFrame) {
            auto& redirectingOriginStatistics = ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);
            if (redirectingOriginStatistics.topFrameUniqueRedirectsTo.add(targetPrimaryDomain).isNewEntry)
                statisticsWereUpdated = true;
            auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
            if (targetStatistics.topFrameUniqueRedirectsFrom.add(sourcePrimaryDomain).isNewEntry)
                statisticsWereUpdated = true;
        } else {
            auto& redirectingOriginStatistics = ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);
            if (redirectingOriginStatistics.subresourceUniqueRedirectsTo.add(targetPrimaryDomain).isNewEntry)
                statisticsWereUpdated = true;
            auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
            if (targetStatistics.subresourceUniqueRedirectsFrom.add(sourcePrimaryDomain).isNewEntry)
                statisticsWereUpdated = true;
        }
    }

    if (statisticsWereUpdated)
        scheduleStatisticsProcessingRequestIfNecessary();
}

void ResourceLoadStatisticsMemoryStore::logUserInteraction(const String& primaryDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
    statistics.hadUserInteraction = true;
    statistics.mostRecentUserInteractionTime = WallTime::now();
}

void ResourceLoadStatisticsMemoryStore::clearUserInteraction(const String& primaryDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
    statistics.hadUserInteraction = false;
    statistics.mostRecentUserInteractionTime = { };
}

bool ResourceLoadStatisticsMemoryStore::hasHadUserInteraction(const String& primaryDomain)
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
    return mapEntry == m_resourceStatisticsMap.end() ? false: hasHadUnexpiredRecentUserInteraction(mapEntry->value);
}

void ResourceLoadStatisticsMemoryStore::setPrevalentResource(WebCore::ResourceLoadStatistics& resourceStatistic, ResourceLoadPrevalence newPrevalence)
{
    ASSERT(!RunLoop::isMain());

    resourceStatistic.isPrevalentResource = true;
    resourceStatistic.isVeryPrevalentResource = newPrevalence == ResourceLoadPrevalence::VeryHigh;
    HashSet<String> domainsThatHaveRedirectedTo;
    recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(resourceStatistic, domainsThatHaveRedirectedTo, 0);
    for (auto& domain : domainsThatHaveRedirectedTo) {
        auto mapEntry = m_resourceStatisticsMap.find(domain);
        if (mapEntry == m_resourceStatisticsMap.end())
            continue;
        ASSERT(!mapEntry->value.isPrevalentResource);
        mapEntry->value.isPrevalentResource = true;
    }
}
    
String ResourceLoadStatisticsMemoryStore::dumpResourceLoadStatistics() const
{
    ASSERT(!RunLoop::isMain());

    StringBuilder result;
    result.appendLiteral("Resource load statistics:\n\n");
    for (auto& mapEntry : m_resourceStatisticsMap.values())
        result.append(mapEntry.toString());
    return result.toString();
}

bool ResourceLoadStatisticsMemoryStore::isPrevalentResource(const String& primaryDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.isPrevalentResource;
}

bool ResourceLoadStatisticsMemoryStore::isVeryPrevalentResource(const String& primaryDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.isPrevalentResource && mapEntry->value.isVeryPrevalentResource;
}

bool ResourceLoadStatisticsMemoryStore::isRegisteredAsSubresourceUnder(const String& subresourcePrimaryDomain, const String& topFramePrimaryDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(subresourcePrimaryDomain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.subresourceUnderTopFrameOrigins.contains(topFramePrimaryDomain);
}

bool ResourceLoadStatisticsMemoryStore::isRegisteredAsSubFrameUnder(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(subFramePrimaryDomain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.subframeUnderTopFrameOrigins.contains(topFramePrimaryDomain);
}

bool ResourceLoadStatisticsMemoryStore::isRegisteredAsRedirectingTo(const String& hostRedirectedFromPrimaryDomain, const String& hostRedirectedToPrimaryDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(hostRedirectedFromPrimaryDomain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.subresourceUniqueRedirectsTo.contains(hostRedirectedToPrimaryDomain);
}

void ResourceLoadStatisticsMemoryStore::clearPrevalentResource(const String& primaryDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
    statistics.isPrevalentResource = false;
    statistics.isVeryPrevalentResource = false;
}

void ResourceLoadStatisticsMemoryStore::setGrandfathered(const String& primaryDomain, bool value)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
    statistics.grandfathered = value;
}

bool ResourceLoadStatisticsMemoryStore::isGrandfathered(const String& primaryDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.grandfathered;
}

void ResourceLoadStatisticsMemoryStore::setSubframeUnderTopFrameOrigin(const String& primarySubFrameDomain, const String& primaryTopFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(primarySubFrameDomain);
    statistics.subframeUnderTopFrameOrigins.add(primaryTopFrameDomain);
    // For consistency, make sure we also have a statistics entry for the top frame domain.
    ensureResourceStatisticsForPrimaryDomain(primaryTopFrameDomain);
}

void ResourceLoadStatisticsMemoryStore::setSubresourceUnderTopFrameOrigin(const String& primarySubresourceDomain, const String& primaryTopFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomain);
    statistics.subresourceUnderTopFrameOrigins.add(primaryTopFrameDomain);
    // For consistency, make sure we also have a statistics entry for the top frame domain.
    ensureResourceStatisticsForPrimaryDomain(primaryTopFrameDomain);
}

void ResourceLoadStatisticsMemoryStore::setSubresourceUniqueRedirectTo(const String& primarySubresourceDomain, const String& primaryRedirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomain);
    statistics.subresourceUniqueRedirectsTo.add(primaryRedirectDomain);
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForPrimaryDomain(primaryRedirectDomain);
}

void ResourceLoadStatisticsMemoryStore::setSubresourceUniqueRedirectFrom(const String& primarySubresourceDomain, const String& primaryRedirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomain);
    statistics.subresourceUniqueRedirectsFrom.add(primaryRedirectDomain);
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForPrimaryDomain(primaryRedirectDomain);
}

void ResourceLoadStatisticsMemoryStore::setTopFrameUniqueRedirectTo(const String& topFramePrimaryDomain, const String& primaryRedirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(topFramePrimaryDomain);
    statistics.topFrameUniqueRedirectsTo.add(primaryRedirectDomain);
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForPrimaryDomain(primaryRedirectDomain);
}

void ResourceLoadStatisticsMemoryStore::setTopFrameUniqueRedirectFrom(const String& topFramePrimaryDomain, const String& primaryRedirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(topFramePrimaryDomain);
    statistics.topFrameUniqueRedirectsFrom.add(primaryRedirectDomain);
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForPrimaryDomain(primaryRedirectDomain);
}

void ResourceLoadStatisticsMemoryStore::setTimeToLiveUserInteraction(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.timeToLiveUserInteraction = seconds;
}

void ResourceLoadStatisticsMemoryStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.minimumTimeBetweenDataRecordsRemoval = seconds;
}

void ResourceLoadStatisticsMemoryStore::setGrandfatheringTime(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.grandfatheringTime = seconds;
}

void ResourceLoadStatisticsMemoryStore::setCacheMaxAgeCap(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.cacheMaxAgeCapTime = seconds;
    updateCacheMaxAgeCap();
}

void ResourceLoadStatisticsMemoryStore::updateCacheMaxAgeCap()
{
    ASSERT(!RunLoop::isMain());
    
    RunLoop::main().dispatch([store = makeRef(m_store), seconds = m_parameters.cacheMaxAgeCapTime] () {
        store->setCacheMaxAgeCap(seconds, [] { });
    });
}

void ResourceLoadStatisticsMemoryStore::setAgeCapForClientSideCookies(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);
    
    m_parameters.clientSideCookiesAgeCapTime = seconds;
    updateClientSideCookiesAgeCap();
}

void ResourceLoadStatisticsMemoryStore::updateClientSideCookiesAgeCap()
{
    ASSERT(!RunLoop::isMain());

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    RunLoop::main().dispatch([store = makeRef(m_store), seconds = m_parameters.clientSideCookiesAgeCapTime] () {
        if (auto* websiteDataStore = store->websiteDataStore())
            websiteDataStore->setAgeCapForClientSideCookies(seconds, [] { });
    });
#endif
}

bool ResourceLoadStatisticsMemoryStore::shouldRemoveDataRecords() const
{
    ASSERT(!RunLoop::isMain());

    if (m_dataRecordsBeingRemoved)
        return false;

#if ENABLE(NETSCAPE_PLUGIN_API)
    for (auto plugin : PluginProcessManager::singleton().pluginProcesses()) {
        if (!m_activePluginTokens.contains(plugin->pluginProcessToken()))
            return true;
    }
#endif

    return !m_lastTimeDataRecordsWereRemoved || MonotonicTime::now() >= (m_lastTimeDataRecordsWereRemoved + m_parameters.minimumTimeBetweenDataRecordsRemoval);
}

void ResourceLoadStatisticsMemoryStore::setDataRecordsBeingRemoved(bool value)
{
    ASSERT(!RunLoop::isMain());

    m_dataRecordsBeingRemoved = value;
    if (m_dataRecordsBeingRemoved)
        m_lastTimeDataRecordsWereRemoved = MonotonicTime::now();
}

ResourceLoadStatistics& ResourceLoadStatisticsMemoryStore::ensureResourceStatisticsForPrimaryDomain(const String& primaryDomain)
{
    ASSERT(!RunLoop::isMain());

    return m_resourceStatisticsMap.ensure(primaryDomain, [&primaryDomain] {
        return ResourceLoadStatistics(primaryDomain);
    }).iterator->value;
}

std::unique_ptr<KeyedEncoder> ResourceLoadStatisticsMemoryStore::createEncoderFromData() const
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

void ResourceLoadStatisticsMemoryStore::mergeWithDataFromDecoder(KeyedDecoder& decoder)
{
    ASSERT(!RunLoop::isMain());

    unsigned versionOnDisk;
    if (!decoder.decodeUInt32("version", versionOnDisk))
        return;

    if (versionOnDisk > statisticsModelVersion) {
        WTFLogAlways("Found resource load statistics on disk with model version %u whereas the highest supported version is %u. Resetting.", versionOnDisk, statisticsModelVersion);
        return;
    }

    double endOfGrandfatheringTimestamp;
    if (decoder.decodeDouble("endOfGrandfatheringTimestamp", endOfGrandfatheringTimestamp))
        m_endOfGrandfatheringTimestamp = WallTime::fromRawSeconds(endOfGrandfatheringTimestamp);
    else
        m_endOfGrandfatheringTimestamp = { };

    Vector<ResourceLoadStatistics> loadedStatistics;
    bool succeeded = decoder.decodeObjects("browsingStatistics", loadedStatistics, [versionOnDisk](KeyedDecoder& decoderInner, ResourceLoadStatistics& statistics) {
        return statistics.decode(decoderInner, versionOnDisk);
    });

    if (!succeeded)
        return;

    mergeStatistics(WTFMove(loadedStatistics));
    updateCookieBlocking([]() { });

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

void ResourceLoadStatisticsMemoryStore::clear(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    m_resourceStatisticsMap.clear();
    m_operatingDates.clear();

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    removeAllStorageAccess([callbackAggregator = callbackAggregator.copyRef()] { });

    auto primaryDomainsToBlock = ensurePrevalentResourcesForDebugMode();
    updateCookieBlockingForDomains(primaryDomainsToBlock, [callbackAggregator = callbackAggregator.copyRef()] { });
}

bool ResourceLoadStatisticsMemoryStore::wasAccessedAsFirstPartyDueToUserInteraction(const ResourceLoadStatistics& current, const ResourceLoadStatistics& updated) const
{
    if (!current.hadUserInteraction && !updated.hadUserInteraction)
        return false;

    auto mostRecentUserInteractionTime = std::max(current.mostRecentUserInteractionTime, updated.mostRecentUserInteractionTime);

    return updated.lastSeen <= mostRecentUserInteractionTime + 24_h;
}

void ResourceLoadStatisticsMemoryStore::mergeStatistics(Vector<ResourceLoadStatistics>&& statistics)
{
    ASSERT(!RunLoop::isMain());

    for (auto& statistic : statistics) {
        auto result = m_resourceStatisticsMap.ensure(statistic.highLevelDomain, [&statistic] {
            return WTFMove(statistic);
        });
        if (!result.isNewEntry) {
            if (wasAccessedAsFirstPartyDueToUserInteraction(result.iterator->value, statistic))
                result.iterator->value.timesAccessedAsFirstPartyDueToUserInteraction++;
            result.iterator->value.merge(statistic);
        }
    }
}

bool ResourceLoadStatisticsMemoryStore::shouldBlockAndKeepCookies(const ResourceLoadStatistics& statistic)
{
    return statistic.isPrevalentResource && statistic.hadUserInteraction;
}

bool ResourceLoadStatisticsMemoryStore::shouldBlockAndPurgeCookies(const ResourceLoadStatistics& statistic)
{
    return statistic.isPrevalentResource && !statistic.hadUserInteraction;
}

bool ResourceLoadStatisticsMemoryStore::hasUserGrantedStorageAccessThroughPrompt(const ResourceLoadStatistics& statistic, const String& firstPartyPrimaryDomain)
{
    return statistic.storageAccessUnderTopFrameOrigins.contains(firstPartyPrimaryDomain);
}

static void debugLogDomainsInBatches(const char* action, const Vector<String>& domains)
{
#if !RELEASE_LOG_DISABLED
    static const auto maxNumberOfDomainsInOneLogStatement = 50;
    if (domains.isEmpty())
        return;

    if (domains.size() <= maxNumberOfDomainsInOneLogStatement) {
        RELEASE_LOG_INFO(ResourceLoadStatisticsDebug, "About to %{public}s cookies in third-party contexts for: %{public}s.", action, domainsToString(domains).utf8().data());
        return;
    }
    
    Vector<String> batch;
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

void ResourceLoadStatisticsMemoryStore::updateCookieBlocking(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    Vector<String> domainsToBlock;
    for (auto& resourceStatistic : m_resourceStatisticsMap.values()) {
        if (resourceStatistic.isPrevalentResource)
            domainsToBlock.append(resourceStatistic.highLevelDomain);
    }

    if (domainsToBlock.isEmpty()) {
        completionHandler();
        return;
    }

    if (m_debugLoggingEnabled && !domainsToBlock.isEmpty())
            debugLogDomainsInBatches("block", domainsToBlock);

    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), store = makeRef(m_store), domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)] () mutable {
        store->callUpdatePrevalentDomainsToBlockCookiesForHandler(domainsToBlock, [weakThis = WTFMove(weakThis), store = store.copyRef(), completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
                if (!weakThis)
                    return;
#if !RELEASE_LOG_DISABLED
                RELEASE_LOG_INFO_IF(weakThis->m_debugLoggingEnabled, ResourceLoadStatisticsDebug, "Done updating cookie blocking.");
#endif
            });
        });
    });
}

void ResourceLoadStatisticsMemoryStore::updateCookieBlockingForDomains(const Vector<String>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
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

void ResourceLoadStatisticsMemoryStore::clearBlockingStateForDomains(const Vector<String>& domains, CompletionHandler<void()>&& completionHandler)
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

void ResourceLoadStatisticsMemoryStore::processStatistics(const Function<void(const ResourceLoadStatistics&)>& processFunction) const
{
    ASSERT(!RunLoop::isMain());

    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        processFunction(resourceStatistic);
}

bool ResourceLoadStatisticsMemoryStore::hasHadUnexpiredRecentUserInteraction(ResourceLoadStatistics& resourceStatistic) const
{
    ASSERT(!RunLoop::isMain());

    if (resourceStatistic.hadUserInteraction && hasStatisticsExpired(resourceStatistic)) {
        // Drop privacy sensitive data because we no longer need it.
        // Set timestamp to 0 so that statistics merge will know
        // it has been reset as opposed to its default -1.
        resourceStatistic.mostRecentUserInteractionTime = { };
        resourceStatistic.storageAccessUnderTopFrameOrigins.clear();
        resourceStatistic.hadUserInteraction = false;
    }

    return resourceStatistic.hadUserInteraction;
}

Vector<String> ResourceLoadStatisticsMemoryStore::topPrivatelyControlledDomainsToRemoveWebsiteDataFor()
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

void ResourceLoadStatisticsMemoryStore::includeTodayAsOperatingDateIfNecessary()
{
    ASSERT(!RunLoop::isMain());

    auto today = OperatingDate::today();
    if (!m_operatingDates.isEmpty() && today <= m_operatingDates.last())
        return;

    while (m_operatingDates.size() >= operatingDatesWindow)
        m_operatingDates.remove(0);

    m_operatingDates.append(today);
}

bool ResourceLoadStatisticsMemoryStore::hasStatisticsExpired(const ResourceLoadStatistics& resourceStatistic) const
{
    ASSERT(!RunLoop::isMain());

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

void ResourceLoadStatisticsMemoryStore::setMaxStatisticsEntries(size_t maximumEntryCount)
{
    ASSERT(!RunLoop::isMain());

    m_parameters.maxStatisticsEntries = maximumEntryCount;
}

void ResourceLoadStatisticsMemoryStore::setPruneEntriesDownTo(size_t pruneTargetCount)
{
    ASSERT(!RunLoop::isMain());

    m_parameters.pruneEntriesDownTo = pruneTargetCount;
}

void ResourceLoadStatisticsMemoryStore::pruneStatisticsIfNeeded()
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

void ResourceLoadStatisticsMemoryStore::resetParametersToDefaultValues()
{
    ASSERT(!RunLoop::isMain());

    m_parameters = { };
}

void ResourceLoadStatisticsMemoryStore::logTestingEvent(const String& event)
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([store = makeRef(m_store), event = event.isolatedCopy()] {
        store->logTestingEvent(event);
    });
}

void ResourceLoadStatisticsMemoryStore::setLastSeen(const String& primaryDomain, Seconds seconds)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
    statistics.lastSeen = WallTime::fromRawSeconds(seconds.seconds());
}

void ResourceLoadStatisticsMemoryStore::setPrevalentResource(const String& primaryDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& resourceStatistic = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
    setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::High);
}

void ResourceLoadStatisticsMemoryStore::setVeryPrevalentResource(const String& primaryDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& resourceStatistic = ensureResourceStatisticsForPrimaryDomain(primaryDomain);
    setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::VeryHigh);
}

void ResourceLoadStatisticsMemoryStore::removeAllStorageAccess(CompletionHandler<void()>&& completionHandler)
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

void ResourceLoadStatisticsMemoryStore::didCreateNetworkProcess()
{
    ASSERT(!RunLoop::isMain());

    updateCookieBlocking([]() { });
    updateCacheMaxAgeCap();
    updateClientSideCookiesAgeCap();
}

} // namespace WebKit
