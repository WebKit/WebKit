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

#import "config.h"

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebCore/Color.h>
#import <WebCore/ColorSerialization.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/TextStream.h>

#if PLATFORM(COCOA)
#import <WebCore/ColorCocoa.h>
#endif

#if PLATFORM(MAC)
#import <WebCore/ColorMac.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <pal/spi/mac/NSColorSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <UIKit/UIKit.h>
#endif

namespace TestWebKitAPI {

#if USE(APPKIT)

static WebCore::Color getColorAtPointInWebView(TestWKWebView *webView, NSInteger x, NSInteger y)
{
    WebCore::Color color;

    CGFloat viewWidth = CGRectGetWidth([webView frame]);
    CGFloat viewHeight = CGRectGetHeight([webView frame]);
    CGFloat backingScaleFactor = [[webView hostWindow] backingScaleFactor];

    bool isDone = false;

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:[&](NSImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);

        CGImageRef cgImage = [snapshotImage CGImageForProposedRect:nil context:nil hints:nil];
        auto colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());

        NSInteger viewWidthInPixels = viewWidth * backingScaleFactor;
        NSInteger viewHeightInPixels = viewHeight * backingScaleFactor;

        uint8_t *rgba = (unsigned char *)calloc(viewWidthInPixels * viewHeightInPixels * 4, sizeof(unsigned char));
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, viewWidthInPixels, viewHeightInPixels), cgImage);

        NSInteger (^getPixelIndex)(NSInteger, NSInteger, NSInteger) = ^(NSInteger x, NSInteger y, NSInteger width) {
            return (y * width + x) * 4;
        };

        NSInteger pixelIndex = getPixelIndex(x * backingScaleFactor, y * backingScaleFactor, viewWidthInPixels);
        color = { WebCore::SRGBA<uint8_t> { rgba[pixelIndex], rgba[pixelIndex + 1], rgba[pixelIndex + 2], rgba[pixelIndex + 3] } };

        free(rgba);

        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    return color;
}

#endif

TEST(WebKit, LinkColor)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<a href>Test</a>"];

    NSString *linkColor = [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.links[0]).color"];
    EXPECT_WK_STREQ("rgb(0, 0, 238)", linkColor);
}

#if PLATFORM(MAC)
TEST(WebKit, LinkColorWithSystemAppearance)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setUseSystemAppearance:YES];

    [webView synchronouslyLoadHTMLString:@"<a href>Test</a>"];

    NSColor *linkColor = [NSColor.linkColor colorUsingColorSpace:NSColorSpace.sRGBColorSpace];

    CGFloat red = linkColor.redComponent * 255;
    CGFloat green = linkColor.greenComponent * 255;
    CGFloat blue = linkColor.blueComponent * 255;

    NSString *expectedString = [NSString stringWithFormat:@"rgb(%.0f, %.0f, %.0f)", red, green, blue];

    NSString *cssLinkColor = [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.links[0]).color"];
    EXPECT_WK_STREQ(expectedString.UTF8String, cssLinkColor);
}

TEST(WebKit, AppAccentColorToggleAppearance)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setUseSystemAppearance:YES];

    [webView synchronouslyLoadHTMLString:@"<body style='color: -apple-system-control-accent;'>Test</body>"];

    NSColor *originalAccentColor = [NSApp _effectiveAccentColor];
    NSColor *newAccentColor = [NSColor systemPurpleColor];

    [NSApp _setAccentColor:newAccentColor];

    auto [lightColor, darkColor] = WebCore::lightAndDarkColorsFromNSColor(newAccentColor);

    NSAppearance *lightAppearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    [webView setAppearance:lightAppearance];
    NSString *lightCSSColor = [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.body).color"];
    EXPECT_WK_STREQ(WebCore::serializationForCSS(lightColor), lightCSSColor);

    NSAppearance *darkAppearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    [webView setAppearance:darkAppearance];
    NSString *darkCSSColor = [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.body).color"];
    EXPECT_WK_STREQ(WebCore::serializationForCSS(darkColor), darkCSSColor);

    [NSApp _setAccentColor:originalAccentColor];
}

TEST(WebKit, AppAccentColorWithColorScheme)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [[webView hostWindow] makeKeyAndOrderFront:nil];

    [webView _setUseSystemAppearance:YES];
    [webView synchronouslyLoadHTMLString:@"<body style='background: white'><div style='background: -apple-system-selected-text-background;'>________</div></body>"];

    NSAppearance *originalAppearance = [webView appearance];
    NSColor *originalAccentColor = [NSApp _effectiveAccentColor];
    NSColor *newAccentColor = [NSColor systemPurpleColor];

    [NSApp _setAccentColor:newAccentColor];

    NSAppearance *lightAppearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    [webView setAppearance:lightAppearance];
    auto lightSelectionColor = getColorAtPointInWebView(webView.get(), 20, 20);

    NSAppearance *darkAppearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    [webView setAppearance:darkAppearance];
    auto darkSelectionColor = getColorAtPointInWebView(webView.get(), 20, 20);

    [webView setAppearance:originalAppearance];
    [webView _setUseSystemAppearance:NO];
    [webView synchronouslyLoadHTMLString:@"<body contenteditable><div style='color-scheme: light;'>________</div><div style='color-scheme: dark;'>________</div></body>"];

    [webView stringByEvaluatingJavaScript:@"document.body.focus(); window.getSelection().selectAllChildren(document.body);"];

    auto lightSelectionColorObserved = getColorAtPointInWebView(webView.get(), 20, 20);
    auto darkSelectionColorObserved = getColorAtPointInWebView(webView.get(), 20, 40);

    ALWAYS_LOG_WITH_STREAM(stream << "<SYSTEM-COLOR-TEST>: lightSelectionColor: " << lightSelectionColor);
    ALWAYS_LOG_WITH_STREAM(stream << "<SYSTEM-COLOR-TEST>: lightSelectionColorObserved: " << lightSelectionColorObserved);
    ALWAYS_LOG_WITH_STREAM(stream << "<SYSTEM-COLOR-TEST>: darkSelectionColor: " << darkSelectionColor);
    ALWAYS_LOG_WITH_STREAM(stream << "<SYSTEM-COLOR-TEST>: darkSelectionColorObserved: " << darkSelectionColorObserved);

    EXPECT_EQ(lightSelectionColor, lightSelectionColorObserved);
    EXPECT_EQ(darkSelectionColor, darkSelectionColorObserved);

    [NSApp _setAccentColor:originalAccentColor];
}
#endif

#if PLATFORM(IOS_FAMILY)
TEST(WebKit, TintColorAffectsInteractionColor)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setTintColor:[UIColor greenColor]];
    [webView synchronouslyLoadHTMLString:@"<body contenteditable></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    UIView<UITextInputTraits_Private> *textInput = (UIView<UITextInputTraits_Private> *) [webView textInputContentView];
    EXPECT_TRUE([textInput.insertionPointColor isEqual:[UIColor greenColor]]);
    EXPECT_TRUE([textInput.selectionBarColor isEqual:[UIColor greenColor]]);
}
#endif

} // namespace TestWebKitAPI
