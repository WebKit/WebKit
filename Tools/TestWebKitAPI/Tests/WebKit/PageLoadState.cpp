/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

struct PageLoadTestState {
    int didChangeActiveURL { 0 };
    int didChangeCanGoBack { 0 };
    int didChangeCanGoForward { 0 };
    int didChangeCertificateInfo { 0 };
    int didChangeEstimatedProgress { 0 };
    int didChangeHasOnlySecureContent { 0 };
    int didChangeIsLoading { 0 };
    int didChangeNetworkRequestsInProgress { 0 };
    int didChangeTitle { 0 };
    int didChangeWebProcessIsResponsive { 0 };
    int didSwapWebProcesses { 0 };
    int willChangeActiveURL { 0 };
    int willChangeCanGoBack { 0 };
    int willChangeCanGoForward { 0 };
    int willChangeCertificateInfo { 0 };
    int willChangeEstimatedProgress { 0 };
    int willChangeHasOnlySecureContent { 0 };
    int willChangeIsLoading { 0 };
    int willChangeNetworkRequestsInProgress { 0 };
    int willChangeTitle { 0 };
    int willChangeWebProcessIsResponsive { 0 };
};

static void didChangeActiveURL(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didChangeActiveURL++;
}

static void didChangeCanGoBack(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didChangeCanGoBack++;
}

static void didChangeCanGoForward(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didChangeCanGoForward++;
}

static void didChangeCertificateInfo(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didChangeCertificateInfo++;
}

static void didChangeEstimatedProgress(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didChangeEstimatedProgress++;
}

static void didChangeHasOnlySecureContent(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didChangeHasOnlySecureContent++;
}

static void didChangeIsLoading(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didChangeIsLoading++;
}

static void didChangeNetworkRequestsInProgress(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didChangeNetworkRequestsInProgress++;
}

static void didChangeTitle(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didChangeTitle++;
}

static void didChangeWebProcessIsResponsive(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didChangeWebProcessIsResponsive++;
}

static void didSwapWebProcesses(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->didSwapWebProcesses++;
}

static void willChangeActiveURL(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->willChangeActiveURL++;
}

static void willChangeCanGoBack(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->willChangeCanGoBack++;
}

static void willChangeCanGoForward(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->willChangeCanGoForward++;
}

static void willChangeCertificateInfo(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->willChangeCertificateInfo++;
}

static void willChangeEstimatedProgress(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->willChangeEstimatedProgress++;
}

static void willChangeHasOnlySecureContent(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->willChangeHasOnlySecureContent++;
}

static void willChangeIsLoading(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->willChangeIsLoading++;
}

static void willChangeNetworkRequestsInProgress(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->willChangeNetworkRequestsInProgress++;
}

static void willChangeTitle(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->willChangeTitle++;
}

static void willChangeWebProcessIsResponsive(const void* clientInfo)
{
    PageLoadTestState* state = reinterpret_cast<PageLoadTestState*>(const_cast<void*>(clientInfo));
    state->willChangeWebProcessIsResponsive++;
}
static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    test1Done = true;
}

