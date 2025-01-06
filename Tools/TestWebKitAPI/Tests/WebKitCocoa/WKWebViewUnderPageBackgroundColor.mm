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
#import <WebCore/ColorCocoa.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#define EXPECT_IN_RANGE(actual, min, max) \
    EXPECT_GE(actual, min); \
    EXPECT_LE(actual, max);

#define EXPECT_NSSTRING_EQ(expected, actual) \
    EXPECT_TRUE([actual isKindOfClass:[NSString class]]); \
    EXPECT_WK_STREQ(expected, (NSString *)actual);

static RetainPtr<CGColor> defaultBackgroundColor()
{
#if PLATFORM(MAC)
    auto color = retainPtr(NSColor.controlBackgroundColor);
#elif PLATFORM(IOS_FAMILY)
    auto color = retainPtr(UIColor.systemBackgroundColor);
#else
    auto color = retainPtr([WebCore::CocoaColor whiteColor]);
#endif

    // Some of the above can sometimes be a monochrome color, so convert it to sRGB so the comparisons below work.
    // `WebCore::ColorSpace` doesn't have an equivalent monochrome enum value, but treats `CGColor` with only two components as monochrome and converts them to `SRGB`.
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto sRGBColor = adoptCF(CGColorCreateCopyByMatchingToColorSpace(sRGBColorSpace.get(), kCGRenderingIntentDefault, [color CGColor], NULL));
    return sRGBColor.get();
}

TEST(WKWebViewUnderPageBackgroundColor, OnLoad)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));
}

TEST(WKWebViewUnderPageBackgroundColor, SingleSolidColor)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: red; } </style>"];
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));
}

TEST(WKWebViewUnderPageBackgroundColor, SingleBlendedColor)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: rgba(255, 0, 0, 0.5); } </style>"];
    auto components = CGColorGetComponents([webView underPageBackgroundColor].CGColor);
    EXPECT_EQ(components[0], 1);
    EXPECT_IN_RANGE(components[1], 0.45, 0.55);
    EXPECT_IN_RANGE(components[2], 0.45, 0.55);
    EXPECT_EQ(components[3], 1);
}

TEST(WKWebViewUnderPageBackgroundColor, MultipleSolidColors)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> html { background-color: blue; } body { background-color: red; } </style>"];
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));
}

TEST(WKWebViewUnderPageBackgroundColor, MultipleBlendedColors)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> html { background-color: rgba(255, 0, 0, 0.5); } body { background-color: rgba(0, 0, 255, 0.5); } </style>"];
    auto components = CGColorGetComponents([webView underPageBackgroundColor].CGColor);
    EXPECT_IN_RANGE(components[0], 0.45, 0.55);
    EXPECT_IN_RANGE(components[1], 0.2, 0.25);
    EXPECT_IN_RANGE(components[2], 0.7, 0.75);
    EXPECT_EQ(components[3], 1);
}

@interface WKWebViewUnderPageBackgroundColorObserver : NSObject

- (instancetype)initWithWebView:(WKWebView *)webView;

@property (nonatomic, readonly) WKWebView *webView;
@property (nonatomic, copy) NSString *state;

@end

@implementation WKWebViewUnderPageBackgroundColorObserver

- (instancetype)initWithWebView:(WKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _state = @"before-init";

    _webView = webView;
    [_webView addObserver:self forKeyPath:@"underPageBackgroundColor" options:NSKeyValueObservingOptionInitial context:nil];

    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if ([_state isEqualToString:@"before-init"]) {
        _state = @"after-init";
        return;
    }

    if ([_state isEqualToString:@"before-css-class-added"]) {
        _state = @"after-css-class-added";
        return;
    }

    if ([_state isEqualToString:@"before-css-class-changed"]) {
        _state = @"after-css-class-changed";
        return;
    }

    if ([_state isEqualToString:@"before-css-class-removed"]) {
        _state = @"after-css-class-removed";
        return;
    }

    if ([_state isEqualToString:@"before-nonnull-override"]) {
        _state = @"after-nonnull-override";
        return;
    }

    if ([_state isEqualToString:@"before-null-override"]) {
        _state = @"after-null-override";
        return;
    }

    EXPECT_NSSTRING_EQ("not-reached", _state);
}

