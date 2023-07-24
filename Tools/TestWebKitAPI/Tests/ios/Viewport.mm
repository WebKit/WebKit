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

#if PLATFORM(IOS_FAMILY)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UserInterfaceSwizzler.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static inline String makeViewportMetaTag(unsigned viewportWidth, float initialScale)
{
    return makeString("<meta name='viewport' content='width=", viewportWidth, ", initial-scale=", initialScale, "'>");
}

TEST(Viewport, MinimumEffectiveDeviceWidthWithInitialScale)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 690, 690)]);

    [webView synchronouslyLoadHTMLString:makeViewportMetaTag(1000, 0.69)];
    [webView waitForNextPresentationUpdate];

    [webView _setMinimumEffectiveDeviceWidth:1024];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"window.innerWidth"]).floatValue, 1024);
}

TEST(Viewport, RespectInitialScaleExceptOnWikipediaDomain)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto screenDimensions = CGRectMake(0, 0, 2732, 2048);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:screenDimensions]);
    [[webView configuration] preferences]._shouldIgnoreMetaViewport = YES;
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto runTest = [&webView, &navigationDelegate, screenWidth = screenDimensions.size.width] (StringView requestURL, Function<void(float, float)>&& scaleExpectation) mutable {
        bool done = false;
        constexpr auto viewportWidth = 1000;
        auto expectedViewportScale = screenWidth / viewportWidth;

        auto request = [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithUTF8String:requestURL.utf8().data()]]];
        auto response = makeViewportMetaTag(viewportWidth, expectedViewportScale);
        [webView loadSimulatedRequest:request responseHTMLString:response];
        [navigationDelegate waitForDidFinishNavigation];
        [webView _doAfterNextPresentationUpdate:makeBlockPtr([&done, &webView, expectedViewportScale, scaleExpectation = WTFMove(scaleExpectation)]() mutable {
            [webView evaluateJavaScript:@"visualViewport.scale" completionHandler:makeBlockPtr([&done, expectedViewportScale, scaleExpectation = WTFMove(scaleExpectation)] (NSNumber *value, NSError *error) mutable {
                auto actualViewportScale = value.floatValue;
                scaleExpectation(actualViewportScale, expectedViewportScale);
                done = true;
            }).get()];
        }).get()];

        TestWebKitAPI::Util::run(&done);
        done = false;
    };

    runTest("https://en.wikipedia.org/path1"_s, [] (float actualViewportScale, float expectedViewportScale) {
        EXPECT_LT(actualViewportScale, expectedViewportScale);
    });
    runTest("https://webkit.org/path2"_s, [] (float actualViewportScale, float expectedViewportScale) {
        EXPECT_EQ(actualViewportScale, expectedViewportScale);
    });
}

}

#endif
