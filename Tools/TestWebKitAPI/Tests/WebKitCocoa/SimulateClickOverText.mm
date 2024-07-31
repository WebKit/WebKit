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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

@interface WKWebView (SimulateClickOverText)
- (BOOL)simulateClickOverText:(NSString *)text;
@end

@implementation WKWebView (SimulateClickOverText)

- (BOOL)simulateClickOverText:(NSString *)text
{
    __block bool done = false;
    __block BOOL result = NO;
    [self _simulateClickOverFirstMatchingTextInViewportWithUserInteraction:text completionHandler:^(BOOL success) {
        result = success;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

@end

namespace TestWebKitAPI {

TEST(SimulateClickOverText, ClickTargets)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"click-targets"];

    EXPECT_TRUE([webView simulateClickOverText:@"Bugzilla"]);
    EXPECT_TRUE([webView simulateClickOverText:@"Sign up"]);
    EXPECT_TRUE([webView simulateClickOverText:@"First name"]);
    EXPECT_TRUE([webView simulateClickOverText:@"Log in"]);
    EXPECT_TRUE([webView simulateClickOverText:@"More info"]);
    EXPECT_TRUE([webView simulateClickOverText:@"Close"]);

    RetainPtr expectedEvents = @[ @"mousedown", @"mouseup", @"click" ];
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('a.top').events"]]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('button.top').events"]]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('input[type=text].top').events"]]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('input[type=submit].top').events"]]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('input[type=button].top').events"]]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('div.close.top').events"]]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('a.bottom').events"]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('button.bottom').events"]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('input.bottom').events"]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('input[type=submit].bottom').events"]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('input[type=button].bottom').events"]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('div.close.bottom').events"]);
}

TEST(SimulateClickOverText, ClickTargetsAfterScrolling)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"click-targets"];
    [webView stringByEvaluatingJavaScript:@"scrollBy(0, 5000)"];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([webView simulateClickOverText:@"Bugzilla"]);
    EXPECT_TRUE([webView simulateClickOverText:@"Sign up"]);
    EXPECT_TRUE([webView simulateClickOverText:@"First name"]);
    EXPECT_TRUE([webView simulateClickOverText:@"Log in"]);
    EXPECT_TRUE([webView simulateClickOverText:@"More info"]);
    EXPECT_TRUE([webView simulateClickOverText:@"Close"]);

    RetainPtr expectedEvents = @[ @"mousedown", @"mouseup", @"click" ];
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('a.top').events"]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('button.top').events"]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('input.top').events"]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('input[type=submit].top').events"]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('input[type=button].top').events"]);
    EXPECT_NULL([webView objectByEvaluatingJavaScript:@"document.querySelector('div.close.top').events"]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('a.bottom').events"]]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('button.bottom').events"]]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('input[type=text].bottom').events"]]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('input[type=submit].bottom').events"]]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('input[type=button].bottom').events"]]);
    EXPECT_TRUE([expectedEvents isEqualToArray:[webView objectByEvaluatingJavaScript:@"document.querySelector('div.close.bottom').events"]]);
}

} // namespace TestWebKitAPI
