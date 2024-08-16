/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "CGImagePixelReader.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebCore/ColorSerialization.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WKWebView, WebViewSnapshot)
{
    auto webViewFrame = CGRectMake(0, 0, 800, 600);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:webViewFrame]);
    [webView synchronouslyLoadTestPageNamed:@"checkered-background"];

    auto *window = [webView hostWindow];
    [window setIsVisible:YES];
    [window makeKeyWindow];

    auto testImageColors = [&] (WebCore::Color color1, WebCore::Color color2, unsigned size) {

        NSString *script = [NSString stringWithFormat:@"setChecker('%@', '%@', %d)",
            (NSString *)serializationForCSS(color1), (NSString *)serializationForCSS(color2), size];
        [webView stringByEvaluatingJavaScript:script];

        [webView waitForNextPresentationUpdate];
        RetainPtr nsSnapshot = [webView _windowSnapshotInRect:CGRectNull];
        ASSERT_NE(nsSnapshot.get(), nil);
        if (!nsSnapshot)
            return;

        RetainPtr cgSnapshot = [nsSnapshot.get() CGImageForProposedRect:nullptr context:nil hints:nil];
        CGImagePixelReader pixelReader { cgSnapshot.get() };

        auto midpoint = size / 2;
        EXPECT_EQ(pixelReader.at(midpoint, midpoint), color2) << "color2";
        EXPECT_EQ(pixelReader.at(midpoint, midpoint + size), color1) << "color1";
    };

    testImageColors(WebCore::Color::white, WebCore::Color::black, 40);
    testImageColors(WebCore::Color::lightGray, WebCore::Color::gray, 60);
    testImageColors(WebCore::Color::gray, WebCore::Color::lightGray, 200);
}

} // namespace TestWebKitAPI
