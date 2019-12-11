/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import <WebKit/WKPreferences.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)

static bool viewportSizeTestDone;

TEST(WebKit, ViewportSizeForViewportUnits)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);

    // No _setViewportSizeForCSSViewportUnits call -> original size is used (100px 100px).
    [webView loadHTMLString:@"<div id=divWithViewportUnits style='width: 99vw;'></div>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"window.getComputedStyle(document.getElementById('divWithViewportUnits'), null).getPropertyValue('width')" completionHandler:^(NSString *result, NSError *error) {
        EXPECT_STREQ("99px", [result UTF8String]);
        viewportSizeTestDone = true;
    }];
    viewportSizeTestDone = false;
    TestWebKitAPI::Util::run(&viewportSizeTestDone);

    // viewport width is 50px.
    [webView _setViewportSizeForCSSViewportUnits:NSMakeSize(50, 50)];
    [webView loadHTMLString:@"<div id=divWithViewportUnits style='width: 50vw;'></div>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.getComputedStyle(document.getElementById('divWithViewportUnits'), null).getPropertyValue('width')" completionHandler:^(NSString *result, NSError *error) {
        EXPECT_STREQ("25px", [result UTF8String]);
        viewportSizeTestDone = true;
    }];
    viewportSizeTestDone = false;
    TestWebKitAPI::Util::run(&viewportSizeTestDone);

    // viewport height 10px.
    [webView _setViewportSizeForCSSViewportUnits:NSMakeSize(10, 10)];
    [webView loadHTMLString:@"<div id=divWithViewportUnits style='height: 10vh;'></div>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.getComputedStyle(document.getElementById('divWithViewportUnits'), null).getPropertyValue('height')" completionHandler:^(NSString *result, NSError *error) {
        EXPECT_STREQ("1px", [result UTF8String]);
        viewportSizeTestDone = true;
    }];
    viewportSizeTestDone = false;
    TestWebKitAPI::Util::run(&viewportSizeTestDone);

    bool exceptionRaised = false;
    @try {
        [webView _setViewportSizeForCSSViewportUnits:NSMakeSize(-1, 10)];
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        exceptionRaised = true;
    }
    EXPECT_TRUE(exceptionRaised);

    exceptionRaised = false;
    @try {
        [webView _setViewportSizeForCSSViewportUnits:NSMakeSize(10, 0)];
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        exceptionRaised = true;
    }
    EXPECT_TRUE(exceptionRaised);

    exceptionRaised = false;
    @try {
        [webView _setViewportSizeForCSSViewportUnits:NSMakeSize(0.5, 0.5)];
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        exceptionRaised = true;
    }
    EXPECT_TRUE(exceptionRaised);

    exceptionRaised = false;
    @try {
        [webView _setViewportSizeForCSSViewportUnits:NSMakeSize(1, 1)];
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        exceptionRaised = true;
    }
    EXPECT_FALSE(exceptionRaised);
}

#endif