@end

TEST(WKWebViewUnderPageBackgroundColor, KVO)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    auto blueColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), blueColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto underPageBackgroundColorObserver = adoptNS([[WKWebViewUnderPageBackgroundColorObserver alloc] initWithWebView:webView.get()]);
    EXPECT_NSSTRING_EQ("after-init", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body.red { background-color: red; } body.blue { background-color: blue; } </style>"];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));

    [underPageBackgroundColorObserver setState:@"before-css-class-added"];
    [webView objectByEvaluatingJavaScript:@"document.body.className = 'red'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-css-class-added", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView setUnderPageBackgroundColor:[WebCore::CocoaColor colorWithCGColor:redColor.get()]];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView setUnderPageBackgroundColor:nil];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));

    [underPageBackgroundColorObserver setState:@"before-css-class-changed"];
    [webView objectByEvaluatingJavaScript:@"document.body.className = 'blue'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-css-class-changed", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, blueColor.get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView setUnderPageBackgroundColor:[WebCore::CocoaColor colorWithCGColor:blueColor.get()]];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, blueColor.get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView setUnderPageBackgroundColor:nil];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, blueColor.get()));

    [underPageBackgroundColorObserver setState:@"before-css-class-removed"];
    [webView objectByEvaluatingJavaScript:@"document.body.className = ''"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-css-class-removed", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView setUnderPageBackgroundColor:[WebCore::CocoaColor colorWithCGColor:defaultBackgroundColor().get()]];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView setUnderPageBackgroundColor:nil];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));

    [underPageBackgroundColorObserver setState:@"before-nonnull-override"];
    [webView setUnderPageBackgroundColor:[WebCore::CocoaColor colorWithCGColor:redColor.get()]];
    EXPECT_NSSTRING_EQ("after-nonnull-override", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView setUnderPageBackgroundColor:[WebCore::CocoaColor colorWithCGColor:redColor.get()]];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView objectByEvaluatingJavaScript:@"document.body.className = 'red'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView objectByEvaluatingJavaScript:@"document.body.className = 'blue'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));

    [underPageBackgroundColorObserver setState:@"should-not-change"];
    [webView objectByEvaluatingJavaScript:@"document.body.className = ''"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("should-not-change", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));

    [underPageBackgroundColorObserver setState:@"before-null-override"];
    [webView setUnderPageBackgroundColor:nil];
    EXPECT_NSSTRING_EQ("after-null-override", [underPageBackgroundColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));
}

#if PLATFORM(IOS_FAMILY)

constexpr CGFloat whiteColorComponents[4] = { 1, 1, 1, 1 };

// There's no API/SPI to get the background color of the scroll area on macOS.

TEST(WKWebViewUnderPageBackgroundColor, MatchesScrollView)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    auto blueColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), blueColorComponents));
    auto whiteColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), whiteColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, defaultBackgroundColor().get()));
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, defaultBackgroundColor().get()));

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: red; } </style>"];
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, redColor.get()));
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, redColor.get()));
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleWhite);

    [webView setUnderPageBackgroundColor:[WebCore::CocoaColor colorWithCGColor:blueColor.get()]];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, blueColor.get()));
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, blueColor.get()));
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleWhite);

    [webView setUnderPageBackgroundColor:[WebCore::CocoaColor colorWithCGColor:whiteColor.get()]];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(CGColorEqualToColor([webView underPageBackgroundColor].CGColor, whiteColor.get()));
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, whiteColor.get()));
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleWhite);
}

#endif // PLATFORM(IOS_FAMILY)

