/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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
#import "JavaScriptTest.h"
#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKPage.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {
    
static bool didFinishLoad;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

TEST(WebKit, FirstResponderScrollingPosition)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextWithInjectedBundle());

    // Turn off threaded scrolling; synchronously waiting for the main thread scroll position to
    // update using WKPageForceRepaint would be better, but for some reason the test still fails occasionally.
    WKRetainPtr<WKPageGroupRef> pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("NoThreadedScrollingPageGroup").get()));
    WKPreferencesRef preferences = WKPageGroupGetPreferences(pageGroup.get());
    WKPreferencesSetThreadedScrollingEnabled(preferences, false);

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600)
        styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [window.get() makeKeyAndOrderFront:nil];
    EXPECT_TRUE([window.get() isVisible]);

    PlatformWebView webView(context.get(), pageGroup.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);
    
    [window.get().contentView addSubview:webView.platformView()];
    [window.get() makeFirstResponder:webView.platformView()];

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("simple-tall", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    EXPECT_JS_EQ(webView.page(), "var input = document.createElement('input');"
        "document.body.insertBefore(input, document.body.firstChild);"
        "input.focus(); false", "false");
    EXPECT_JS_EQ(webView.page(), "window.scrollY", "0");

    ASSERT_TRUE([webView.platformView() respondsToSelector:@selector(scrollLineDown:)]);
    [webView.platformView() scrollLineDown:nil];

    EXPECT_JS_EQ(webView.page(), "window.scrollY", "40");

    PlatformWebView newWebView(context.get(), pageGroup.get());
    WKPageSetPageNavigationClient(newWebView.page(), &loaderClient.base);

    [window.get().contentView addSubview:newWebView.platformView()];
    [window.get() makeFirstResponder:newWebView.platformView()];

    WKPageLoadURL(newWebView.page(), url.get());
    Util::run(&didFinishLoad);

    EXPECT_JS_EQ(webView.page(), "window.scrollY", "40");
    EXPECT_JS_EQ(newWebView.page(), "window.scrollY", "0");

    [window.get() makeFirstResponder:webView.platformView()];

    EXPECT_JS_EQ(webView.page(), "window.scrollY", "40");
    EXPECT_JS_EQ(newWebView.page(), "window.scrollY", "0");
}
    
} // namespace TestWebKitAPI
