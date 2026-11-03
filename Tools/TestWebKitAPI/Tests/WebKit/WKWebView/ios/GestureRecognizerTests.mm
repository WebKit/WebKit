/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>

namespace TestWebKitAPI {

TEST(GestureRecognizerTests, DoNotAllowTapToRecognizeAlongsideReparentedScrollViewPanGesture)
{
    auto frame = CGRectMake(0, 0, 320, 568);
    RetainPtr hiddenScrollView = adoptNS([[UIScrollView alloc] initWithFrame:frame]);
    RetainPtr webViewContainer = adoptNS([[UIView alloc] initWithFrame:frame]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:frame]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    [[webView window] addSubview:webViewContainer.get()];
    [webViewContainer addSubview:hiddenScrollView.get()];
    [webViewContainer addSubview:webView.get()];
    [webViewContainer addGestureRecognizer:[hiddenScrollView panGestureRecognizer]];

    auto gestureDelegate = static_cast<id<UIGestureRecognizerDelegate>>([webView wkContentView]);
    auto canRecognizeSimultaneouslyWithSingleTap = [&](UIGestureRecognizer *other) {
        return [gestureDelegate gestureRecognizer:[webView _singleTapGestureRecognizer] shouldRecognizeSimultaneouslyWithGestureRecognizer:other];
    };

    EXPECT_FALSE(canRecognizeSimultaneouslyWithSingleTap([hiddenScrollView panGestureRecognizer]));
    EXPECT_TRUE(canRecognizeSimultaneouslyWithSingleTap([webView scrollView].panGestureRecognizer));
}

TEST(GestureRecognizerTests, GestureRecognizersAreNotDuplicatedAfterProcessSwap)
{
    HTTPServer server({
        { "/page"_s, { "<body>Hello world</body>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:server.httpsProxyConfiguration()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/page"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr contentView = [webView wkContentView];
    auto numberOfGestureRecognizersBeforeProcessSwap = [contentView gestureRecognizers].count;

    EXPECT_GT(numberOfGestureRecognizersBeforeProcessSwap, 0U);

    auto firstProcessIdentifier = [webView _webProcessIdentifier];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/page"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_NE([webView _webProcessIdentifier], firstProcessIdentifier);

    EXPECT_EQ([webView wkContentView], contentView);
    EXPECT_EQ([contentView gestureRecognizers].count, numberOfGestureRecognizersBeforeProcessSwap);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
