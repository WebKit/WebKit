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

#import "CocoaColor.h"
#import "TestCocoa.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKApplicationManifest.h>
#import <WebKit/_WKInternalDebugFeature.h>
#import <wtf/RetainPtr.h>

#define EXPECT_NSSTRING_EQ(expected, actual) \
    EXPECT_TRUE([actual isKindOfClass:[NSString class]]); \
    EXPECT_WK_STREQ(expected, (NSString *)actual);

constexpr CGFloat redColorComponents[4] = { 1, 0, 0, 1 };
constexpr CGFloat blueColorComponents[4] = { 0, 0, 1, 1 };

TEST(WKWebViewThemeColor, MetaElementOnLoad)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='theme-color' content='red'>"];

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, redColor.get()));

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='not-theme-color' content='red'>"];

    EXPECT_TRUE(![webView themeColor]);
}

TEST(WKWebViewThemeColor, MultipleMetaElements)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_TRUE(![webView themeColor]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='theme-color' content='red'><meta name='theme-color' content='blue'>"];

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto blueColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), blueColorComponents));
    EXPECT_TRUE(CGColorEqualToColor([webView themeColor].CGColor, blueColor.get()));
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

    if ([_state isEqualToString:@"before-content-change"]) {
        _state = @"after-content-change";
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

    [themeColorObserver setState:@"before-content-change"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('meta').setAttribute('content', 'blue')"];
    [webView waitForNextPresentationUpdate];
    EXPECT_NSSTRING_EQ("after-content-change", [themeColorObserver state]);
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

#if PLATFORM(IOS_FAMILY)

// There's no API/SPI to get the background color of the scroll area on macOS.

enum class UseThemeColorForScrollAreaBackgroundColor : bool { Yes, No };
static RetainPtr<TestWKWebView> createWebView(UseThemeColorForScrollAreaBackgroundColor useThemeColorForScrollAreaBackgroundColor)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"UseThemeColorForScrollAreaBackgroundColor"]) {
            [[configuration preferences] _setEnabled:(useThemeColorForScrollAreaBackgroundColor == UseThemeColorForScrollAreaBackgroundColor::Yes) forInternalDebugFeature:feature];
            break;
        }
    }

    return adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
}

TEST(WKWebViewThemeColor, ExperimentalUseThemeColorForScrollAreaBackgroundColor)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    auto blueColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), blueColorComponents));

    auto webViewWithoutThemeColorForScrollAreaBackgroundColor = createWebView(UseThemeColorForScrollAreaBackgroundColor::No);
    EXPECT_TRUE(![webViewWithoutThemeColorForScrollAreaBackgroundColor themeColor]);

    [webViewWithoutThemeColorForScrollAreaBackgroundColor synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body style='background-color: blue'>"];
    EXPECT_TRUE(![webViewWithoutThemeColorForScrollAreaBackgroundColor themeColor]);
    EXPECT_TRUE(CGColorEqualToColor([webViewWithoutThemeColorForScrollAreaBackgroundColor scrollView].backgroundColor.CGColor, blueColor.get()));

    [webViewWithoutThemeColorForScrollAreaBackgroundColor synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='theme-color' content='red'><body style='background-color: blue'>"];
    EXPECT_TRUE(CGColorEqualToColor([webViewWithoutThemeColorForScrollAreaBackgroundColor themeColor].CGColor, redColor.get()));
    EXPECT_TRUE(CGColorEqualToColor([webViewWithoutThemeColorForScrollAreaBackgroundColor scrollView].backgroundColor.CGColor, blueColor.get()));

    auto webViewWithThemeColorForScrollAreaBackgroundColor = createWebView(UseThemeColorForScrollAreaBackgroundColor::Yes);
    EXPECT_TRUE(![webViewWithThemeColorForScrollAreaBackgroundColor themeColor]);

    [webViewWithThemeColorForScrollAreaBackgroundColor synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body style='background-color: blue'>"];
    EXPECT_TRUE(![webViewWithThemeColorForScrollAreaBackgroundColor themeColor]);
    EXPECT_TRUE(CGColorEqualToColor([webViewWithThemeColorForScrollAreaBackgroundColor scrollView].backgroundColor.CGColor, blueColor.get()));

    [webViewWithThemeColorForScrollAreaBackgroundColor synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<meta name='theme-color' content='red'><body style='background-color: blue'>"];
    EXPECT_TRUE(CGColorEqualToColor([webViewWithThemeColorForScrollAreaBackgroundColor themeColor].CGColor, redColor.get()));
    EXPECT_TRUE(CGColorEqualToColor([webViewWithThemeColorForScrollAreaBackgroundColor scrollView].backgroundColor.CGColor, redColor.get()));
}

#endif // PLATFORM(IOS_FAMILY)
