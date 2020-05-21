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
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/KeyedCoding.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/UserGestureIndicator.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

constexpr unsigned statisticsModelVersion { 17 };

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

    includeTodayAsOperatingDateIfNecessary();
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

static void ensureThirdPartyDataForSpecificFirstPartyDomain(Vector<WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty>& thirdPartyDataForSpecificFirstPartyDomain, const RegistrableDomain& firstPartyDomain, bool thirdPartyHasStorageAccess)
{
    WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty thirdPartyDataForSpecificFirstParty { firstPartyDomain, thirdPartyHasStorageAccess, Seconds { ResourceLoadStatistics::NoExistingTimestamp }};
    thirdPartyDataForSpecificFirstPartyDomain.appendIfNotContains(thirdPartyDataForSpecificFirstParty);
}

static Vector<WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty> getThirdPartyDataForSpecificFirstPartyDomains(const ResourceLoadStatistics& thirdPartyStatistic)
{
    Vector<WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty> thirdPartyDataForSpecificFirstPartyDomains;

    for (auto firstPartyDomain : thirdPartyStatistic.subframeUnderTopFrameDomains)
        ensureThirdPartyDataForSpecificFirstPartyDomain(thirdPartyDataForSpecificFirstPartyDomains, firstPartyDomain, thirdPartyStatistic.storageAccessUnderTopFrameDomains.contains(firstPartyDomain));
    for (auto firstPartyDomain : thirdPartyStatistic.subresourceUnderTopFrameDomains)
        ensureThirdPartyDataForSpecificFirstPartyDomain(thirdPartyDataForSpecificFirstPartyDomains, firstPartyDomain, thirdPartyStatistic.storageAccessUnderTopFrameDomains.contains(firstPartyDomain));
    for (auto firstPartyDomain : thirdPartyStatistic.subresourceUniqueRedirectsTo)
        ensureThirdPartyDataForSpecificFirstPartyDomain(thirdPartyDataForSpecificFirstPartyDomains, firstPartyDomain, thirdPartyStatistic.storageAccessUnderTopFrameDomains.contains(firstPartyDomain));

    return thirdPartyDataForSpecificFirstPartyDomains;
}

static bool hasBeenThirdParty(const ResourceLoadStatistics& statistic)
{
    return !statistic.subframeUnderTopFrameDomains.isEmpty()
        || !statistic.subresourceUnderTopFrameDomains.isEmpty()
        || !statistic.subresourceUniqueRedirectsTo.isEmpty();
}

Vector<WebResourceLoadStatisticsStore::ThirdPartyData> ResourceLoadStatisticsMemoryStore::aggregatedThirdPartyData() const
{
    ASSERT(!RunLoop::isMain());

    Vector<WebResourceLoadStatisticsStore::ThirdPartyData> thirdPartyDataList;
    for (auto& statistic : m_resourceStatisticsMap.values()) {
        if (hasBeenThirdParty(statistic) && statistic.isPrevalentResource)
            thirdPartyDataList.append(WebResourceLoadStatisticsStore::ThirdPartyData { statistic.registrableDomain, getThirdPartyDataForSpecificFirstPartyDomains(statistic) });
    }
    std::sort(thirdPartyDataList.rbegin(), thirdPartyDataList.rend());
    return thirdPartyDataList;
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

bool ResourceLoadStatisticsMemoryStore::areAllThirdPartyCookiesBlockedUnder(const TopFrameDomain& topFrameDomain)
{
    if (thirdPartyCookieBlockingMode() == ThirdPartyCookieBlockingMode::All)
        return true;

    if (thirdPartyCookieBlockingMode() == ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction && !hasHadUserInteraction(topFrameDomain, OperatingDatesWindow::Long))
        return true;

    return false;
}

CookieAccess ResourceLoadStatisticsMemoryStore::cookieAccess(const ResourceLoadStatistics& resourceStatistic, const TopFrameDomain& topFrameDomain)
{
    bool isPrevalent = resourceStatistic.isPrevalentResource;
    bool hadUserInteraction = resourceStatistic.hadUserInteraction;

    if (!areAllThirdPartyCookiesBlockedUnder(topFrameDomain) && !isPrevalent)
        return CookieAccess::BasedOnCookiePolicy;

    if (!hadUserInteraction)
        return CookieAccess::CannotRequest;
    
    return CookieAccess::OnlyIfGranted;
}

void ResourceLoadStatisticsMemoryStore::hasStorageAccess(const SubFrameDomain& subFrameDomain, const TopFrameDomain& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto& subFrameStatistic = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    switch (cookieAccess(subFrameStatistic, topFrameDomain)) {
    case CookieAccess::CannotRequest:
        completionHandler(false);
        return;
    case CookieAccess::BasedOnCookiePolicy:
        RunLoop::main().dispatch([store = makeRef(store()), subFrameDomain = subFrameDomain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            store->hasCookies(subFrameDomain, [store = store.copyRef(), completionHandler = WTFMove(completionHandler)](bool result) mutable {
                store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler), result] () mutable {
                    completionHandler(result);
                });
            });
        });
        return;
    case CookieAccess::OnlyIfGranted:
        // Handled below.
        break;
    }

    RunLoop::main().dispatch([store = makeRef(store()), subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, completionHandler = WTFMove(completionHandler)]() mutable {
        store->callHasStorageAccessForFrameHandler(subFrameDomain, topFrameDomain, frameID.value(), pageID, [store = store.copyRef(), completionHandler = WTFMove(completionHandler)](bool result) mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler), result] () mutable {
                completionHandler(result);
            });
        });
    });
}

