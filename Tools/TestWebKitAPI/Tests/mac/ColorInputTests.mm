/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(ColorInputTests, SetColorUsingColorPicker)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<input type='color' id='color' style='width: 200px; height: 200px;'>"];

    [webView sendClickAtPoint:NSMakePoint(50, 350)];
    [webView waitForNextPresentationUpdate];

    [webView _setSelectedColorForColorPicker:NSColor.redColor];
    [webView waitForNextPresentationUpdate];

    NSString *colorValue = [webView stringByEvaluatingJavaScript:@"document.getElementById('color').value"];

    EXPECT_WK_STREQ(colorValue, "#ff0000");
}

TEST(ColorInputTests, SetColorWithAlphaUsingColorPicker)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<input type='color' id='color' style='width: 200px; height: 200px;'>"];

    [webView sendClickAtPoint:NSMakePoint(50, 350)];
    [webView waitForNextPresentationUpdate];

    NSColor *color = [NSColor colorWithRed:0 green:0 blue:1 alpha:0.5];
    [webView _setSelectedColorForColorPicker:color];
    [webView waitForNextPresentationUpdate];

    NSString *colorValue = [webView stringByEvaluatingJavaScript:@"document.getElementById('color').value"];

    EXPECT_WK_STREQ(colorValue, "#0000ff");
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
