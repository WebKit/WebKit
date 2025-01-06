/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#import "config.h"

#if WK_HAVE_C_SPI

#import "JavaScriptTest.h"
#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import "Test.h"
#import <JavaScriptCore/JSContextRef.h>
#import <WebKit/WKContextPrivate.h>
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKSerializedScriptValue.h>
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

static bool didLoad;

static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void *clientInfo)
{
    didLoad = true;
}

TEST(WebKit, ScrollPinningBehaviors)
{
    auto configuration = adoptWK(WKPageConfigurationCreate());
    auto preferences = adoptWK(WKPreferencesCreate());
    WKPageConfigurationSetPreferences(configuration.get(), preferences.get());

    // Turn off threaded scrolling; synchronously waiting for the main thread scroll position to
    // update using WKPageForceRepaint would be better, but for some reason doesn't block until
    // it's updated after the initial WKPageSetScrollPinningBehavior above.
    WKPreferencesSetThreadedScrollingEnabled(preferences.get(), false);

    PlatformWebView webView(configuration.get());

    WKPageNavigationClientV0 loaderClient;
    zeroBytes(loaderClient);

    loaderClient.base.version = 0;
    loaderClient.base.clientInfo = &webView;
    loaderClient.didFinishNavigation = didFinishNavigation;
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple-tall", "html")).get());

    Util::run(&didLoad);

    WKPageSetScrollPinningBehavior(webView.page(), kWKScrollPinningBehaviorPinToBottom);

    __block bool didUpdatePresentation = false;
    [webView.platformView() _doAfterNextPresentationUpdate:^{
        didUpdatePresentation = true;
    }];
    Util::run(&didUpdatePresentation);

    EXPECT_JS_EQ(webView.page(), "window.scrollY", "2434");

    webView.resizeTo(800, 200);

    didUpdatePresentation = false;
    [webView.platformView() _doAfterNextPresentationUpdate:^{
        didUpdatePresentation = true;
    }];
    Util::run(&didUpdatePresentation);

    EXPECT_JS_EQ(webView.page(), "window.scrollY", "2834");
    EXPECT_JS_EQ(webView.page(), "window.scrollTo(0,0)", "undefined");

    didUpdatePresentation = false;
    [webView.platformView() _doAfterNextPresentationUpdate:^{
        didUpdatePresentation = true;
    }];
    Util::run(&didUpdatePresentation);

    EXPECT_JS_EQ(webView.page(), "window.scrollY", "2834");

    didUpdatePresentation = false;
    WKPageSetScrollPinningBehavior(webView.page(), kWKScrollPinningBehaviorPinToTop);
    [webView.platformView() _doAfterNextPresentationUpdate:^{
        didUpdatePresentation = true;
    }];
    Util::run(&didUpdatePresentation);

    EXPECT_JS_EQ(webView.page(), "window.scrollY", "0");
    EXPECT_JS_EQ(webView.page(), "window.scrollTo(0,200)", "undefined");

    didUpdatePresentation = false;
    [webView.platformView() _doAfterNextPresentationUpdate:^{
        didUpdatePresentation = true;
    }];
    Util::run(&didUpdatePresentation);

    EXPECT_JS_EQ(webView.page(), "window.scrollY", "0");

    didUpdatePresentation = false;
    WKPageSetScrollPinningBehavior(webView.page(), kWKScrollPinningBehaviorDoNotPin);
    [webView.platformView() _doAfterNextPresentationUpdate:^{
        didUpdatePresentation = true;
    }];
    Util::run(&didUpdatePresentation);

    EXPECT_JS_EQ(webView.page(), "window.scrollY", "0");
    EXPECT_JS_EQ(webView.page(), "window.scrollTo(0,200)", "undefined");

    didUpdatePresentation = false;
    [webView.platformView() _doAfterNextPresentationUpdate:^{
        didUpdatePresentation = true;
    }];
    Util::run(&didUpdatePresentation);
    EXPECT_JS_EQ(webView.page(), "window.scrollY", "200");
}

} // namespace TestWebKitAPI

#endif
