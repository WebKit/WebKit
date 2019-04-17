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

#include "JavaScriptTest.h"
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <JavaScriptCore/JSContextRef.h>
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKSerializedScriptValue.h>

namespace TestWebKitAPI {

static bool testDone;

static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void *clientInfo)
{
    WKPageSetScrollPinningBehavior(page, kWKScrollPinningBehaviorPinToBottom);

    EXPECT_JS_EQ(page, "window.scrollY", "2434");

    PlatformWebView* webView = (PlatformWebView*)clientInfo;
    webView->resizeTo(800, 200);

    EXPECT_JS_EQ(page, "window.scrollY", "2834");
    EXPECT_JS_EQ(page, "window.scrollTo(0,0)", "undefined");
    EXPECT_JS_EQ(page, "window.scrollY", "2834");

    WKPageSetScrollPinningBehavior(page, kWKScrollPinningBehaviorPinToTop);
    
    EXPECT_JS_EQ(page, "window.scrollY", "0");
    EXPECT_JS_EQ(page, "window.scrollTo(0,200)", "undefined");
    EXPECT_JS_EQ(page, "window.scrollY", "0");
    
    WKPageSetScrollPinningBehavior(page, kWKScrollPinningBehaviorDoNotPin);
    
    EXPECT_JS_EQ(page, "window.scrollY", "0");
    EXPECT_JS_EQ(page, "window.scrollTo(0,200)", "undefined");
    EXPECT_JS_EQ(page, "window.scrollY", "200");
    
    testDone = true;
}

TEST(WebKit, ScrollPinningBehaviors)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));

    // Turn off threaded scrolling; synchronously waiting for the main thread scroll position to
    // update using WKPageForceRepaint would be better, but for some reason doesn't block until
    // it's updated after the initial WKPageSetScrollPinningBehavior above.
    WKRetainPtr<WKPageGroupRef> pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("NoThreadedScrollingPageGroup").get()));
    WKPreferencesRef preferences = WKPageGroupGetPreferences(pageGroup.get());
    WKPreferencesSetThreadedScrollingEnabled(preferences, false);

    PlatformWebView webView(context.get(), pageGroup.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.base.clientInfo = &webView;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple-tall", "html")).get());

    Util::run(&testDone);
    EXPECT_TRUE(testDone);
}

} // namespace TestWebKitAPI

#endif
