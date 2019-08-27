/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/KeyedCoding.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/UserGestureIndicator.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

constexpr unsigned statisticsModelVersion { 16 };

struct StatisticsLastSeen {
    RegistrableDomain domain;
    WallTime lastSeen;
};

static void pruneResources(HashMap<RegistrableDomain, ResourceLoadStatistics>& statisticsMap, Vector<StatisticsLastSeen>& statisticsToPrune, size_t& numberOfEntriesToPrune)
{
    if (statisticsToPrune.size() > numberOfEntriesToPrune) {
        std::sort(statisticsToPrune.begin(), statisticsToPrune.end(), [](const StatisticsLastSeen& a, const StatisticsLastSeen& b) {
            return a.lastSeen < b.lastSeen;
        });
    }

    for (size_t i = 0, end = std::min(numberOfEntriesToPrune, statisticsToPrune.size()); i != end; ++i, --numberOfEntriesToPrune)
        statisticsMap.remove(statisticsToPrune[i].domain);
}

ResourceLoadStatisticsMemoryStore::ResourceLoadStatisticsMemoryStore(WebResourceLoadStatisticsStore& store, WorkQueue& workQueue, ShouldIncludeLocalhost shouldIncludeLocalhost)
    : ResourceLoadStatisticsStore(store, workQueue, shouldIncludeLocalhost)
{
    RELEASE_ASSERT(!RunLoop::isMain());

    workQueue.dispatchAfter(5_s, [weakThis = makeWeakPtr(*this)] {
        if (weakThis)
            weakThis->calculateAndSubmitTelemetry();
    });
}

bool ResourceLoadStatisticsMemoryStore::isEmpty() const
{
    RELEASE_ASSERT(!RunLoop::isMain());

    return m_resourceStatisticsMap.isEmpty();
}

void ResourceLoadStatisticsMemoryStore::setPersistentStorage(ResourceLoadStatisticsPersistentStorage& persistentStorage)
{
    ASSERT(!RunLoop::isMain());

    m_persistentStorage = makeWeakPtr(persistentStorage);
}

void ResourceLoadStatisticsMemoryStore::calculateAndSubmitTelemetry() const
{
    ASSERT(!RunLoop::isMain());

    if (parameters().shouldSubmitTelemetry)
        WebResourceLoadStatisticsTelemetry::calculateAndSubmit(*this);
}

void ResourceLoadStatisticsMemoryStore::incrementRecordsDeletedCountForDomains(HashSet<RegistrableDomain>&& domainsWithDeletedWebsiteData)
{
    ASSERT(!RunLoop::isMain());

    for (auto& domain : domainsWithDeletedWebsiteData) {
        auto& statistic = ensureResourceStatisticsForRegistrableDomain(domain);
        ++statistic.dataRecordsRemoved;
    }
}

unsigned ResourceLoadStatisticsMemoryStore::recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(const ResourceLoadStatistics& resourceStatistic, HashSet<RegistrableDomain>& domainsThatHaveRedirectedTo, unsigned numberOfRecursiveCalls) const
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

    for (auto& subresourceUniqueRedirectFromDomain : resourceStatistic.subresourceUniqueRedirectsFrom) {
        auto mapEntry = m_resourceStatisticsMap.find(RegistrableDomain { subresourceUniqueRedirectFromDomain });
        if (mapEntry == m_resourceStatisticsMap.end() || mapEntry->value.isPrevalentResource)
            continue;
        if (domainsThatHaveRedirectedTo.add(mapEntry->value.registrableDomain).isNewEntry)
            numberOfRecursiveCalls = recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(mapEntry->value, domainsThatHaveRedirectedTo, numberOfRecursiveCalls);
    }
    for (auto& topFrameUniqueRedirectFromDomain : resourceStatistic.topFrameUniqueRedirectsFrom) {
        auto mapEntry = m_resourceStatisticsMap.find(RegistrableDomain { topFrameUniqueRedirectFromDomain });
        if (mapEntry == m_resourceStatisticsMap.end() || mapEntry->value.isPrevalentResource)
            continue;
        if (domainsThatHaveRedirectedTo.add(mapEntry->value.registrableDomain).isNewEntry)
            numberOfRecursiveCalls = recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(mapEntry->value, domainsThatHaveRedirectedTo, numberOfRecursiveCalls);
    }

    return numberOfRecursiveCalls;
}

