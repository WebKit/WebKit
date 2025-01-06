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

#import "config.h"

#if WK_HAVE_C_SPI && PLATFORM(MAC)

#import "JavaScriptTest.h"
#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

static bool didFinishLoad;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageNavigationClientV0 loaderClient;
    zeroBytes(loaderClient);

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(page, &loaderClient.base);
}

TEST(WebKit, DeferredViewInWindowStateChange)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("MouseMoveAfterCrashTest"));

    PlatformWebView webView(context.get());
    WKWebView *wkView = webView.platformView();
    setPageLoaderClient(webView.page());

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("lots-of-text", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    EXPECT_JS_FALSE(webView.page(), "document.hidden");

    [wkView _beginDeferringViewInWindowChanges];
    [wkView removeFromSuperview];

    // The document should still not be hidden, even though we're not in-window, because we are deferring in-window changes.
    EXPECT_JS_FALSE(webView.page(), "document.hidden");

    [wkView _endDeferringViewInWindowChanges];

    __block bool done = false;
    WKPageRef page = webView.page();
    dispatch_async(dispatch_get_main_queue(), ^{
        EXPECT_JS_TRUE(page, "document.hidden");
        done = true;
    });

    Util::run(&done);
}

} // namespace TestWebKitAPI

#endif
