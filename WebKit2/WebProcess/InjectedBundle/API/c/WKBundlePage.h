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

#include <JavaScriptCore/JavaScriptCore.h>
#include <WebKit2/WKBase.h>
#include <WebKit2/WKBundleBase.h>

#ifdef __cplusplus
extern "C" {
#endif

// Client
typedef void (*WKBundleDidClearWindowObjectForFrameCallback)(WKBundlePageRef page, WKBundleFrameRef frame, JSContextRef ctx, JSObjectRef window, const void *clientInfo);

struct WKBundlePageClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    WKBundleDidClearWindowObjectForFrameCallback                        didClearWindowObjectForFrame;
};
typedef struct WKBundlePageClient WKBundlePageClient;

WK_EXPORT void WKBundlePageSetClient(WKBundlePageRef page, WKBundlePageClient * client);

WK_EXPORT WKURLRef WKBundlePageGetMainFrameURL(WKBundlePageRef page);

#ifdef __cplusplus
}
#endif

#endif /* WKBundlePage_h */
