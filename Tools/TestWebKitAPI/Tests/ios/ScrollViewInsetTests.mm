/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED && PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>

@interface AsyncPolicyDelegateForInsetTest : NSObject<WKNavigationDelegate> {
    @public BOOL _navigationComplete;
}
@property (nonatomic, copy) void (^webContentProcessDidTerminate)(WKWebView *);
@end

@implementation AsyncPolicyDelegateForInsetTest

- (void)webView:(WKWebView *)webView didFinishNavigation:(null_unspecified WKNavigation *)navigation
{
    _navigationComplete = YES;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    int64_t deferredWaitTime = 100 * NSEC_PER_MSEC;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, deferredWaitTime);
    dispatch_after(when, dispatch_get_main_queue(), ^{
        decisionHandler(WKNavigationActionPolicyAllow);
    });
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    int64_t deferredWaitTime = 100 * NSEC_PER_MSEC;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, deferredWaitTime);
    dispatch_after(when, dispatch_get_main_queue(), ^{
        decisionHandler(WKNavigationResponsePolicyAllow);
    });
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    if (_webContentProcessDidTerminate)
        _webContentProcessDidTerminate(webView);
}
@end

namespace TestWebKitAPI {

static const CGFloat viewHeight = 500;
static NSString *veryTallDocumentMarkup = @"<meta name='viewport' content='width=device-width, initial-scale=1'><body style='width: 100%; height: 5000px'>";

static void waitUntilInnerHeightIs(TestWKWebView *webView, CGFloat expectedValue)
{
    int tries = 0;
    do {
        Util::sleep(0.1);
    } while ([[webView objectByEvaluatingJavaScript:@"innerHeight"] floatValue] != expectedValue && ++tries <= 100);
}

TEST(ScrollViewInsetTests, InnerHeightWithLargeTopContentInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView scrollView].contentInset = UIEdgeInsetsMake(400, 0, 0, 0);
    [webView synchronouslyLoadHTMLString:veryTallDocumentMarkup];

    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 10)"];
    waitUntilInnerHeightIs(webView.get(), viewHeight);
    EXPECT_EQ(viewHeight, [[webView objectByEvaluatingJavaScript:@"innerHeight"] floatValue]);
    EXPECT_EQ(10, [[webView objectByEvaluatingJavaScript:@"pageYOffset"] floatValue]);

    [webView stringByEvaluatingJavaScript:@"scrollBy(0, -10)"];
    waitUntilInnerHeightIs(webView.get(), viewHeight);
    EXPECT_EQ(viewHeight, [[webView objectByEvaluatingJavaScript:@"innerHeight"] floatValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"pageYOffset"] floatValue]);

    [webView stringByEvaluatingJavaScript:@"scrollBy(0, 20)"];
    waitUntilInnerHeightIs(webView.get(), viewHeight);
    EXPECT_EQ(viewHeight, [[webView objectByEvaluatingJavaScript:@"innerHeight"] floatValue]);
    EXPECT_EQ(20, [[webView objectByEvaluatingJavaScript:@"pageYOffset"] floatValue]);
}

