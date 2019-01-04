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

#include "config.h"

#if WK_HAVE_C_SPI

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

static bool test1Done;

struct State {
    State()
        : didDecidePolicyForNavigationAction(false)
        , didStartProvisionalNavigation(false)
        , didCommitNavigation(false)
    {
    }

    bool didDecidePolicyForNavigationAction;
    bool didStartProvisionalNavigation;
    bool didCommitNavigation;
};

static void didStartProvisionalNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    EXPECT_TRUE(state->didDecidePolicyForNavigationAction);
    EXPECT_FALSE(state->didCommitNavigation);

    // The commited URL should be null.
    EXPECT_NULL(WKFrameCopyURL(WKPageGetMainFrame(page)));

    EXPECT_FALSE(state->didStartProvisionalNavigation);

    state->didStartProvisionalNavigation = true;
}

static void didCommitNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    EXPECT_TRUE(state->didDecidePolicyForNavigationAction);
    EXPECT_TRUE(state->didStartProvisionalNavigation);

    // The provisional URL should be null.
    EXPECT_NULL(WKFrameCopyProvisionalURL(WKPageGetMainFrame(page)));

    state->didCommitNavigation = true;
}

static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    EXPECT_TRUE(state->didDecidePolicyForNavigationAction);
    EXPECT_TRUE(state->didStartProvisionalNavigation);
    EXPECT_TRUE(state->didCommitNavigation);

    // The provisional URL should be null.
    EXPECT_NULL(WKFrameCopyProvisionalURL(WKPageGetMainFrame(page)));

    test1Done = true;
}

static void decidePolicyForNavigationAction(WKPageRef page, WKNavigationActionRef navigationAction, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    EXPECT_FALSE(state->didStartProvisionalNavigation);
    EXPECT_FALSE(state->didCommitNavigation);

    state->didDecidePolicyForNavigationAction = true;

    WKFramePolicyListenerUse(listener);
}

static void decidePolicyForResponse(WKPageRef page, WKNavigationResponseRef navigationResponse, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
{
    WKFramePolicyListenerUse(listener);
}

TEST(WebKit, PageLoadBasic)
{
    State state;

    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.base.clientInfo = &state;
    loaderClient.didStartProvisionalNavigation = didStartProvisionalNavigation;
    loaderClient.didCommitNavigation = didCommitNavigation;
    loaderClient.didFinishNavigation = didFinishNavigation;
    loaderClient.decidePolicyForNavigationAction = decidePolicyForNavigationAction;
    loaderClient.decidePolicyForNavigationResponse = decidePolicyForResponse;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    // Before loading anything, the active url should be null
    EXPECT_NULL(WKPageCopyActiveURL(webView.page()));

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());

    // But immediately after starting a load, the active url should reflect the request
    WKRetainPtr<WKURLRef> activeUrl = adoptWK(WKPageCopyActiveURL(webView.page()));
    ASSERT_NOT_NULL(activeUrl.get());
    EXPECT_TRUE(WKURLIsEqual(activeUrl.get(), url.get()));

    Util::run(&test1Done);
}

TEST(WebKit, PageReload)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    // Reload test before url loading.
    WKPageReload(webView.page());
    WKPageReload(webView.page());

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());

    // Reload test after url loading.
    WKPageReload(webView.page());

    WKRetainPtr<WKURLRef> activeUrl = adoptWK(WKPageCopyActiveURL(webView.page()));
    ASSERT_NOT_NULL(activeUrl.get());
    EXPECT_TRUE(WKURLIsEqual(activeUrl.get(), url.get()));
}

TEST(WebKit, PageLoadTwiceAndReload)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    test1Done = false;
    static unsigned loadsCount = 0;

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.base.version = 0;
    loaderClient.base.clientInfo = nullptr;
    loaderClient.didFailProvisionalNavigation = [](WKPageRef page, WKNavigationRef, WKErrorRef error, WKTypeRef userData, const void *clientInfo) {
        loadsCount++;
        WKPageReload(page);
    };
    loaderClient.didFinishNavigation = [](WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo) {
        if (++loadsCount == 3) {
            test1Done = true;
            return;
        }
        WKPageReload(page);
    };
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> url1(AdoptWK, Util::createURLForResource("simple", "html"));
    WKRetainPtr<WKURLRef> url2(AdoptWK, Util::createURLForResource("simple2", "html"));
    WKPageLoadURL(webView.page(), url1.get());
    WKPageLoadURL(webView.page(), url2.get());

    Util::run(&test1Done);
}

} // namespace TestWebKitAPI

#endif
