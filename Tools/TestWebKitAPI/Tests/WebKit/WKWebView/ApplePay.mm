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

#import "ClassMethodSwizzler.h"
#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestProtocol.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKFeature.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>

#import <pal/cocoa/PassKitSoftLink.h>

#if ENABLE(APPLE_PAY_REMOTE_UI)

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

    RetainPtr messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:YES canMakePaymentsExpectation:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability.html"]]];

    Util::run(&isDone);

    [TestProtocol unregister];
}

TEST(ApplePay, ApplePayAvailableInFrame)
{
    [TestProtocol registerWithScheme:@"https"];

    RetainPtr messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:YES canMakePaymentsExpectation:YES]);
    RetainPtr userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addUserScript:userScript.get()];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability-in-iframe.html"]]];
    [webView _test_waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"loadApplePayFrame();" completionHandler:nil];

    Util::run(&isDone);

    EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);

    [TestProtocol unregister];
}

TEST(ApplePay, UserScriptAtDocumentStartDoesNotDisableApplePay)
{
    [TestProtocol registerWithScheme:@"https"];

    RetainPtr messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:YES canMakePaymentsExpectation:YES]);
    RetainPtr userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addUserScript:userScript.get()];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability.html"]]];

    Util::run(&isDone);

    EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);

    [TestProtocol unregister];
}

TEST(ApplePay, UserScriptAtDocumentEndDoesNotDisableApplePay)
{
    [TestProtocol registerWithScheme:@"https"];
    
    RetainPtr messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:YES canMakePaymentsExpectation:YES]);
    RetainPtr userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentEnd forMainFrameOnly:YES]);
    
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addUserScript:userScript.get()];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];
    
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability.html"]]];
    
    Util::run(&isDone);
    
    EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);
    
    [TestProtocol unregister];
}

TEST(ApplePay, UserAgentScriptEvaluationDoesNotDisableApplePay)
{
    [TestProtocol registerWithScheme:@"https"];

    RetainPtr messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:YES canMakePaymentsExpectation:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability-in-iframe.html"]]];
    [webView _test_waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"window.wkScriptEvaluated = true; loadApplePayFrame();" completionHandler:nil];

    Util::run(&isDone);

    EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkScriptEvaluated"] boolValue]);

    [TestProtocol unregister];
}

