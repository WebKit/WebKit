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

#if PLATFORM(MAC)
#import <pal/spi/mac/NSColorSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"
#import <UIKit/UIKit.h>

@interface UIColor (TestWebKitAPI_SystemColors)

- (void)expectRed:(uint8_t)expectedRed green:(uint8_t)expectedGreen blue:(uint8_t)expectedBlue alpha:(CGFloat)expectedAlpha;

@end

@implementation UIColor (TestWebKitAPI_SystemColors)

- (void)expectRed:(uint8_t)expectedRed green:(uint8_t)expectedGreen blue:(uint8_t)expectedBlue alpha:(CGFloat)expectedAlpha
{
    CGFloat red, green, blue, alpha;
    [self getRed:&red green:&green blue:&blue alpha:&alpha];

    EXPECT_EQ(static_cast<uint8_t>(red * 255), expectedRed);
    EXPECT_EQ(static_cast<uint8_t>(green * 255), expectedGreen);
    EXPECT_EQ(static_cast<uint8_t>(blue * 255), expectedBlue);
    EXPECT_EQ(alpha, expectedAlpha);
}

@end

#endif // PLATFORM(IOS_FAMILY)

namespace TestWebKitAPI {

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
#endif

#if PLATFORM(IOS_FAMILY)

TEST(WebKit, TintColorAffectsInteractionColor)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto textInput = [webView textInputContentView];

    [webView setTintColor:[UIColor greenColor]];
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "  <head>"
        "    <style>div { border: 1px solid tomato; }</style>"
        "  </head>"
        "  <body>"
        "    <div id='default-style' contenteditable></div>"
        "    <div id='caret-color' style='caret-color: red;' contenteditable></div>"
        "    <div id='color' style='color: red;' contenteditable></div>"
        "  </body>"
        "</html>"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById('default-style').focus()"];
    [webView waitForNextPresentationUpdate];
    [textInput.insertionPointColor expectRed:0 green:255 blue:0 alpha:1];
    [textInput.selectionBarColor expectRed:0 green:255 blue:0 alpha:1];

    [webView stringByEvaluatingJavaScript:@"document.getElementById('caret-color').focus()"];
    [webView waitForNextPresentationUpdate];
    [textInput.insertionPointColor expectRed:255 green:0 blue:0 alpha:1];
    [textInput.selectionBarColor expectRed:255 green:0 blue:0 alpha:1];

    [webView stringByEvaluatingJavaScript:@"document.getElementById('color').focus()"];
    [webView waitForNextPresentationUpdate];
    [textInput.insertionPointColor expectRed:0 green:255 blue:0 alpha:1];
    [textInput.selectionBarColor expectRed:0 green:255 blue:0 alpha:1];
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace TestWebKitAPI
