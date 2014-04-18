/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef WKContext_h
#define WKContext_h

#include <WebKit2/WKBase.h>
#include <WebKit2/WKContextConnectionClient.h>
#include <WebKit2/WKContextDownloadClient.h>
#include <WebKit2/WKContextHistoryClient.h>
#include <WebKit2/WKContextInjectedBundleClient.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKCacheModelDocumentViewer = 0,
    kWKCacheModelDocumentBrowser = 1,
    kWKCacheModelPrimaryWebBrowser = 2
};
typedef uint32_t WKCacheModel;

// Context Client
typedef void (*WKContextPlugInAutoStartOriginHashesChangedCallback)(WKContextRef context, const void *clientInfo);
typedef void (*WKContextNetworkProcessDidCrashCallback)(WKContextRef context, const void *clientInfo);
typedef void (*WKContextPlugInInformationBecameAvailableCallback)(WKContextRef context, WKArrayRef plugIn, const void *clientInfo);
typedef WKDataRef (*WKContextCopyWebCryptoMasterKeyCallback)(WKContextRef context, const void *clientInfo);

typedef struct WKContextClientBase {
    int                                                                 version;
    const void *                                                        clientInfo;
} WKContextClientBase;

typedef struct WKContextClientV0 {
    WKContextClientBase                                                 base;

    // Version 0.
    WKContextPlugInAutoStartOriginHashesChangedCallback                 plugInAutoStartOriginHashesChanged;
    WKContextNetworkProcessDidCrashCallback                             networkProcessDidCrash;
    WKContextPlugInInformationBecameAvailableCallback                   plugInInformationBecameAvailable;
} WKContextClientV0;

typedef struct WKContextClientV1 {
    WKContextClientBase                                                 base;

    // Version 0.
    WKContextPlugInAutoStartOriginHashesChangedCallback                 plugInAutoStartOriginHashesChanged;
    WKContextNetworkProcessDidCrashCallback                             networkProcessDidCrash;
    WKContextPlugInInformationBecameAvailableCallback                   plugInInformationBecameAvailable;

    // Version 1.
    WKContextCopyWebCryptoMasterKeyCallback                             copyWebCryptoMasterKey;
} WKContextClientV1;

enum { kWKContextClientCurrentVersion WK_ENUM_DEPRECATED("Use an explicit version number instead") = 1 };
typedef struct WKContextClient {
    int                                                                 version;
    const void *                                                        clientInfo;

    // Version 0.
    WKContextPlugInAutoStartOriginHashesChangedCallback                 plugInAutoStartOriginHashesChanged;
    WKContextNetworkProcessDidCrashCallback                             networkProcessDidCrash;
    WKContextPlugInInformationBecameAvailableCallback                   plugInInformationBecameAvailable;
} WKContextClient WK_DEPRECATED("Use an explicit versioned struct instead");

enum {
    kWKProcessModelSharedSecondaryProcess = 0,
    kWKProcessModelMultipleSecondaryProcesses = 1
};
typedef uint32_t WKProcessModel;

enum {
    kWKStatisticsOptionsWebContent = 1 << 0,
    kWKStatisticsOptionsNetworking = 1 << 1
};
typedef uint32_t WKStatisticsOptions;

WK_EXPORT WKTypeID WKContextGetTypeID();

WK_EXPORT WKContextRef WKContextCreate();
WK_EXPORT WKContextRef WKContextCreateWithInjectedBundlePath(WKStringRef path);

WK_EXPORT void WKContextSetClient(WKContextRef context, const WKContextClientBase* client);
WK_EXPORT void WKContextSetInjectedBundleClient(WKContextRef context, const WKContextInjectedBundleClientBase* client);
WK_EXPORT void WKContextSetHistoryClient(WKContextRef context, const WKContextHistoryClientBase* client);
WK_EXPORT void WKContextSetDownloadClient(WKContextRef context, const WKContextDownloadClientBase* client);
WK_EXPORT void WKContextSetConnectionClient(WKContextRef context, const WKContextConnectionClientBase* client);

