/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "TestCocoa.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebCore/Color.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKFeature.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringBuilder.h>

#define EXPECT_IN_RANGE(actual, min, max) \
    EXPECT_GE(actual, min); \
    EXPECT_LE(actual, max);

@interface TestKVOWrapper : NSObject

- (instancetype)initWithObservable:(NSObject *)observable keyPath:(NSString *)keyPath callback:(Function<void()>&&)callback;

@end

@implementation TestKVOWrapper {
    RetainPtr<NSObject> _observable;
    RetainPtr<NSString> _keyPath;
    Function<void()> _callback;
}

- (instancetype)initWithObservable:(NSObject *)observable keyPath:(NSString *)keyPath callback:(Function<void()>&&)callback
{
    if (!(self = [super init]))
        return nil;

    _observable = observable;
    _keyPath = keyPath;
    _callback = WTFMove(callback);

    [_observable addObserver:self forKeyPath:_keyPath.get() options:0 context:nil];

    return self;
}

- (void)dealloc
{
    [_observable removeObserver:self forKeyPath:_keyPath.get() context:nullptr];

    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    _callback();
}

@end

static RetainPtr<TestWKWebView> createWebViewWithSampledPageTopColorMaxDifference(double sampledPageTopColorMaxDifference, double sampledPageTopColorMinHeight = 0)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setSampledPageTopColorMaxDifference:sampledPageTopColorMaxDifference];
    [configuration _setSampledPageTopColorMinHeight:sampledPageTopColorMinHeight];

    return adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
}

static void waitForSampledPageTopColorToChange(TestWKWebView *webView, Function<void()>&& trigger = nullptr)
{
    bool done = false;
    auto sampledPageTopColorObserver = adoptNS([[TestKVOWrapper alloc] initWithObservable:webView keyPath:@"_sampledPageTopColor" callback:[&] {
        done = true;
    }]);

    if (trigger)
        trigger();

    TestWebKitAPI::Util::run(&done);
}

static void waitForSampledPageTopColorToChangeForHTML(TestWKWebView *webView, String&& html)
{
    waitForSampledPageTopColorToChange(webView, [webView, html = WTFMove(html)] () mutable {
        [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:html.createNSString().get()];
    });
}

static String createHTMLGradientWithColorStops(String&& direction, Vector<String>&& colorStops)
{
    EXPECT_GE(colorStops.size(), 2UL);

    StringBuilder gradientBuilder;
    gradientBuilder.append("<body style=\"background-image: linear-gradient(to "_s);
    gradientBuilder.append(WTFMove(direction));
    for (auto&& colorStop : WTFMove(colorStops)) {
        gradientBuilder.append(", "_s);
        gradientBuilder.append(WTFMove(colorStop));
    }
    gradientBuilder.append(")\">Test"_s);
    return gradientBuilder.toString();
}

TEST(SampledPageTopColor, ZeroMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(0);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("right"_s, { "red"_s, "red"_s }).createNSString().get()];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, NegativeMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(-5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("right"_s, { "red"_s, "red"_s }).createNSString().get()];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, SolidColor)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    waitForSampledPageTopColorToChangeForHTML(webView.get(), createHTMLGradientWithColorStops("right"_s, { "red"_s, "red"_s }));
    EXPECT_EQ(WebCore::roundAndClampToSRGBALossy([webView _sampledPageTopColor].CGColor), WebCore::Color::red);
}

TEST(SampledPageTopColor, DifferentColorsWithoutOutlierBelowMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    waitForSampledPageTopColorToChangeForHTML(webView.get(), createHTMLGradientWithColorStops("right"_s, {
        "lab(1% 0 0)"_s,
        "lab(2% 0 0)"_s,
        "lab(3% 0 0)"_s,
        "lab(4% 0 0)"_s,
        "lab(5% 0 0)"_s,
    }));

    auto components = CGColorGetComponents([webView _sampledPageTopColor].CGColor);
    EXPECT_IN_RANGE(components[0], 0.04, 0.05);
    EXPECT_IN_RANGE(components[1], 0.04, 0.05);
    EXPECT_IN_RANGE(components[2], 0.04, 0.05);
    EXPECT_EQ(components[3], 1);
}