void ResourceLoadStatisticsMemoryStore::markAsPrevalentIfHasRedirectedToPrevalent(ResourceLoadStatistics& resourceStatistic)
{
    ASSERT(!RunLoop::isMain());

    if (resourceStatistic.isPrevalentResource)
        return;

    for (auto& subresourceDomainRedirectedTo : resourceStatistic.subresourceUniqueRedirectsTo) {
        auto mapEntry = m_resourceStatisticsMap.find(RegistrableDomain { subresourceDomainRedirectedTo });
        if (mapEntry != m_resourceStatisticsMap.end() && mapEntry->value.isPrevalentResource) {
            setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::High);
            return;
        }
    }

    for (auto& topFrameDomainRedirectedTo : resourceStatistic.topFrameUniqueRedirectsTo) {
        auto mapEntry = m_resourceStatisticsMap.find(RegistrableDomain { topFrameDomainRedirectedTo });
        if (mapEntry != m_resourceStatisticsMap.end() && mapEntry->value.isPrevalentResource) {
            setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::High);
            return;
        }
    }
}

bool ResourceLoadStatisticsMemoryStore::isPrevalentDueToDebugMode(ResourceLoadStatistics& resourceStatistic)
{
    if (!debugModeEnabled())
        return false;

    return resourceStatistic.registrableDomain == debugStaticPrevalentResource() || resourceStatistic.registrableDomain == debugManualPrevalentResource();
}

void ResourceLoadStatisticsMemoryStore::classifyPrevalentResources()
{
    ASSERT(!RunLoop::isMain());

    for (auto& resourceStatistic : m_resourceStatisticsMap.values()) {
        if (shouldSkip(resourceStatistic.registrableDomain))
            continue;
        if (isPrevalentDueToDebugMode(resourceStatistic))
            setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::High);
        else if (!resourceStatistic.isVeryPrevalentResource) {
            markAsPrevalentIfHasRedirectedToPrevalent(resourceStatistic);
            auto currentPrevalence = resourceStatistic.isPrevalentResource ? ResourceLoadPrevalence::High : ResourceLoadPrevalence::Low;
            auto newPrevalence = classifier().calculateResourcePrevalence(resourceStatistic, currentPrevalence);
            if (newPrevalence != currentPrevalence)
                setPrevalentResource(resourceStatistic, newPrevalence);
        }
    }
}

void ResourceLoadStatisticsMemoryStore::syncStorageIfNeeded()
{
    ASSERT(!RunLoop::isMain());

    if (m_persistentStorage)
        m_persistentStorage->scheduleOrWriteMemoryStore(ResourceLoadStatisticsPersistentStorage::ForceImmediateWrite::No);
}

void ResourceLoadStatisticsMemoryStore::syncStorageImmediately()
{
    ASSERT(!RunLoop::isMain());

    if (m_persistentStorage)
        m_persistentStorage->scheduleOrWriteMemoryStore(ResourceLoadStatisticsPersistentStorage::ForceImmediateWrite::Yes);
}

void ResourceLoadStatisticsMemoryStore::hasStorageAccess(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto& subFrameStatistic = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    if (shouldBlockAndPurgeCookies(subFrameStatistic)) {
        completionHandler(false);
        return;
    }

    if (!shouldBlockAndKeepCookies(subFrameStatistic)) {
        completionHandler(true);
        return;
    }

    RunLoop::main().dispatch([store = makeRef(store()), subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, completionHandler = WTFMove(completionHandler)]() mutable {
        store->callHasStorageAccessForFrameHandler(subFrameDomain, topFrameDomain, frameID.value(), pageID, [store = store.copyRef(), completionHandler = WTFMove(completionHandler)](bool result) mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler), result] () mutable {
                completionHandler(result);
            });
        });
    });
}

