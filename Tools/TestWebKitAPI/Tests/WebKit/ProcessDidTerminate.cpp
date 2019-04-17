/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <signal.h>

namespace TestWebKitAPI {

static bool loadBeforeCrash = false;
static bool clientTerminationHandlerCalled = false;
static bool crashHandlerCalled = false;

static void didFinishNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef userData, const void* clientInfo)
{
    if (!loadBeforeCrash) {
        loadBeforeCrash = true;
        return;
    }

    EXPECT_TRUE(false);
}

static void webProcessWasTerminatedByClient(WKPageRef page, WKProcessTerminationReason reason, const void* clientInfo)
{
    // Test if first load actually worked.
    EXPECT_TRUE(loadBeforeCrash);

    // Should only be called once.
    EXPECT_FALSE(clientTerminationHandlerCalled);

    EXPECT_EQ(kWKProcessTerminationReasonRequestedByClient, reason);

    clientTerminationHandlerCalled = true;
}

static void webProcessCrashed(WKPageRef page, WKProcessTerminationReason reason, const void* clientInfo)
{
    // Test if first load actually worked.
    EXPECT_TRUE(loadBeforeCrash);

    // Should only be called once.
    EXPECT_FALSE(crashHandlerCalled);

    EXPECT_EQ(kWKProcessTerminationReasonCrash, reason);

    crashHandlerCalled = true;
}

TEST(WebKit, ProcessDidTerminateRequestedByClient)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV1 navigationClient;
    memset(&navigationClient, 0, sizeof(navigationClient));
    navigationClient.base.version = 1;
    navigationClient.didFinishNavigation = didFinishNavigation;
    navigationClient.webProcessDidTerminate = webProcessWasTerminatedByClient;

    WKPageSetPageNavigationClient(webView.page(), &navigationClient.base);

    loadBeforeCrash = false;
    WKRetainPtr<WKURLRef> url = adoptWK(WKURLCreateWithUTF8CString("about:blank"));
    // Load a blank page and next kills WebProcess.
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&loadBeforeCrash);

    WKPageTerminate(webView.page());

    Util::run(&clientTerminationHandlerCalled);
}

TEST(WebKit, ProcessDidTerminateWithReasonCrash)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV1 navigationClient;
    memset(&navigationClient, 0, sizeof(navigationClient));
    navigationClient.base.version = 1;
    navigationClient.didFinishNavigation = didFinishNavigation;
    navigationClient.webProcessDidTerminate = webProcessCrashed;

    WKPageSetPageNavigationClient(webView.page(), &navigationClient.base);

    loadBeforeCrash = false;
    WKRetainPtr<WKURLRef> url = adoptWK(WKURLCreateWithUTF8CString("about:blank"));
    // Load a blank page and next kills WebProcess.
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&loadBeforeCrash);

    // Simulate a crash by killing the WebProcess.
    kill(WKPageGetProcessIdentifier(webView.page()), SIGKILL);

    Util::run(&crashHandlerCalled);
}

} // namespace TestWebKitAPI

#endif