TEST(ApplePay, UserAgentScriptEvaluationDoesNotDisableApplePayInExistingObjects)
{
    [TestProtocol registerWithScheme:@"https"];

    RetainPtr messageHandler = adoptNS([[TestApplePayAvailableScriptMessageHandler alloc] initWithAPIsAvailableExpectation:YES canMakePaymentsExpectation:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-availability-existing-object.html"]]];

    Util::run(&isDone);

    isDone = false;
    [messageHandler setCanMakePaymentsExpectation:YES];
    [webView evaluateJavaScript:@"document.location.hash = '#test'" completionHandler:nil];

    Util::run(&isDone);

    [TestProtocol unregister];
}

static void runActiveSessionTest(NSURL *url, BOOL shouldBlockScripts)
{
    [TestProtocol registerWithScheme:@"https"];

    RetainPtr messageHandler = adoptNS([[TestApplePayActiveSessionScriptMessageHandler alloc] init]);
    RetainPtr userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testApplePay"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
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

TEST(ApplePay, CanMakePaymentsDoesNotBlockUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payments.html"], NO);
}

TEST(ApplePay, CanMakePaymentsFalseDoesNotBlockUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payments.html?false"], NO);
}

TEST(ApplePay, CanMakePaymentsWithActiveCardDoesNotBlockUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payments-with-active-card.html"], NO);
}

TEST(ApplePay, CanMakePaymentsWithActiveCardFalseDoesNotBlockUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payments-with-active-card.html?false"], NO);
}

TEST(ApplePay, CanMakePaymentDoesNotBlockUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payment.html"], NO);
}

TEST(ApplePay, CanMakePaymentFalseDoesNotBlockUserAgentScripts)
{
    runActiveSessionTest([NSURL URLWithString:@"https://bundle-file/apple-pay-can-make-payment.html?false"], NO);
}

} // namespace TestWebKitAPI

#endif // ENABLE(APPLE_PAY_REMOTE_UI)

#if PLATFORM(MAC) && HAVE(PASSKIT_DELEGATED_WEB_CHECKOUT)

// FIXME: <rdar://193748128> (Remove delegated web checkout staging code from WebKit)
@protocol TestDelegatedWebCheckoutDelegate <PKPaymentAuthorizationViewControllerDelegate>
- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didRequestDelegateSessionUpdate:(void (^)(PKPaymentRequestDelegateSessionUpdate *update))handler;
- (void)paymentAuthorizationViewControllerDidCompleteDelegatedWebCheckoutPayment:(PKPaymentAuthorizationViewController *)controller;
@end

@interface TestDelegatedWebCheckoutViewController : NSViewController
@property (nonatomic, assign) id delegate;
@property (nonatomic, assign) id privateDelegate;
@property (nonatomic, readonly) NSURL *originatingURL;
@property (nonatomic, readonly) NSArray<NSURL *> *thumbnailURLs;
- (void)setOriginatingURL:(NSURL *)originatingURL thumbnailURLs:(NSArray<NSURL *> *)thumbnailURLs;
@end

@implementation TestDelegatedWebCheckoutViewController {
    RetainPtr<NSURL> _originatingURL;
    RetainPtr<NSArray<NSURL *>> _thumbnailURLs;
}

- (void)loadView
{
    self.view = adoptNS([[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240)]).get();
}

- (NSURL *)originatingURL
{
    return _originatingURL.get();
}

- (NSArray<NSURL *> *)thumbnailURLs
{
    return _thumbnailURLs.get();
}

- (void)setOriginatingURL:(NSURL *)originatingURL thumbnailURLs:(NSArray<NSURL *> *)thumbnailURLs
{
    _originatingURL = originatingURL;
    _thumbnailURLs = thumbnailURLs;
}

- (void)prepareForDelegatedWebCheckoutPresentationWithCompletion:(void (^)(BOOL ready))completion
{
    completion(YES);
}

@end

namespace TestWebKitAPI {

// FIXME: <rdar://193748128> (Remove delegated web checkout staging code from WebKit)
static bool passKitSupportsDelegatedWebCheckout()
{
    return [PAL::getPKPaymentAuthorizationViewControllerClassSingleton() respondsToSelector:@selector(delegatedWebCheckoutViewControllerWithOriginatingURL:thumbnailURLs:)]
        && [PAL::getPKPaymentDelegateSessionClassSingleton() instancesRespondToSelector:@selector(initWithSessionData:)]
        && [PAL::getPKPaymentRequestClassSingleton() instancesRespondToSelector:@selector(setUsesDelegateSession:)];
}

static RetainPtr<TestDelegatedWebCheckoutViewController>& testDelegatedWebCheckoutViewController()
{
    static NeverDestroyed<RetainPtr<TestDelegatedWebCheckoutViewController>> viewController;
    return viewController.get();
}

static RetainPtr<PKPaymentRequest>& testDelegatedPaymentRequest()
{
    static NeverDestroyed<RetainPtr<PKPaymentRequest>> paymentRequest;
    return paymentRequest.get();
}

static id delegatedWebCheckoutViewControllerOverride(id, SEL, NSURL *originatingURL, NSArray<NSURL *> *thumbnailURLs)
{
    testDelegatedWebCheckoutViewController() = adoptNS([[TestDelegatedWebCheckoutViewController alloc] init]);
    [testDelegatedWebCheckoutViewController() setOriginatingURL:originatingURL thumbnailURLs:thumbnailURLs];
    return testDelegatedWebCheckoutViewController().get();
}

static void requestViewControllerOverride(id, SEL, PKPaymentRequest *paymentRequest, void (^completion)(id, NSError *))
{
    testDelegatedPaymentRequest() = paymentRequest;
    testDelegatedWebCheckoutViewController() = adoptNS([[TestDelegatedWebCheckoutViewController alloc] init]);
    completion(testDelegatedWebCheckoutViewController().get(), nil);
}

static RetainPtr<TestWKWebView> delegatedWebCheckoutWebView()
{
    [TestProtocol registerWithScheme:@"https"];
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setApplePayEnabled:YES];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"ApplePayDelegatedWebCheckoutEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/apple-pay-delegated-web-checkout.html"]]];
    return webView;
}