WK_EXPORT WKDownloadRef WKContextDownloadURLRequest(WKContextRef context, const WKURLRequestRef request);

WK_EXPORT void WKContextSetInitializationUserDataForInjectedBundle(WKContextRef context, WKTypeRef userData);
WK_EXPORT void WKContextPostMessageToInjectedBundle(WKContextRef context, WKStringRef messageName, WKTypeRef messageBody);

WK_EXPORT void WKContextAddVisitedLink(WKContextRef context, WKStringRef visitedURL);

WK_EXPORT void WKContextSetCacheModel(WKContextRef context, WKCacheModel cacheModel);
WK_EXPORT WKCacheModel WKContextGetCacheModel(WKContextRef context);

WK_EXPORT void WKContextSetProcessModel(WKContextRef context, WKProcessModel processModel);
WK_EXPORT WKProcessModel WKContextGetProcessModel(WKContextRef context);

WK_EXPORT void WKContextSetMaximumNumberOfProcesses(WKContextRef context, unsigned numberOfProcesses);
WK_EXPORT unsigned WKContextGetMaximumNumberOfProcesses(WKContextRef context);

WK_EXPORT void WKContextStartMemorySampler(WKContextRef context, WKDoubleRef interval);
WK_EXPORT void WKContextStopMemorySampler(WKContextRef context);

WK_EXPORT WKApplicationCacheManagerRef WKContextGetApplicationCacheManager(WKContextRef context);
WK_EXPORT WKBatteryManagerRef WKContextGetBatteryManager(WKContextRef context);
WK_EXPORT WKCookieManagerRef WKContextGetCookieManager(WKContextRef context);
WK_EXPORT WKDatabaseManagerRef WKContextGetDatabaseManager(WKContextRef context);
WK_EXPORT WKGeolocationManagerRef WKContextGetGeolocationManager(WKContextRef context);
WK_EXPORT WKIconDatabaseRef WKContextGetIconDatabase(WKContextRef context);
WK_EXPORT WKKeyValueStorageManagerRef WKContextGetKeyValueStorageManager(WKContextRef context);
WK_EXPORT WKMediaCacheManagerRef WKContextGetMediaCacheManager(WKContextRef context);
WK_EXPORT WKNotificationManagerRef WKContextGetNotificationManager(WKContextRef context);
WK_EXPORT WKPluginSiteDataManagerRef WKContextGetPluginSiteDataManager(WKContextRef context);
WK_EXPORT WKResourceCacheManagerRef WKContextGetResourceCacheManager(WKContextRef context);
WK_EXPORT WKOriginDataManagerRef WKContextGetOriginDataManager(WKContextRef context);

typedef void (*WKContextGetStatisticsFunction)(WKDictionaryRef statistics, WKErrorRef error, void* functionContext);
WK_EXPORT void WKContextGetStatistics(WKContextRef context, void* functionContext, WKContextGetStatisticsFunction function);
WK_EXPORT void WKContextGetStatisticsWithOptions(WKContextRef context, WKStatisticsOptions statisticsMask, void* functionContext, WKContextGetStatisticsFunction function);

WK_EXPORT void WKContextGarbageCollectJavaScriptObjects(WKContextRef context);
WK_EXPORT void WKContextSetJavaScriptGarbageCollectorTimerEnabled(WKContextRef context, bool enable);

WK_EXPORT WKDictionaryRef WKContextCopyPlugInAutoStartOriginHashes(WKContextRef context);
WK_EXPORT void WKContextSetPlugInAutoStartOriginHashes(WKContextRef context, WKDictionaryRef dictionary);
WK_EXPORT void WKContextSetPlugInAutoStartOrigins(WKContextRef contextRef, WKArrayRef arrayRef);
WK_EXPORT void WKContextSetPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(WKContextRef contextRef, WKDictionaryRef dictionaryRef, double time);

#ifdef __cplusplus
}
#endif

#endif /* WKContext_h */