void ResourceLoadStatisticsMemoryStore::requestStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto& subFrameStatistic = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    if (shouldBlockAndPurgeCookies(subFrameStatistic)) {
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ITPDebug, "Cannot grant storage access to %{public}s since its cookies are blocked in third-party contexts and it has not received user interaction as first-party.", subFrameDomain.string().utf8().data());
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;
    }

    if (!shouldBlockAndKeepCookies(subFrameStatistic)) {
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ITPDebug, "No need to grant storage access to %{public}s since its cookies are not blocked in third-party contexts.", subFrameDomain.string().utf8().data());
        completionHandler(StorageAccessStatus::HasAccess);
        return;
    }

    auto userWasPromptedEarlier = hasUserGrantedStorageAccessThroughPrompt(subFrameStatistic, topFrameDomain);
    if (userWasPromptedEarlier == StorageAccessPromptWasShown::No) {
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ITPDebug, "About to ask the user whether they want to grant storage access to %{public}s under %{public}s or not.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
        completionHandler(StorageAccessStatus::RequiresUserPrompt);
        return;
    }

    if (userWasPromptedEarlier == StorageAccessPromptWasShown::Yes)
        RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ITPDebug, "Storage access was granted to %{public}s under %{public}s.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());

    subFrameStatistic.timesAccessedAsFirstPartyDueToStorageAccessAPI++;

    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, userWasPromptedEarlier, [completionHandler = WTFMove(completionHandler)] (StorageAccessWasGranted wasGranted) mutable {
        completionHandler(wasGranted == StorageAccessWasGranted::Yes ? StorageAccessStatus::HasAccess : StorageAccessStatus::CannotRequestAccess);
    });
}

void ResourceLoadStatisticsMemoryStore::requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, OpenerDomain&& openerDomain)
{
    ASSERT(domainInNeedOfStorageAccess != openerDomain);
    ASSERT(!RunLoop::isMain());

    if (domainInNeedOfStorageAccess == openerDomain)
        return;

    auto& domainInNeedOfStorageAccessStatistic = ensureResourceStatisticsForRegistrableDomain(domainInNeedOfStorageAccess);
    auto cookiesBlockedAndPurged = shouldBlockAndPurgeCookies(domainInNeedOfStorageAccessStatistic);

    // The domain already has access if its cookies are not blocked.
    if (!cookiesBlockedAndPurged && !shouldBlockAndKeepCookies(domainInNeedOfStorageAccessStatistic))
        return;

    RELEASE_LOG_INFO_IF(debugLoggingEnabled(), ITPDebug, "[Temporary combatibility fix] Storage access was granted for %{public}s under opener page from %{public}s, with user interaction in the opened window.", domainInNeedOfStorageAccess.string().utf8().data(), openerDomain.string().utf8().data());
    grantStorageAccessInternal(WTFMove(domainInNeedOfStorageAccess), WTFMove(openerDomain), WTF::nullopt, openerPageID, StorageAccessPromptWasShown::No, [](StorageAccessWasGranted) { });
}

void ResourceLoadStatisticsMemoryStore::grantStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (promptWasShown == StorageAccessPromptWasShown::Yes) {
        auto& subFrameStatistic = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        ASSERT(subFrameStatistic.hadUserInteraction);
        subFrameStatistic.storageAccessUnderTopFrameDomains.add(topFrameDomain);
    }
    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, promptWasShown, WTFMove(completionHandler));
}

void ResourceLoadStatisticsMemoryStore::grantStorageAccessInternal(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShownNowOrEarlier, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (subFrameDomain == topFrameDomain) {
        completionHandler(StorageAccessWasGranted::Yes);
        return;
    }

    if (promptWasShownNowOrEarlier == StorageAccessPromptWasShown::Yes) {
        auto& subFrameStatistic = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        ASSERT(subFrameStatistic.hadUserInteraction);
        ASSERT(subFrameStatistic.storageAccessUnderTopFrameDomains.contains(topFrameDomain));
        subFrameStatistic.mostRecentUserInteractionTime = WallTime::now();
    }

    RunLoop::main().dispatch([subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, store = makeRef(store()), completionHandler = WTFMove(completionHandler)]() mutable {
        store->callGrantStorageAccessHandler(subFrameDomain, topFrameDomain, frameID, pageID, [completionHandler = WTFMove(completionHandler), store = store.copyRef()](StorageAccessWasGranted wasGranted) mutable {
            store->statisticsQueue().dispatch([wasGranted, completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(wasGranted);
            });
        });
    });
}

