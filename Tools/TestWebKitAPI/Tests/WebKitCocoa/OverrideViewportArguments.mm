/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferences.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)

TEST(WebKit, OverrideViewportArguments)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 20, 20)]);

    auto bodyWidth = ^{
        return [webView stringByEvaluatingJavaScript:@"document.body.clientWidth"];
    };

    auto setViewport = ^(NSDictionary *viewport) {
        [webView _overrideViewportWithArguments:viewport];
        [webView waitForNextPresentationUpdate];
    };

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='initial-scale=1'><div id='divWithViewportUnits' style='width: 100vw;'></div>"];
    EXPECT_WK_STREQ("20", bodyWidth());

    setViewport(@{ @"width" : @"1000" });
    EXPECT_WK_STREQ("1000", bodyWidth());

    setViewport(@{ @"width" : @"1000", @"initial-scale": @"1" });
    EXPECT_WK_STREQ("1000", bodyWidth());
    EXPECT_EQ(1., [webView scrollView].zoomScale);

    setViewport(@{ @"width" : @"1000", @"initial-scale": @"5" });
    EXPECT_WK_STREQ("1000", bodyWidth());
    EXPECT_EQ(5., [webView scrollView].zoomScale);

    setViewport(nil);
    EXPECT_WK_STREQ("20", bodyWidth());

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=10'><div id='divWithViewportUnits' style='width: 100vw;'></div>"];
    EXPECT_WK_STREQ("10", bodyWidth());

    setViewport(@{ @"width" : @"1000", @"initial-scale": @"1" });
    EXPECT_WK_STREQ("1000", bodyWidth());

    setViewport(@{ @"width" : @"device-width", @"initial-scale": @"1" });
    EXPECT_WK_STREQ("20", bodyWidth());

    setViewport(@{ @"width" : @"500", @"initial-scale": @"1", @"garbage": @"nonsense" });
    EXPECT_WK_STREQ("500", bodyWidth());

    // Call overrideViewportWithArguments before loading anything in the
    // view and ensure it is respected.
    webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 20, 20)]);
    setViewport(@{ @"width" : @"1000", @"initial-scale": @"1" });
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='initial-scale=1'><div id='divWithViewportUnits' style='width: 100vw;'></div>"];
    EXPECT_WK_STREQ("1000", bodyWidth());
    EXPECT_EQ(1., [webView scrollView].zoomScale);
}

#endif