void ResourceLoadStatisticsMemoryStore::requestStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessScope scope, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto& subFrameStatistic = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
    switch (cookieAccess(subFrameStatistic, topFrameDomain)) {
    case CookieAccess::CannotRequest:
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "Cannot grant storage access to %" PUBLIC_LOG_STRING " since its cookies are blocked in third-party contexts and it has not received user interaction as first-party.", subFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Cannot grant storage access to '"_s, subFrameDomain.string(), "' since its cookies are blocked in third-party contexts and it has not received user interaction as first-party."_s));
        }
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;

    case CookieAccess::BasedOnCookiePolicy:
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "No need to grant storage access to %" PUBLIC_LOG_STRING " since its cookies are not blocked in third-party contexts. Note that the underlying cookie policy may still block this third-party from setting cookies.", subFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] No need to grant storage access to '"_s, subFrameDomain.string(), "' since its cookies are not blocked in third-party contexts. Note that the underlying cookie policy may still block this third-party from setting cookies."_s));
        }
        completionHandler(StorageAccessStatus::HasAccess);
        return;

    case CookieAccess::OnlyIfGranted:
        // Handled below.
        break;
    }

    auto userWasPromptedEarlier = hasUserGrantedStorageAccessThroughPrompt(subFrameStatistic, topFrameDomain);
    if (userWasPromptedEarlier == StorageAccessPromptWasShown::No) {
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "About to ask the user whether they want to grant storage access to %" PUBLIC_LOG_STRING " under %" PUBLIC_LOG_STRING " or not.", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] About to ask the user whether they want to grant storage access to '"_s, subFrameDomain.string(), "' under '"_s, topFrameDomain.string(), "' or not."_s));
        }
        completionHandler(StorageAccessStatus::RequiresUserPrompt);
        return;
    }

    if (userWasPromptedEarlier == StorageAccessPromptWasShown::Yes) {
        if (UNLIKELY(debugLoggingEnabled())) {
            RELEASE_LOG_INFO(ITPDebug, "Storage access was granted to %" PUBLIC_LOG_STRING " under %" PUBLIC_LOG_STRING ".", subFrameDomain.string().utf8().data(), topFrameDomain.string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Storage access was granted to '"_s, subFrameDomain.string(), "' under '"_s, topFrameDomain.string(), "'."_s));
        }
    }

    subFrameStatistic.timesAccessedAsFirstPartyDueToStorageAccessAPI++;

    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, userWasPromptedEarlier, scope, [completionHandler = WTFMove(completionHandler)] (StorageAccessWasGranted wasGranted) mutable {
        completionHandler(wasGranted == StorageAccessWasGranted::Yes ? StorageAccessStatus::HasAccess : StorageAccessStatus::CannotRequestAccess);
    });
}

