/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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
#include "WKWebsiteDataStoreRef.h"

#include "APIArray.h"
#include "APIHTTPCookieStore.h"
#include "NetworkProcessProxy.h"
#include "ShouldGrandfatherStatistics.h"
#include "WKAPICast.h"
#include "WKDictionary.h"
#include "WKMutableArray.h"
#include "WKNumber.h"
#include "WKRetainPtr.h"
#include "WKSecurityOriginRef.h"
#include "WKString.h"
#include "WebDeviceOrientationAndMotionAccessController.h"
#include "WebPageProxy.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebsiteData.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataRecord.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataType.h"
#include <WebCore/RegistrableDomain.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/URL.h>

WKTypeID WKWebsiteDataStoreGetTypeID()
{
    return WebKit::toAPI(WebKit::WebsiteDataStore::APIType);
}

WKWebsiteDataStoreRef WKWebsiteDataStoreGetDefaultDataStore()
{
    return WebKit::toAPI(protect(WebKit::WebsiteDataStore::defaultDataStore()).get());
}

WKWebsiteDataStoreRef WKWebsiteDataStoreCreateNonPersistentDataStore()
{
    return WebKit::toAPILeakingRef(WebKit::WebsiteDataStore::createNonPersistent());
}

WKWebsiteDataStoreRef WKWebsiteDataStoreCreateWithConfiguration(WKWebsiteDataStoreConfigurationRef configuration)
{
    auto sessionID = WebKit::toImpl(configuration)->isPersistent() ? PAL::SessionID::generatePersistentSessionID() : PAL::SessionID::generateEphemeralSessionID();
    return WebKit::toAPILeakingRef(WebKit::WebsiteDataStore::create(*WebKit::toImpl(configuration), sessionID));
}

void WKWebsiteDataStoreTerminateNetworkProcess(WKWebsiteDataStoreRef dataStore)
{
    protect(WebKit::toImpl(dataStore))->terminateNetworkProcess();
}

WKProcessID WKWebsiteDataStoreGetNetworkProcessIdentifier(WKWebsiteDataStoreRef dataStore)
{
    return protect(WebKit::toImpl(dataStore))->networkProcess().processID();
}