static void waitForPresentedDelegatedWebCheckoutViewController()
{
    Util::waitFor([] {
        return testDelegatedWebCheckoutViewController() && [testDelegatedWebCheckoutViewController() delegate];
    });
}

static NSArray<NSString *> *pageEvents(TestWKWebView *webView, NSUInteger count)
{
    Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.events.length"] unsignedIntegerValue] >= count;
    });
    Util::runFor(100_ms);
    return [webView objectByEvaluatingJavaScript:@"window.events"];
}

TEST(ApplePay, DelegatedWebCheckoutCompletedPayment)
{
    if (!passKitSupportsDelegatedWebCheckout())
        return;

    ClassMethodSwizzler factorySwizzler { PAL::getPKPaymentAuthorizationViewControllerClassSingleton(), @selector(delegatedWebCheckoutViewControllerWithOriginatingURL:thumbnailURLs:), reinterpret_cast<IMP>(delegatedWebCheckoutViewControllerOverride) };

    RetainPtr webView = delegatedWebCheckoutWebView();
    testDelegatedWebCheckoutViewController() = nil;
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"startDelegatedWebCheckout()"];
    waitForPresentedDelegatedWebCheckoutViewController();

    EXPECT_WK_STREQ(@"https://bundle-file/apple-pay-delegated-web-checkout.html", [testDelegatedWebCheckoutViewController() originatingURL].absoluteString);
    EXPECT_EQ(1U, [testDelegatedWebCheckoutViewController() thumbnailURLs].count);
    EXPECT_WK_STREQ(@"https://bundle-file/icon.png", [testDelegatedWebCheckoutViewController() thumbnailURLs].firstObject.absoluteString);

    id<TestDelegatedWebCheckoutDelegate> delegate = [testDelegatedWebCheckoutViewController() delegate];
    auto *viewController = (PKPaymentAuthorizationViewController *)testDelegatedWebCheckoutViewController().get();
    EXPECT_TRUE([delegate respondsToSelector:@selector(paymentAuthorizationViewController:didRequestDelegateSessionUpdate:)]);

    __block RetainPtr<PKPaymentRequestDelegateSessionUpdate> update;
    [delegate paymentAuthorizationViewController:viewController didRequestDelegateSessionUpdate:^(PKPaymentRequestDelegateSessionUpdate *result) {
        update = result;
    }];
    Util::waitFor(^{
        return !!update;
    });

    EXPECT_EQ(PKPaymentAuthorizationStatusSuccess, [update status]);
    NSString *sessionString = [webView objectByEvaluatingJavaScript:@"window.delegateSession"];
    RetainPtr expectedSession = adoptNS([PAL::allocPKPaymentDelegateSessionInstance() initWithSessionData:sessionString]);
    EXPECT_TRUE([[update session] isEqual:expectedSession.get()]);

    [delegate paymentAuthorizationViewControllerDidCompleteDelegatedWebCheckoutPayment:viewController];
    [delegate paymentAuthorizationViewControllerDidFinish:viewController];

    NSArray<NSString *> *events = pageEvents(webView.get(), 2);
    EXPECT_EQ(2U, events.count);
    EXPECT_WK_STREQ(@"validatedelegate", events.firstObject);
    EXPECT_WK_STREQ(@"paymentcompleted", events.lastObject);

    [TestProtocol unregister];
}

TEST(ApplePay, DelegatedWebCheckoutPaymentNotCompleted)
{
    if (!passKitSupportsDelegatedWebCheckout())
        return;

    ClassMethodSwizzler factorySwizzler { PAL::getPKPaymentAuthorizationViewControllerClassSingleton(), @selector(delegatedWebCheckoutViewControllerWithOriginatingURL:thumbnailURLs:), reinterpret_cast<IMP>(delegatedWebCheckoutViewControllerOverride) };

    RetainPtr webView = delegatedWebCheckoutWebView();
    testDelegatedWebCheckoutViewController() = nil;
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"startDelegatedWebCheckout()"];
    waitForPresentedDelegatedWebCheckoutViewController();

    [[testDelegatedWebCheckoutViewController() delegate] paymentAuthorizationViewControllerDidFinish:(PKPaymentAuthorizationViewController *)testDelegatedWebCheckoutViewController().get()];

    NSArray<NSString *> *events = pageEvents(webView.get(), 1);
    EXPECT_EQ(1U, events.count);
    EXPECT_WK_STREQ(@"cancel unknown", events.firstObject);

    [TestProtocol unregister];
}

