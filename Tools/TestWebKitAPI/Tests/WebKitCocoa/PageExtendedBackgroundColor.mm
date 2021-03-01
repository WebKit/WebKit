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

#import "CocoaColor.h"
#import "TestCocoa.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#define EXPECT_NSSTRING_EQ(expected, actual) \
    EXPECT_TRUE([actual isKindOfClass:[NSString class]]); \
    EXPECT_WK_STREQ(expected, (NSString *)actual);

constexpr CGFloat whiteColorComponents[4] = { 1, 1, 1, 1 };
constexpr CGFloat redColorComponents[4] = { 1, 0, 0, 1 };
constexpr CGFloat blueColorComponents[4] = { 0, 0, 1, 1 };

TEST(PageExtendedBackgroundColor, OnLoad)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto whiteColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), whiteColorComponents));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView _pageExtendedBackgroundColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: red; } </style>"];

    EXPECT_TRUE(CGColorEqualToColor([webView _pageExtendedBackgroundColor].CGColor, redColor.get()));

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { not-background-color: red; } </style>"];

    EXPECT_TRUE(CGColorEqualToColor([webView _pageExtendedBackgroundColor].CGColor, whiteColor.get()));
}

TEST(PageExtendedBackgroundColor, MultipleStyles)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto blueColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), blueColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView _pageExtendedBackgroundColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: red; } body { background-color: blue; } </style>"];

    EXPECT_TRUE(CGColorEqualToColor([webView _pageExtendedBackgroundColor].CGColor, blueColor.get()));
}

@interface WKWebViewPageExtendedBackgroundColorObserver : NSObject

- (instancetype)initWithWebView:(WKWebView *)webView;

@property (nonatomic, readonly) WKWebView *webView;
@property (nonatomic, copy) NSString *state;

@end

@implementation WKWebViewPageExtendedBackgroundColorObserver

- (instancetype)initWithWebView:(WKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _state = @"before-init";

    _webView = webView;
    [_webView addObserver:self forKeyPath:@"_pageExtendedBackgroundColor" options:NSKeyValueObservingOptionInitial context:nil];

    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if ([_state isEqualToString:@"before-init"]) {
        _state = @"after-init";
        return;
    }

    if ([_state isEqualToString:@"before-load"]) {
        _state = @"after-load";
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

    RELEASE_ASSERT_NOT_REACHED();
}

@end

TEST(PageExtendedBackgroundColor, KVO)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto whiteColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), whiteColorComponents));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    auto blueColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), blueColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto themeColorObserver = adoptNS([[WKWebViewPageExtendedBackgroundColorObserver alloc] initWithWebView:webView.get()]);
    EXPECT_NSSTRING_EQ("after-init", [themeColorObserver state]);
    EXPECT_TRUE(![webView _pageExtendedBackgroundColor]);

    [themeColorObserver setState:@"before-load"];
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body.red { background-color: red; } body.blue { background-color: blue; } </style>"];
    EXPECT_NSSTRING_EQ("after-load", [themeColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView _pageExtendedBackgroundColor].CGColor, whiteColor.get()));

    [themeColorObserver setState:@"before-css-class-added"];
    [webView objectByEvaluatingJavaScript:@"document.body.className = 'red'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-css-class-added", [themeColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView _pageExtendedBackgroundColor].CGColor, redColor.get()));

    [themeColorObserver setState:@"before-css-class-changed"];
    [webView objectByEvaluatingJavaScript:@"document.body.className = 'blue'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-css-class-changed", [themeColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView _pageExtendedBackgroundColor].CGColor, blueColor.get()));

    [themeColorObserver setState:@"before-css-class-removed"];
    [webView objectByEvaluatingJavaScript:@"document.body.className = ''"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-css-class-removed", [themeColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView _pageExtendedBackgroundColor].CGColor, whiteColor.get()));
}
