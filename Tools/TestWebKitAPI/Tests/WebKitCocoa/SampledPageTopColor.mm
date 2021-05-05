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
#import <WebKit/_WKInternalDebugFeature.h>
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

enum class UseSampledPageTopColorForScrollAreaBackgroundColor : bool { Yes, No };
static RetainPtr<TestWKWebView> createWebViewWithSampledPageTopColorMaxDifference(double sampledPageTopColorMaxDifference, double sampledPageTopColorMinHeight = 0, UseSampledPageTopColorForScrollAreaBackgroundColor useSampledPageTopColorForScrollAreaBackgroundColor = UseSampledPageTopColorForScrollAreaBackgroundColor::No)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setSampledPageTopColorMaxDifference:sampledPageTopColorMaxDifference];
    [configuration _setSampledPageTopColorMinHeight:sampledPageTopColorMinHeight];

    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"UseSampledPageTopColorForScrollAreaBackgroundColor"]) {
            [[configuration preferences] _setEnabled:(useSampledPageTopColorForScrollAreaBackgroundColor == UseSampledPageTopColorForScrollAreaBackgroundColor::Yes) forInternalDebugFeature:feature];
            break;
        }
    }

    return adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
}

static void waitForSampledPageTopColorToChangeForHTML(TestWKWebView *webView, String&& html)
{
    bool done = false;
    auto sampledPageTopColorObserver = adoptNS([[TestKVOWrapper alloc] initWithObservable:webView keyPath:@"_sampledPageTopColor" callback:[&] {
        done = true;
    }]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:WTFMove(html)];
    TestWebKitAPI::Util::run(&done);
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

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("right"_s, { "red"_s, "red"_s })];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, NegativeMaxDifference)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(-5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("right"_s, { "red"_s, "red"_s })];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, SolidColor)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    waitForSampledPageTopColorToChangeForHTML(webView.get(), createHTMLGradientWithColorStops("right"_s, { "red"_s, "red"_s }));
    EXPECT_EQ(WebCore::Color([webView _sampledPageTopColor].CGColor), WebCore::Color::red);
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
    })];
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
    })];
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
    })];
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
    })];
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

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("bottom"_s, { "red"_s, "blue"_s })];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, HitTestHTMLImage)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body style='margin: 0'><img src='enormous.svg' style='width: 100%; height: 100%'>Test"];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, HitTestCSSBackgroundImage)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body style='margin: 0;'><div style='width: 100%; height: 100%; background-image: url(\'enormous.svg\')'>Test"];
    EXPECT_NULL([webView _sampledPageTopColor]);
}

TEST(SampledPageTopColor, HitTestCSSAnimation)
{
    auto webView = createWebViewWithSampledPageTopColorMaxDifference(5);
    EXPECT_NULL([webView _sampledPageTopColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style>@keyframes fadeIn { from { opacity: 0; } }</style><body style='animation: fadeIn 1s infinite alternate'>Test"];
    EXPECT_NULL([webView _sampledPageTopColor]);
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
    })];

    auto components = CGColorGetComponents([webView _sampledPageTopColor].CGColor);
    EXPECT_IN_RANGE(components[0], 0.3, 0.4);
    EXPECT_IN_RANGE(components[1], 1.00, 1.01);
    EXPECT_IN_RANGE(components[2], 1.00, 1.01);
    EXPECT_EQ(components[3], 1);
}

#if PLATFORM(IOS_FAMILY)

// There's no API/SPI to get the background color of the scroll area on macOS.

TEST(SampledPageTopColor, ExperimentalUseSampledPageTopColorForScrollAreaBackgroundColor)
{
    auto webViewWithoutThemeColorForScrollAreaBackgroundColor = createWebViewWithSampledPageTopColorMaxDifference(5, 0, UseSampledPageTopColorForScrollAreaBackgroundColor::No);
    EXPECT_NULL([webViewWithoutThemeColorForScrollAreaBackgroundColor _sampledPageTopColor]);

    [webViewWithoutThemeColorForScrollAreaBackgroundColor synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("right"_s, { "red"_s, "blue"_s })];
    EXPECT_NULL([webViewWithoutThemeColorForScrollAreaBackgroundColor _sampledPageTopColor]);
    EXPECT_EQ(WebCore::Color([webViewWithoutThemeColorForScrollAreaBackgroundColor scrollView].backgroundColor.CGColor), WebCore::Color::white);

    waitForSampledPageTopColorToChangeForHTML(webViewWithoutThemeColorForScrollAreaBackgroundColor.get(), createHTMLGradientWithColorStops("right"_s, { "red"_s, "red"_s }));
    EXPECT_EQ(WebCore::Color([webViewWithoutThemeColorForScrollAreaBackgroundColor _sampledPageTopColor].CGColor), WebCore::Color::red);
    EXPECT_EQ(WebCore::Color([webViewWithoutThemeColorForScrollAreaBackgroundColor scrollView].backgroundColor.CGColor), WebCore::Color::white);

    auto webViewWithThemeColorForScrollAreaBackgroundColor = createWebViewWithSampledPageTopColorMaxDifference(5, 0, UseSampledPageTopColorForScrollAreaBackgroundColor::Yes);
    EXPECT_NULL([webViewWithThemeColorForScrollAreaBackgroundColor _sampledPageTopColor]);

    [webViewWithThemeColorForScrollAreaBackgroundColor synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:createHTMLGradientWithColorStops("right"_s, { "red"_s, "blue"_s })];
    EXPECT_NULL([webViewWithThemeColorForScrollAreaBackgroundColor _sampledPageTopColor]);
    EXPECT_EQ(WebCore::Color([webViewWithThemeColorForScrollAreaBackgroundColor scrollView].backgroundColor.CGColor), WebCore::Color::white);

    waitForSampledPageTopColorToChangeForHTML(webViewWithThemeColorForScrollAreaBackgroundColor.get(), createHTMLGradientWithColorStops("right"_s, { "red"_s, "red"_s }));
    EXPECT_EQ(WebCore::Color([webViewWithThemeColorForScrollAreaBackgroundColor _sampledPageTopColor].CGColor), WebCore::Color::red);
    EXPECT_EQ(WebCore::Color([webViewWithThemeColorForScrollAreaBackgroundColor scrollView].backgroundColor.CGColor), WebCore::Color::red);
}

#endif // PLATFORM(IOS_FAMILY)
