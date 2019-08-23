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
#include "APIWebsiteDataStore.h"
#include "MockWebAuthenticationConfiguration.h"
#include "ShouldGrandfatherStatistics.h"
#include "WKAPICast.h"
#include "WKDictionary.h"
#include "WKNumber.h"
#include "WKRetainPtr.h"
#include "WKSecurityOriginRef.h"
#include "WKString.h"
#include "WebDeviceOrientationAndMotionAccessController.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebsiteData.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataRecord.h"
#include "WebsiteDataType.h"
#include <wtf/CallbackAggregator.h>
#include <wtf/URL.h>

WKTypeID WKWebsiteDataStoreGetTypeID()
{
    return WebKit::toAPI(API::WebsiteDataStore::APIType);
}

WKWebsiteDataStoreRef WKWebsiteDataStoreGetDefaultDataStore()
{
    return WebKit::toAPI(API::WebsiteDataStore::defaultDataStore().ptr());
}

WKWebsiteDataStoreRef WKWebsiteDataStoreCreateNonPersistentDataStore()
{
    return WebKit::toAPI(&API::WebsiteDataStore::createNonPersistentDataStore().leakRef());
}

void WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(WKWebsiteDataStoreRef dataStoreRef, bool enable)
{
    WebKit::toImpl(dataStoreRef)->setResourceLoadStatisticsEnabled(enable);
}

bool WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(WKWebsiteDataStoreRef dataStoreRef)
{
    return WebKit::toImpl(dataStoreRef)->resourceLoadStatisticsEnabled();
}

void WKWebsiteDataStoreSetResourceLoadStatisticsDebugMode(WKWebsiteDataStoreRef dataStoreRef, bool enable)
{
    WebKit::toImpl(dataStoreRef)->setResourceLoadStatisticsDebugMode(enable);
}

