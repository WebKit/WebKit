/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
    return WebKit::toAPI(WebKit::WebsiteDataStore::defaultDataStore().ptr());
}

WKWebsiteDataStoreRef WKWebsiteDataStoreCreateNonPersistentDataStore()
{
    return WebKit::toAPI(&WebKit::WebsiteDataStore::createNonPersistent().leakRef());
}

WKWebsiteDataStoreRef WKWebsiteDataStoreCreateWithConfiguration(WKWebsiteDataStoreConfigurationRef configuration)
{
    auto sessionID = WebKit::toImpl(configuration)->isPersistent() ? PAL::SessionID::generatePersistentSessionID() : PAL::SessionID::generateEphemeralSessionID();
    return WebKit::toAPI(&WebKit::WebsiteDataStore::create(*WebKit::toImpl(configuration), sessionID).leakRef());
}

void WKWebsiteDataStoreTerminateNetworkProcess(WKWebsiteDataStoreRef dataStore)
{
    WebKit::toImpl(dataStore)->terminateNetworkProcess();
}

WKProcessID WKWebsiteDataStoreGetNetworkProcessIdentifier(WKWebsiteDataStoreRef dataStore)
{
    return WebKit::toImpl(dataStore)->networkProcess().processIdentifier();
}

void WKWebsiteDataStoreRemoveITPDataForDomain(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreRemoveITPDataForDomainFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::WebsiteDataRecord dataRecord;
    dataRecord.types.add(WebKit::WebsiteDataType::ResourceLoadStatistics);
    dataRecord.addResourceLoadStatisticsRegistrableDomain(WebCore::RegistrableDomain::uncheckedCreateFromHost(WebKit::toImpl(host)->string()));
    Vector<WebKit::WebsiteDataRecord> dataRecords = { WTFMove(dataRecord) };

    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::ResourceLoadStatistics;
    WebKit::toImpl(dataStoreRef)->removeData(dataTypes, dataRecords, [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabase(WKWebsiteDataStoreRef dataStoreRef, int domainID, void* context, WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabaseFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->domainIDExistsInDatabase(domainID, [context, callback](bool exists) {
        callback(exists, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetServiceWorkerFetchTimeoutForTesting(WKWebsiteDataStoreRef dataStore, double seconds)
{
    WebKit::toImpl(dataStore)->setServiceWorkerTimeoutForTesting(Seconds(seconds));
}

void WKWebsiteDataStoreResetServiceWorkerFetchTimeoutForTesting(WKWebsiteDataStoreRef dataStore)
{
    WebKit::toImpl(dataStore)->resetServiceWorkerTimeoutForTesting();
}

void WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(WKWebsiteDataStoreRef dataStoreRef, bool enable)
{
    auto* websiteDataStore = WebKit::toImpl(dataStoreRef);
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    websiteDataStore->useExplicitITPState();
#endif
    websiteDataStore->setResourceLoadStatisticsEnabled(enable);
}

void WKWebsiteDataStoreIsStatisticsEphemeral(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsEphemeralFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->isResourceLoadStatisticsEphemeral([context, completionHandler](bool isEphemeral) {
        completionHandler(isEphemeral, context);
    });
#else
    completionHandler(false, context);
#endif
}

bool WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(WKWebsiteDataStoreRef dataStoreRef)
{
    return WebKit::toImpl(dataStoreRef)->resourceLoadStatisticsEnabled();
}

void WKWebsiteDataStoreSetResourceLoadStatisticsDebugMode(WKWebsiteDataStoreRef dataStoreRef, bool enable)
{
    WebKit::toImpl(dataStoreRef)->setResourceLoadStatisticsDebugMode(enable);
}

void WKWebsiteDataStoreSyncLocalStorage(WKWebsiteDataStoreRef dataStore, void* context, WKWebsiteDataStoreSyncLocalStorageCallback callback)
{
    WebKit::toImpl(dataStore)->syncLocalStorage([context, callback] {
        if (callback)
            callback(context);
    });
}

WKHTTPCookieStoreRef WKWebsiteDataStoreGetHTTPCookieStore(WKWebsiteDataStoreRef dataStoreRef)
{
    return WebKit::toAPI(&WebKit::toImpl(dataStoreRef)->cookieStore());
}

void WKWebsiteDataStoreSetAllowsAnySSLCertificateForWebSocketTesting(WKWebsiteDataStoreRef dataStore, bool allows)
{
    WebKit::toImpl(dataStore)->setAllowsAnySSLCertificateForWebSocket(allows);
}

void WKWebsiteDataStoreClearCachedCredentials(WKWebsiteDataStoreRef dataStoreRef)
{
    WebKit::toImpl(dataStoreRef)->clearCachedCredentials();
}

void WKWebsiteDataStoreSetResourceLoadStatisticsDebugModeWithCompletionHandler(WKWebsiteDataStoreRef dataStoreRef, bool enable, void* context, WKWebsiteDataStoreStatisticsDebugModeFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setResourceLoadStatisticsDebugMode(enable, [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreStatisticsDebugModeFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setPrevalentResourceForDebugMode(URL(URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}
void WKWebsiteDataStoreSetStatisticsLastSeen(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, double seconds, void* context, WKWebsiteDataStoreStatisticsLastSeenFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setLastSeen(URL(URL(), WebKit::toImpl(host)->string()), Seconds { seconds }, [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetStatisticsMergeStatistic(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef topFrameDomain1, WKStringRef topFrameDomain2, double lastSeen, bool hadUserInteraction, double mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, void* context, WKWebsiteDataStoreStatisticsMergeStatisticFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->mergeStatisticForTesting(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(topFrameDomain1)->string()), URL(URL(), WebKit::toImpl(topFrameDomain2)->string()), Seconds { lastSeen }, hadUserInteraction, Seconds { mostRecentUserInteraction }, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetStatisticsExpiredStatistic(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, void* context, WKWebsiteDataStoreStatisticsMergeStatisticFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->insertExpiredStatisticForTesting(URL(URL(), WebKit::toImpl(host)->string()), hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetStatisticsPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsPrevalentResourceFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto& websiteDataStore = *WebKit::toImpl(dataStoreRef);

    if (value)
        websiteDataStore.setPrevalentResource(URL(URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
    else
        websiteDataStore.clearPrevalentResource(URL(URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsVeryPrevalentResourceFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto& websiteDataStore = *WebKit::toImpl(dataStoreRef);

    if (value)
        websiteDataStore.setVeryPrevalentResource(URL(URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
    else
        websiteDataStore.clearPrevalentResource(URL(URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreDumpResourceLoadStatistics(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreDumpResourceLoadStatisticsFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->dumpResourceLoadStatistics([context, callback] (const String& resourceLoadStatistics) {
        callback(WebKit::toAPI(resourceLoadStatistics.impl()), context);
    });
#else
    callback(WebKit::toAPI(emptyString().impl()), context);
#endif
}

void WKWebsiteDataStoreIsStatisticsPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsPrevalentResourceFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->isPrevalentResource(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool isPrevalentResource) {
        callback(isPrevalentResource, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsPrevalentResourceFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->isVeryPrevalentResource(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool isVeryPrevalentResource) {
        callback(isVeryPrevalentResource, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(WKWebsiteDataStoreRef dataStoreRef, WKStringRef subresourceHost, WKStringRef topFrameHost, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnderFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->isRegisteredAsSubresourceUnder(URL(URL(), WebKit::toImpl(subresourceHost)->string()), URL(URL(), WebKit::toImpl(topFrameHost)->string()), [context, callback](bool isRegisteredAsSubresourceUnder) {
        callback(isRegisteredAsSubresourceUnder, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(WKWebsiteDataStoreRef dataStoreRef, WKStringRef subFrameHost, WKStringRef topFrameHost, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnderFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->isRegisteredAsSubFrameUnder(URL(URL(), WebKit::toImpl(subFrameHost)->string()), URL(URL(), WebKit::toImpl(topFrameHost)->string()), [context, callback](bool isRegisteredAsSubFrameUnder) {
        callback(isRegisteredAsSubFrameUnder, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef hostRedirectedFrom, WKStringRef hostRedirectedTo, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingToFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->isRegisteredAsRedirectingTo(URL(URL(), WebKit::toImpl(hostRedirectedFrom)->string()), URL(URL(), WebKit::toImpl(hostRedirectedTo)->string()), [context, callback](bool isRegisteredAsRedirectingTo) {
        callback(isRegisteredAsRedirectingTo, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsHasHadUserInteractionFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto& dataStore = *WebKit::toImpl(dataStoreRef);

    if (value)
        dataStore.logUserInteraction(URL(URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
    else
        dataStore.clearUserInteraction(URL(URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreIsStatisticsHasHadUserInteraction(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsHasHadUserInteractionFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->hasHadUserInteraction(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool hasHadUserInteraction) {
        callback(hasHadUserInteraction, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnce(WKWebsiteDataStoreRef dataStoreRef, WKStringRef subHost, WKStringRef topHost, void* context, WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnceFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->isRelationshipOnlyInDatabaseOnce(URL(URL(), WebKit::toImpl(subHost)->string()), URL(URL(), WebKit::toImpl(topHost)->string()), [context, callback](bool onlyInDatabaseOnce) {
        callback(onlyInDatabaseOnce, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetStatisticsGrandfathered(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setGrandfathered(URL(URL(), WebKit::toImpl(host)->string()), value, [] { });
#endif
}

void WKWebsiteDataStoreIsStatisticsGrandfathered(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsGrandfatheredFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->isGrandfathered(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool isGrandfathered) {
        callback(isGrandfathered, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef topFrameHost)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setSubframeUnderTopFrameDomain(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(topFrameHost)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef topFrameHost)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setSubresourceUnderTopFrameDomain(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(topFrameHost)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedTo)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setSubresourceUniqueRedirectTo(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(hostRedirectedTo)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedFrom)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setSubresourceUniqueRedirectFrom(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(hostRedirectedFrom)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedTo)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setTopFrameUniqueRedirectTo(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(hostRedirectedTo)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedFrom)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setTopFrameUniqueRedirectFrom(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(hostRedirectedFrom)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(WKWebsiteDataStoreRef dataStoreRef, WKStringRef fromHost, WKStringRef toHost, void* context, WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecorationFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setCrossSiteLoadWithLinkDecorationForTesting(URL(URL(), WebKit::toImpl(fromHost)->string()), URL(URL(), WebKit::toImpl(toHost)->string()), [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(WKWebsiteDataStoreRef dataStoreRef, double seconds, void* context, WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteractionFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setTimeToLiveUserInteraction(Seconds { seconds }, [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecordsFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->scheduleStatisticsAndDataRecordsProcessing([context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsUpdateCookieBlocking(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsUpdateCookieBlockingFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->scheduleCookieBlockingUpdate([context, completionHandler]() {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreStatisticsSubmitTelemetry(WKWebsiteDataStoreRef dataStoreRef)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->submitTelemetry();
#endif
}

void WKWebsiteDataStoreSetStatisticsNotifyPagesWhenDataRecordsWereScanned(WKWebsiteDataStoreRef dataStoreRef, bool value)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setNotifyPagesWhenDataRecordsWereScanned(value, [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsIsRunningTest(WKWebsiteDataStoreRef dataStoreRef, bool value, void* context, WKWebsiteDataStoreSetStatisticsIsRunningTestFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setIsRunningResourceLoadStatisticsTest(value, [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreSetStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(WKWebsiteDataStoreRef dataStoreRef, bool value)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setShouldClassifyResourcesBeforeDataRecordsRemoval(value, []() { });
#endif
}

void WKWebsiteDataStoreSetStatisticsMinimumTimeBetweenDataRecordsRemoval(WKWebsiteDataStoreRef dataStoreRef, double seconds)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setMinimumTimeBetweenDataRecordsRemoval(Seconds { seconds }, []() { });
#endif
}

void WKWebsiteDataStoreSetStatisticsGrandfatheringTime(WKWebsiteDataStoreRef dataStoreRef, double seconds)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setGrandfatheringTime(Seconds { seconds }, []() { });
#endif
}

void WKWebsiteDataStoreSetStatisticsMaxStatisticsEntries(WKWebsiteDataStoreRef dataStoreRef, unsigned entries)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setMaxStatisticsEntries(entries, []() { });
#endif
}

void WKWebsiteDataStoreSetStatisticsPruneEntriesDownTo(WKWebsiteDataStoreRef dataStoreRef, unsigned entries)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setPruneEntriesDownTo(entries, []() { });
#endif
}

void WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->scheduleClearInMemoryAndPersistent(WebKit::ShouldGrandfatherStatistics::Yes, [context, callback]() {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(WKWebsiteDataStoreRef dataStoreRef, unsigned hours, void* context, WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHoursFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->scheduleClearInMemoryAndPersistent(WallTime::now() - Seconds::fromHours(hours), WebKit::ShouldGrandfatherStatistics::Yes, [context, callback]() {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::ResourceLoadStatistics;
    WebKit::toImpl(dataStoreRef)->removeData(dataTypes, WallTime::fromRawSeconds(0), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool includeHttpOnlyCookies, void* context, WKWebsiteDataStoreStatisticsDeleteCookiesForTestingFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->deleteCookiesForTesting(URL(URL(), WebKit::toImpl(host)->string()), includeHttpOnlyCookies, [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsHasLocalStorage(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreStatisticsHasLocalStorageFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->hasLocalStorageForTesting(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool hasLocalStorage) {
        callback(hasLocalStorage, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetStatisticsCacheMaxAgeCap(WKWebsiteDataStoreRef dataStoreRef, double seconds, void* context, WKWebsiteDataStoreSetStatisticsCacheMaxAgeCapFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setCacheMaxAgeCapForPrevalentResources(Seconds { seconds }, [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsHasIsolatedSession(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreStatisticsHasIsolatedSessionFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->hasIsolatedSessionForTesting(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool hasIsolatedSession) {
        callback(hasIsolatedSession, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreHasAppBoundSession(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreHasAppBoundSessionFunction callback)
{
#if ENABLE(APP_BOUND_DOMAINS)
    WebKit::toImpl(dataStoreRef)->hasAppBoundSession([context, callback](bool hasAppBoundSession) {
        callback(hasAppBoundSession, context);
    });
#else
    UNUSED_PARAM(dataStoreRef);
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetResourceLoadStatisticsShouldDowngradeReferrerForTesting(WKWebsiteDataStoreRef dataStoreRef, bool enabled, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsShouldDowngradeReferrerForTestingFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setResourceLoadStatisticsShouldDowngradeReferrerForTesting(enabled, [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(WKWebsiteDataStoreRef dataStoreRef, bool enabled, bool onlyOnSitesWithoutUserInteraction, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsShouldBlockThirdPartyCookiesForTestingFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(enabled, onlyOnSitesWithoutUserInteraction, [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(WKWebsiteDataStoreRef dataStoreRef, bool enabled, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTestingFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(enabled, [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTesting(WKWebsiteDataStoreRef dataStoreRef, WKStringRef hostName, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTestingFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setResourceLoadStatisticsToSameSiteStrictCookiesForTesting(URL(URL(), WebKit::toImpl(hostName)->string()), [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(WKWebsiteDataStoreRef dataStoreRef, WKStringRef firstPartyURLString, WKStringRef cnameURLString, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTestingFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(URL(URL(), WebKit::toImpl(firstPartyURLString)->string()), URL(URL(), WebKit::toImpl(cnameURLString)->string()), [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(WKWebsiteDataStoreRef dataStoreRef, WKStringRef cnameURLString, void* context, WKWebsiteDataStoreSetResourceLoadStatisticsThirdPartyCNAMEDomainForTestingFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->setResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(URL(URL(), WebKit::toImpl(cnameURLString)->string()), [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetAppBoundDomainsForTesting(WKArrayRef originURLsRef, void* context, WKWebsiteDataStoreSetAppBoundDomainsForTestingFunction completionHandler)
{
#if ENABLE(APP_BOUND_DOMAINS)
    RefPtr<API::Array> originURLsArray = WebKit::toImpl(originURLsRef);
    size_t newSize = originURLsArray ? originURLsArray->size() : 0;
    HashSet<WebCore::RegistrableDomain> domains;
    domains.reserveInitialCapacity(newSize);
    for (size_t i = 0; i < newSize; ++i) {
        auto* originURL = originURLsArray->at<API::URL>(i);
        if (!originURL)
            continue;
        
        domains.add(WebCore::RegistrableDomain { URL(URL(), originURL->string()) });
    }

    WebKit::WebsiteDataStore::setAppBoundDomainsForTesting(WTFMove(domains), [context, completionHandler] {
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
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto callbackAggregator = CallbackAggregator::create([context, completionHandler]() {
        completionHandler(context);
    });

    auto& store = *WebKit::toImpl(dataStoreRef);
    store.clearResourceLoadStatisticsInWebProcesses([callbackAggregator] { });
    store.resetCacheMaxAgeCapForPrevalentResources([callbackAggregator] { });
    store.resetCrossSiteLoadsWithLinkDecorationForTesting([callbackAggregator] { });
    store.setResourceLoadStatisticsShouldDowngradeReferrerForTesting(true, [callbackAggregator] { });
    store.setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(false, false, [callbackAggregator] { });
    store.setResourceLoadStatisticsShouldEnbleSameSiteStrictEnforcementForTesting(true, [callbackAggregator] { });
    store.setResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(false, [callbackAggregator] { });
    store.resetParametersToDefaultValues([callbackAggregator] { });
    store.scheduleClearInMemoryAndPersistent(WebKit::ShouldGrandfatherStatistics::No, [callbackAggregator] { });
#else
    UNUSED_PARAM(dataStoreRef);
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreRemoveAllFetchCaches(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveFetchCacheRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::DOMCache;
    WebKit::toImpl(dataStoreRef)->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveFetchCacheForOrigin(WKWebsiteDataStoreRef dataStoreRef, WKSecurityOriginRef origin, void* context, WKWebsiteDataStoreRemoveFetchCacheRemovalFunction callback)
{
    WebKit::WebsiteDataRecord dataRecord;
    dataRecord.add(WebKit::WebsiteDataType::DOMCache, WebKit::toImpl(origin)->securityOrigin().data());
    Vector<WebKit::WebsiteDataRecord> dataRecords = { WTFMove(dataRecord) };

    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::DOMCache;
    WebKit::toImpl(dataStoreRef)->removeData(dataTypes, dataRecords, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveAllIndexedDatabases(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveAllIndexedDatabasesCallback callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::IndexedDBDatabases;
    WebKit::toImpl(dataStoreRef)->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
    if (callback)
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveLocalStorage(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveLocalStorageCallback callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::LocalStorage;
    WebKit::toImpl(dataStoreRef)->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        if (callback)
            callback(context);
    });
}

void WKWebsiteDataStoreRemoveAllServiceWorkerRegistrations(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveAllServiceWorkerRegistrationsCallback callback)
{
#if ENABLE(SERVICE_WORKER)
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::ServiceWorkerRegistrations;
    WebKit::toImpl(dataStoreRef)->removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        callback(context);
    });
#else
    UNUSED_PARAM(dataStoreRef);
    callback(context);
#endif
}

void WKWebsiteDataStoreGetFetchCacheOrigins(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreGetFetchCacheOriginsFunction callback)
{
    WebKit::toImpl(dataStoreRef)->fetchData(WebKit::WebsiteDataType::DOMCache, { }, [context, callback] (auto dataRecords) {
        Vector<RefPtr<API::Object>> securityOrigins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                securityOrigins.append(API::SecurityOrigin::create(origin.securityOrigin()));
        }
        callback(WebKit::toAPI(API::Array::create(WTFMove(securityOrigins)).ptr()), context);
    });
}

void WKWebsiteDataStoreGetFetchCacheSizeForOrigin(WKWebsiteDataStoreRef dataStoreRef, WKStringRef origin, void* context, WKWebsiteDataStoreGetFetchCacheSizeForOriginFunction callback)
{
    OptionSet<WebKit::WebsiteDataFetchOption> fetchOptions = WebKit::WebsiteDataFetchOption::ComputeSizes;

    WebKit::toImpl(dataStoreRef)->fetchData(WebKit::WebsiteDataType::DOMCache, fetchOptions, [origin, context, callback] (auto dataRecords) {
        auto originData = WebCore::SecurityOrigin::createFromString(WebKit::toImpl(origin)->string())->data();
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
    WebKit::toImpl(dataStoreRef)->deviceOrientationAndMotionAccessController().clearPermissions();
#endif
}

void WKWebsiteDataStoreClearAdClickAttributionsThroughWebsiteDataRemoval(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreClearAdClickAttributionsThroughWebsiteDataRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::AdClickAttributions;
    WebKit::toImpl(dataStoreRef)->removeData(dataTypes, WallTime::fromRawSeconds(0), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreSetCacheModelSynchronouslyForTesting(WKWebsiteDataStoreRef dataStoreRef, WKCacheModel cacheModel)
{
    WebKit::toImpl(dataStoreRef)->setCacheModelSynchronouslyForTesting(WebKit::toCacheModel(cacheModel));
}

void WKWebsiteDataStoreResetQuota(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreResetQuotaCallback callback)
{
    WebKit::toImpl(dataStoreRef)->resetQuota([context, callback] {
        if (callback)
            callback(context);
    });
}

void WKWebsiteDataStoreClearAppBoundSession(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreClearAppBoundSessionFunction completionHandler)
{
#if ENABLE(APP_BOUND_DOMAINS)
    WebKit::toImpl(dataStoreRef)->clearAppBoundSession([context, completionHandler] {
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
    WebKit::toImpl(dataStoreRef)->reinitializeAppBoundDomains();
#else
    UNUSED_PARAM(dataStoreRef);
#endif
}

void WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(WKWebsiteDataStoreRef dataStoreRef, const WKStringRef bundleIdentifier, void* context, WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcessFunction completionHandler)
{
    WebKit::toImpl(dataStoreRef)->updateBundleIdentifierInNetworkProcess(WebKit::toImpl(bundleIdentifier)->string(), [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreClearBundleIdentifierInNetworkProcessFunction completionHandler)
{
    WebKit::toImpl(dataStoreRef)->clearBundleIdentifierInNetworkProcess([context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreGetAllStorageAccessEntries(WKWebsiteDataStoreRef dataStoreRef, WKPageRef pageRef, void* context, WKWebsiteDataStoreGetAllStorageAccessEntriesFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->getAllStorageAccessEntries(WebKit::toImpl(pageRef)->identifier(), [context, callback] (Vector<String>&& domains) {
        auto domainArrayRef = WKMutableArrayCreate();
        for (auto domain : domains)
            WKArrayAppendItem(domainArrayRef, adoptWK(WKStringCreateWithUTF8CString(domain.utf8().data())).get());

        callback(context, domainArrayRef);
    });
#else
    callback(context, WKMutableArrayCreate());
#endif
}
