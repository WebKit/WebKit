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
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

#if ENABLE(DARK_MODE_CSS)
#define DEFAULT_BACKGROUND_COLOR [NSColor controlBackgroundColor]
#else
#define DEFAULT_BACKGROUND_COLOR [NSColor whiteColor]
#endif

TEST(WebKit, BackgroundColorDefault)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    NSColor *defaultColor = DEFAULT_BACKGROUND_COLOR;
    NSColor *backgroundColor = [webView _backgroundColor];
    EXPECT_EQ(defaultColor, backgroundColor);

    // Load content so the layer is created.
    [webView synchronouslyLoadHTMLString:@""];

    EXPECT_TRUE(CGColorEqualToColor(defaultColor.CGColor, [webView layer].backgroundColor));
}

TEST(WebKit, BackgroundColorSystemColor)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    NSColor *systemColor = [NSColor textBackgroundColor];
    [webView _setBackgroundColor:systemColor];

    NSColor *backgroundColor = [webView _backgroundColor];
    EXPECT_EQ(systemColor, backgroundColor);

    // Load content so the layer is created.
    [webView synchronouslyLoadHTMLString:@""];

    EXPECT_TRUE(CGColorEqualToColor(systemColor.CGColor, [webView layer].backgroundColor));
}

TEST(WebKit, BackgroundColorNil)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView _setBackgroundColor:nil];

    NSColor *defaultColor = DEFAULT_BACKGROUND_COLOR;
    NSColor *backgroundColor = [webView _backgroundColor];
    EXPECT_EQ(defaultColor, backgroundColor);

    // Load content so the layer is created.
    [webView synchronouslyLoadHTMLString:@""];

    EXPECT_TRUE(CGColorEqualToColor(defaultColor.CGColor, [webView layer].backgroundColor));
}

TEST(WebKit, BackgroundColorNoDrawsBackground)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView _setDrawsBackground:NO];

    NSColor *defaultColor = DEFAULT_BACKGROUND_COLOR;
    NSColor *backgroundColor = [webView _backgroundColor];
    EXPECT_EQ(defaultColor, backgroundColor);

    // Load content so the layer is created.
    [webView synchronouslyLoadHTMLString:@""];

    EXPECT_EQ(CGColorGetConstantColor(kCGColorClear), [webView layer].backgroundColor);
}

TEST(WebKit, BackgroundColorCustomColorNoDrawsBackground)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView _setDrawsBackground:NO];
    [webView _setBackgroundColor:[NSColor textBackgroundColor]];

    NSColor *backgroundColor = [webView _backgroundColor];
    EXPECT_EQ([NSColor textBackgroundColor], backgroundColor);

    // Load content so the layer is created.
    [webView synchronouslyLoadHTMLString:@""];

    EXPECT_EQ(CGColorGetConstantColor(kCGColorClear), [webView layer].backgroundColor);
}

TEST(WebKit, BackgroundColorToggleAppearance)
{
    const NSInteger width = 800;
    const NSInteger height = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, width, height)]);

    const auto& checkWhitePixel = [&] {
        uint8_t* pixelBuffer = static_cast<uint8_t*>(calloc(width * height, 4));
        auto colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
        auto context = adoptCF(CGBitmapContextCreate(pixelBuffer, width, height, 8, 4 * width, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));

        [[webView layer] renderInContext:context.get()];

        EXPECT_EQ(255, pixelBuffer[0]);
        EXPECT_EQ(255, pixelBuffer[1]);
        EXPECT_EQ(255, pixelBuffer[2]);

        free(pixelBuffer);
    };

    [webView _setUseSystemAppearance:YES];
    [webView setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
    [webView synchronouslyLoadHTMLString:@""];

    checkWhitePixel();

    [webView _setDrawsBackground:NO];
    [webView setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    [webView _setDrawsBackground:YES];
    [webView setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];

    [webView waitForNextPresentationUpdate];

    checkWhitePixel();
}

} // namespace TestWebKitAPI
