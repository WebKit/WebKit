/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WKAPICast.h"
#include "WKDictionary.h"
#include "WKNumber.h"
#include "WKRetainPtr.h"
#include "WKSecurityOriginRef.h"
#include "WKString.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebResourceLoadStatisticsTelemetry.h"
#include "WebsiteData.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataRecord.h"
#include "WebsiteDataType.h"
#include <WebCore/URL.h>
#include <wtf/CallbackAggregator.h>

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
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        completionHandler(context);
        return;
    }
    
    store->setResourceLoadStatisticsDebugMode(enable, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreStatisticsDebugModeFunction completionHandler)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        completionHandler(context);
        return;
    }
    
    store->setPrevalentResourceForDebugMode(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
        completionHandler(context);
    });
}
void WKWebsiteDataStoreSetStatisticsLastSeen(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, double seconds, void* context, WKWebsiteDataStoreStatisticsLastSeenFunction completionHandler)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        completionHandler(context);
        return;
    }

    store->setLastSeen(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), Seconds { seconds }, [context, completionHandler] {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreSetStatisticsPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsPrevalentResourceFunction completionHandler)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        completionHandler(context);
        return;
    }

    if (value)
        store->setPrevalentResource(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
    else
        store->clearPrevalentResource(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
}

void WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsVeryPrevalentResourceFunction completionHandler)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        completionHandler(context);
        return;
    }

    if (value)
        store->setVeryPrevalentResource(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
    else
        store->clearPrevalentResource(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
}

void WKWebsiteDataStoreDumpResourceLoadStatistics(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreDumpResourceLoadStatisticsFunction callback)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        callback(WebKit::toAPI(emptyString().impl()), context);
        return;
    }

    store->dumpResourceLoadStatistics([context, callback] (const String& resourceLoadStatistics) {
        callback(WebKit::toAPI(resourceLoadStatistics.impl()), context);
    });
}

void WKWebsiteDataStoreIsStatisticsPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsPrevalentResourceFunction callback)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        callback(false, context);
        return;
    }

    store->isPrevalentResource(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, callback](bool isPrevalentResource) {
        callback(isPrevalentResource, context);
    });
}

void WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsPrevalentResourceFunction callback)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        callback(false, context);
        return;
    }
    
    store->isVeryPrevalentResource(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, callback](bool isVeryPrevalentResource) {
        callback(isVeryPrevalentResource, context);
    });
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(WKWebsiteDataStoreRef dataStoreRef, WKStringRef subresourceHost, WKStringRef topFrameHost, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnderFunction callback)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        callback(false, context);
        return;
    }
    
    store->isRegisteredAsSubresourceUnder(WebCore::URL(WebCore::URL(), WebKit::toImpl(subresourceHost)->string()), WebCore::URL(WebCore::URL(), WebKit::toImpl(topFrameHost)->string()), [context, callback](bool isRegisteredAsSubresourceUnder) {
        callback(isRegisteredAsSubresourceUnder, context);
    });
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(WKWebsiteDataStoreRef dataStoreRef, WKStringRef subFrameHost, WKStringRef topFrameHost, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnderFunction callback)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        callback(false, context);
        return;
    }

    store->isRegisteredAsSubFrameUnder(WebCore::URL(WebCore::URL(), WebKit::toImpl(subFrameHost)->string()), WebCore::URL(WebCore::URL(), WebKit::toImpl(topFrameHost)->string()), [context, callback](bool isRegisteredAsSubFrameUnder) {
        callback(isRegisteredAsSubFrameUnder, context);
    });
}

void WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef hostRedirectedFrom, WKStringRef hostRedirectedTo, void* context, WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingToFunction callback)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        callback(false, context);
        return;
    }

    store->isRegisteredAsRedirectingTo(WebCore::URL(WebCore::URL(), WebKit::toImpl(hostRedirectedFrom)->string()), WebCore::URL(WebCore::URL(), WebKit::toImpl(hostRedirectedTo)->string()), [context, callback](bool isRegisteredAsRedirectingTo) {
        callback(isRegisteredAsRedirectingTo, context);
    });
}

void WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value, void* context, WKWebsiteDataStoreStatisticsHasHadUserInteractionFunction completionHandler)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        completionHandler(context);
        return;
    }

    if (value)
        store->logUserInteraction(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
    else
        store->clearUserInteraction(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, completionHandler] {
            completionHandler(context);
        });
}