void WKWebsiteDataStoreRemoveITPDataForDomain(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreRemoveITPDataForDomainFunction callback)
{
    WebKit::WebsiteDataRecord dataRecord;
    dataRecord.types.add(WebKit::WebsiteDataType::ResourceLoadStatistics);
    dataRecord.addResourceLoadStatisticsRegistrableDomain(WebCore::RegistrableDomain::uncheckedCreateFromHost(protect(WebKit::toImpl(host))->string()));
    Vector<WebKit::WebsiteDataRecord> dataRecords = { WTF::move(dataRecord) };

    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::ResourceLoadStatistics;
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, dataRecords, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabase(WKWebsiteDataStoreRef dataStoreRef, int domainID, void* context, WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabaseFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->domainIDExistsInDatabase(domainID, [context, callback](bool exists) {
        callback(exists, context);
    });
}

void WKWebsiteDataStoreSetServiceWorkerFetchTimeoutForTesting(WKWebsiteDataStoreRef dataStore, double seconds)
{
    protect(WebKit::toImpl(dataStore))->setServiceWorkerTimeoutForTesting(Seconds(seconds));
}

void WKWebsiteDataStoreResetServiceWorkerFetchTimeoutForTesting(WKWebsiteDataStoreRef dataStore)
{
    protect(WebKit::toImpl(dataStore))->resetServiceWorkerTimeoutForTesting();
}

void WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(WKWebsiteDataStoreRef dataStoreRef, bool enable)
{
    protect(WebKit::toImpl(dataStoreRef))->setTrackingPreventionEnabled(enable);
}

void WKWebsiteDataStoreIsStatisticsEphemeral(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsEphemeralFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->isResourceLoadStatisticsEphemeral([context, completionHandler](bool isEphemeral) {
        completionHandler(isEphemeral, context);
    });
}

bool WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(WKWebsiteDataStoreRef dataStoreRef)
{
    return protect(WebKit::toImpl(dataStoreRef))->trackingPreventionEnabled();
}

void WKWebsiteDataStoreSetResourceLoadStatisticsDebugMode(WKWebsiteDataStoreRef dataStoreRef, bool enable)
{
    protect(WebKit::toImpl(dataStoreRef))->setResourceLoadStatisticsDebugMode(enable);
}

void WKWebsiteDataStoreSyncLocalStorage(WKWebsiteDataStoreRef dataStore, void* context, WKWebsiteDataStoreSyncLocalStorageCallback callback)
{
    protect(WebKit::toImpl(dataStore))->syncLocalStorage([context, callback] {
        if (callback)
            callback(context);
    });
}

WKHTTPCookieStoreRef WKWebsiteDataStoreGetHTTPCookieStore(WKWebsiteDataStoreRef dataStoreRef)
{
    return WebKit::toAPI(protect(protect(WebKit::toImpl(dataStoreRef))->cookieStore()).get());
}

void WKWebsiteDataStoreSetAllowsAnySSLCertificateForWebSocketTesting(WKWebsiteDataStoreRef, bool)
{
}

void WKWebsiteDataStoreSetResourceLoadStatisticsDebugModeWithCompletionHandler(WKWebsiteDataStoreRef dataStoreRef, bool enable, void* context, WKWebsiteDataStoreStatisticsDebugModeFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->setResourceLoadStatisticsDebugMode(enable, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreStatisticsDebugModeFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->setPrevalentResourceForDebugMode(URL { protect(WebKit::toImpl(host))->string() }, [context, completionHandler] {
        completionHandler(context);
    });
}
void WKWebsiteDataStoreSetStatisticsLastSeen(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, double seconds, void* context, WKWebsiteDataStoreStatisticsLastSeenFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->setLastSeen(URL { protect(WebKit::toImpl(host))->string() }, Seconds { seconds }, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetStatisticsMergeStatistic(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef topFrameDomain1, WKStringRef topFrameDomain2, double lastSeen, bool hadUserInteraction, double mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, void* context, WKWebsiteDataStoreStatisticsMergeStatisticFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->mergeStatisticForTesting(URL { protect(WebKit::toImpl(host))->string() }, URL { protect(WebKit::toImpl(topFrameDomain1))->string() }, URL { protect(WebKit::toImpl(topFrameDomain2))->string() }, Seconds { lastSeen }, hadUserInteraction, Seconds { mostRecentUserInteraction }, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetStatisticsExpiredStatistic(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, void* context, WKWebsiteDataStoreStatisticsMergeStatisticFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->insertExpiredStatisticForTesting(URL { protect(WebKit::toImpl(host))->string() }, numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetStatisticsPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsPrevalentResourceFunction completionHandler)
{
    Ref websiteDataStore = *WebKit::toImpl(dataStoreRef);

    if (value)
        websiteDataStore->setPrevalentResource(URL { protect(WebKit::toImpl(host))->string() }, [context, completionHandler] {
            completionHandler(context);
        });
    else
        websiteDataStore->clearPrevalentResource(URL { protect(WebKit::toImpl(host))->string() }, [context, completionHandler] {
            completionHandler(context);
        });
}

void WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsVeryPrevalentResourceFunction completionHandler)
{
    Ref websiteDataStore = *WebKit::toImpl(dataStoreRef);

    if (value)
        websiteDataStore->setVeryPrevalentResource(URL { protect(WebKit::toImpl(host))->string() }, [context, completionHandler] {
            completionHandler(context);
        });
    else
        websiteDataStore->clearPrevalentResource(URL { protect(WebKit::toImpl(host))->string() }, [context, completionHandler] {
            completionHandler(context);
        });
}

void WKWebsiteDataStoreDumpResourceLoadStatistics(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreDumpResourceLoadStatisticsFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->dumpResourceLoadStatistics([context, callback] (const String& resourceLoadStatistics) {
        callback(WebKit::toAPI(resourceLoadStatistics.impl()), context);
    });
}

void WKWebsiteDataStoreIsStatisticsPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsPrevalentResourceFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->isPrevalentResource(URL { protect(WebKit::toImpl(host))->string() }, [context, callback](bool isPrevalentResource) {
        callback(isPrevalentResource, context);
    });
}

void WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsPrevalentResourceFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->isVeryPrevalentResource(URL { protect(WebKit::toImpl(host))->string() }, [context, callback](bool isVeryPrevalentResource) {
        callback(isVeryPrevalentResource, context);
    });
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(WKWebsiteDataStoreRef dataStoreRef, WKStringRef subresourceHost, WKStringRef topFrameHost, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnderFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->isRegisteredAsSubresourceUnder(URL { protect(WebKit::toImpl(subresourceHost))->string() }, URL { protect(WebKit::toImpl(topFrameHost))->string() }, [context, callback](bool isRegisteredAsSubresourceUnder) {
        callback(isRegisteredAsSubresourceUnder, context);
    });
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(WKWebsiteDataStoreRef dataStoreRef, WKStringRef subFrameHost, WKStringRef topFrameHost, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnderFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->isRegisteredAsSubFrameUnder(URL { protect(WebKit::toImpl(subFrameHost))->string() }, URL { protect(WebKit::toImpl(topFrameHost))->string() }, [context, callback](bool isRegisteredAsSubFrameUnder) {
        callback(isRegisteredAsSubFrameUnder, context);
    });
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef hostRedirectedFrom, WKStringRef hostRedirectedTo, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingToFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->isRegisteredAsRedirectingTo(URL { protect(WebKit::toImpl(hostRedirectedFrom))->string() }, URL { protect(WebKit::toImpl(hostRedirectedTo))->string() }, [context, callback](bool isRegisteredAsRedirectingTo) {
        callback(isRegisteredAsRedirectingTo, context);
    });
}

void WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsHasHadUserInteractionFunction completionHandler)
{
    Ref dataStore = *protect(WebKit::toImpl(dataStoreRef));

    if (value)
        dataStore->logUserInteraction(URL { protect(WebKit::toImpl(host))->string() }, [context, completionHandler] {
            completionHandler(context);
        });
    else
        dataStore->clearUserInteraction(URL { protect(WebKit::toImpl(host))->string() }, [context, completionHandler] {
            completionHandler(context);
        });
}

void WKWebsiteDataStoreIsStatisticsHasHadUserInteraction(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsHasHadUserInteractionFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->hasHadUserInteraction(URL { protect(WebKit::toImpl(host))->string() }, [context, callback](bool hasHadUserInteraction) {
        callback(hasHadUserInteraction, context);
    });
}

void WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnce(WKWebsiteDataStoreRef dataStoreRef, WKStringRef subHost, WKStringRef topHost, void* context, WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnceFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->isRelationshipOnlyInDatabaseOnce(URL { protect(WebKit::toImpl(subHost))->string() }, URL { protect(WebKit::toImpl(topHost))->string() }, [context, callback](bool onlyInDatabaseOnce) {
        callback(onlyInDatabaseOnce, context);
    });
}

void WKWebsiteDataStoreSetStatisticsGrandfathered(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value)
{
    protect(WebKit::toImpl(dataStoreRef))->setGrandfathered(URL { protect(WebKit::toImpl(host))->string() }, value, [] { });
}

void WKWebsiteDataStoreIsStatisticsGrandfathered(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsGrandfatheredFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->isGrandfathered(URL { protect(WebKit::toImpl(host))->string() }, [context, callback](bool isGrandfathered) {
        callback(isGrandfathered, context);
    });
}

void WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef topFrameHost)
{
    protect(WebKit::toImpl(dataStoreRef))->setSubframeUnderTopFrameDomain(URL { protect(WebKit::toImpl(host))->string() }, URL { protect(WebKit::toImpl(topFrameHost))->string() }, [] { });
}

void WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef topFrameHost)
{
    protect(WebKit::toImpl(dataStoreRef))->setSubresourceUnderTopFrameDomain(URL { protect(WebKit::toImpl(host))->string() }, URL { protect(WebKit::toImpl(topFrameHost))->string() }, [] { });
}

void WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedTo)
{
    protect(WebKit::toImpl(dataStoreRef))->setSubresourceUniqueRedirectTo(URL { protect(WebKit::toImpl(host))->string() }, URL { protect(WebKit::toImpl(hostRedirectedTo))->string() }, [] { });
}

void WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedFrom)
{
    protect(WebKit::toImpl(dataStoreRef))->setSubresourceUniqueRedirectFrom(URL { protect(WebKit::toImpl(host))->string() }, URL { protect(WebKit::toImpl(hostRedirectedFrom))->string() }, [] { });
}

void WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedTo)
{
    protect(WebKit::toImpl(dataStoreRef))->setTopFrameUniqueRedirectTo(URL { protect(WebKit::toImpl(host))->string() }, URL { protect(WebKit::toImpl(hostRedirectedTo))->string() }, [] { });
}

void WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedFrom)
{
    protect(WebKit::toImpl(dataStoreRef))->setTopFrameUniqueRedirectFrom(URL { protect(WebKit::toImpl(host))->string() }, URL { protect(WebKit::toImpl(hostRedirectedFrom))->string() }, [] { });
}

void WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(WKWebsiteDataStoreRef dataStoreRef, WKStringRef fromHost, WKStringRef toHost, bool wasFiltered, void* context, WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecorationFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->setCrossSiteLoadWithLinkDecorationForTesting(URL { protect(WebKit::toImpl(fromHost))->string() }, URL { protect(WebKit::toImpl(toHost))->string() }, wasFiltered, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(WKWebsiteDataStoreRef dataStoreRef, double seconds, void* context, WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteractionFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->setTimeToLiveUserInteraction(Seconds { seconds }, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecordsFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->scheduleStatisticsAndDataRecordsProcessing([context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsUpdateCookieBlocking(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsUpdateCookieBlockingFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->scheduleCookieBlockingUpdate([context, completionHandler]() {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetResourceLoadStatisticsTimeAdvanceForTesting(WKWebsiteDataStoreRef dataStoreRef, double value, void* context, WKWebsiteDataStoreSetStatisticsIsRunningTestFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->setResourceLoadStatisticsTimeAdvanceForTesting(Seconds { value }, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreSetStatisticsIsRunningTest(WKWebsiteDataStoreRef dataStoreRef, bool value, void* context, WKWebsiteDataStoreSetStatisticsIsRunningTestFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->setIsRunningResourceLoadStatisticsTest(value, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreSetStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(WKWebsiteDataStoreRef dataStoreRef, bool value)
{
    protect(WebKit::toImpl(dataStoreRef))->setShouldClassifyResourcesBeforeDataRecordsRemoval(value, []() { });
}

void WKWebsiteDataStoreSetStatisticsMinimumTimeBetweenDataRecordsRemoval(WKWebsiteDataStoreRef dataStoreRef, double seconds)
{
    protect(WebKit::toImpl(dataStoreRef))->setMinimumTimeBetweenDataRecordsRemoval(Seconds { seconds }, []() { });
}

void WKWebsiteDataStoreSetStatisticsGrandfatheringTime(WKWebsiteDataStoreRef dataStoreRef, double seconds)
{
    protect(WebKit::toImpl(dataStoreRef))->setGrandfatheringTime(Seconds { seconds }, []() { });
}

void WKWebsiteDataStoreSetStatisticsMaxStatisticsEntries(WKWebsiteDataStoreRef dataStoreRef, unsigned entries)
{
    protect(WebKit::toImpl(dataStoreRef))->setMaxStatisticsEntries(entries, []() { });
}

void WKWebsiteDataStoreSetStatisticsPruneEntriesDownTo(WKWebsiteDataStoreRef dataStoreRef, unsigned entries)
{
    protect(WebKit::toImpl(dataStoreRef))->setPruneEntriesDownTo(entries, []() { });
}

void WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->scheduleClearInMemoryAndPersistent(WebKit::ShouldGrandfatherStatistics::Yes, [context, callback]() {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(WKWebsiteDataStoreRef dataStoreRef, unsigned hours, void* context, WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHoursFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->scheduleClearInMemoryAndPersistent(WallTime::now() - Seconds::fromHours(hours), WebKit::ShouldGrandfatherStatistics::Yes, [context, callback]() {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::ResourceLoadStatistics;
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, WallTime::fromRawSeconds(0), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool includeHttpOnlyCookies, void* context, WKWebsiteDataStoreStatisticsDeleteCookiesForTestingFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->deleteCookiesForTesting(URL { protect(WebKit::toImpl(host))->string() }, includeHttpOnlyCookies, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsHasLocalStorage(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreStatisticsHasLocalStorageFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->hasLocalStorageForTesting(URL { protect(WebKit::toImpl(host))->string() }, [context, callback](bool hasLocalStorage) {
        callback(hasLocalStorage, context);
    });
}

void WKWebsiteDataStoreSetStatisticsCacheMaxAgeCap(WKWebsiteDataStoreRef dataStoreRef, double seconds, void* context, WKWebsiteDataStoreSetStatisticsCacheMaxAgeCapFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->setCacheMaxAgeCapForPrevalentResources(Seconds { seconds }, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsHasIsolatedSession(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreStatisticsHasIsolatedSessionFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->hasIsolatedSessionForTesting(URL { protect(WebKit::toImpl(host))->string() }, [context, callback](bool hasIsolatedSession) {
        callback(hasIsolatedSession, context);
    });
}

void WKWebsiteDataStoreHasAppBoundSession(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreHasAppBoundSessionFunction callback)
{
#if ENABLE(APP_BOUND_DOMAINS)
    protect(WebKit::toImpl(dataStoreRef))->hasAppBoundSession([context, callback](bool hasAppBoundSession) {
        callback(hasAppBoundSession, context);
    });
#else
    UNUSED_PARAM(dataStoreRef);
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetResourceLoadStatisticsShouldDowngradeReferrerForTesting(WKWebsiteDataStoreRef dataStoreRef, bool enabled, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsShouldDowngradeReferrerForTestingFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->setResourceLoadStatisticsShouldDowngradeReferrerForTesting(enabled, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(WKWebsiteDataStoreRef dataStoreRef, bool enabled, WKThirdPartyCookieBlockingPolicy thirdPartyCookieBlockingPolicy, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsShouldBlockThirdPartyCookiesForTestingFunction completionHandler)
{
    WebCore::ThirdPartyCookieBlockingMode blockingMode = WebCore::ThirdPartyCookieBlockingMode::OnlyAccordingToPerDomainPolicy;
    if (enabled) {
        switch (thirdPartyCookieBlockingPolicy) {
        case kWKThirdPartyCookieBlockingPolicyAllOnlyOnSitesWithoutUserInteraction:
            blockingMode = WebCore::ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction;
            break;
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
        case kWKThirdPartyCookieBlockingPolicyAllExceptPartitioned:
            blockingMode = WebCore::ThirdPartyCookieBlockingMode::AllExceptPartitioned;
            break;
#endif
        default:
            blockingMode = WebCore::ThirdPartyCookieBlockingMode::All;
            break;
        }

    }

    protect(WebKit::toImpl(dataStoreRef))->setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(enabled, blockingMode, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(WKWebsiteDataStoreRef dataStoreRef, bool enabled, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTestingFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->setResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(enabled, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTesting(WKWebsiteDataStoreRef dataStoreRef, WKStringRef hostName, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTestingFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->setResourceLoadStatisticsToSameSiteStrictCookiesForTesting(URL { protect(WebKit::toImpl(hostName))->string() }, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(WKWebsiteDataStoreRef dataStoreRef, WKStringRef firstPartyURLString, WKStringRef cnameURLString, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTestingFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->setResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(URL { protect(WebKit::toImpl(firstPartyURLString))->string() }, URL { protect(WebKit::toImpl(cnameURLString))->string() }, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(WKWebsiteDataStoreRef dataStoreRef, WKStringRef cnameURLString, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsThirdPartyCNAMEDomainForTestingFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->setResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(URL { protect(WebKit::toImpl(cnameURLString))->string() }, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetAppBoundDomainsForTesting(WKArrayRef originURLsRef, void* context, WKWebsiteDataStoreSetAppBoundDomainsForTestingFunction completionHandler)
{
#if ENABLE(APP_BOUND_DOMAINS)
    RefPtr<API::Array> originURLsArray = WebKit::toImpl(originURLsRef);
    size_t newSize = originURLsArray ? originURLsArray->size() : 0;
    HashSet<WebCore::RegistrableDomain> domains;
    domains.reserveInitialCapacity(newSize);
    for (size_t i = 0; i < newSize; ++i) {
        if (RefPtr originURL = originURLsArray->at<API::URL>(i))
            domains.add(WebCore::RegistrableDomain { URL { originURL->string() } });
    }

    WebKit::WebsiteDataStore::setAppBoundDomainsForTesting(WTF::move(domains), [context, completionHandler] {
        completionHandler(context);
    });
#else
    UNUSED_PARAM(originURLsRef);
    UNUSED_PARAM(context);
    UNUSED_PARAM(completionHandler);
#endif
}

void WKWebsiteDataStoreSetManagedDomainsForTesting(WKArrayRef originURLsRef, void* context, WKWebsiteDataStoreSetManagedDomainsForTestingFunction completionHandler)
{
#if ENABLE(MANAGED_DOMAINS)
    RefPtr<API::Array> originURLsArray = WebKit::toImpl(originURLsRef);
    size_t newSize = originURLsArray ? originURLsArray->size() : 0;
    HashSet<WebCore::RegistrableDomain> domains;
    domains.reserveInitialCapacity(newSize);
    for (size_t i = 0; i < newSize; ++i) {
        RefPtr originURL = originURLsArray->at<API::URL>(i);
        if (!originURL)
            continue;

        domains.add(WebCore::RegistrableDomain { URL { originURL->string() } });
    }

    WebKit::WebsiteDataStore::setManagedDomainsForTesting(WTF::move(domains), [context, completionHandler] {
        completionHandler(context);
    });
#else
    UNUSED_PARAM(originURLsRef);
    UNUSED_PARAM(context);
    UNUSED_PARAM(completionHandler);
#endif
}

void WKWebsiteDataStoreStatisticsResetToConsistentState(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsResetToConsistentStateFunction completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create([context, completionHandler]() {
        completionHandler(context);
    });

    Ref store = *WebKit::toImpl(dataStoreRef);
    store->clearResourceLoadStatisticsInWebProcesses([callbackAggregator] { });
    store->resetCacheMaxAgeCapForPrevalentResources([callbackAggregator] { });
    store->resetCrossSiteLoadsWithLinkDecorationForTesting([callbackAggregator] { });
    store->setResourceLoadStatisticsShouldDowngradeReferrerForTesting(true, [callbackAggregator] { });
    store->setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(false, WebCore::ThirdPartyCookieBlockingMode::OnlyAccordingToPerDomainPolicy, [callbackAggregator] { });
    store->setResourceLoadStatisticsShouldEnbleSameSiteStrictEnforcementForTesting(true, [callbackAggregator] { });
    store->setResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(false, [callbackAggregator] { });
    store->resetParametersToDefaultValues([callbackAggregator] { });
    store->scheduleClearInMemoryAndPersistent(WebKit::ShouldGrandfatherStatistics::No, [callbackAggregator] { });
}

void WKWebsiteDataStoreRemoveAllFetchCaches(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveFetchCacheRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::DOMCache;
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveNetworkCache(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveNetworkCacheCallback callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::DiskCache;
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveMemoryCaches(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveMemoryCachesRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::MemoryCache;
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveFetchCacheForOrigin(WKWebsiteDataStoreRef dataStoreRef, WKSecurityOriginRef origin, void* context, WKWebsiteDataStoreRemoveFetchCacheRemovalFunction callback)
{
    WebKit::WebsiteDataRecord dataRecord;
    dataRecord.add(WebKit::WebsiteDataType::DOMCache, WebKit::toImpl(origin)->securityOrigin());
    Vector<WebKit::WebsiteDataRecord> dataRecords = { WTF::move(dataRecord) };

    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::DOMCache;
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, dataRecords, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveAllIndexedDatabases(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveAllIndexedDatabasesCallback callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::IndexedDBDatabases;
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
    if (callback)
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveLocalStorage(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveLocalStorageCallback callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::LocalStorage;
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        if (callback)
            callback(context);
    });
}

void WKWebsiteDataStoreRemoveAllServiceWorkerRegistrations(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveAllServiceWorkerRegistrationsCallback callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::ServiceWorkerRegistrations;
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreGetFetchCacheOrigins(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreGetFetchCacheOriginsFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->fetchData(WebKit::WebsiteDataType::DOMCache, { }, [context, callback] (auto dataRecords) {
        Vector<RefPtr<API::Object>> securityOrigins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                securityOrigins.append(API::SecurityOrigin::create(origin.securityOrigin()));
        }
        callback(WebKit::toAPI(API::Array::create(WTF::move(securityOrigins)).ptr()), context);
    });
}

void WKWebsiteDataStoreGetFetchCacheSizeForOrigin(WKWebsiteDataStoreRef dataStoreRef, WKStringRef origin, void* context, WKWebsiteDataStoreGetFetchCacheSizeForOriginFunction callback)
{
    OptionSet<WebKit::WebsiteDataFetchOption> fetchOptions = WebKit::WebsiteDataFetchOption::ComputeSizes;

    protect(WebKit::toImpl(dataStoreRef))->fetchData(WebKit::WebsiteDataType::DOMCache, fetchOptions, [origin, context, callback] (auto dataRecords) {
        auto originData = WebCore::SecurityOrigin::createFromString(protect(WebKit::toImpl(origin))->string())->data();
        for (auto& dataRecord : dataRecords) {
            for (const auto& recordOrigin : dataRecord.origins) {
                if (originData == recordOrigin) {
                    callback(dataRecord.size ? dataRecord.size->totalSize : 0, context);
                    return;
                }

            }
        }
        callback(0, context);
    });
}

void WKWebsiteDataStoreSetPerOriginStorageQuota(WKWebsiteDataStoreRef, uint64_t)
{
}

void WKWebsiteDataStoreClearAllDeviceOrientationPermissions(WKWebsiteDataStoreRef dataStoreRef)
{
#if ENABLE(DEVICE_ORIENTATION)
    protect(protect(WebKit::toImpl(dataStoreRef))->deviceOrientationAndMotionAccessController())->clearPermissions();
#endif
}

void WKWebsiteDataStoreClearPrivateClickMeasurementsThroughWebsiteDataRemoval(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreClearPrivateClickMeasurementsThroughWebsiteDataRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::PrivateClickMeasurements;
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, WallTime::fromRawSeconds(0), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreSetCacheModelSynchronouslyForTesting(WKWebsiteDataStoreRef dataStoreRef, WKCacheModel cacheModel)
{
    protect(WebKit::toImpl(dataStoreRef))->setCacheModelSynchronouslyForTesting(WebKit::toCacheModel(cacheModel));
}

void WKWebsiteDataStoreResetQuota(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreResetQuotaCallback callback)
{
    protect(WebKit::toImpl(dataStoreRef))->resetQuota([context, callback] {
        if (callback)
            callback(context);
    });
}

void WKWebsiteDataStoreResetStoragePersistedState(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreResetStoragePersistedStateCallback callback)
{
    protect(WebKit::toImpl(dataStoreRef))->resetStoragePersistedState([context, callback] {
        if (callback)
            callback(context);
    });
}

void WKWebsiteDataStoreClearStorage(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreClearStorageCallback callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = {
        WebKit::WebsiteDataType::LocalStorage,
        WebKit::WebsiteDataType::IndexedDBDatabases,
        WebKit::WebsiteDataType::FileSystem,
        WebKit::WebsiteDataType::DOMCache,
        WebKit::WebsiteDataType::Credentials,
        WebKit::WebsiteDataType::ServiceWorkerRegistrations
    };
    protect(WebKit::toImpl(dataStoreRef))->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        if (callback)
            callback(context);
    });
}

void WKWebsiteDataStoreSetOriginQuotaRatioEnabled(WKWebsiteDataStoreRef dataStoreRef, bool enabled, void* context, WKWebsiteDataStoreResetQuotaCallback callback)
{
    protect(WebKit::toImpl(dataStoreRef))->setOriginQuotaRatioEnabledForTesting(enabled, [context, callback] {
        if (callback)
            callback(context);
    });
}

void WKWebsiteDataStoreClearAppBoundSession(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreClearAppBoundSessionFunction completionHandler)
{
#if ENABLE(APP_BOUND_DOMAINS)
    protect(WebKit::toImpl(dataStoreRef))->clearAppBoundSession([context, completionHandler] {
        completionHandler(context);
    });
#else
    UNUSED_PARAM(dataStoreRef);
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreReinitializeAppBoundDomains(WKWebsiteDataStoreRef dataStoreRef)
{
#if ENABLE(APP_BOUND_DOMAINS)
    protect(WebKit::toImpl(dataStoreRef))->reinitializeAppBoundDomains();
#else
    UNUSED_PARAM(dataStoreRef);
#endif
}

void WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(WKWebsiteDataStoreRef dataStoreRef, const WKStringRef bundleIdentifier, void* context, WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcessFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->updateBundleIdentifierInNetworkProcess(protect(WebKit::toImpl(bundleIdentifier))->string(), [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreClearBundleIdentifierInNetworkProcessFunction completionHandler)
{
    protect(WebKit::toImpl(dataStoreRef))->clearBundleIdentifierInNetworkProcess([context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreGetAllStorageAccessEntries(WKWebsiteDataStoreRef dataStoreRef, WKPageRef pageRef, void* context, WKWebsiteDataStoreGetAllStorageAccessEntriesFunction callback)
{
    protect(WebKit::toImpl(dataStoreRef))->getAllStorageAccessEntries(WebKit::toImpl(pageRef)->identifier(), [context, callback] (Vector<String>&& domains) {
        auto domainArrayRef = WKMutableArrayCreate();
        for (auto domain : domains)
            WKArrayAppendItem(domainArrayRef, adoptWK(WKStringCreateWithUTF8CString(domain.utf8().data())).get());

        callback(context, domainArrayRef);
    });
}

void WKWebsiteDataStoreResetResourceMonitorThrottler(WKWebsiteDataStoreRef dataStoreRef, void* context, KWebsiteDataStoreResetResourceMonitorThrottler callback)
{
#if ENABLE(CONTENT_EXTENSIONS)
    protect(WebKit::toImpl(dataStoreRef))->resetResourceMonitorThrottlerForTesting([context, callback] () {
        if (callback)
            callback(context);
    });
#else
    UNUSED_PARAM(dataStoreRef);
    if (callback)
        callback(context);
#endif
}

void WKWebsiteDataStoreSetStorageAccessPermissionForTesting(WKWebsiteDataStoreRef dataStoreRef, WKPageRef pageRef, bool granted, WKStringRef topFrame, WKStringRef subFrame, void* context, WKWebsiteDataStoreSetStorageAccessPermissionForTestingFunction completionHandler)
{
    Ref callbackAggregator = CallbackAggregator::create([context, completionHandler] {
        completionHandler(context);
    });

    Ref store = *WebKit::toImpl(dataStoreRef);
    if (!granted)
        store->clearResourceLoadStatisticsInWebProcesses([callbackAggregator] { });
    store->setStorageAccessPermissionForTesting(granted, WebKit::toImpl(pageRef)->identifier(), protect(WebKit::toImpl(topFrame))->string(), protect(WebKit::toImpl(subFrame))->string(), [callbackAggregator] { });
}

void WKWebsiteDataStoreSetStorageAccessForTesting(WKWebsiteDataStoreRef dataStoreRef, bool blocked, void* context, WKWebsiteDataStoreSetStorageAccessForTestingFunction completionHandler)
{
    Ref callbackAggregator = CallbackAggregator::create([context, completionHandler] {
        completionHandler(context);
    });

    Ref store = *WebKit::toImpl(dataStoreRef);
    if (blocked)
        store->clearStorageAccessForTesting([callbackAggregator] { });
    store->setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(blocked, WebCore::ThirdPartyCookieBlockingMode::All, [callbackAggregator] { });
}