TEST(ScrollViewInsetTests, InnerHeightWithLargeBottomContentInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView scrollView].contentInset = UIEdgeInsetsMake(0, 0, 400, 0);
    [webView synchronouslyLoadHTMLString:veryTallDocumentMarkup];
    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 10)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(viewHeight, [[webView objectByEvaluatingJavaScript:@"innerHeight"] floatValue]);
    EXPECT_EQ(10, [[webView objectByEvaluatingJavaScript:@"pageYOffset"] floatValue]);

    [webView stringByEvaluatingJavaScript:@"scrollBy(0, -10)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(viewHeight, [[webView objectByEvaluatingJavaScript:@"innerHeight"] floatValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"pageYOffset"] floatValue]);

    [webView stringByEvaluatingJavaScript:@"scrollBy(0, 20)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(viewHeight, [[webView objectByEvaluatingJavaScript:@"innerHeight"] floatValue]);
    EXPECT_EQ(20, [[webView objectByEvaluatingJavaScript:@"pageYOffset"] floatValue]);
}

TEST(ScrollViewInsetTests, RestoreInitialContentOffsetAfterNavigation)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView scrollView].contentInset = UIEdgeInsetsMake(400, 0, 0, 0);
    [webView synchronouslyLoadHTMLString:veryTallDocumentMarkup];
    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 1000)"];
    [webView waitForNextPresentationUpdate];

    CGPoint contentOffsetAfterScrolling = [webView scrollView].contentOffset;
    EXPECT_EQ(0, contentOffsetAfterScrolling.x);
    EXPECT_EQ(1000, contentOffsetAfterScrolling.y);

    [webView synchronouslyLoadHTMLString:@"<body bgcolor='red'>"];
    [webView waitForNextPresentationUpdate];

    CGPoint contentOffsetAfterNavigation = [webView scrollView].contentOffset;
    EXPECT_EQ(0, contentOffsetAfterNavigation.x);
    EXPECT_EQ(-400, contentOffsetAfterNavigation.y);
}

TEST(ScrollViewInsetTests, RestoreInitialContentOffsetAfterCrash)
{
    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView scrollView].contentInset = UIEdgeInsetsMake(400, 0, 0, 0);
    [webView synchronouslyLoadHTMLString:veryTallDocumentMarkup];
    [webView setNavigationDelegate:delegate.get()];

    CGPoint initialContentOffset = [webView scrollView].contentOffset;
    __block CGPoint contentOffsetAfterCrash = CGPointZero;
    __block bool done = false;
    [delegate setWebContentProcessDidTerminate:^(WKWebView *webView) {
        contentOffsetAfterCrash = webView.scrollView.contentOffset;
        done = true;
    }];

    [webView _killWebContentProcessAndResetState];
    Util::run(&done);

    EXPECT_EQ(initialContentOffset.x, contentOffsetAfterCrash.x);
    EXPECT_EQ(initialContentOffset.y, contentOffsetAfterCrash.y);
    EXPECT_EQ(0, initialContentOffset.x);
    EXPECT_EQ(-400, initialContentOffset.y);
}

TEST(ScrollViewInsetTests, RestoreInitialContentOffsetAfterCrashWithAsyncPolicyDelegates)
{
    auto delegate = adoptNS([[AsyncPolicyDelegateForInsetTest alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView scrollView].contentInset = UIEdgeInsetsMake(400, 0, 0, 0);
    [webView setNavigationDelegate:delegate.get()];
    delegate->_navigationComplete = NO;
    NSURL *testResourceURL = [[[NSBundle mainBundle] bundleURL] URLByAppendingPathComponent:@"TestWebKitAPI.resources"];
    [webView loadHTMLString:veryTallDocumentMarkup baseURL:testResourceURL];
    Util::run(&delegate->_navigationComplete);

    __block bool presentationUpdateHappened = false;
    [webView _doAfterNextPresentationUpdate:^{
        presentationUpdateHappened = true;
    }];
    TestWebKitAPI::Util::run(&presentationUpdateHappened);

    CGPoint initialContentOffset = [webView scrollView].contentOffset;
    __block CGPoint contentOffsetAfterCrash = CGPointZero;
    __block bool done = false;
    [delegate setWebContentProcessDidTerminate:^(WKWebView *webView) {
        contentOffsetAfterCrash = webView.scrollView.contentOffset;
        done = true;
    }];

    [webView _killWebContentProcessAndResetState];
    Util::run(&done);

    EXPECT_EQ(initialContentOffset.x, contentOffsetAfterCrash.x);
    EXPECT_EQ(initialContentOffset.y, contentOffsetAfterCrash.y);
    EXPECT_EQ(0, initialContentOffset.x);
    EXPECT_EQ(-400, initialContentOffset.y);
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(IOS_FAMILY)
