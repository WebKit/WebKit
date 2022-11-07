/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <WebKit/WKString.h>

namespace TestWebKitAPI {

static bool finished = false;
static int didRemoveFrameFromHierarchyCount;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    finished = true;
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageNavigationClientV1 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 1;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(page, &loaderClient.base);
}

static void didReceivePageMessageFromInjectedBundle(WKPageRef, WKStringRef messageName, WKTypeRef, const void*)
{
    if (WKStringIsEqualToUTF8CString(messageName, "DidRemoveFrameFromHierarchy"))
        ++didRemoveFrameFromHierarchyCount;
}

static void setInjectedBundleClient(WKPageRef page)
{
    WKPageInjectedBundleClientV0 injectedBundleClient = {
        { 0, nullptr },
        didReceivePageMessageFromInjectedBundle,
        nullptr,
    };
    WKPageSetPageInjectedBundleClient(page, &injectedBundleClient.base);
}

TEST(WebKit, DidRemoveFrameFromHiearchyInBackForwardCache)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("DidRemoveFrameFromHiearchyInBackForwardCache"));
    // Enable the page cache so we can test the WKBundlePageDidRemoveFrameFromHierarchyCallback API
    WKContextSetCacheModel(context.get(), kWKCacheModelPrimaryWebBrowser);

    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());
    setInjectedBundleClient(webView.page());

    finished = false;
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("many-iframes", "html")).get());
    Util::run(&finished);

    // Perform a couple of loads so "many-iframes" gets kicked out of the PageCache.
    finished = false;
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple", "html")).get());
    Util::run(&finished);

    finished = false;
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple2", "html")).get());
    Util::run(&finished);

    finished = false;
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple3", "html")).get());
    Util::run(&finished);

    EXPECT_EQ(didRemoveFrameFromHierarchyCount, 10);
}

} // namespace TestWebKitAPI

#endif