TEST(SampledPageTopColor, DifferentColorsWithLeftOutlierAboveMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    waitForSampledPageTopColorToChangeForHTML(webView.get(), createHTMLGradientWithColorStops("right"_s, {
        "lab(100% 0 0)"_s, // outlier
        "lab(1% 0 0)"_s,
        "lab(2% 0 0)"_s,
        "lab(3% 0 0)"_s,
        "lab(4% 0 0)"_s,
    }));

    auto components = CGColorGetComponents([webView _sampledPageTopColor].CGColor);
    EXPECT_IN_RANGE(components[0], 0.03, 0.04);
    EXPECT_IN_RANGE(components[1], 0.03, 0.04);
    EXPECT_IN_RANGE(components[2], 0.03, 0.04);
    EXPECT_EQ(components[3], 1);
}

TEST(SampledPageTopColor, DifferentColorsWithMiddleOutlierAboveMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("right"_s, {
        "lab(1% 0 0)"_s,
        "lab(2% 0 0)"_s,
        "lab(100% 0 0)"_s, // outlier
        "lab(3% 0 0)"_s,
        "lab(4% 0 0)"_s,
    }).createNSString().get()];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, DifferentColorsWithRightOutlierAboveMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    waitForSampledPageTopColorToChangeForHTML(webView.get(), createHTMLGradientWithColorStops("right"_s, {
        "lab(1% 0 0)"_s,
        "lab(2% 0 0)"_s,
        "lab(3% 0 0)"_s,
        "lab(4% 0 0)"_s,
        "lab(100% 0 0)"_s, // outlier
    }));

    auto components = CGColorGetComponents([webView _sampledPageTopColor].CGColor);
    EXPECT_IN_RANGE(components[0], 0.03, 0.04);
    EXPECT_IN_RANGE(components[1], 0.03, 0.04);
    EXPECT_IN_RANGE(components[2], 0.03, 0.04);
    EXPECT_EQ(components[3], 1);
}

TEST(SampledPageTopColor, DifferentColorsIndividuallyAboveMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("right"_s, {
        "lab(10% 0 0)"_s,
        "lab(20% 0 0)"_s,
        "lab(30% 0 0)"_s,
        "lab(40% 0 0)"_s,
        "lab(50% 0 0)"_s,
    }).createNSString().get()];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, DifferentColorsCumulativelyAboveMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("right"_s, {
        "lab(1% 0 0)"_s,
        "lab(3% 0 0)"_s,
        "lab(5% 0 0)"_s,
        "lab(7% 0 0)"_s,
        "lab(9% 0 0)"_s,
    }).createNSString().get()];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, VerticalGradientBelowMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5, 100);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("bottom"_s, {
        "lab(1% 0 0)"_s,
        "lab(2% 0 0)"_s,
        "lab(3% 0 0)"_s,
        "lab(4% 0 0)"_s,
        "lab(5% 0 0)"_s,
        "lab(6% 0 0)"_s,
    }).createNSString().get()];
    auto components = CGColorGetComponents([webView _sampledPageTopColor].CGColor);
    EXPECT_IN_RANGE(components[0], 0.01, 0.02);
    EXPECT_IN_RANGE(components[1], 0.01, 0.02);
    EXPECT_IN_RANGE(components[2], 0.01, 0.02);
    EXPECT_EQ(components[3], 1);
}

TEST(SampledPageTopColor, VerticalGradientAboveMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5, 100);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("bottom"_s, { "red"_s, "blue"_s }).createNSString().get()];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, HitTestHTMLImage)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body style='margin: 0'><img src='enormous.svg' style='width: 100%; height: 100%'>Test"];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, HitTestHTMLCanvasWithoutRenderingContext)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    waitForSampledPageTopColorToChangeForHTML(webView.get(), @"<body style='margin: 0'><canvas style='width: 100%; height: 100%; background-color: red'></canvas>Test");
    EXPECT_EQ(WebCore::roundAndClampToSRGBALossy([webView _sampledPageTopColor].CGColor), WebCore::Color::red);
}

