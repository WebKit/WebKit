/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"

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

static WKRetainPtr<WKSessionStateRef> createSessionState(WKContextRef context)
{
    PlatformWebView webView(context);
    setPageLoaderClient(webView.page());

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple", "html")).get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    return adoptWK(static_cast<WKSessionStateRef>(WKPageCopySessionState(webView.page(), reinterpret_cast<void*>(1), nullptr)));
}

TEST(WebKit, WKPageGetScaleFactorNotZero)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));

    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());

    auto sessionState = createSessionState(context.get());
    EXPECT_NOT_NULL(sessionState);

    WKPageRestoreFromSessionState(webView.page(), sessionState.get());
    Util::run(&didFinishLoad);

    EXPECT_TRUE(WKPageGetScaleFactor(webView.page()) == 1.0);
}

} // namespace TestWebKitAPI

#endif
