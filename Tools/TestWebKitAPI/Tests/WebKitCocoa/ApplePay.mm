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

#import "config.h"

#if ENABLE(APPLE_PAY_REMOTE_UI)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WebKit.h>

static bool isDone;

@interface TestApplePayScriptMessageHandler : NSObject <WKScriptMessageHandler>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithExpectation:(BOOL)expectation;

@end

@implementation TestApplePayScriptMessageHandler {
    BOOL _expectation;
}

- (instancetype)initWithExpectation:(BOOL)expectation
{
    if (!(self = [super init]))
        return nil;

    _expectation = expectation;
    return self;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_EQ(_expectation, [[message.body objectForKey:@"supportsVersion"] boolValue]);
    EXPECT_EQ(_expectation, [[message.body objectForKey:@"canMakePayments"] boolValue]);
    EXPECT_EQ(_expectation, [[message.body objectForKey:@"canMakePaymentsWithActiveCard"] boolValue]);
    EXPECT_EQ(_expectation, [[message.body objectForKey:@"canMakePayment"] boolValue]);
    isDone = true;
}

@end

namespace TestWebKitAPI {
    
TEST(ApplePay, ApplePayAvailableByDefault)
{
    [TestProtocol registerWithScheme:@"https"];

    auto messageHandler = adoptNS([[TestApplePayScriptMessageHandler alloc] initWithExpectation:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-html-file/apple-pay-availability"]]];

    Util::run(&isDone);

    [TestProtocol unregister];
}

TEST(ApplePay, UserScriptDisablesApplePay)
{
    [TestProtocol registerWithScheme:@"https"];

    auto messageHandler = adoptNS([[TestApplePayScriptMessageHandler alloc] initWithExpectation:NO]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:@"window.wkUserScriptInjected = true" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addUserScript:userScript.get()];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-html-file/apple-pay-availability"]]];

    Util::run(&isDone);

    EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);

    [TestProtocol unregister];
}

TEST(ApplePay, UserAgentScriptEvaluationDisablesApplePay)
{
    [TestProtocol registerWithScheme:@"https"];

    auto messageHandler = adoptNS([[TestApplePayScriptMessageHandler alloc] initWithExpectation:NO]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-html-file/apple-pay-availability-in-iframe"]]];
    [webView _test_waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"window.wkScriptEvaluated = true; loadApplePayFrame();" completionHandler:nil];

    Util::run(&isDone);

    EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkScriptEvaluated"] boolValue]);

    [TestProtocol unregister];
}

TEST(ApplePay, ActiveSessionBlocksUserAgentScripts)
{
    [TestProtocol registerWithScheme:@"https"];

    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:@"window.wkUserScriptInjected = true" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-html-file/apple-pay-active-session"]]];
    [webView _test_waitForDidFinishNavigation];

    [configuration.userContentController _addUserScriptImmediately:userScript.get()];
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_NULL(result);
        EXPECT_NOT_NULL(error);
        isDone = true;
    }];
    Util::run(&isDone);

    [TestProtocol unregister];
}

} // namespace TestWebKitAPI

#endif // ENABLE(APPLE_PAY_REMOTE_UI)