TEST(SampledPageTopColor, HitTestHTMLCanvasWithRenderingContext)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body style='margin: 0'><canvas style='width: 100%; height: 100%; background-color: red'></canvas>Test<script>document.querySelector('canvas').getContext('2d')</script>"];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, HitTestCSSBackgroundImage)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body style='margin: 0;'><div style='width: 100%; height: 100%; background-image: url(\'enormous.svg\')'>Test"];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, HitTestBeforeCSSTransition)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    waitForSampledPageTopColorToChangeForHTML(webView.get(), @"<body style='margin: 0; transition: background-color 1s'>Test");
    EXPECT_EQ(WebCore::roundAndClampToSRGBALossy([webView _sampledPageTopColor].CGColor), WebCore::Color::white);
}

TEST(SampledPageTopColor, HitTestDuringCSSTransition)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body style='margin: 0; transition: background-color 1000s'>"];
    [webView objectByEvaluatingJavaScript:@"document.body.style.setProperty('background-color', 'red')"];

    TestWebKitAPI::Util::runFor(1_s);

    // Not setting this until now prevents the sampling logic from running because without it the page isn't considered contentful.
    [webView objectByEvaluatingJavaScript:@"document.body.textContent = 'Test'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, HitTestAfterCSSTransition)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body style='margin: 0; transition: background-color 0.1s'>"];
    [webView objectByEvaluatingJavaScript:@"document.body.style.setProperty('background-color', 'red')"];

    TestWebKitAPI::Util::runFor(1_s);

    // Not setting this until now prevents the sampling logic from running because without it the page isn't considered contentful.
    [webView objectByEvaluatingJavaScript:@"document.body.textContent = 'Test'"];
    waitForSampledPageTopColorToChange(webView.get());
    EXPECT_EQ(WebCore::roundAndClampToSRGBALossy([webView _sampledPageTopColor].CGColor), WebCore::Color::red);
}

TEST(SampledPageTopColor, HitTestBeforeCSSAnimation)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    waitForSampledPageTopColorToChangeForHTML(webView.get(), @"<style>@keyframes changeBackgroundRed { to { background-color: red; } }</style><body style='margin: 0; animation: changeBackgroundRed 1s forwards paused'>Test");
    EXPECT_EQ(WebCore::roundAndClampToSRGBALossy([webView _sampledPageTopColor].CGColor), WebCore::Color::white);
}

TEST(SampledPageTopColor, HitTestDuringCSSAnimation)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style>@keyframes changeBackgroundRed { to { background-color: red; } }</style><body style='margin: 0; animation: changeBackgroundRed 1000s forwards'>"];

    TestWebKitAPI::Util::runFor(1_s);

    // Not setting this until now prevents the sampling logic from running because without it the page isn't considered contentful.
    [webView objectByEvaluatingJavaScript:@"document.body.textContent = 'Test'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, HitTestAfterCSSAnimation)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style>@keyframes changeBackgroundRed { to { background-color: red; } }</style><body style='margin: 0; animation: changeBackgroundRed 0.1s forwards'>"];

    TestWebKitAPI::Util::runFor(1_s);

    // Not setting this until now prevents the sampling logic from running because without it the page isn't considered contentful.
    [webView objectByEvaluatingJavaScript:@"document.body.textContent = 'Test'"];
    waitForSampledPageTopColorToChange(webView.get());
    EXPECT_EQ(WebCore::roundAndClampToSRGBALossy([webView _sampledPageTopColor].CGColor), WebCore::Color::red);
}

TEST(SampledPageTopColor, HitTestCSSPointerEventsNone)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body style='margin: 0'><div style='width: 100%; height: 100%; background-color: red; pointer-events: none'></div>Test"];
    EXPECT_EQ(WebCore::roundAndClampToSRGBALossy([webView _sampledPageTopColor].CGColor), WebCore::Color::red);
}

