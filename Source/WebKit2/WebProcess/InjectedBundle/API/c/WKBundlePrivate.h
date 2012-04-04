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

#ifndef WKBundlePrivate_h
#define WKBundlePrivate_h

#include <WebKit2/WKBase.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

WK_EXPORT void WKBundleSetShouldTrackVisitedLinks(WKBundleRef bundle, bool shouldTrackVisitedLinks);
WK_EXPORT void WKBundleRemoveAllVisitedLinks(WKBundleRef bundle);
WK_EXPORT void WKBundleActivateMacFontAscentHack(WKBundleRef bundle);
WK_EXPORT void WKBundleGarbageCollectJavaScriptObjects(WKBundleRef bundle);
WK_EXPORT void WKBundleGarbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(WKBundleRef bundle, bool waitUntilDone);
WK_EXPORT size_t WKBundleGetJavaScriptObjectsCount(WKBundleRef bundle);

enum WKUserScriptInjectionTime {
    kWKInjectAtDocumentStart,
    kWKInjectAtDocumentEnd
};
typedef enum WKUserScriptInjectionTime WKUserScriptInjectionTime;

enum WKUserContentInjectedFrames {
    kWKInjectInAllFrames,
    kWKInjectInTopFrameOnly
};
typedef enum WKUserContentInjectedFrames WKUserContentInjectedFrames;

WK_EXPORT void WKBundleAddUserScript(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, WKBundleScriptWorldRef scriptWorld, WKStringRef source, WKURLRef url, WKArrayRef whitelist, WKArrayRef blacklist, WKUserScriptInjectionTime injectionTime, WKUserContentInjectedFrames injectedFrames);
WK_EXPORT void WKBundleAddUserStyleSheet(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, WKBundleScriptWorldRef scriptWorld, WKStringRef source, WKURLRef url, WKArrayRef whitelist, WKArrayRef blacklist, WKUserContentInjectedFrames injectedFrames);
WK_EXPORT void WKBundleRemoveUserScript(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, WKBundleScriptWorldRef scriptWorld, WKURLRef url);
WK_EXPORT void WKBundleRemoveUserStyleSheet(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, WKBundleScriptWorldRef scriptWorld, WKURLRef url);
WK_EXPORT void WKBundleRemoveUserScripts(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, WKBundleScriptWorldRef scriptWorld);
WK_EXPORT void WKBundleRemoveUserStyleSheets(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, WKBundleScriptWorldRef scriptWorld);
WK_EXPORT void WKBundleRemoveAllUserContent(WKBundleRef bundle, WKBundlePageGroupRef pageGroup);

// Will make WebProcess ignore this preference until a preferences change notification, only for WebKitTestRunner use.
WK_EXPORT void WKBundleOverrideBoolPreferenceForTestRunner(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, WKStringRef preference, bool enabled);

WK_EXPORT void WKBundleSetAllowUniversalAccessFromFileURLs(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, bool enabled);
WK_EXPORT void WKBundleSetAllowFileAccessFromFileURLs(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, bool enabled);
WK_EXPORT void WKBundleSetFrameFlatteningEnabled(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, bool enabled);
WK_EXPORT void WKBundleSetGeolocationPermission(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, bool enabled);
WK_EXPORT void WKBundleSetJavaScriptCanAccessClipboard(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, bool enabled);
WK_EXPORT void WKBundleSetPrivateBrowsingEnabled(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, bool enabled);
WK_EXPORT void WKBundleSetPopupBlockingEnabled(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, bool enabled);
WK_EXPORT void WKBundleSetAuthorAndUserStylesEnabled(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, bool enabled);
WK_EXPORT void WKBundleClearAllDatabases(WKBundleRef bundle);
WK_EXPORT void WKBundleSetDatabaseQuota(WKBundleRef bundle, uint64_t);
WK_EXPORT void WKBundleClearApplicationCache(WKBundleRef bundle);
WK_EXPORT void WKBundleSetAppCacheMaximumSize(WKBundleRef bundle, uint64_t size);
WK_EXPORT int WKBundleNumberOfPages(WKBundleRef bundle, WKBundleFrameRef frameRef, double pageWidthInPixels, double pageHeightInPixels);
WK_EXPORT int WKBundlePageNumberForElementById(WKBundleRef bundle, WKBundleFrameRef frameRef, WKStringRef idRef, double pageWidthInPixels, double pageHeightInPixels);
WK_EXPORT WKStringRef WKBundlePageSizeAndMarginsInPixels(WKBundleRef bundle, WKBundleFrameRef frameRef, int, int, int, int, int, int, int);
WK_EXPORT bool WKBundleIsPageBoxVisible(WKBundleRef bundle, WKBundleFrameRef frameRef, int);
WK_EXPORT void WKBundleAddOriginAccessWhitelistEntry(WKBundleRef bundle, WKStringRef, WKStringRef, WKStringRef, bool);
WK_EXPORT void WKBundleRemoveOriginAccessWhitelistEntry(WKBundleRef bundle, WKStringRef, WKStringRef, WKStringRef, bool);
WK_EXPORT void WKBundleResetOriginAccessWhitelists(WKBundleRef bundle);

WK_EXPORT bool WKBundleIsProcessingUserGesture(WKBundleRef bundle);

WK_EXPORT void WKBundleSwitchNetworkLoaderToNewTestingSession(WKBundleRef bundle);

WK_EXPORT void WKBundleSetPageVisibilityState(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, int state, bool isInitialState);

#ifdef __cplusplus
}
#endif

#endif /* WKBundlePrivate_h */