void ResourceLoadStatisticsMemoryStore::grandfatherDataForDomains(const HashSet<RegistrableDomain>& domains)
{
    ASSERT(!RunLoop::isMain());

    for (auto& domain : domains) {
        auto& statistic = ensureResourceStatisticsForRegistrableDomain(domain);
        statistic.grandfathered = true;
    }
}

Vector<RegistrableDomain> ResourceLoadStatisticsMemoryStore::ensurePrevalentResourcesForDebugMode()
{
    ASSERT(!RunLoop::isMain());

    if (!debugModeEnabled())
        return { };

    Vector<RegistrableDomain> domainsToBlock;
    domainsToBlock.reserveInitialCapacity(2);

    auto& staticSesourceStatistic = ensureResourceStatisticsForRegistrableDomain(debugStaticPrevalentResource());
    setPrevalentResource(staticSesourceStatistic, ResourceLoadPrevalence::High);
    domainsToBlock.uncheckedAppend(debugStaticPrevalentResource());

    if (!debugManualPrevalentResource().isEmpty()) {
        auto& manualResourceStatistic = ensureResourceStatisticsForRegistrableDomain(debugManualPrevalentResource());
        setPrevalentResource(manualResourceStatistic, ResourceLoadPrevalence::High);
        domainsToBlock.uncheckedAppend(debugManualPrevalentResource());
        RELEASE_LOG_INFO(ITPDebug, "Did set %{public}s as prevalent resource for the purposes of ITP Debug Mode.", debugManualPrevalentResource().string().utf8().data());
    }
    
    return domainsToBlock;
}

void ResourceLoadStatisticsMemoryStore::logFrameNavigation(const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, const RegistrableDomain& sourceDomain, bool isRedirect, bool isMainFrame)
{
    ASSERT(!RunLoop::isMain());

    bool areTargetAndTopFrameDomainsSameSite = targetDomain == topFrameDomain;
    bool areTargetAndSourceDomainsSameSite = targetDomain == sourceDomain;

    bool statisticsWereUpdated = false;
    if (!isMainFrame && !(areTargetAndTopFrameDomainsSameSite || areTargetAndSourceDomainsSameSite)) {
        auto& targetStatistics = ensureResourceStatisticsForRegistrableDomain(targetDomain);
        targetStatistics.lastSeen = ResourceLoadStatistics::reduceTimeResolution(WallTime::now());
        if (targetStatistics.subframeUnderTopFrameDomains.add(topFrameDomain).isNewEntry)
            statisticsWereUpdated = true;
    }

    if (isRedirect && !areTargetAndSourceDomainsSameSite) {
        if (isMainFrame) {
            auto& redirectingDomainStatistics = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
            if (redirectingDomainStatistics.topFrameUniqueRedirectsTo.add(targetDomain).isNewEntry)
                statisticsWereUpdated = true;
            auto& targetStatistics = ensureResourceStatisticsForRegistrableDomain(targetDomain);
            if (targetStatistics.topFrameUniqueRedirectsFrom.add(sourceDomain).isNewEntry)
                statisticsWereUpdated = true;
        } else {
            auto& redirectingDomainStatistics = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
            if (redirectingDomainStatistics.subresourceUniqueRedirectsTo.add(targetDomain).isNewEntry)
                statisticsWereUpdated = true;
            auto& targetStatistics = ensureResourceStatisticsForRegistrableDomain(targetDomain);
            if (targetStatistics.subresourceUniqueRedirectsFrom.add(sourceDomain).isNewEntry)
                statisticsWereUpdated = true;
        }
    }

    if (statisticsWereUpdated)
        scheduleStatisticsProcessingRequestIfNecessary();
}