void ResourceLoadStatisticsMemoryStore::requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, OpenerDomain&& openerDomain)
{
    ASSERT(domainInNeedOfStorageAccess != openerDomain);
    ASSERT(!RunLoop::isMain());

    if (domainInNeedOfStorageAccess == openerDomain)
        return;

    if (UNLIKELY(debugLoggingEnabled())) {
        RELEASE_LOG_INFO(ITPDebug, "[Temporary combatibility fix] Storage access was granted for %" PUBLIC_LOG_STRING " under opener page from %" PUBLIC_LOG_STRING ", with user interaction in the opened window.", domainInNeedOfStorageAccess.string().utf8().data(), openerDomain.string().utf8().data());
        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Storage access was granted for '"_s, domainInNeedOfStorageAccess.string(), "' under opener page from '"_s, openerDomain.string(), "', with user interaction in the opened window."_s));
    }

    grantStorageAccessInternal(WTFMove(domainInNeedOfStorageAccess), WTFMove(openerDomain), WTF::nullopt, openerPageID, StorageAccessPromptWasShown::No, StorageAccessScope::PerPage, [](StorageAccessWasGranted) { });
}

void ResourceLoadStatisticsMemoryStore::grantStorageAccess(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, StorageAccessScope scope, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (promptWasShown == StorageAccessPromptWasShown::Yes) {
        auto& subFrameStatistic = ensureResourceStatisticsForRegistrableDomain(subFrameDomain);
        ASSERT(subFrameStatistic.hadUserInteraction);
        subFrameStatistic.storageAccessUnderTopFrameDomains.add(topFrameDomain);
    }
    grantStorageAccessInternal(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, promptWasShown, scope, WTFMove(completionHandler));
}

