/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKApplicationManifest.h>
#import <WebKit/_WKFeature.h>
#import <wtf/RetainPtr.h>

#define EXPECT_NSSTRING_EQ(expected, actual) \
    EXPECT_TRUE([actual isKindOfClass:[NSString class]]); \
    EXPECT_WK_STREQ(expected, (NSString *)actual);

TEST(WKWebViewThemeColor, MetaElementValidNameAndColor)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='theme-color' content='red'>"];

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));
}

TEST(WKWebViewThemeColor, MetaElementValidNameAndColorAndMedia)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='theme-color' content='red' media='screen'>"];

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));
}

TEST(WKWebViewThemeColor, MetaElementInvalidName)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='not-theme-color' content='blue'><meta name='theme-color' content='red'>"];

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));
}

TEST(WKWebViewThemeColor, MetaElementInvalidColor)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='theme-color' content='invalid'><meta name='theme-color' content='red'>"];

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));
}

TEST(WKWebViewThemeColor, MetaElementInvalidMedia)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='theme-color' content='blue' media='invalid'><meta name='theme-color' content='red' media='screen'>"];

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));
}

TEST(WKWebViewThemeColor, MetaElementMultipleValid)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<div><meta name='theme-color' content='red'></div><meta name='theme-color' content='blue'>"];

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));
}

TEST(WKWebViewThemeColor, MetaElementValidSubframe)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<iframe srcdoc=\"<meta name='theme-color' content='blue'>\"></iframe><meta name='theme-color' content='red'>"];

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));
}

@interface WKWebViewThemeColorObserver : NSObject

- (instancetype)initWithWebView:(WKWebView *)webView;

@property (nonatomic, readonly) WKWebView *webView;
@property (nonatomic, copy) NSString *state;

@end

@implementation WKWebViewThemeColorObserver

- (instancetype)initWithWebView:(WKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _state = @"before-init";

    _webView = webView;
    [_webView addObserver:self forKeyPath:@"themeColor" options:NSKeyValueObservingOptionInitial context:nil];

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

    if ([_state isEqualToString:@"before-content-change-invalid"]) {
        _state = @"after-content-change-invalid";
        return;
    }

    if ([_state isEqualToString:@"before-content-change-valid"]) {
        _state = @"after-content-change-valid";
        return;
    }

    if ([_state isEqualToString:@"before-name-change-not-theme-color"]) {
        _state = @"after-name-change-not-theme-color";
        return;
    }

    if ([_state isEqualToString:@"before-name-change-theme-color"]) {
        _state = @"after-name-change-theme-color";
        return;
    }

    if ([_state isEqualToString:@"before-invalid-media-added"]) {
        _state = @"after-invalid-media-added";
        return;
    }

    if ([_state isEqualToString:@"before-media-changed-valid"]) {
        _state = @"after-media-changed-valid";
        return;
    }

    if ([_state isEqualToString:@"before-media-state-changed"]) {
        _state = @"after-media-state-changed";
        return;
    }

    if ([_state isEqualToString:@"before-node-inserted"]) {
        _state = @"after-node-inserted";
        return;
    }

    if ([_state isEqualToString:@"before-node-removed"]) {
        _state = @"after-node-removed";
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

@end

TEST(WKWebViewThemeColor, KVO)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    auto blueColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), blueColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto themeColorObserver = adoptNS([[WKWebViewThemeColorObserver alloc] initWithWebView:webView.get()]);
    EXPECT_NSSTRING_EQ("after-init", [themeColorObserver state]);
    EXPECT_TRUE(![webView themeColor]);

    [themeColorObserver setState:@"before-load"];
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='theme-color' content='red'>"];
    EXPECT_NSSTRING_EQ("after-load", [themeColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));

    [themeColorObserver setState:@"before-content-change-invalid"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('meta').setAttribute('content', 'invalid')"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-content-change-invalid", [themeColorObserver state]);
    EXPECT_TRUE(![webView themeColor]);

    [themeColorObserver setState:@"before-content-change-valid"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('meta').setAttribute('content', 'blue')"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-content-change-valid", [themeColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, blueColor.get()));

    [themeColorObserver setState:@"before-name-change-not-theme-color"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('meta').setAttribute('name', 'not-theme-color')"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-name-change-not-theme-color", [themeColorObserver state]);
    EXPECT_TRUE(![webView themeColor]);

    [themeColorObserver setState:@"before-name-change-theme-color"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('meta').setAttribute('name', 'theme-color')"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-name-change-theme-color", [themeColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, blueColor.get()));

    [themeColorObserver setState:@"before-invalid-media-added"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('meta').setAttribute('media', 'invalid')"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-invalid-media-added", [themeColorObserver state]);
    EXPECT_TRUE(![webView themeColor]);

    [themeColorObserver setState:@"before-media-changed-valid"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('meta').setAttribute('media', 'screen')"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-media-changed-valid", [themeColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, blueColor.get()));

    [themeColorObserver setState:@"before-media-state-changed"];
    [webView setMediaType:@"print"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-media-state-changed", [themeColorObserver state]);
    EXPECT_TRUE(![webView themeColor]);

    [themeColorObserver setState:@"before-node-inserted"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('meta').insertAdjacentHTML('beforebegin', \"<meta name='theme-color' content='red'>\")"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-node-inserted", [themeColorObserver state]);
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));

    [themeColorObserver setState:@"before-node-removed"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('meta').remove()"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-node-removed", [themeColorObserver state]);
    EXPECT_TRUE(![webView themeColor]);
}

#if ENABLE(APPLICATION_MANIFEST)

TEST(WKWebViewThemeColor, ApplicationManifest)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    NSDictionary *manifestObject = @{ @"name": @"Test", @"theme_color": @"red" };
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:[NSString stringWithFormat:@"<link rel='manifest' href='data:application/manifest+json;charset=utf-8;base64,%@'>", [[NSJSONSerialization dataWithJSONObject:manifestObject options:0 error:nil] base64EncodedStringWithOptions:0]]];

    static bool didGetApplicationManifest = false;
    [webView _getApplicationManifestWithCompletionHandler:[&] (_WKApplicationManifest *manifest) {
        EXPECT_TRUE(CGColorEqualToColor(manifest.themeColor.CGColor, redColor.get()));

        didGetApplicationManifest = true;
    }];
    TestWebKitAPI::Util::run(&didGetApplicationManifest);

    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));
}

TEST(WKWebViewThemeColor, MetaElementOverridesApplicationManifest)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    NSDictionary *manifestObject = @{ @"name": @"Test", @"theme_color": @"blue" };
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:[NSString stringWithFormat:@"<link rel='manifest' href='data:application/manifest+json;charset=utf-8;base64,%@'><meta name='theme-color' content='red'>", [[NSJSONSerialization dataWithJSONObject:manifestObject options:0 error:nil] base64EncodedStringWithOptions:0]]];

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));
}

#endif // ENABLE(APPLICATION_MANIFEST)
