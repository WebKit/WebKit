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

#ifndef WKBundlePage_h
#define WKBundlePage_h

#include <JavaScriptCore/JavaScript.h>
#include <WebKit2/WKBase.h>
#include <WebKit2/WKBundleBase.h>

#ifdef __cplusplus
extern "C" {
#endif

// Loader Client
typedef void (*WKBundlePageDidStartProvisionalLoadForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageDidReceiveServerRedirectForProvisionalLoadForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageDidFailProvisionalLoadWithErrorForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo); // FIXME: Add WKErrorRef.
typedef void (*WKBundlePageDidCommitLoadForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageDidFinishLoadForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageDidFailLoadWithErrorForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo); // FIXME: Add WKErrorRef.
typedef void (*WKBundlePageDidReceiveTitleForFrameCallback)(WKBundlePageRef page, WKStringRef title, WKBundleFrameRef frame, const void *clientInfo);
typedef void (*WKBundlePageDidClearWindowObjectForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, JSContextRef ctx, JSObjectRef window, const void *clientInfo);

struct WKBundlePageLoaderClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKBundlePageDidStartProvisionalLoadForFrameCallback                 didStartProvisionalLoadForFrame;
    WKBundlePageDidReceiveServerRedirectForProvisionalLoadForFrameCallback    didReceiveServerRedirectForProvisionalLoadForFrame;
    WKBundlePageDidFailProvisionalLoadWithErrorForFrameCallback         didFailProvisionalLoadWithErrorForFrame;
    WKBundlePageDidCommitLoadForFrameCallback                           didCommitLoadForFrame;
    WKBundlePageDidFinishLoadForFrameCallback                           didFinishLoadForFrame;
    WKBundlePageDidFailLoadWithErrorForFrameCallback                    didFailLoadWithErrorForFrame;
    WKBundlePageDidReceiveTitleForFrameCallback                         didReceiveTitleForFrame;
    WKBundlePageDidClearWindowObjectForFrameCallback                    didClearWindowObjectForFrame;
};
typedef struct WKBundlePageLoaderClient WKBundlePageLoaderClient;

// UI Client
typedef void (*WKBundlePageAddMessageToConsoleCallback)(WKBundlePageRef page, WKStringRef message, uint32_t lineNumber, const void *clientInfo);

struct WKBundlePageUIClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKBundlePageAddMessageToConsoleCallback                             addMessageToConsole;
};
typedef struct WKBundlePageUIClient WKBundlePageUIClient;

WK_EXPORT void WKBundlePageSetLoaderClient(WKBundlePageRef page, WKBundlePageLoaderClient * client);
WK_EXPORT void WKBundlePageSetUIClient(WKBundlePageRef page, WKBundlePageUIClient * client);

WK_EXPORT WKBundleFrameRef WKBundlePageGetMainFrame(WKBundlePageRef page);

#ifdef __cplusplus
}
#endif

#endif /* WKBundlePage_h */
