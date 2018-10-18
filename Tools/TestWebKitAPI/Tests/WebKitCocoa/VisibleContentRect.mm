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

#import "config.h"

#if WK_API_ENABLED && PLATFORM(IOS_FAMILY)

#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakPtr.h>

@interface WKWebView ()
- (CGRect)_visibleContentRect;
@end

@interface TestWKWebViewWithEnclosingView : TestWKWebView
@property (nonatomic, assign) UIView *test_enclosingViewForExposedRectComputation;
@end

@implementation TestWKWebViewWithEnclosingView
- (UIView *)_enclosingViewForExposedRectComputation
{
    if (_test_enclosingViewForExposedRectComputation)
        return _test_enclosingViewForExposedRectComputation;
    return [super _enclosingViewForExposedRectComputation];
}
@end

namespace TestWebKitAPI {

TEST(WebKit, VisibleContentRect_FullBounds)
{
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebViewWithEnclosingView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:config.get()]);

    EXPECT_TRUE(CGRectEqualToRect([webView _visibleContentRect], CGRectMake(0, 0, 800, 600)));
}

TEST(WebKit, VisibleContentRect_FullBoundsWithinScrollView)
{
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebViewWithEnclosingView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:config.get()]);
    auto window = webView.get().window;
    auto scrollView = adoptNS([[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:scrollView.get()];
    [scrollView addSubview:webView.get()];

    EXPECT_TRUE(CGRectEqualToRect([webView _visibleContentRect], CGRectMake(0, 0, 800, 600)));
}

TEST(WebKit, VisibleContentRect_FullBoundsWhenClippedByNonScrollView)
{
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebViewWithEnclosingView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:config.get()]);
    auto window = webView.get().window;
    auto view = adoptNS([[UIView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:view.get()];
    [view addSubview:webView.get()];

    webView.get().frame = CGRectMake(-100, -100, 1000, 800);

    EXPECT_TRUE(CGRectEqualToRect([webView _visibleContentRect], CGRectMake(0, 0, 1000, 800)));
}

TEST(WebKit, VisibleContentRect_ClippedBoundsWhenClippedByScrollView)
{
    CGRect windowBounds = CGRectMake(0, 0, 800, 600);
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebViewWithEnclosingView alloc] initWithFrame:windowBounds configuration:config.get()]);
    auto window = webView.get().window;
    auto scrollView = adoptNS([[UIScrollView alloc] initWithFrame:windowBounds]);
    [window addSubview:scrollView.get()];
    [scrollView addSubview:webView.get()];

    webView.get().frame = CGRectMake(-100, -100, 1000, 800);
    scrollView.get().contentSize = CGSizeMake(1000, 800);
    scrollView.get().contentOffset = CGPointMake(50, 50);

    EXPECT_TRUE(CGRectEqualToRect([webView _visibleContentRect], CGRectMake(150, 150, 800, 600)));
}

TEST(WebKit, VisibleContentRect_ClippedBoundsWhenClippedByEnclosingView)
{
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebViewWithEnclosingView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:config.get()]);
    auto window = webView.get().window;
    auto view = adoptNS([[UIView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:view.get()];
    [view addSubview:webView.get()];

    webView.get().frame = CGRectMake(-100, -100, 1000, 800);
    webView.get().test_enclosingViewForExposedRectComputation = view.get();

    EXPECT_TRUE(CGRectEqualToRect([webView _visibleContentRect], CGRectMake(100, 100, 800, 600)));
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(IOS_FAMILY)