void ResourceLoadStatisticsMemoryStore::grantStorageAccessInternal(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShownNowOrEarlier, StorageAccessScope scope, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
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

    RunLoop::main().dispatch([subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, store = makeRef(store()), scope, completionHandler = WTFMove(completionHandler)]() mutable {
        store->callGrantStorageAccessHandler(subFrameDomain, topFrameDomain, frameID, pageID, scope, [completionHandler = WTFMove(completionHandler), store = store.copyRef()](StorageAccessWasGranted wasGranted) mutable {
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

        if (debugLoggingEnabled()) {
            RELEASE_LOG_INFO(ITPDebug, "Did set %" PUBLIC_LOG_STRING " as prevalent resource for the purposes of ITP Debug Mode.", debugManualPrevalentResource().string().utf8().data());
            debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] Did set '"_s, debugManualPrevalentResource().string(), "' as prevalent resource for the purposes of ITP Debug Mode."_s));
        }
    }
    
    return domainsToBlock;
}

void ResourceLoadStatisticsMemoryStore::logFrameNavigation(const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, const RegistrableDomain& sourceDomain, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser)
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

    if (!areTargetAndSourceDomainsSameSite) {
        if (isMainFrame) {
            auto& redirectingDomainStatistics = ensureResourceStatisticsForRegistrableDomain(sourceDomain);
            bool wasNavigatedAfterShortDelayWithoutUserInteraction = !wasPotentiallyInitiatedByUser && delayAfterMainFrameDocumentLoad < parameters().minDelayAfterMainFrameDocumentLoadToNotBeARedirect;
            if (isRedirect || wasNavigatedAfterShortDelayWithoutUserInteraction) {
                if (redirectingDomainStatistics.topFrameUniqueRedirectsTo.add(targetDomain).isNewEntry)
                    statisticsWereUpdated = true;
                if (isRedirect && redirectingDomainStatistics.topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement.add(targetDomain).isNewEntry) {
                    statisticsWereUpdated = true;

                    if (UNLIKELY(debugLoggingEnabled())) {
                        RELEASE_LOG_INFO(ITPDebug, "Did set %" PUBLIC_LOG_STRING " as making a top frame redirect to %" PUBLIC_LOG_STRING ".", sourceDomain.string().utf8().data(), targetDomain.string().utf8().data());
                        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("Did set '", sourceDomain.string(), "' as making a top frame redirect to '", targetDomain.string(), "'."));
                    }
                }
                auto& targetStatistics = ensureResourceStatisticsForRegistrableDomain(targetDomain);
                if (targetStatistics.topFrameUniqueRedirectsFrom.add(sourceDomain).isNewEntry)
                    statisticsWereUpdated = true;
            }
        } else if (isRedirect) {
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

void ResourceLoadStatisticsMemoryStore::logUserInteraction(const TopFrameDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(domain);
    bool didHavePreviousUserInteraction = statistics.hadUserInteraction;
    statistics.hadUserInteraction = true;
    statistics.mostRecentUserInteractionTime = WallTime::now();
    if (didHavePreviousUserInteraction) {
        completionHandler();
        return;
    }
    updateCookieBlocking(WTFMove(completionHandler));
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

void ResourceLoadStatisticsMemoryStore::clearUserInteraction(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto& statistics = ensureResourceStatisticsForRegistrableDomain(domain);
    bool didHavePreviousUserInteraction = statistics.hadUserInteraction;
    statistics.hadUserInteraction = false;
    statistics.mostRecentUserInteractionTime = { };
    if (!didHavePreviousUserInteraction) {
        completionHandler();
        return;
    }
    updateCookieBlocking(WTFMove(completionHandler));
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
    Vector<String> sortedMapEntries;
    for (auto& mapEntry : m_resourceStatisticsMap.values())
        sortedMapEntries.append(mapEntry.toString());

    std::sort(sortedMapEntries.begin(), sortedMapEntries.end(), WTF::codePointCompareLessThan);

    for (auto& entry : sortedMapEntries)
        result.append(entry);
    
    auto thirdPartyData = aggregatedThirdPartyData();
    if (!thirdPartyData.isEmpty()) {
        result.append("\nITP Data:\n");
        for (auto thirdParty : thirdPartyData) {
            result.append(thirdParty.toString());
            result.append('\n');
        }
    }
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
    statistics.topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement.add(redirectDomain);
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
    RegistrableDomainsToBlockCookiesFor domainsToBlock { registrableDomainsToBlockAndDeleteCookiesFor, { }, { } };
    updateCookieBlockingForDomains(domainsToBlock, [callbackAggregator = callbackAggregator.copyRef()] { });
}

void ResourceLoadStatisticsMemoryStore::mergeStatistics(Vector<ResourceLoadStatistics>&& statistics)
{
    ASSERT(!RunLoop::isMain());

    for (auto& statistic : statistics) {
        auto result = m_resourceStatisticsMap.ensure(statistic.registrableDomain, [&statistic] {
            return WTFMove(statistic);
        });
        if (!result.isNewEntry)
            result.iterator->value.merge(statistic);
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
    Vector<RegistrableDomain> domainsWithUserInteractionAsFirstParty;
    for (auto& resourceStatistic : m_resourceStatisticsMap.values()) {
        if (hasHadUnexpiredRecentUserInteraction(resourceStatistic, OperatingDatesWindow::Long)) {
            if (resourceStatistic.isPrevalentResource)
                domainsToBlockButKeepCookiesFor.append(resourceStatistic.registrableDomain.isolatedCopy());
            domainsWithUserInteractionAsFirstParty.append(resourceStatistic.registrableDomain);
        } else if (resourceStatistic.isPrevalentResource)
            domainsToBlockAndDeleteCookiesFor.append(resourceStatistic.registrableDomain);
    }

    if (domainsToBlockAndDeleteCookiesFor.isEmpty() && domainsToBlockButKeepCookiesFor.isEmpty() && domainsWithUserInteractionAsFirstParty.isEmpty() && !debugModeEnabled()) {
        completionHandler();
        return;
    }

    RegistrableDomainsToBlockCookiesFor domainsToBlock { domainsToBlockAndDeleteCookiesFor, domainsToBlockButKeepCookiesFor, domainsWithUserInteractionAsFirstParty };

    if (debugLoggingEnabled() && (!domainsToBlockAndDeleteCookiesFor.isEmpty() || !domainsToBlockButKeepCookiesFor.isEmpty()))
        debugLogDomainsInBatches("Applying cross-site tracking restrictions", domainsToBlock);

    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), store = makeRef(store()), domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)] () mutable {
        store->callUpdatePrevalentDomainsToBlockCookiesForHandler(domainsToBlock, [weakThis = WTFMove(weakThis), store = store.copyRef(), completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();

                if (!weakThis)
                    return;

                if (UNLIKELY(weakThis->debugLoggingEnabled())) {
                    RELEASE_LOG_INFO(ITPDebug, "Done applying cross-site tracking restrictions.");
                    weakThis->debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, "[ITP] Done applying cross-site tracking restrictions."_s);
                }
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
        if (operatingDatesWindow == OperatingDatesWindow::Long) {
            // Drop privacy sensitive data because we no longer need it.
            // Set timestamp to 0 so that statistics merge will know
            // it has been reset as opposed to its default -1.
            resourceStatistic.mostRecentUserInteractionTime = { };
            resourceStatistic.storageAccessUnderTopFrameDomains.clear();
            resourceStatistic.hadUserInteraction = false;
        }
        
        return false;
    }

    return resourceStatistic.hadUserInteraction;
}

bool ResourceLoadStatisticsMemoryStore::shouldRemoveAllWebsiteDataFor(ResourceLoadStatistics& resourceStatistic, bool shouldCheckForGrandfathering) const
{
    return resourceStatistic.isPrevalentResource && !hasHadUnexpiredRecentUserInteraction(resourceStatistic, OperatingDatesWindow::Long) && (!shouldCheckForGrandfathering || !resourceStatistic.grandfathered);
}

bool ResourceLoadStatisticsMemoryStore::shouldRemoveAllButCookiesFor(ResourceLoadStatistics& resourceStatistic, bool shouldCheckForGrandfathering) const
{
    bool isRemovalEnabled = firstPartyWebsiteDataRemovalMode() != FirstPartyWebsiteDataRemovalMode::None || resourceStatistic.gotLinkDecorationFromPrevalentResource;
    bool isResourceGrandfathered = shouldCheckForGrandfathering && resourceStatistic.grandfathered;
    
    OperatingDatesWindow window { };
    switch (firstPartyWebsiteDataRemovalMode()) {
    case FirstPartyWebsiteDataRemovalMode::AllButCookies:
        FALLTHROUGH;
    case FirstPartyWebsiteDataRemovalMode::None:
        window = OperatingDatesWindow::Short;
        break;
    case FirstPartyWebsiteDataRemovalMode::AllButCookiesLiveOnTestingTimeout:
        window = OperatingDatesWindow::ForLiveOnTesting;
        break;
    case FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout:
        window = OperatingDatesWindow::ForReproTesting;
    }

    return isRemovalEnabled && !isResourceGrandfathered && !hasHadUnexpiredRecentUserInteraction(resourceStatistic, window);
}

bool ResourceLoadStatisticsMemoryStore::shouldEnforceSameSiteStrictFor(ResourceLoadStatistics& resourceStatistic, bool shouldCheckForGrandfathering)
{
    if ((!isSameSiteStrictEnforcementEnabled() && !shouldEnforceSameSiteStrictForSpecificDomain(resourceStatistic.registrableDomain))
        || (shouldCheckForGrandfathering && resourceStatistic.grandfathered))
        return false;

    if (resourceStatistic.topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement.size() > parameters().minimumTopFrameRedirectsForSameSiteStrictEnforcement) {
        resourceStatistic.topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement.clear();
        return true;
    }

    return false;
}

RegistrableDomainsToDeleteOrRestrictWebsiteDataFor ResourceLoadStatisticsMemoryStore::registrableDomainsToDeleteOrRestrictWebsiteDataFor()
{
    ASSERT(!RunLoop::isMain());

    bool shouldCheckForGrandfathering = endOfGrandfatheringTimestamp() > WallTime::now();
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && endOfGrandfatheringTimestamp();

    if (shouldClearGrandfathering)
        clearEndOfGrandfatheringTimeStamp();

    auto now = WallTime::now();
    auto oldestUserInteraction = now;
    RegistrableDomainsToDeleteOrRestrictWebsiteDataFor toDeleteOrRestrictFor;
    for (auto& statistic : m_resourceStatisticsMap.values()) {
        if (shouldExemptFromWebsiteDataDeletion(statistic.registrableDomain))
            continue;
        oldestUserInteraction = std::min(oldestUserInteraction, statistic.mostRecentUserInteractionTime);
        if (shouldRemoveAllWebsiteDataFor(statistic, shouldCheckForGrandfathering)) {
            toDeleteOrRestrictFor.domainsToDeleteAllCookiesFor.append(statistic.registrableDomain);
            toDeleteOrRestrictFor.domainsToDeleteAllNonCookieWebsiteDataFor.append(statistic.registrableDomain);
        } else {
            if (shouldRemoveAllButCookiesFor(statistic, shouldCheckForGrandfathering)) {
                toDeleteOrRestrictFor.domainsToDeleteAllNonCookieWebsiteDataFor.append(statistic.registrableDomain);
                statistic.gotLinkDecorationFromPrevalentResource = false;
            }
            if (shouldEnforceSameSiteStrictFor(statistic, shouldCheckForGrandfathering)) {
                toDeleteOrRestrictFor.domainsToEnforceSameSiteStrictFor.append(statistic.registrableDomain);

                if (UNLIKELY(debugLoggingEnabled())) {
                    RELEASE_LOG_INFO(ITPDebug, "Scheduled %" PUBLIC_LOG_STRING " to have its cookies set to SameSite=strict.", statistic.registrableDomain.string().utf8().data());
                    debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("Scheduled '", statistic.registrableDomain.string(), "' to have its cookies set to SameSite=strict'."));
                }
            }
        }

        if (shouldClearGrandfathering && statistic.grandfathered)
            statistic.grandfathered = false;
    }

    // Give the user enough time to interact with websites until we remove non-cookie website data.
    if (!parameters().isRunningTest && now - oldestUserInteraction > parameters().minimumTimeBetweenDataRecordsRemoval)
        toDeleteOrRestrictFor.domainsToDeleteAllNonCookieWebsiteDataFor.clear();

    return toDeleteOrRestrictFor;
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

void ResourceLoadStatisticsMemoryStore::removeDataForDomain(const RegistrableDomain& domain)
{
    m_resourceStatisticsMap.remove(domain);
    
    for (auto& statistic : m_resourceStatisticsMap) {
        statistic.value.topFrameUniqueRedirectsTo.remove(domain);
        statistic.value.topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement.remove(domain);
        statistic.value.topFrameUniqueRedirectsFrom.remove(domain);
        statistic.value.topFrameLinkDecorationsFrom.remove(domain);
        statistic.value.topFrameLoadedThirdPartyScripts.remove(domain);
        statistic.value.subframeUnderTopFrameDomains.remove(domain);
        statistic.value.subresourceUnderTopFrameDomains.remove(domain);
        statistic.value.subresourceUniqueRedirectsTo.remove(domain);
        statistic.value.subresourceUniqueRedirectsFrom.remove(domain);
    }
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

Vector<OperatingDate> ResourceLoadStatisticsMemoryStore::mergeOperatingDates(const Vector<OperatingDate>& existingDates, Vector<OperatingDate>&& newDates)
{
    if (existingDates.isEmpty())
        return WTFMove(newDates);

    Vector<OperatingDate> mergedDates(existingDates.size() + newDates.size());

    // Merge the two sorted vectors of dates.
    std::merge(existingDates.begin(), existingDates.end(), newDates.begin(), newDates.end(), mergedDates.begin());
    // Remove duplicate dates.
    removeRepeatedElements(mergedDates);

    // Drop old dates until the Vector size reaches operatingDatesWindowLong.
    while (mergedDates.size() > operatingDatesWindowLong)
        mergedDates.remove(0);

    return mergedDates;
}

void ResourceLoadStatisticsMemoryStore::mergeOperatingDates(Vector<OperatingDate>&& newDates)
{
    ASSERT(!RunLoop::isMain());

    m_operatingDates = mergeOperatingDates(m_operatingDates, WTFMove(newDates));
}

void ResourceLoadStatisticsMemoryStore::includeTodayAsOperatingDateIfNecessary()
{
    ASSERT(!RunLoop::isMain());

    auto today = OperatingDate::today();
    if (!m_operatingDates.isEmpty() && today <= m_operatingDates.last())
        return;

    while (m_operatingDates.size() >= operatingDatesWindowLong)
        m_operatingDates.remove(0);

    m_operatingDates.append(today);
}

bool ResourceLoadStatisticsMemoryStore::hasStatisticsExpired(WallTime mostRecentUserInteractionTime, OperatingDatesWindow operatingDatesWindow) const
{
    ASSERT(!RunLoop::isMain());

    unsigned operatingDatesWindowInDays = 0;
    switch (operatingDatesWindow) {
    case OperatingDatesWindow::Long:
        operatingDatesWindowInDays = operatingDatesWindowLong;
        break;
    case OperatingDatesWindow::Short:
        operatingDatesWindowInDays = operatingDatesWindowShort;
        break;
    case OperatingDatesWindow::ForLiveOnTesting:
        return WallTime::now() > mostRecentUserInteractionTime + operatingTimeWindowForLiveOnTesting;
    case OperatingDatesWindow::ForReproTesting:
        return true;
    }

    if (m_operatingDates.size() >= operatingDatesWindowInDays) {
        if (OperatingDate::fromWallTime(mostRecentUserInteractionTime) < m_operatingDates.first())
            return true;
    }

    // If we don't meet the real criteria for an expired statistic, check the user setting for a tighter restriction (mainly for testing).
    if (this->parameters().timeToLiveUserInteraction) {
        if (WallTime::now() > mostRecentUserInteractionTime + this->parameters().timeToLiveUserInteraction.value())
            return true;
    }

    return false;
}

bool ResourceLoadStatisticsMemoryStore::hasStatisticsExpired(const ResourceLoadStatistics& resourceStatistic, OperatingDatesWindow operatingDatesWindow) const
{
    return hasStatisticsExpired(resourceStatistic.mostRecentUserInteractionTime, operatingDatesWindow);
}

void ResourceLoadStatisticsMemoryStore::insertExpiredStatisticForTesting(const RegistrableDomain& domain, bool hasUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent)
{
    // Populate the Operating Dates table with enough days to require pruning.
    double daysAgoInSeconds = 0;
    for (unsigned i = 1; i <= operatingDatesWindowLong; i++) {
        double daysToSubtract = Seconds::fromHours(24 * i).value();
        daysAgoInSeconds = WallTime::now().secondsSinceEpoch().value() - daysToSubtract;
        auto dateToInsert = OperatingDate::fromWallTime(WallTime::fromRawSeconds(daysAgoInSeconds));
        m_operatingDates.append(OperatingDate(dateToInsert.year(), dateToInsert.month(), dateToInsert.monthDay()));
    }

    // Make sure mostRecentUserInteractionTime is the least recent of all entries.
    daysAgoInSeconds -= Seconds::fromHours(24).value();

    auto statistic = ResourceLoadStatistics(domain);
    statistic.lastSeen = WallTime::fromRawSeconds(daysAgoInSeconds);
    statistic.hadUserInteraction = hasUserInteraction;
    statistic.mostRecentUserInteractionTime = WallTime::fromRawSeconds(daysAgoInSeconds);
    statistic.isPrevalentResource = isPrevalent;
    statistic.gotLinkDecorationFromPrevalentResource = isScheduledForAllButCookieDataRemoval;

    m_resourceStatisticsMap.ensure(statistic.registrableDomain, [&statistic] {
        return WTFMove(statistic);
    });
}

} // namespace WebKit

#endif
