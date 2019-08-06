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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <UIKit/UIKit.h>
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

static NSString *scrollableDocumentMarkup = @"<meta name='viewport' content='width=device-width, initial-scale=1'><body style='width: 100%; height: 5000px;'>";
static NSString *nonScrollableDocumentMarkup = @"<meta name='viewport' content='width=device-width, initial-scale=1'><body style='width: 100%; height: 5000px; overflow: hidden'>";
static NSString *nonScrollableWithInputDocumentMarkup = @"<meta name='viewport' content='width=device-width, initial-scale=1'><body style='width: 100%; height: 5000px; overflow: hidden'><input autofocus>";

static const CGFloat viewHeight = 500;

static RetainPtr<TestWKWebView> webViewWithAutofocusedInput(const RetainPtr<TestInputDelegate>& inputDelegate)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);

    bool doneWaiting = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        doneWaiting = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:nonScrollableWithInputDocumentMarkup];

    TestWebKitAPI::Util::run(&doneWaiting);
    doneWaiting = false;
    return webView;
}

TEST(ScrollViewScrollabilityTests, ScrollableWithOverflowVisible)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:scrollableDocumentMarkup];
    EXPECT_EQ([[webView scrollView] isScrollEnabled], YES);
}

TEST(ScrollViewScrollabilityTests, NonScrollableWithOverflowHidden)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:nonScrollableDocumentMarkup];
    EXPECT_EQ([[webView scrollView] isScrollEnabled], NO);
}

TEST(ScrollViewScrollabilityTests, ScrollableWithOverflowHiddenWhenZoomed)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:nonScrollableDocumentMarkup];
    [[webView scrollView] setZoomScale:1.5 animated:NO];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([[webView scrollView] isScrollEnabled], YES);
}

TEST(ScrollViewScrollabilityTests, ScrollableWithOverflowHiddenAndInputView)
{
    auto inputView = adoptNS([[UIView alloc] init]);
    auto inputAccessoryView = adoptNS([[UIView alloc] init]);
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setWillStartInputSessionHandler:[inputView, inputAccessoryView] (WKWebView *, id<_WKFormInputSession> session) {
        session.customInputView = inputView.get();
        session.customInputAccessoryView = inputAccessoryView.get();
    }];

    auto webView = webViewWithAutofocusedInput(inputDelegate);
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([[webView scrollView] isScrollEnabled], YES);
}

TEST(ScrollViewScrollabilityTests, ScrollableWithOverflowHiddenAndVisibleUI)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);

    // Simulate portrait phone with:
    // Top bar size: 50
    // Shrunk top bar size: 40
    // Bottom bar size: 44
    UIEdgeInsets obscuredInsets;
    obscuredInsets.top = 50;
    obscuredInsets.left = 0;
    obscuredInsets.bottom = 44;
    obscuredInsets.right = 0;

    [webView _setObscuredInsets:obscuredInsets];
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(320, 406) maximumUnobscuredSizeOverride:CGSizeMake(320, 490)];

    [webView synchronouslyLoadHTMLString:nonScrollableDocumentMarkup];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([[webView scrollView] isScrollEnabled], NO);
}

TEST(ScrollViewScrollabilityTests, ScrollableWithOverflowHiddenAndShrunkUI)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, viewHeight, 414)]);

    // Simulate landscape phone with hidden bars.
    UIEdgeInsets obscuredInsets;
    obscuredInsets.top = 0;
    obscuredInsets.left = 0;
    obscuredInsets.bottom = 0;
    obscuredInsets.right = 0;

    [webView _setObscuredInsets:obscuredInsets];
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(viewHeight, 414) maximumUnobscuredSizeOverride:CGSizeMake(viewHeight, 414)];

    [webView synchronouslyLoadHTMLString:nonScrollableDocumentMarkup];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([[webView scrollView] isScrollEnabled], YES);
}

TEST(ScrollViewScrollabilityTests, ScrollableAfterNavigateToPDF)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, viewHeight, 414)]);

    [webView synchronouslyLoadHTMLString:nonScrollableDocumentMarkup];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([[webView scrollView] isScrollEnabled], NO);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    [webView _test_waitForDidFinishNavigation];

    EXPECT_EQ([[webView scrollView] isScrollEnabled], YES);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