void ResourceLoadStatisticsMemoryStore::logUserInteraction(const TopFrameDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(domain);
    statistics.hadUserInteraction = true;
    statistics.mostRecentUserInteractionTime = WallTime::now();
}

void ResourceLoadStatisticsMemoryStore::logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain& fromDomain, const NavigatedToDomain& toDomain)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(fromDomain != toDomain);

    auto& toStatistics = ensureResourceStatisticsForRegistrableDomain(toDomain);
    toStatistics.topFrameLinkDecorationsFrom.add(fromDomain);
    
    auto& fromStatistics = ensureResourceStatisticsForRegistrableDomain(fromDomain);
    if (fromStatistics.isPrevalentResource)
        toStatistics.gotLinkDecorationFromPrevalentResource = true;
}

void ResourceLoadStatisticsMemoryStore::clearUserInteraction(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(domain);
    statistics.hadUserInteraction = false;
    statistics.mostRecentUserInteractionTime = { };
}

bool ResourceLoadStatisticsMemoryStore::hasHadUserInteraction(const RegistrableDomain& domain, OperatingDatesWindow operatingDatesWindow)
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(domain);
    return mapEntry == m_resourceStatisticsMap.end() ? false: hasHadUnexpiredRecentUserInteraction(mapEntry->value, operatingDatesWindow);
}

void ResourceLoadStatisticsMemoryStore::setPrevalentResource(ResourceLoadStatistics& resourceStatistic, ResourceLoadPrevalence newPrevalence)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(resourceStatistic.registrableDomain))
        return;

    resourceStatistic.isPrevalentResource = true;
    resourceStatistic.isVeryPrevalentResource = newPrevalence == ResourceLoadPrevalence::VeryHigh;
    HashSet<RegistrableDomain> domainsThatHaveRedirectedTo;
    recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(resourceStatistic, domainsThatHaveRedirectedTo, 0);
    for (auto& domain : domainsThatHaveRedirectedTo) {
        auto mapEntry = m_resourceStatisticsMap.find(domain);
        if (mapEntry == m_resourceStatisticsMap.end())
            continue;
        ASSERT(!mapEntry->value.isPrevalentResource);
        mapEntry->value.isPrevalentResource = true;
    }
}
    
void ResourceLoadStatisticsMemoryStore::dumpResourceLoadStatistics(CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    if (dataRecordsBeingRemoved()) {
        m_dataRecordRemovalCompletionHandlers.append([this, completionHandler = WTFMove(completionHandler)]() mutable {
            dumpResourceLoadStatistics(WTFMove(completionHandler));
        });
        return;
    }

    StringBuilder result;
    result.appendLiteral("Resource load statistics:\n\n");
    for (auto& mapEntry : m_resourceStatisticsMap.values())
        result.append(mapEntry.toString());
    completionHandler(result.toString());
}