void WKWebsiteDataStoreIsStatisticsHasHadUserInteraction(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsHasHadUserInteractionFunction callback)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        callback(false, context);
        return;
    }

    store->hasHadUserInteraction(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, callback](bool hasHadUserInteraction) {
        callback(hasHadUserInteraction, context);
    });
}

void WKWebsiteDataStoreSetStatisticsGrandfathered(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, bool value)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setGrandfathered(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), value);
}

void WKWebsiteDataStoreIsStatisticsGrandfathered(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, void* context, WKWebsiteDataStoreIsStatisticsGrandfatheredFunction callback)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        callback(false, context);
        return;
    }

    store->hasHadUserInteraction(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), [context, callback](bool isGrandfathered) {
        callback(isGrandfathered, context);
    });
}

void WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef topFrameHost)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setSubframeUnderTopFrameOrigin(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), WebCore::URL(WebCore::URL(), WebKit::toImpl(topFrameHost)->string()));
}

void WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef topFrameHost)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setSubresourceUnderTopFrameOrigin(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), WebCore::URL(WebCore::URL(), WebKit::toImpl(topFrameHost)->string()));
}

void WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedTo)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setSubresourceUniqueRedirectTo(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), WebCore::URL(WebCore::URL(), WebKit::toImpl(hostRedirectedTo)->string()));
}

void WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedFrom)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;
    
    store->setSubresourceUniqueRedirectFrom(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), WebCore::URL(WebCore::URL(), WebKit::toImpl(hostRedirectedFrom)->string()));
}

void WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedTo)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;
    
    store->setTopFrameUniqueRedirectTo(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), WebCore::URL(WebCore::URL(), WebKit::toImpl(hostRedirectedTo)->string()));
}

void WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(WKWebsiteDataStoreRef dataStoreRef, WKStringRef host, WKStringRef hostRedirectedFrom)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;
    
    store->setTopFrameUniqueRedirectFrom(WebCore::URL(WebCore::URL(), WebKit::toImpl(host)->string()), WebCore::URL(WebCore::URL(), WebKit::toImpl(hostRedirectedFrom)->string()));
}

void WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(WKWebsiteDataStoreRef dataStoreRef, double seconds)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setTimeToLiveUserInteraction(Seconds { seconds });
}

void WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(WKWebsiteDataStoreRef dataStoreRef)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->scheduleStatisticsAndDataRecordsProcessing();
}

void WKWebsiteDataStoreStatisticsUpdateCookieBlocking(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsUpdateCookieBlockingFunction completionHandler)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        completionHandler(context);
        return;
    }

    store->scheduleCookieBlockingUpdate([context, completionHandler]() {
        completionHandler(context);
    });
}

void WKWebsiteDataStoreStatisticsSubmitTelemetry(WKWebsiteDataStoreRef dataStoreRef)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->submitTelemetry();
}

void WKWebsiteDataStoreSetStatisticsNotifyPagesWhenDataRecordsWereScanned(WKWebsiteDataStoreRef dataStoreRef, bool value)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setNotifyPagesWhenDataRecordsWereScanned(value);
}

void WKWebsiteDataStoreSetStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(WKWebsiteDataStoreRef dataStoreRef, bool value)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setShouldClassifyResourcesBeforeDataRecordsRemoval(value);
}

void WKWebsiteDataStoreSetStatisticsNotifyPagesWhenTelemetryWasCaptured(WKWebsiteDataStoreRef, bool value)
{
    WebKit::WebResourceLoadStatisticsTelemetry::setNotifyPagesWhenTelemetryWasCaptured(value);
}

void WKWebsiteDataStoreSetStatisticsMinimumTimeBetweenDataRecordsRemoval(WKWebsiteDataStoreRef dataStoreRef, double seconds)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setMinimumTimeBetweenDataRecordsRemoval(Seconds { seconds });
}

void WKWebsiteDataStoreSetStatisticsGrandfatheringTime(WKWebsiteDataStoreRef dataStoreRef, double seconds)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setGrandfatheringTime(Seconds {seconds });
}

void WKWebsiteDataStoreSetStatisticsMaxStatisticsEntries(WKWebsiteDataStoreRef dataStoreRef, unsigned entries)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setMaxStatisticsEntries(entries);
}

