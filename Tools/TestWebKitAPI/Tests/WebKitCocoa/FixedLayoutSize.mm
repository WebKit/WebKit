/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferences.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)

TEST(WebKit, OverrideMinimumLayoutSizeWithNegativeHeight)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(320, -1000) maximumUnobscuredSizeOverride:CGSizeMake(0, 0)];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'><body>Hello</body>"];
    EXPECT_GE([webView _fixedLayoutSize].height, 0);
}

#else

static bool fixedLayoutSizeDone;
static bool fixedLayoutSizeAfterNavigationDone;
static bool fixedLayoutSizeDisabledDone;

TEST(WebKit, FixedLayoutSize)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);

    [webView evaluateJavaScript:@"document.body.clientWidth" completionHandler:^(id result, NSError *error) {
        EXPECT_EQ(100, [result integerValue]);

        [webView _setLayoutMode:_WKLayoutModeFixedSize];
        [webView _setFixedLayoutSize:NSMakeSize(200, 200)];

        [webView evaluateJavaScript:@"document.body.clientWidth" completionHandler:^(id result, NSError *error) {
            EXPECT_EQ(200, [result integerValue]);

            fixedLayoutSizeDone = true;
        }];
    }];

    TestWebKitAPI::Util::run(&fixedLayoutSizeDone);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // After navigating, fixed layout size should be persisted.
    [webView evaluateJavaScript:@"document.body.clientWidth" completionHandler:^(id result, NSError *error) {
        EXPECT_EQ(200, [result integerValue]);
        fixedLayoutSizeAfterNavigationDone = true;
    }];

    TestWebKitAPI::Util::run(&fixedLayoutSizeAfterNavigationDone);

    [webView _setLayoutMode:_WKLayoutModeViewSize];
    [webView evaluateJavaScript:@"document.body.clientWidth" completionHandler:^(id result, NSError *error) {
        EXPECT_EQ(100, [result integerValue]);
        fixedLayoutSizeDisabledDone = true;
    }];

    TestWebKitAPI::Util::run(&fixedLayoutSizeDisabledDone);
}

#endif
