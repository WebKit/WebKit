/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <UIKit/UIKit.h>
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

static const CGFloat viewHeight = 500;

static NSString *overscrollBehaviorAuto = @"<meta name='viewport' content='width=device-width, initial-scale=1'><html style='width: 100%; height: 5000px; overscroll-behavior: auto;'>";
static NSString *overscrollBehaviorNone = @"<meta name='viewport' content='width=device-width, initial-scale=1'><html style='width: 100%; height: 5000px; overscroll-behavior: none;'>";
static NSString *overscrollBehaviorContain = @"<meta name='viewport' content='width=device-width, initial-scale=1'><html style='width: 100%; height: 5000px; overscroll-behavior: contain;'>";

static NSString *overscrollBehaviorAutoX = @"<meta name='viewport' content='width=device-width, initial-scale=1'><html style='width: 100%; height: 5000px; overscroll-behavior-x: auto;'>";
static NSString *overscrollBehaviorNoneX = @"<meta name='viewport' content='width=device-width, initial-scale=1'><html style='width: 100%; height: 5000px; overscroll-behavior-x: none;'>";
static NSString *overscrollBehaviorContainX = @"<meta name='viewport' content='width=device-width, initial-scale=1'><html style='width: 100%; height: 5000px; overscroll-behavior-x: contain;'>";

static NSString *overscrollBehaviorAutoY = @"<meta name='viewport' content='width=device-width, initial-scale=1'><html style='width: 100%; height: 5000px; overscroll-behavior-y: auto;'>";
static NSString *overscrollBehaviorNoneY = @"<meta name='viewport' content='width=device-width, initial-scale=1'><html style='width: 100%; height: 5000px; overscroll-behavior-y: none;'>";
static NSString *overscrollBehaviorContainY = @"<meta name='viewport' content='width=device-width, initial-scale=1'><html style='width: 100%; height: 5000px; overscroll-behavior-y: contain;'>";

TEST(ScrollViewBouncesTests, OverscrollBehaviorAutoNotSet)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    EXPECT_EQ([[webView scrollView] bounces], YES);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorAuto];
    EXPECT_EQ([[webView scrollView] bounces], YES);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorAutoSet)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorAuto];
    [[webView scrollView] setBounces:NO];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorAutoNotSetDynamicallyChange)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorNone];
    EXPECT_EQ([[webView scrollView] bounces], NO);
    [webView stringByEvaluatingJavaScript:@"document.documentElement.style.overscrollBehavior = 'auto'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([[webView scrollView] bounces], YES);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorAutoSetDynamicallyChange)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorNone];
    EXPECT_EQ([[webView scrollView] bounces], NO);
    [[webView scrollView] setBounces:NO];
    EXPECT_EQ([[webView scrollView] bounces], NO);
    [webView stringByEvaluatingJavaScript:@"document.documentElement.style.overscrollBehavior = 'auto'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorNoneDynamicallyChange)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorAuto];
    EXPECT_EQ([[webView scrollView] bounces], YES);
    [webView objectByEvaluatingJavaScript:@"document.documentElement.style.overscrollBehavior = 'none'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorNoneSet)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorNone];
    [[webView scrollView] setBounces:NO];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorContainNotSet)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorContain];
    EXPECT_EQ([[webView scrollView] bounces], YES);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorContainSet)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorContain];
    [[webView scrollView] setBounces:NO];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorAutoNotSetX)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorAutoX];
    EXPECT_EQ([[webView scrollView] bounces], YES);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorAutoSetX)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorAutoX];
    [[webView scrollView] setBounces:NO];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorNoneNotSetX)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorNoneX];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorNoneSetX)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorNoneX];
    [[webView scrollView] setBounces:NO];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorContainNotSetX)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorContainX];
    EXPECT_EQ([[webView scrollView] bounces], YES);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorContainSetX)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorContainX];
    [[webView scrollView] setBounces:NO];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorAutoNotSetY)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorAutoY];
    EXPECT_EQ([[webView scrollView] bounces], YES);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorAutoSety)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorAutoY];
    [[webView scrollView] setBounces:NO];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorNoneNotSetY)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorNoneY];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorNoneSetY)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorNoneY];
    [[webView scrollView] setBounces:NO];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorContainNotSetY)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorContainY];
    EXPECT_EQ([[webView scrollView] bounces], YES);
}

TEST(ScrollViewBouncesTests, OverscrollBehaviorContainSetY)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    [webView synchronouslyLoadHTMLString:overscrollBehaviorContainY];
    [[webView scrollView] setBounces:NO];
    EXPECT_EQ([[webView scrollView] bounces], NO);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)

