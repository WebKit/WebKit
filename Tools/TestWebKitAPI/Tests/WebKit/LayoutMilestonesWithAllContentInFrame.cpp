/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

static bool didFirstVisuallyNonEmptyLayout;
static bool didNavigate;

static void renderingProgressDidChange(WKPageRef page, WKPageRenderingProgressEvents milestones, WKTypeRef, const void* clientInfo)
{
    // This test ensures that the DidFirstVisuallyNonEmptyLayout will be reached for the main frame
    // even when all of the content is in a subframe.
    if (milestones & WKPageRenderingProgressEventFirstVisuallyNonEmptyLayout)
        didFirstVisuallyNonEmptyLayout = true;
}

static void didFinishNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef userData, const void* clientInfo)
{
    didNavigate = true;
}

TEST(WebKit, LayoutMilestonesWithAllContentInFrame)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV3 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 3;
    loaderClient.base.clientInfo = &webView;
    loaderClient.renderingProgressDidChange = renderingProgressDidChange;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageListenForLayoutMilestones(webView.page(), WKPageRenderingProgressEventFirstVisuallyNonEmptyLayout);
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("all-content-in-one-iframe", "html")).get());

    Util::run(&didFirstVisuallyNonEmptyLayout);
    EXPECT_TRUE(didFirstVisuallyNonEmptyLayout);
}

TEST(WebKit, FirstVisuallyNonEmptyLayoutAfterPageCacheRestore)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));

    WKContextSetCacheModel(context.get(), kWKCacheModelPrimaryWebBrowser); // Enables the Page Cache.

    PlatformWebView webView(context.get());

    WKPageNavigationClientV3 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 3;
    loaderClient.base.clientInfo = &webView;
    loaderClient.renderingProgressDidChange = renderingProgressDidChange;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageListenForLayoutMilestones(webView.page(), WKPageRenderingProgressEventFirstVisuallyNonEmptyLayout);
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple-tall", "html")).get());

    Util::run(&didFirstVisuallyNonEmptyLayout);
    EXPECT_TRUE(didFirstVisuallyNonEmptyLayout);
    didFirstVisuallyNonEmptyLayout = false;
    Util::run(&didNavigate);
    EXPECT_TRUE(didNavigate);
    didNavigate = false;

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("large-red-square-image", "html")).get());
    Util::run(&didFirstVisuallyNonEmptyLayout);
    EXPECT_TRUE(didFirstVisuallyNonEmptyLayout);
    didFirstVisuallyNonEmptyLayout = false;
    Util::run(&didNavigate);
    EXPECT_TRUE(didNavigate);
    didNavigate = false;

    WKPageGoBack(webView.page());
    Util::run(&didFirstVisuallyNonEmptyLayout);
    EXPECT_TRUE(didFirstVisuallyNonEmptyLayout);
    didFirstVisuallyNonEmptyLayout = false;
    Util::run(&didNavigate);
    EXPECT_TRUE(didNavigate);
    didNavigate = false;
}

TEST(WebKit, FirstVisuallyNonEmptyMilestoneWithLoadComplete)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV3 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 3;
    loaderClient.base.clientInfo = &webView;
    loaderClient.renderingProgressDidChange = renderingProgressDidChange;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);
    didFirstVisuallyNonEmptyLayout = false;

    WKPageListenForLayoutMilestones(webView.page(), WKPageRenderingProgressEventFirstVisuallyNonEmptyLayout);
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("async-script-load", "html")).get());

    Util::run(&didFirstVisuallyNonEmptyLayout);
    EXPECT_TRUE(didFirstVisuallyNonEmptyLayout);
}

} // namespace TestWebKitAPI

#endif