TEST(ApplePay, DelegatedWebCheckoutDelegateValidationFailed)
{
    if (!passKitSupportsDelegatedWebCheckout())
        return;

    ClassMethodSwizzler factorySwizzler { PAL::getPKPaymentAuthorizationViewControllerClassSingleton(), @selector(delegatedWebCheckoutViewControllerWithOriginatingURL:thumbnailURLs:), reinterpret_cast<IMP>(delegatedWebCheckoutViewControllerOverride) };

    RetainPtr webView = delegatedWebCheckoutWebView();
    testDelegatedWebCheckoutViewController() = nil;
    [webView objectByEvaluatingJavaScript:@"window.failDelegateValidation = true"];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"startDelegatedWebCheckout()"];
    waitForPresentedDelegatedWebCheckoutViewController();

    id<TestDelegatedWebCheckoutDelegate> delegate = [testDelegatedWebCheckoutViewController() delegate];
    auto *viewController = (PKPaymentAuthorizationViewController *)testDelegatedWebCheckoutViewController().get();

    __block RetainPtr<PKPaymentRequestDelegateSessionUpdate> update;
    [delegate paymentAuthorizationViewController:viewController didRequestDelegateSessionUpdate:^(PKPaymentRequestDelegateSessionUpdate *result) {
        update = result;
    }];
    Util::waitFor(^{
        return !!update;
    });

    EXPECT_EQ(PKPaymentAuthorizationStatusFailure, [update status]);
    EXPECT_NULL([update session]);

    [delegate paymentAuthorizationViewControllerDidFinish:viewController];

    NSArray<NSString *> *events = pageEvents(webView.get(), 2);
    EXPECT_EQ(2U, events.count);
    EXPECT_WK_STREQ(@"validatedelegate", events.firstObject);
    EXPECT_WK_STREQ(@"cancel unknown", events.lastObject);

    [TestProtocol unregister];
}

TEST(ApplePay, DelegatedRequestUsesDelegateValidationOnlyWithAListener)
{
    if (!passKitSupportsDelegatedWebCheckout())
        return;

    ClassMethodSwizzler requestSwizzler { PAL::getPKPaymentAuthorizationViewControllerClassSingleton(), @selector(requestViewControllerWithPaymentRequest:completion:), reinterpret_cast<IMP>(requestViewControllerOverride) };

    RetainPtr webView = delegatedWebCheckoutWebView();
    for (BOOL usesDelegateValidation : { YES, NO }) {
        testDelegatedWebCheckoutViewController() = nil;
        testDelegatedPaymentRequest() = nil;
        [webView objectByEvaluatingJavaScript:@"window.events = []"];
        [webView objectByEvaluatingJavaScriptWithUserGesture:[NSString stringWithFormat:@"startDelegatedRequest(%@)", usesDelegateValidation ? @"true" : @"false"]];
        waitForPresentedDelegatedWebCheckoutViewController();

        id delegate = [testDelegatedWebCheckoutViewController() delegate];
        EXPECT_EQ(usesDelegateValidation, [delegate respondsToSelector:@selector(paymentAuthorizationViewController:didRequestDelegateSessionUpdate:)]);
        EXPECT_EQ(usesDelegateValidation, [testDelegatedPaymentRequest() usesDelegateSession]);

        [delegate paymentAuthorizationViewControllerDidFinish:(PKPaymentAuthorizationViewController *)testDelegatedWebCheckoutViewController().get()];
        EXPECT_WK_STREQ(@"cancel unknown", pageEvents(webView.get(), 1).firstObject);
    }

    [TestProtocol unregister];
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && HAVE(PASSKIT_DELEGATED_WEB_CHECKOUT)