bool ResourceLoadStatisticsMemoryStore::isPrevalentResource(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return false;

    auto mapEntry = m_resourceStatisticsMap.find(domain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.isPrevalentResource;
}

bool ResourceLoadStatisticsMemoryStore::isVeryPrevalentResource(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return false;
    
    auto mapEntry = m_resourceStatisticsMap.find(domain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.isPrevalentResource && mapEntry->value.isVeryPrevalentResource;
}

bool ResourceLoadStatisticsMemoryStore::isRegisteredAsSubresourceUnder(const SubResourceDomain& subresourceDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(subresourceDomain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.subresourceUnderTopFrameDomains.contains(topFrameDomain);
}

bool ResourceLoadStatisticsMemoryStore::isRegisteredAsSubFrameUnder(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(subFrameDomain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.subframeUnderTopFrameDomains.contains(topFrameDomain);
}

bool ResourceLoadStatisticsMemoryStore::isRegisteredAsRedirectingTo(const RedirectedFromDomain& redirectedFromDomain, const RedirectedToDomain& redirectedToDomain) const
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(redirectedFromDomain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.subresourceUniqueRedirectsTo.contains(redirectedToDomain);
}

void ResourceLoadStatisticsMemoryStore::clearPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(domain);
    statistics.isPrevalentResource = false;
    statistics.isVeryPrevalentResource = false;
}

void ResourceLoadStatisticsMemoryStore::setGrandfathered(const RegistrableDomain& domain, bool value)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(domain);
    statistics.grandfathered = value;
}

bool ResourceLoadStatisticsMemoryStore::isGrandfathered(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());

    auto mapEntry = m_resourceStatisticsMap.find(domain);
    return mapEntry == m_resourceStatisticsMap.end() ? false : mapEntry->value.grandfathered;
}

void ResourceLoadStatisticsMemoryStore::setSubframeUnderTopFrameDomain(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    statistics.subframeUnderTopFrameDomains.add(topFrameDomain);
    // For consistency, make sure we also have a statistics entry for the top frame domain.
    ensureResourceStatisticsForRegistrableDomain(topFrameDomain);
}

void ResourceLoadStatisticsMemoryStore::setSubresourceUnderTopFrameDomain(const SubResourceDomain& subresourceDomain, const RegistrableDomain& topFrameDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);
    statistics.subresourceUnderTopFrameDomains.add(topFrameDomain);
    // For consistency, make sure we also have a statistics entry for the top frame domain.
    ensureResourceStatisticsForRegistrableDomain(topFrameDomain);
}

void ResourceLoadStatisticsMemoryStore::setSubresourceUniqueRedirectTo(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);
    statistics.subresourceUniqueRedirectsTo.add(redirectDomain);
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForRegistrableDomain(redirectDomain);
}

void ResourceLoadStatisticsMemoryStore::setSubresourceUniqueRedirectFrom(const SubResourceDomain& subresourceDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(subresourceDomain);
    statistics.subresourceUniqueRedirectsFrom.add(redirectDomain);
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForRegistrableDomain(redirectDomain);
}

void ResourceLoadStatisticsMemoryStore::setTopFrameUniqueRedirectTo(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);
    statistics.topFrameUniqueRedirectsTo.add(redirectDomain);
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForRegistrableDomain(redirectDomain);
}

void ResourceLoadStatisticsMemoryStore::setTopFrameUniqueRedirectFrom(const TopFrameDomain& topFrameDomain, const RedirectDomain& redirectDomain)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);
    statistics.topFrameUniqueRedirectsFrom.add(redirectDomain);
    // For consistency, make sure we also have a statistics entry for the redirect domain.
    ensureResourceStatisticsForRegistrableDomain(redirectDomain);
}

ResourceLoadStatistics& ResourceLoadStatisticsMemoryStore::ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    return m_resourceStatisticsMap.ensure(domain, [&domain] {
        return ResourceLoadStatistics(domain);
    }).iterator->value;
}