TEST(WebKit, PageLoadState)
{
    PageLoadTestState state;

    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient { };

    loaderClient.base.version = 0;
    loaderClient.base.clientInfo = &state;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageStateClientV0 stateClient = { };
    stateClient.base.version = 0;
    stateClient.base.clientInfo = &state;
    stateClient.didChangeActiveURL = didChangeActiveURL;
    stateClient.didChangeCanGoBack = didChangeCanGoBack;
    stateClient.didChangeCanGoForward = didChangeCanGoForward;
    stateClient.didChangeCertificateInfo = didChangeCertificateInfo;
    stateClient.didChangeEstimatedProgress = didChangeEstimatedProgress;
    stateClient.didChangeHasOnlySecureContent = didChangeHasOnlySecureContent;
    stateClient.didChangeIsLoading = didChangeIsLoading;
    stateClient.didChangeNetworkRequestsInProgress = didChangeNetworkRequestsInProgress;
    stateClient.didChangeTitle = didChangeTitle;
    stateClient.didChangeWebProcessIsResponsive = didChangeWebProcessIsResponsive;
    stateClient.didSwapWebProcesses = didSwapWebProcesses;
    stateClient.willChangeActiveURL = willChangeActiveURL;
    stateClient.willChangeCanGoBack = willChangeCanGoBack;
    stateClient.willChangeCanGoForward = willChangeCanGoForward;
    stateClient.willChangeCertificateInfo = willChangeCertificateInfo;
    stateClient.willChangeEstimatedProgress = willChangeEstimatedProgress;
    stateClient.willChangeHasOnlySecureContent = willChangeHasOnlySecureContent;
    stateClient.willChangeIsLoading = willChangeIsLoading;
    stateClient.willChangeNetworkRequestsInProgress = willChangeNetworkRequestsInProgress;
    stateClient.willChangeTitle = willChangeTitle;
    stateClient.willChangeWebProcessIsResponsive = willChangeWebProcessIsResponsive;
    WKPageSetPageStateClient(webView.page(), &stateClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&test1Done);

    EXPECT_EQ(state.didChangeActiveURL, 1);
    EXPECT_EQ(state.didChangeCanGoBack, 0);
    EXPECT_EQ(state.didChangeCanGoForward, 0);
    EXPECT_EQ(state.didChangeCertificateInfo, 1);
    EXPECT_GT(state.didChangeEstimatedProgress, 0);
    EXPECT_EQ(state.didChangeHasOnlySecureContent, 0);
    EXPECT_GT(state.didChangeIsLoading, 0);
    EXPECT_EQ(state.didChangeNetworkRequestsInProgress, 0);
    EXPECT_EQ(state.didChangeTitle, 0);
    EXPECT_EQ(state.didChangeWebProcessIsResponsive, 0);
    EXPECT_EQ(state.didSwapWebProcesses, 1);
    EXPECT_EQ(state.willChangeActiveURL, 1);
    EXPECT_EQ(state.willChangeCanGoBack, 0);
    EXPECT_EQ(state.willChangeCanGoForward, 0);
    EXPECT_EQ(state.willChangeCertificateInfo, 1);
    EXPECT_GT(state.willChangeEstimatedProgress, 0);
    EXPECT_EQ(state.willChangeHasOnlySecureContent, 0);
    EXPECT_GT(state.willChangeIsLoading, 0);
    EXPECT_EQ(state.willChangeNetworkRequestsInProgress, 0);
    EXPECT_EQ(state.willChangeTitle, 0);
    EXPECT_EQ(state.willChangeWebProcessIsResponsive, 0);

    test1Done = false;
    url = adoptWK(Util::createURLForResource("simple2", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&test1Done);

    EXPECT_EQ(state.didChangeActiveURL, 2);
    EXPECT_EQ(state.didChangeCanGoBack, 1);
    EXPECT_EQ(state.didChangeCanGoForward, 0);
    EXPECT_EQ(state.willChangeCanGoBack, 1);
    EXPECT_EQ(state.willChangeCanGoForward, 0);

    test1Done = false;
    WKPageGoBack(webView.page());
    Util::run(&test1Done);

    EXPECT_EQ(state.didChangeActiveURL, 3);
    EXPECT_EQ(state.didChangeCanGoBack, 2);
    EXPECT_EQ(state.didChangeCanGoForward, 1);
    EXPECT_EQ(state.willChangeCanGoBack, 2);
    EXPECT_EQ(state.willChangeCanGoForward, 1);

    test1Done = false;
    url = adoptWK(Util::createURLForResource("set-long-title", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&test1Done);

    EXPECT_EQ(state.didChangeActiveURL, 4);
    EXPECT_EQ(state.didChangeTitle, 2);
    EXPECT_EQ(state.willChangeTitle, 2);

    WKPageSetPageStateClient(webView.page(), nullptr);

    test1Done = false;
    url = adoptWK(Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&test1Done);

    EXPECT_EQ(state.didChangeActiveURL, 4);

    WKPageSetPageStateClient(webView.page(), &stateClient.base);
}

} // namespace TestWebKitAPI

#endif