void WKWebsiteDataStoreSetStatisticsPruneEntriesDownTo(WKWebsiteDataStoreRef dataStoreRef, unsigned entries)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store)
        return;

    store->setPruneEntriesDownTo(entries);
}

void WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreFunction callback)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        callback(context);
        return;
    }

    store->scheduleClearInMemoryAndPersistent(WebKit::WebResourceLoadStatisticsStore::ShouldGrandfather::Yes, [context, callback]() {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(WKWebsiteDataStoreRef dataStoreRef, unsigned hours, void* context, WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHoursFunction callback)
{
    auto* store = WebKit::toImpl(dataStoreRef)->websiteDataStore().resourceLoadStatistics();
    if (!store) {
        callback(context);
        return;
    }

    store->scheduleClearInMemoryAndPersistent(WallTime::now() - Seconds::fromHours(hours), WebKit::WebResourceLoadStatisticsStore::ShouldGrandfather::Yes, [context, callback]() {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemovalFunction callback)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::ResourceLoadStatistics;
    WebKit::toImpl(dataStoreRef)->websiteDataStore().removeData(dataTypes, WallTime::fromRawSeconds(0), [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreSetStatisticsCacheMaxAgeCap(WKWebsiteDataStoreRef dataStoreRef, double seconds, void* context, WKWebsiteDataStoreSetStatisticsCacheMaxAgeCapFunction callback)
{
    WebKit::toImpl(dataStoreRef)->websiteDataStore().setCacheMaxAgeCapForPrevalentResources(Seconds { seconds }, [context, callback] {
        callback(context);
    });
}

void WKWebsiteDataStoreStatisticsResetToConsistentState(WKWebsiteDataStoreRef dataStoreRef, void* context, WKWebsiteDataStoreStatisticsResetToConsistentStateFunction completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create([context, completionHandler]() {
        completionHandler(context);
    });

    auto& store = WebKit::toImpl(dataStoreRef)->websiteDataStore();
    store.clearResourceLoadStatisticsInWebProcesses([callbackAggregator = callbackAggregator.copyRef()] { });
    store.resetCacheMaxAgeCapForPrevalentResources([callbackAggregator = callbackAggregator.copyRef()] { });

    auto* statisticsStore = store.resourceLoadStatistics();
    if (!statisticsStore)
        return;

    statisticsStore->resetParametersToDefaultValues([callbackAggregator = callbackAggregator.copyRef()] { });
    statisticsStore->scheduleClearInMemoryAndPersistent(WebKit::WebResourceLoadStatisticsStore::ShouldGrandfather::No, [callbackAggregator = callbackAggregator.copyRef()] { });
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

void WKWebsiteDataStoreRemoveAllIndexedDatabases(WKWebsiteDataStoreRef dataStoreRef)
{
    OptionSet<WebKit::WebsiteDataType> dataTypes = WebKit::WebsiteDataType::IndexedDBDatabases;
    WebKit::toImpl(dataStoreRef)->websiteDataStore().removeData(dataTypes, -WallTime::infinity(), [] { });
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


void WKWebsiteDataStoreSetWebAuthenticationMockConfiguration(WKWebsiteDataStoreRef dataStoreRef, WKDictionaryRef configurationRef)
{
#if ENABLE(WEB_AUTHN)
    MockWebAuthenticationConfiguration configuration;

    auto localRef = static_cast<WKDictionaryRef>(WKDictionaryGetItemForKey(configurationRef, adoptWK(WKStringCreateWithUTF8CString("Local")).get()));
    if (localRef) {
        MockWebAuthenticationConfiguration::Local local;
        local.acceptAuthentication = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(localRef, adoptWK(WKStringCreateWithUTF8CString("AcceptAuthentication")).get())));
        local.acceptAttestation = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(localRef, adoptWK(WKStringCreateWithUTF8CString("AcceptAttestation")).get())));
        local.privateKeyBase64 = WebKit::toImpl(static_cast<WKStringRef>(WKDictionaryGetItemForKey(localRef, adoptWK(WKStringCreateWithUTF8CString("PrivateKeyBase64")).get())))->string();
        configuration.local = WTFMove(local);
    }

    WebKit::toImpl(dataStoreRef)->websiteDataStore().setMockWebAuthenticationConfiguration(WTFMove(configuration));
#endif
}