std::unique_ptr<KeyedEncoder> ResourceLoadStatisticsMemoryStore::createEncoderFromData() const
{
    ASSERT(!RunLoop::isMain());

    auto encoder = KeyedEncoder::encoder();
    encoder->encodeUInt32("version", statisticsModelVersion);
    encoder->encodeDouble("endOfGrandfatheringTimestamp", endOfGrandfatheringTimestamp().secondsSinceEpoch().value());

    encoder->encodeObjects("browsingStatistics", m_resourceStatisticsMap.begin(), m_resourceStatisticsMap.end(), [](KeyedEncoder& encoderInner, const auto& domain) {
        domain.value.encode(encoderInner);
    });

    auto& operatingDates = this->operatingDates();
    encoder->encodeObjects("operatingDates", operatingDates.begin(), operatingDates.end(), [](KeyedEncoder& encoderInner, OperatingDate date) {
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
        setEndOfGrandfatheringTimestamp(WallTime::fromRawSeconds(endOfGrandfatheringTimestamp));
    else
        clearEndOfGrandfatheringTimeStamp();

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

    mergeOperatingDates(WTFMove(operatingDates));
}

void ResourceLoadStatisticsMemoryStore::clear(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    m_resourceStatisticsMap.clear();
    clearOperatingDates();

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    removeAllStorageAccess([callbackAggregator = callbackAggregator.copyRef()] { });

    auto registrableDomainsToBlockAndDeleteCookiesFor = ensurePrevalentResourcesForDebugMode();
    RegistrableDomainsToBlockCookiesFor domainsToBlock { registrableDomainsToBlockAndDeleteCookiesFor, { } };
    updateCookieBlockingForDomains(domainsToBlock, [callbackAggregator = callbackAggregator.copyRef()] { });
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
        auto result = m_resourceStatisticsMap.ensure(statistic.registrableDomain, [&statistic] {
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

StorageAccessPromptWasShown ResourceLoadStatisticsMemoryStore::hasUserGrantedStorageAccessThroughPrompt(const ResourceLoadStatistics& statistic, const RegistrableDomain& firstPartyDomain)
{
    return statistic.storageAccessUnderTopFrameDomains.contains(firstPartyDomain) ? StorageAccessPromptWasShown::Yes : StorageAccessPromptWasShown::No;
}

void ResourceLoadStatisticsMemoryStore::updateCookieBlocking(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    Vector<RegistrableDomain> domainsToBlockAndDeleteCookiesFor;
    Vector<RegistrableDomain> domainsToBlockButKeepCookiesFor;
    for (auto& resourceStatistic : m_resourceStatisticsMap.values()) {
        if (resourceStatistic.isPrevalentResource) {
            if (hasHadUnexpiredRecentUserInteraction(resourceStatistic, OperatingDatesWindow::Long))
                domainsToBlockButKeepCookiesFor.append(resourceStatistic.registrableDomain);
            else
                domainsToBlockAndDeleteCookiesFor.append(resourceStatistic.registrableDomain);
        }
    }

    if (domainsToBlockAndDeleteCookiesFor.isEmpty() && domainsToBlockButKeepCookiesFor.isEmpty() && !debugModeEnabled()) {
        completionHandler();
        return;
    }

    RegistrableDomainsToBlockCookiesFor domainsToBlock { domainsToBlockAndDeleteCookiesFor, domainsToBlockButKeepCookiesFor };

    if (debugLoggingEnabled() && !domainsToBlockAndDeleteCookiesFor.isEmpty() && !domainsToBlockButKeepCookiesFor.isEmpty())
        debugLogDomainsInBatches("block", domainsToBlock);

    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), store = makeRef(store()), domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)] () mutable {
        store->callUpdatePrevalentDomainsToBlockCookiesForHandler(domainsToBlock, [weakThis = WTFMove(weakThis), store = store.copyRef(), completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
                if (!weakThis)
                    return;
                RELEASE_LOG_INFO_IF(weakThis->debugLoggingEnabled(), ITPDebug, "Done updating cookie blocking.");
            });
        });
    });
}

void ResourceLoadStatisticsMemoryStore::processStatistics(const Function<void(const ResourceLoadStatistics&)>& processFunction) const
{
    ASSERT(!RunLoop::isMain());

    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        processFunction(resourceStatistic);
}

bool ResourceLoadStatisticsMemoryStore::hasHadUnexpiredRecentUserInteraction(ResourceLoadStatistics& resourceStatistic, OperatingDatesWindow operatingDatesWindow) const
{
    ASSERT(!RunLoop::isMain());

    if (resourceStatistic.hadUserInteraction && hasStatisticsExpired(resourceStatistic, operatingDatesWindow)) {
        // Drop privacy sensitive data because we no longer need it.
        // Set timestamp to 0 so that statistics merge will know
        // it has been reset as opposed to its default -1.
        resourceStatistic.mostRecentUserInteractionTime = { };
        resourceStatistic.storageAccessUnderTopFrameDomains.clear();
        resourceStatistic.hadUserInteraction = false;
    }

    return resourceStatistic.hadUserInteraction;
}

bool ResourceLoadStatisticsMemoryStore::shouldRemoveAllWebsiteDataFor(ResourceLoadStatistics& resourceStatistic, bool shouldCheckForGrandfathering) const
{
    return resourceStatistic.isPrevalentResource && !hasHadUnexpiredRecentUserInteraction(resourceStatistic, OperatingDatesWindow::Long) && (!shouldCheckForGrandfathering || !resourceStatistic.grandfathered);
}

