/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKCacheModelDocumentViewer = 0,
    kWKCacheModelDocumentBrowser = 1,
    kWKCacheModelPrimaryWebBrowser = 2
};
typedef uint32_t WKCacheModel;

// Injected Bundle Client
typedef void (*WKContextDidReceiveMessageFromInjectedBundleCallback)(WKContextRef page, WKStringRef messageName, WKTypeRef messageBody, const void *clientInfo);
typedef void (*WKContextDidReceiveSynchronousMessageFromInjectedBundleCallback)(WKContextRef page, WKStringRef messageName, WKTypeRef messageBody, WKTypeRef* returnData, const void *clientInfo);

struct WKContextInjectedBundleClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKContextDidReceiveMessageFromInjectedBundleCallback                didReceiveMessageFromInjectedBundle;
    WKContextDidReceiveSynchronousMessageFromInjectedBundleCallback     didReceiveSynchronousMessageFromInjectedBundle;
};
typedef struct WKContextInjectedBundleClient WKContextInjectedBundleClient;

// History Client
typedef void (*WKContextDidNavigateWithNavigationDataCallback)(WKContextRef context, WKPageRef page, WKNavigationDataRef navigationData, WKFrameRef frame, const void *clientInfo);
typedef void (*WKContextDidPerformClientRedirectCallback)(WKContextRef context, WKPageRef page, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef frame, const void *clientInfo);
typedef void (*WKContextDidPerformServerRedirectCallback)(WKContextRef context, WKPageRef page, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef frame, const void *clientInfo);
typedef void (*WKContextDidUpdateHistoryTitleCallback)(WKContextRef context, WKPageRef page, WKStringRef title, WKURLRef URL, WKFrameRef frame, const void *clientInfo);
typedef void (*WKContextPopulateVisitedLinksCallback)(WKContextRef context, const void *clientInfo);

struct WKContextHistoryClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKContextDidNavigateWithNavigationDataCallback                      didNavigateWithNavigationData;
    WKContextDidPerformClientRedirectCallback                           didPerformClientRedirect;
    WKContextDidPerformServerRedirectCallback                           didPerformServerRedirect;
    WKContextDidUpdateHistoryTitleCallback                              didUpdateHistoryTitle;
    WKContextPopulateVisitedLinksCallback                               populateVisitedLinks;
};
typedef struct WKContextHistoryClient WKContextHistoryClient;

WK_EXPORT WKTypeID WKContextGetTypeID();

WK_EXPORT WKContextRef WKContextCreate();
WK_EXPORT WKContextRef WKContextCreateWithInjectedBundlePath(WKStringRef path);
WK_EXPORT WKContextRef WKContextGetSharedProcessContext();

WK_EXPORT void WKContextSetPreferences(WKContextRef context, WKPreferencesRef preferences);
WK_EXPORT WKPreferencesRef WKContextGetPreferences(WKContextRef context);

WK_EXPORT void WKContextSetHistoryClient(WKContextRef context, const WKContextHistoryClient* client);
WK_EXPORT void WKContextSetInjectedBundleClient(WKContextRef context, const WKContextInjectedBundleClient* client);

WK_EXPORT void WKContextPostMessageToInjectedBundle(WKContextRef context, WKStringRef messageName, WKTypeRef messageBody);

WK_EXPORT void WKContextAddVisitedLink(WKContextRef context, WKStringRef visitedURL);

WK_EXPORT void WKContextSetCacheModel(WKContextRef context, WKCacheModel cacheModel);
WK_EXPORT WKCacheModel WKContextGetCacheModel(WKContextRef context);

#ifdef __cplusplus
}
#endif

#endif /* WKContext_h */
