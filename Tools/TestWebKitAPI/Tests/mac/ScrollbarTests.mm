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

#if PLATFORM(MAC)

#import "CGImagePixelReader.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/Color.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static double scrollbarLuminanceForWebView(WKWebView *webView)
{
    RetainPtr snapshotImage = adoptNS([webView _windowSnapshotInRect:CGRectNull withOptions:0]);
    RetainPtr snapshotCGImage = [snapshotImage CGImageForProposedRect:NULL context:nil hints:nil];
    CGImagePixelReader reader { snapshotCGImage.get()  };

    auto scrollbarTrackColor = reader.at(reader.width() - 10, reader.height() - 10);
    return scrollbarTrackColor.luminance();
}

TEST(ScrollbarTests, AppearanceChangeAfterSystemAppearanceChange)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
    [webView synchronouslyLoadHTMLString:@"<head><meta name='color-scheme' content='dark light'></head><body style='height: 2000px;'></body>"];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_GT(scrollbarLuminanceForWebView(webView.get()), 0.5f);

    [webView setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_LT(scrollbarLuminanceForWebView(webView.get()), 0.5f);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