void WKWebsiteDataStoreSetResourceLoadStatisticsDebugModeWithCompletionHandler(WKWebsiteDataStoreRef dataStoreRef, bool enable, void* context, WKWebsiteDataStoreStatisticsDebugModeFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setResourceLoadStatisticsDebugMode(enable, [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreStatisticsDebugModeFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setPrevalentResourceForDebugMode(URL(URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}
void WKWebsiteDataStoreSetStatisticsLastSeen(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, double seconds, void* context, WKWebsiteDataStoreStatisticsLastSeenFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setLastSeen(URL(URL(), WebKit::toImpl(host)->string()), Seconds { seconds }, [context, completionHandler] {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreSetStatisticsPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsPrevalentResourceFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto& websiteDataStore = WebKit::toImpl(dataStoreRef)->websiteDataStore();

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
    auto& websiteDataStore = WebKit::toImpl(dataStoreRef)->websiteDataStore();

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
    WebKit::toImpl(dataStoreRef)->websiteDataStore().dumpResourceLoadStatistics([context, callback] (const String& resourceLoadStatistics) {
        callback(WebKit::toAPI(resourceLoadStatistics.impl()), context);
    });
#else
    callback(WebKit::toAPI(emptyString().impl()), context);
#endif
}

void WKWebsiteDataStoreIsStatisticsPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsPrevalentResourceFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().isPrevalentResource(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool isPrevalentResource) {
        callback(isPrevalentResource, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsPrevalentResourceFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().isVeryPrevalentResource(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool isVeryPrevalentResource) {
        callback(isVeryPrevalentResource, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(WKWebsiteDataStoreRef dataStoreRef, WKStringRef subresourceHost, WKStringRef topFrameHost, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnderFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().isRegisteredAsSubresourceUnder(URL(URL(), WebKit::toImpl(subresourceHost)->string()), URL(URL(), WebKit::toImpl(topFrameHost)->string()), [context, callback](bool isRegisteredAsSubresourceUnder) {
        callback(isRegisteredAsSubresourceUnder, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(WKWebsiteDataStoreRef dataStoreRef, WKStringRef subFrameHost, WKStringRef topFrameHost, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnderFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().isRegisteredAsSubFrameUnder(URL(URL(), WebKit::toImpl(subFrameHost)->string()), URL(URL(), WebKit::toImpl(topFrameHost)->string()), [context, callback](bool isRegisteredAsSubFrameUnder) {
        callback(isRegisteredAsSubFrameUnder, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef hostRedirectedFrom, WKStringRef hostRedirectedTo, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingToFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().isRegisteredAsRedirectingTo(URL(URL(), WebKit::toImpl(hostRedirectedFrom)->string()), URL(URL(), WebKit::toImpl(hostRedirectedTo)->string()), [context, callback](bool isRegisteredAsRedirectingTo) {
        callback(isRegisteredAsRedirectingTo, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsHasHadUserInteractionFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto& dataStore = WebKit::toImpl(dataStoreRef)->websiteDataStore();

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
    WebKit::toImpl(dataStoreRef)->websiteDataStore().hasHadUserInteraction(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool hasHadUserInteraction) {
        callback(hasHadUserInteraction, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetStatisticsGrandfathered(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setGrandfathered(URL(URL(), WebKit::toImpl(host)->string()), value, [] { });
#endif
}

void WKWebsiteDataStoreIsStatisticsGrandfathered(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsGrandfatheredFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().hasHadUserInteraction(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool isGrandfathered) {
        callback(isGrandfathered, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef topFrameHost)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setSubframeUnderTopFrameDomain(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(topFrameHost)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef topFrameHost)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setSubresourceUnderTopFrameDomain(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(topFrameHost)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedTo)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setSubresourceUniqueRedirectTo(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(hostRedirectedTo)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedFrom)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setSubresourceUniqueRedirectFrom(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(hostRedirectedFrom)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedTo)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setTopFrameUniqueRedirectTo(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(hostRedirectedTo)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedFrom)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setTopFrameUniqueRedirectFrom(URL(URL(), WebKit::toImpl(host)->string()), URL(URL(), WebKit::toImpl(hostRedirectedFrom)->string()), [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(WKWebsiteDataStoreRef dataStoreRef, WKStringRef fromHost, WKStringRef toHost, void* context, WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecorationFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setCrossSiteLoadWithLinkDecorationForTesting(URL(URL(), WebKit::toImpl(fromHost)->string()), URL(URL(), WebKit::toImpl(toHost)->string()), [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(WKWebsiteDataStoreRef dataStoreRef, double seconds, void* context, WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteractionFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setTimeToLiveUserInteraction(Seconds { seconds }, [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecordsFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().scheduleStatisticsAndDataRecordsProcessing([context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsUpdateCookieBlocking(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsUpdateCookieBlockingFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().scheduleCookieBlockingUpdate([context, completionHandler]() {
        completionHandler(context);
    });
#else
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreStatisticsSubmitTelemetry(WKWebsiteDataStoreRef dataStoreRef)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().submitTelemetry();
#endif
}

void WKWebsiteDataStoreSetStatisticsNotifyPagesWhenDataRecordsWereScanned(WKWebsiteDataStoreRef dataStoreRef, bool value)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setNotifyPagesWhenDataRecordsWereScanned(value, [] { });
#endif
}

void WKWebsiteDataStoreSetStatisticsIsRunningTest(WKWebsiteDataStoreRef dataStoreRef, bool value, void* context, WKWebsiteDataStoreSetStatisticsIsRunningTestFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setIsRunningResourceLoadStatisticsTest(value, [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreSetStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(WKWebsiteDataStoreRef dataStoreRef, bool value)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setShouldClassifyResourcesBeforeDataRecordsRemoval(value, []() { });
#endif
}

void WKWebsiteDataStoreSetStatisticsNotifyPagesWhenTelemetryWasCaptured(WKWebsiteDataStoreRef dataStoreRef, bool value)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setNotifyPagesWhenTelemetryWasCaptured(value, []() { });
#endif
}

void WKWebsiteDataStoreSetStatisticsMinimumTimeBetweenDataRecordsRemoval(WKWebsiteDataStoreRef dataStoreRef, double seconds)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setMinimumTimeBetweenDataRecordsRemoval(Seconds { seconds }, []() { });
#endif
}

void WKWebsiteDataStoreSetStatisticsGrandfatheringTime(WKWebsiteDataStoreRef dataStoreRef, double seconds)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setGrandfatheringTime(Seconds { seconds }, []() { });
#endif
}

void WKWebsiteDataStoreSetStatisticsMaxStatisticsEntries(WKWebsiteDataStoreRef dataStoreRef, unsigned entries)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setMaxStatisticsEntries(entries, []() { });
#endif
}

void WKWebsiteDataStoreSetStatisticsPruneEntriesDownTo(WKWebsiteDataStoreRef dataStoreRef, unsigned entries)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setPruneEntriesDownTo(entries, []() { });
#endif
}

void WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().scheduleClearInMemoryAndPersistent(WebKit::ShouldGrandfatherStatistics::Yes, [context, callback]() {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(WKWebsiteDataStoreRef dataStoreRef, unsigned hours, void* context, WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHoursFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().scheduleClearInMemoryAndPersistent(WallTime::now() - Seconds::fromHours(hours), WebKit::ShouldGrandfatherStatistics::Yes, [context, callback]() {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::ResourceLoadStatistics;
    WebKit::toImpl(dataStoreRef)->websiteDataStore().removeData(dataTypes, WallTime::fromRawSeconds(0), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool includeHttpOnlyCookies, void* context, WKWebsiteDataStoreStatisticsDeleteCookiesForTestingFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().deleteCookiesForTesting(URL(URL(), WebKit::toImpl(host)->string()), includeHttpOnlyCookies, [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsHasLocalStorage(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreStatisticsHasLocalStorageFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().hasLocalStorageForTesting(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool hasLocalStorage) {
        callback(hasLocalStorage, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreSetStatisticsCacheMaxAgeCap(WKWebsiteDataStoreRef dataStoreRef, double seconds, void* context, WKWebsiteDataStoreSetStatisticsCacheMaxAgeCapFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setCacheMaxAgeCapForPrevalentResources(Seconds { seconds }, [context, callback] {
        callback(context);
    });
#else
    callback(context);
#endif
}

void WKWebsiteDataStoreStatisticsHasIsolatedSession(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreStatisticsHasIsolatedSessionFunction callback)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().hasIsolatedSessionForTesting(URL(URL(), WebKit::toImpl(host)->string()), [context, callback](bool hasIsolatedSession) {
        callback(hasIsolatedSession, context);
    });
#else
    callback(false, context);
#endif
}

void WKWebsiteDataStoreStatisticsResetToConsistentState(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsResetToConsistentStateFunction completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto callbackAggregator = CallbackAggregator::create([context, completionHandler]() {
        completionHandler(context);
    });

    auto& store = WebKit::toImpl(dataStoreRef)->websiteDataStore();
    store.clearResourceLoadStatisticsInWebProcesses([callbackAggregator = callbackAggregator.copyRef()] { });
    store.resetCacheMaxAgeCapForPrevalentResources([callbackAggregator = callbackAggregator.copyRef()] { });
    store.resetCrossSiteLoadsWithLinkDecorationForTesting([callbackAggregator = callbackAggregator.copyRef()] { });
    store.resetParametersToDefaultValues([callbackAggregator = callbackAggregator.copyRef()] { });
    store.scheduleClearInMemoryAndPersistent(WebKit::ShouldGrandfatherStatistics::No, [callbackAggregator = callbackAggregator.copyRef()] { });
#else
    UNUSED_PARAM(dataStoreRef);
    completionHandler(context);
#endif
}

void WKWebsiteDataStoreRemoveAllFetchCaches(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveFetchCacheRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::DOMCache;
    WebKit::toImpl(dataStoreRef)->websiteDataStore().removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveFetchCacheForOrigin(WKWebsiteDataStoreRef dataStoreRef, WKSecurityOriginRef origin, void* context, WKWebsiteDataStoreRemoveFetchCacheRemovalFunction callback)
{
    WebKit::WebsiteDataRecord dataRecord;
    dataRecord.add(WebKit::WebsiteDataType::DOMCache, WebKit::toImpl(origin)->securityOrigin().data());
    Vector<WebKit::WebsiteDataRecord> dataRecords = { WTFMove(dataRecord) };

    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::DOMCache;
    WebKit::toImpl(dataStoreRef)->websiteDataStore().removeData(dataTypes, dataRecords, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveAllIndexedDatabases(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveAllIndexedDatabasesCallback callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::IndexedDBDatabases;
    WebKit::toImpl(dataStoreRef)->websiteDataStore().removeData(dataTypes, -WallTime::infinity(), [context, callback] {
    if (callback)
        callback(context);
    });
}

void WKWebsiteDataStoreRemoveLocalStorage(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveLocalStorageCallback callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::LocalStorage;
    WebKit::toImpl(dataStoreRef)->websiteDataStore().removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        if (callback)
            callback(context);
    });
}

void WKWebsiteDataStoreRemoveAllServiceWorkerRegistrations(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreRemoveAllServiceWorkerRegistrationsCallback callback)
{
#if ENABLE(SERVICE_WORKER)
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::ServiceWorkerRegistrations;
    WebKit::toImpl(dataStoreRef)->websiteDataStore().removeData(dataTypes, -WallTime::infinity(), [context, callback] {
        callback(context);
    });
#else
    UNUSED_PARAM(dataStoreRef);
    callback(context);
#endif
}

void WKWebsiteDataStoreGetFetchCacheOrigins(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreGetFetchCacheOriginsFunction callback)
{
    WebKit::toImpl(dataStoreRef)->websiteDataStore().fetchData(WebKit::WebsiteDataType::DOMCache, { }, [context, callback] (auto dataRecords) {
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

    WebKit::toImpl(dataStoreRef)->websiteDataStore().fetchData(WebKit::WebsiteDataType::DOMCache, fetchOptions, [origin, context, callback] (auto dataRecords) {
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

WKStringRef WKWebsiteDataStoreCopyServiceWorkerRegistrationDirectory(WKWebsiteDataStoreRef dataStoreRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(dataStoreRef)->websiteDataStore().serviceWorkerRegistrationDirectory());
}

void WKWebsiteDataStoreSetServiceWorkerRegistrationDirectory(WKWebsiteDataStoreRef dataStoreRef, WKStringRef serviceWorkerRegistrationDirectory)
{
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setServiceWorkerRegistrationDirectory(WebKit::toImpl(serviceWorkerRegistrationDirectory)->string());
}

void WKWebsiteDataStoreSetPerOriginStorageQuota(WKWebsiteDataStoreRef dataStoreRef, uint64_t quota)
{
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setPerOriginStorageQuota(quota);
}

void WKWebsiteDataStoreClearAllDeviceOrientationPermissions(WKWebsiteDataStoreRef dataStoreRef)
{
#if ENABLE(DEVICE_ORIENTATION)
    WebKit::toImpl(dataStoreRef)->websiteDataStore().deviceOrientationAndMotionAccessController().clearPermissions();
#endif
}

void WKWebsiteDataStoreSetWebAuthenticationMockConfiguration(WKWebsiteDataStoreRef dataStoreRef, WKDictionaryRef configurationRef)
{
#if ENABLE(WEB_AUTHN)
    WebKit::MockWebAuthenticationConfiguration configuration;

    if (auto silentFailureRef = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(configurationRef, adoptWK(WKStringCreateWithUTF8CString("SilentFailure")).get())))
        configuration.silentFailure = WKBooleanGetValue(silentFailureRef);

    if (auto localRef = static_cast<WKDictionaryRef>(WKDictionaryGetItemForKey(configurationRef, adoptWK(WKStringCreateWithUTF8CString("Local")).get()))) {
        WebKit::MockWebAuthenticationConfiguration::Local local;
        local.acceptAuthentication = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(localRef, adoptWK(WKStringCreateWithUTF8CString("AcceptAuthentication")).get())));
        local.acceptAttestation = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(localRef, adoptWK(WKStringCreateWithUTF8CString("AcceptAttestation")).get())));
        if (local.acceptAttestation) {
            local.privateKeyBase64 = WebKit::toImpl(static_cast<WKStringRef>(WKDictionaryGetItemForKey(localRef, adoptWK(WKStringCreateWithUTF8CString("PrivateKeyBase64")).get())))->string();
            local.userCertificateBase64 = WebKit::toImpl(static_cast<WKStringRef>(WKDictionaryGetItemForKey(localRef, adoptWK(WKStringCreateWithUTF8CString("UserCertificateBase64")).get())))->string();
            local.intermediateCACertificateBase64 = WebKit::toImpl(static_cast<WKStringRef>(WKDictionaryGetItemForKey(localRef, adoptWK(WKStringCreateWithUTF8CString("IntermediateCACertificateBase64")).get())))->string();
        }
        configuration.local = WTFMove(local);
    }

    if (auto hidRef = static_cast<WKDictionaryRef>(WKDictionaryGetItemForKey(configurationRef, adoptWK(WKStringCreateWithUTF8CString("Hid")).get()))) {
        WebKit::MockWebAuthenticationConfiguration::Hid hid;

        auto stage = WebKit::toImpl(static_cast<WKStringRef>(WKDictionaryGetItemForKey(hidRef, adoptWK(WKStringCreateWithUTF8CString("Stage")).get())))->string();
        if (stage == "info")
            hid.stage = WebKit::MockWebAuthenticationConfiguration::Hid::Stage::Info;
        else if (stage == "request")
            hid.stage = WebKit::MockWebAuthenticationConfiguration::Hid::Stage::Request;

        auto subStage = WebKit::toImpl(static_cast<WKStringRef>(WKDictionaryGetItemForKey(hidRef, adoptWK(WKStringCreateWithUTF8CString("SubStage")).get())))->string();
        if (subStage == "init")
            hid.subStage = WebKit::MockWebAuthenticationConfiguration::Hid::SubStage::Init;
        else if (subStage == "msg")
            hid.subStage = WebKit::MockWebAuthenticationConfiguration::Hid::SubStage::Msg;

        auto error = WebKit::toImpl(static_cast<WKStringRef>(WKDictionaryGetItemForKey(hidRef, adoptWK(WKStringCreateWithUTF8CString("Error")).get())))->string();
        if (error == "success")
            hid.error = WebKit::MockWebAuthenticationConfiguration::Hid::Error::Success;
        else if (error == "data-not-sent")
            hid.error = WebKit::MockWebAuthenticationConfiguration::Hid::Error::DataNotSent;
        else if (error == "empty-report")
            hid.error = WebKit::MockWebAuthenticationConfiguration::Hid::Error::EmptyReport;
        else if (error == "wrong-channel-id")
            hid.error = WebKit::MockWebAuthenticationConfiguration::Hid::Error::WrongChannelId;
        else if (error == "malicious-payload")
            hid.error = WebKit::MockWebAuthenticationConfiguration::Hid::Error::MaliciousPayload;
        else if (error == "unsupported-options")
            hid.error = WebKit::MockWebAuthenticationConfiguration::Hid::Error::UnsupportedOptions;
        else if (error == "wrong-nonce")
            hid.error = WebKit::MockWebAuthenticationConfiguration::Hid::Error::WrongNonce;

        if (auto payloadBase64 = static_cast<WKArrayRef>(WKDictionaryGetItemForKey(hidRef, adoptWK(WKStringCreateWithUTF8CString("PayloadBase64")).get())))
            hid.payloadBase64 = WebKit::toImpl(payloadBase64)->toStringVector();

        if (auto isU2f = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(hidRef, adoptWK(WKStringCreateWithUTF8CString("IsU2f")).get())))
            hid.isU2f = WKBooleanGetValue(isU2f);

        if (auto keepAlive = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(hidRef, adoptWK(WKStringCreateWithUTF8CString("KeepAlive")).get())))
            hid.keepAlive = WKBooleanGetValue(keepAlive);

        if (auto fastDataArrival = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(hidRef, adoptWK(WKStringCreateWithUTF8CString("FastDataArrival")).get())))
            hid.fastDataArrival = WKBooleanGetValue(fastDataArrival);

        if (auto continueAfterErrorData = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(hidRef, adoptWK(WKStringCreateWithUTF8CString("ContinueAfterErrorData")).get())))
            hid.continueAfterErrorData = WKBooleanGetValue(continueAfterErrorData);

        if (auto canDowngrade = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(hidRef, adoptWK(WKStringCreateWithUTF8CString("CanDowngrade")).get())))
            hid.canDowngrade = WKBooleanGetValue(canDowngrade);

        configuration.hid = WTFMove(hid);
    }

    if (auto nfcRef = static_cast<WKDictionaryRef>(WKDictionaryGetItemForKey(configurationRef, adoptWK(WKStringCreateWithUTF8CString("Nfc")).get()))) {
        WebKit::MockWebAuthenticationConfiguration::Nfc nfc;

        auto error = WebKit::toImpl(static_cast<WKStringRef>(WKDictionaryGetItemForKey(nfcRef, adoptWK(WKStringCreateWithUTF8CString("Error")).get())))->string();
        if (error == "success")
            nfc.error = WebKit::MockWebAuthenticationConfiguration::Nfc::Error::Success;
        else if (error == "no-tags")
            nfc.error = WebKit::MockWebAuthenticationConfiguration::Nfc::Error::NoTags;
        else if (error == "wrong-tag-type")
            nfc.error = WebKit::MockWebAuthenticationConfiguration::Nfc::Error::WrongTagType;
        else if (error == "no-connections")
            nfc.error = WebKit::MockWebAuthenticationConfiguration::Nfc::Error::NoConnections;
        else if (error == "malicious-payload")
            nfc.error = WebKit::MockWebAuthenticationConfiguration::Nfc::Error::MaliciousPayload;

        if (auto payloadBase64 = static_cast<WKArrayRef>(WKDictionaryGetItemForKey(nfcRef, adoptWK(WKStringCreateWithUTF8CString("PayloadBase64")).get())))
            nfc.payloadBase64 = WebKit::toImpl(payloadBase64)->toStringVector();

        if (auto multipleTags = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(nfcRef, adoptWK(WKStringCreateWithUTF8CString("MultipleTags")).get())))
            nfc.multipleTags = WKBooleanGetValue(multipleTags);

        configuration.nfc = WTFMove(nfc);
    }

    WebKit::toImpl(dataStoreRef)->websiteDataStore().setMockWebAuthenticationConfiguration(WTFMove(configuration));
#endif
}

void WKWebsiteDataStoreClearAdClickAttributionsThroughWebsiteDataRemoval(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreClearAdClickAttributionsThroughWebsiteDataRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::AdClickAttributions;
    WebKit::toImpl(dataStoreRef)->websiteDataStore().removeData(dataTypes, WallTime::fromRawSeconds(0), [context, callback] {
        callback(context);
    });
}