bool ResourceLoadStatisticsMemoryStore::shouldRemoveAllButCookiesFor(ResourceLoadStatistics& resourceStatistic, bool shouldCheckForGrandfathering) const
{
    return resourceStatistic.gotLinkDecorationFromPrevalentResource && !hasHadUnexpiredRecentUserInteraction(resourceStatistic, OperatingDatesWindow::Short) && (!shouldCheckForGrandfathering || !resourceStatistic.grandfathered);
}

Vector<std::pair<RegistrableDomain, WebsiteDataToRemove>> ResourceLoadStatisticsMemoryStore::registrableDomainsToRemoveWebsiteDataFor()
{
    ASSERT(!RunLoop::isMain());

    bool shouldCheckForGrandfathering = endOfGrandfatheringTimestamp() > WallTime::now();
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && endOfGrandfatheringTimestamp();

    if (shouldClearGrandfathering)
        clearEndOfGrandfatheringTimeStamp();

    Vector<std::pair<RegistrableDomain, WebsiteDataToRemove>> domainsToRemoveWebsiteDataFor;
    for (auto& statistic : m_resourceStatisticsMap.values()) {
        if (shouldRemoveAllWebsiteDataFor(statistic, shouldCheckForGrandfathering))
            domainsToRemoveWebsiteDataFor.append(std::make_pair(statistic.registrableDomain, WebsiteDataToRemove::All));
        else if (shouldRemoveAllButCookiesFor(statistic, shouldCheckForGrandfathering)) {
            domainsToRemoveWebsiteDataFor.append(std::make_pair(statistic.registrableDomain, WebsiteDataToRemove::AllButCookies));
            statistic.gotLinkDecorationFromPrevalentResource = false;
        }

        if (shouldClearGrandfathering && statistic.grandfathered)
            statistic.grandfathered = false;
    }

    return domainsToRemoveWebsiteDataFor;
}

void ResourceLoadStatisticsMemoryStore::pruneStatisticsIfNeeded()
{
    ASSERT(!RunLoop::isMain());

    if (m_resourceStatisticsMap.size() <= parameters().maxStatisticsEntries)
        return;

    ASSERT(parameters().pruneEntriesDownTo <= parameters().maxStatisticsEntries);

    size_t numberOfEntriesLeftToPrune = m_resourceStatisticsMap.size() - parameters().pruneEntriesDownTo;
    ASSERT(numberOfEntriesLeftToPrune);

    Vector<StatisticsLastSeen> resourcesToPrunePerImportance[maxImportance + 1];
    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        resourcesToPrunePerImportance[computeImportance(resourceStatistic)].append({ resourceStatistic.registrableDomain, resourceStatistic.lastSeen });

    for (unsigned importance = 0; numberOfEntriesLeftToPrune && importance <= maxImportance; ++importance)
        pruneResources(m_resourceStatisticsMap, resourcesToPrunePerImportance[importance], numberOfEntriesLeftToPrune);

    ASSERT(!numberOfEntriesLeftToPrune);
}

void ResourceLoadStatisticsMemoryStore::setLastSeen(const RegistrableDomain& domain, Seconds seconds)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(domain);
    statistics.lastSeen = WallTime::fromRawSeconds(seconds.seconds());
}

void ResourceLoadStatisticsMemoryStore::setPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;
    
    auto& resourceStatistic = ensureResourceStatisticsForRegistrableDomain(domain);
    setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::High);
}

void ResourceLoadStatisticsMemoryStore::setVeryPrevalentResource(const RegistrableDomain& domain)
{
    ASSERT(!RunLoop::isMain());

    if (shouldSkip(domain))
        return;
    
    auto& resourceStatistic = ensureResourceStatisticsForRegistrableDomain(domain);
    setPrevalentResource(resourceStatistic, ResourceLoadPrevalence::VeryHigh);
}

} // namespace WebKit

#endif
