/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000
static NSString *const UIScrollViewIndicatorClass = @"_UIScrollViewScrollIndicator";
#else
static NSString *const UIScrollViewIndicatorClass = @"UIImageView";
#endif

namespace TestWebKitAPI {

TEST(OverflowScrollViewTests, ContentChangeMaintainsScrollbars)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"overflow-scroll" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView _test_waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    UIView *childScrollView = [webView wkFirstSubviewWithClass:NSClassFromString(@"WKChildScrollView")];
    EXPECT_NOT_NULL(childScrollView);

    UIView *scrollbar = [childScrollView wkFirstSubviewWithClass:NSClassFromString(UIScrollViewIndicatorClass)];
    EXPECT_NOT_NULL(scrollbar);

    bool contentChanged = false;
    [webView evaluateJavaScript:@"changeScrollerSubviews()" completionHandler: [&] (id result, NSError *error) {
        contentChanged = true;
    }];
    TestWebKitAPI::Util::run(&contentChanged);
    [webView waitForNextPresentationUpdate];

    childScrollView = [webView wkFirstSubviewWithClass:NSClassFromString(@"WKChildScrollView")];
    EXPECT_NOT_NULL(childScrollView);

    scrollbar = [childScrollView wkFirstSubviewWithClass:NSClassFromString(UIScrollViewIndicatorClass)];
    EXPECT_NOT_NULL(scrollbar);
}

TEST(OverflowScrollViewTests, CompositingChangeMaintainsCustomView)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"composited" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView _test_waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    UIView *compositingView = [webView wkFirstSubviewWithBoundsSize:CGSizeMake(200, 200)];
    EXPECT_NOT_NULL(compositingView);

    auto customView = adoptNS([[UIView alloc] initWithFrame:CGRectMake(20, 20, 200, 100)]);
    [compositingView addSubview:customView.get()];

    bool contentChanged = false;
    [webView evaluateJavaScript:@"applyChange()" completionHandler: [&] (id result, NSError *error) {
        contentChanged = true;
    }];
    TestWebKitAPI::Util::run(&contentChanged);
    [webView waitForNextPresentationUpdate];

    EXPECT_NOT_NULL([customView superview]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
