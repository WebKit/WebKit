/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if WK_HAVE_C_SPI

#include "JavaScriptTest.h"
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKSessionStateRef.h>

namespace TestWebKitAPI {

static bool didFinishLoad;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(page, &loaderClient.base);
}

static WKRetainPtr<WKDataRef> createSessionStateDataContainingFormData(WKContextRef context)
{
    PlatformWebView webView(context);
    setPageLoaderClient(webView.page());

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple-form", "html")).get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    EXPECT_JS_EQ(webView.page(), "submitForm()", "undefined");
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    auto sessionState = adoptWK(static_cast<WKSessionStateRef>(WKPageCopySessionState(webView.page(), reinterpret_cast<void*>(1), nullptr)));
    return adoptWK(WKSessionStateCopyData(sessionState.get()));
}

TEST(WebKit, RestoreSessionStateContainingFormData)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));

    // FIXME: Once <rdar://problem/8708435> is fixed, we can move the creation of this
    // PlatformWebView after the call to createSessionStaetContainingFormData. Until then, it must
    // remain here to avoid a race condition between the UI and web processes.
    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());

    WKRetainPtr<WKDataRef> data = createSessionStateDataContainingFormData(context.get());
    EXPECT_NOT_NULL(data);

    auto sessionState = adoptWK(WKSessionStateCreateFromData(data.get()));
    WKPageRestoreFromSessionState(webView.page(), sessionState.get());

    Util::run(&didFinishLoad);

    EXPECT_TRUE(WKPageCanGoBack(webView.page()));
}

} // namespace TestWebKitAPI

#endif