// FIXME: <https://webkit.org/b/225167> (Sampled Page Top Color: hook into painting logic instead of taking snapshots)
TEST(SampledPageTopColor, DISABLED_DisplayP3)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("right"_s, {
        "color(display-p3 0.99 0 0)"_s,
        "color(display-p3 0.98 0 0)"_s,
        "color(display-p3 0.97 0 0)"_s,
        "color(display-p3 0.96 0 0)"_s,
        "color(display-p3 0.95 0 0)"_s,
    }).createNSString().get()];

    auto components = CGColorGetComponents([webView _sampledPageTopColor].CGColor);
    EXPECT_IN_RANGE(components[0], 0.3, 0.4);
    EXPECT_IN_RANGE(components[1], 1.00, 1.01);
    EXPECT_IN_RANGE(components[2], 1.00, 1.01);
    EXPECT_EQ(components[3], 1);
}

TEST(SampledPageTopColor, MainDocumentChange)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    size_t notificationCount = 0;
    auto sampledPageTopColorObserver = adoptNS([[TestKVOWrapper alloc] initWithObservable:webView.get() keyPath:@"_sampledPageTopColor" callback:[&] {
        ++notificationCount;
    }]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body>Test"];
    EXPECT_EQ(WebCore::roundAndClampToSRGBALossy([webView _sampledPageTopColor].CGColor), WebCore::Color::white);
    EXPECT_EQ(notificationCount, 1UL);

    notificationCount = 0;

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body>Test"];
    EXPECT_EQ(WebCore::roundAndClampToSRGBALossy([webView _sampledPageTopColor].CGColor), WebCore::Color::white);
    // Depending on timing, a notification can be sent for when the main document changes and then
    // when the new main document renders or both can be coalesced if rendering is fast enough.
    EXPECT_TRUE(notificationCount == 0 || notificationCount == 2);

    notificationCount = 0;

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body>"];
    EXPECT_NULL([webView _sampledPageTopColor]);
    EXPECT_EQ(notificationCount, 1UL);

    notificationCount = 0;

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body>"];
    EXPECT_NULL([webView _sampledPageTopColor]);
    EXPECT_EQ(notificationCount, 0UL);

    notificationCount = 0;

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body>Test"];
    EXPECT_EQ(WebCore::roundAndClampToSRGBALossy([webView _sampledPageTopColor].CGColor), WebCore::Color::white);
    EXPECT_EQ(notificationCount, 1UL);
}

#if PLATFORM(IOS_FAMILY) && ENABLE(CONTENT_INSET_BACKGROUND_FILL)

TEST(SampledPageTopColor, TopColorExtensionWhenRubberBanding)
{
    RetainPtr webView = createWebViewWithSampledPageTopColorMaxDifference(5);

    auto insets = UIEdgeInsetsMake(75, 0, 0, 0);
    auto insetSize = UIEdgeInsetsInsetRect([webView bounds], insets).size;
    [webView _setObscuredInsets:insets];
    RetainPtr scrollView = [webView scrollView];
    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];
    [webView _overrideLayoutParametersWithMinimumLayoutSize:insetSize minimumUnobscuredSizeOverride:insetSize maximumUnobscuredSizeOverride:insetSize];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    {
        auto components = CGColorGetComponents([webView _sampledPageTopColor].CGColor);
        EXPECT_IN_RANGE(components[0], 0.99, 1.01);
        EXPECT_IN_RANGE(components[1], 0.38, 0.39);
        EXPECT_IN_RANGE(components[2], 0.27, 0.28);
        EXPECT_EQ(components[3], 1);
    }

    auto colorExtensionViewHeight = [webView] {
        return CGRectGetHeight([webView _colorExtensionViewForTesting:UIRectEdgeTop].bounds);
    };

    EXPECT_EQ(colorExtensionViewHeight(), 75.f);

    [scrollView setContentOffset:CGPointMake(0, -100) animated:NO];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(colorExtensionViewHeight(), 100.f);
}

#endif // PLATFORM(IOS_FAMILY) && ENABLE(CONTENT_INSET_BACKGROUND_FILL)
