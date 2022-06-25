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
#include <WebKit/WKRetainPtr.h>
#include <cmath>

namespace TestWebKitAPI {

static bool done;
static bool loadDone;
static bool messageReceived;

static void didReceiveMessageFromInjectedBundle(WKContextRef context, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    messageReceived = true;

    EXPECT_WK_STREQ("DidAttemptToSendAppleEvent", messageName);
    ASSERT_NOT_NULL(messageBody);
    EXPECT_EQ(WKDoubleGetTypeID(), WKGetTypeID(messageBody));

    WKDoubleRef returnCodeMessage = static_cast<WKDoubleRef>(messageBody);
    double returnCode = WKDoubleGetValue(returnCodeMessage);
    EXPECT_TRUE(std::isfinite(returnCode));

    // Zero return code indicates the WebProcess successfully issued an Apple Event.
    EXPECT_NE(0, returnCode);

    if (loadDone)
        done = true;
}

static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    loadDone = true;
    if (messageReceived)
        done = true;
}

TEST(WebKit, InjectedBundleAppleEvent)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("InjectedBundleAppleEventTest"));

    WKContextInjectedBundleClientV0 injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(injectedBundleClient));

    injectedBundleClient.base.version = 0;
    injectedBundleClient.didReceiveMessageFromInjectedBundle = didReceiveMessageFromInjectedBundle;

    WKContextSetInjectedBundleClient(context.get(), &injectedBundleClient.base);

    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&done);
}

} // namespace TestWebKitAPI

#endif
