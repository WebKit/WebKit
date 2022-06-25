/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "TestCocoa.h"

#if PLATFORM(IOS_FAMILY)

#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivateForTesting.h>

namespace TestWebKitAPI {

static void runTest(TestWKWebView *webView, TestNavigationDelegate *navigationDelegate, NSString *selector)
{
    __block bool mouseMoved = false;
    __block bool navigationAttempted = false;
    __block bool done = false;

    navigationDelegate.decidePolicyForNavigationAction = ^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyCancel);
        EXPECT_FALSE(mouseMoved);
        EXPECT_FALSE(navigationAttempted);
        EXPECT_NS_EQUAL(@"https://www.apple.com/", navigationAction.request.URL.absoluteString);
        navigationAttempted = true;
        done = true;
    };

    [webView performAfterReceivingMessage:@"mousemove" action:^{
        if (mouseMoved)
            done = true;
        mouseMoved = true;
    }];

    id array = [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"boundingClientRectArray('%@')", selector]];
    auto boundingRect = CGRectMake([array[0] floatValue], [array[1] floatValue], [array[2] floatValue], [array[3] floatValue]);
    [webView _simulateElementAction:_WKElementActionTypeOpen atLocation:CGPointMake(CGRectGetMidX(boundingRect), CGRectGetMidY(boundingRect))];

    Util::run(&done);

    EXPECT_FALSE(mouseMoved);
    EXPECT_TRUE(navigationAttempted);

    navigationDelegate.decidePolicyForNavigationAction = nil;
    [webView clearMessageHandlers:@[ @"mousemove" ]];
}

TEST(ElementActionTests, OpenLinkWithHoverMenu)
{
    auto frame = CGRectMake(0, 0, 320, 500);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:frame]);
    [webView synchronouslyLoadTestPageNamed:@"link-with-hover-menu"];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    runTest(webView.get(), navigationDelegate.get(), @"#link");
    runTest(webView.get(), navigationDelegate.get(), @"#link");
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
