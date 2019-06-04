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
static NSString * const userScriptSource = @"window.wkUserScriptInjected = true";

@interface TestApplePayAvailableScriptMessageHandler : NSObject <WKScriptMessageHandler>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithAPIsAvailableExpectation:(BOOL)apisAvailableExpectation canMakePaymentsExpectation:(BOOL)canMakePaymentsExpectation;

@property (nonatomic, setter=setAPIsAvailableExpectation:) BOOL apisAvailableExpectation;
@property (nonatomic) BOOL canMakePaymentsExpectation;

@end

@implementation TestApplePayAvailableScriptMessageHandler

- (instancetype)initWithAPIsAvailableExpectation:(BOOL)apisAvailableExpectation canMakePaymentsExpectation:(BOOL)canMakePaymentsExpectation
{
    if (!(self = [super init]))
        return nil;

    _apisAvailableExpectation = apisAvailableExpectation;
    _canMakePaymentsExpectation = canMakePaymentsExpectation;
    return self;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_EQ(_apisAvailableExpectation, [[message.body objectForKey:@"applePaySessionAvailable"] boolValue]);
    EXPECT_EQ(_apisAvailableExpectation, [[message.body objectForKey:@"paymentRequestAvailable"] boolValue]);
    EXPECT_EQ(_apisAvailableExpectation, [[message.body objectForKey:@"supportsVersion"] boolValue]);
    EXPECT_EQ(_canMakePaymentsExpectation, [[message.body objectForKey:@"canMakePayments"] boolValue]);
    EXPECT_EQ(_canMakePaymentsExpectation, [[message.body objectForKey:@"canMakePaymentsWithActiveCard"] boolValue]);
    EXPECT_EQ(_canMakePaymentsExpectation, [[message.body objectForKey:@"canMakePayment"] boolValue]);
    isDone = true;
}

@end

@interface TestApplePayActiveSessionScriptMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation TestApplePayActiveSessionScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    isDone = true;
}

@end

namespace TestWebKitAPI {
    
TEST(ApplePay, ApplePayAvailableByDefault)
{
    [TestProtocol registerWithScheme:@"https"];

    auto messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:YES canMakePaymentsExpectation:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability.html"]]];

    Util::run(&isDone);

    [TestProtocol unregister];
}

TEST(ApplePay, ApplePayAvailableInUnrestrictedClients)
{
    [TestProtocol registerWithScheme:@"https"];

    auto messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:YES canMakePaymentsExpectation:YES]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addUserScript:userScript.get()];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability-in-iframe.html?unrestricted"]]];
    [webView _test_waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"loadApplePayFrame();" completionHandler:nil];

    Util::run(&isDone);

    EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);

    [TestProtocol unregister];
}

TEST(ApplePay, UserScriptAtDocumentStartDisablesApplePay)
{
    [TestProtocol registerWithScheme:@"https"];

    auto messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:NO canMakePaymentsExpectation:NO]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addUserScript:userScript.get()];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability.html"]]];

    Util::run(&isDone);

    EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);

    [TestProtocol unregister];
}

TEST(ApplePay, UserScriptAtDocumentEndDisablesApplePay)
{
    [TestProtocol registerWithScheme:@"https"];
    
    auto messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:NO canMakePaymentsExpectation:NO]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentEnd forMainFrameOnly:YES]);
    
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addUserScript:userScript.get()];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability.html"]]];
    
    Util::run(&isDone);
    
    EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);
    
    [TestProtocol unregister];
}

TEST(ApplePay, UserAgentScriptEvaluationDisablesApplePay)
{
    [TestProtocol registerWithScheme:@"https"];

    auto messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:YES canMakePaymentsExpectation:NO]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability-in-iframe.html"]]];
    [webView _test_waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"window.wkScriptEvaluated = true; loadApplePayFrame();" completionHandler:nil];

    Util::run(&isDone);

    EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkScriptEvaluated"] boolValue]);

    [TestProtocol unregister];
}

TEST(ApplePay, UserAgentScriptEvaluationDisablesApplePayInExistingObjects)
{
    [TestProtocol registerWithScheme:@"https"];

    auto messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:YES canMakePaymentsExpectation:NO]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability-existing-object.html"]]];

    Util::run(&isDone);

    isDone = false;
    [messageHandler setCanMakePaymentsExpectation:NO];
    [webView evaluateJavaScript:@"document.location.hash = '#test'" completionHandler:nil];

    Util::run(&isDone);

    [TestProtocol unregister];
}

static void runActiveSessionTest(NSURL *url, BOOL shouldBlockScripts)
{
    [TestProtocol registerWithScheme:@"https"];

    auto messageHandler = adoptNS([[TestApplePayActiveSessionScriptMessageHandler alloc] init]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    Util::run(&isDone);

    [configuration.userContentController _addUserScriptImmediately:userScript.get()];
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_EQ(shouldBlockScripts, !result);
        EXPECT_EQ(shouldBlockScripts, !!error);
        isDone = true;
    }];

    isDone = false;
    Util::run(&isDone);

    [TestProtocol unregister];
}

TEST(ApplePay, ActiveSessionBlocksUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-active-session.html"], YES);
}

TEST(ApplePay, CanMakePaymentsBlocksUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payments.html"], YES);
}

TEST(ApplePay, CanMakePaymentsFalseDoesNotBlockUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payments.html?false"], NO);
}

TEST(ApplePay, CanMakePaymentsWithActiveCardBlocksUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payments-with-active-card.html"], YES);
}

TEST(ApplePay, CanMakePaymentsWithActiveCardFalseDoesNotBlockUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payments-with-active-card.html?false"], NO);
}

TEST(ApplePay, CanMakePaymentBlocksUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payment.html"], YES);
}

TEST(ApplePay, CanMakePaymentFalseDoesNotBlockUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payment.html?false"], NO);
}

} // namespace TestWebKitAPI

#endif // ENABLE(APPLE_PAY_REMOTE_UI)
