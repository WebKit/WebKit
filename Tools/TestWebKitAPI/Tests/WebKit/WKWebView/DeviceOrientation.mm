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

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/Function.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

static bool askedClientForPermission;

@interface DeviceOrientationPermissionUIDelegate : NSObject <WKUIDelegatePrivate> {
}
- (instancetype)initWithHandler:(Function<bool()>&&)decisionHandler;
@end

@implementation DeviceOrientationPermissionUIDelegate {
Function<bool()> _decisionHandler;
}

- (instancetype)initWithHandler:(Function<bool()>&&)decisionHandler
{
    self = [super init];
    _decisionHandler = WTF::move(decisionHandler);
    askedClientForPermission = false;
    return self;
}

- (void)webView:(WKWebView *)webView requestDeviceOrientationAndMotionPermissionForOrigin:(WKSecurityOrigin*)origin initiatedByFrame:(WKFrameInfo *)requestingFrame decisionHandler:(void (^)(WKPermissionDecision))decisionHandler
{
    decisionHandler(_decisionHandler() ? WKPermissionDecisionGrant : WKPermissionDecisionDeny);
    askedClientForPermission = true;
}

@end

static RetainPtr<TestMessageHandler> addTestMessageHandler(WKWebViewConfiguration *configuration, bool* didReceiveMessage)
{
    RetainPtr messageHandler = adoptNS([TestMessageHandler new]);
    [messageHandler setDidReceiveScriptMessage:^(NSString *) {
        *didReceiveMessage = true;
    }];
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
    return messageHandler;
}

enum class DeviceOrientationPermission { GrantedByClient, DeniedByClient, GrantedByUser, DeniedByUser };
static void runDeviceOrientationTest(DeviceOrientationPermission deviceOrientationPermission)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    bool didReceiveMessage = false;
    RetainPtr messageHandler = addTestMessageHandler(configuration.get(), &didReceiveMessage);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<DeviceOrientationPermissionUIDelegate> uiDelegate;
    switch (deviceOrientationPermission) {
    case DeviceOrientationPermission::GrantedByClient:
        uiDelegate = adoptNS([[DeviceOrientationPermissionUIDelegate alloc] initWithHandler:[] { return true; }]);
        break;
    case DeviceOrientationPermission::DeniedByClient:
        uiDelegate = adoptNS([[DeviceOrientationPermissionUIDelegate alloc] initWithHandler:[] { return false; }]);
        break;
    case DeviceOrientationPermission::GrantedByUser:
        [webView _setDeviceOrientationUserPermissionHandlerForTesting:^{ return YES; }];
        break;
    case DeviceOrientationPermission::DeniedByUser:
        [webView _setDeviceOrientationUserPermissionHandlerForTesting:^{ return NO; }];
        break;
    }
    [webView setUIDelegate:uiDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];

    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"DeviceOrientationEvent.requestPermission().then((granted) => { webkit.messageHandlers.testHandler.postMessage(granted) });" completionHandler: [&] (id result, NSError *error) { }];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    switch (deviceOrientationPermission) {
    case DeviceOrientationPermission::GrantedByClient:
    case DeviceOrientationPermission::GrantedByUser:
        EXPECT_WK_STREQ(@"granted", [messageHandler receivedMessages][0]);
        break;
    case DeviceOrientationPermission::DeniedByClient:
    case DeviceOrientationPermission::DeniedByUser:
        EXPECT_WK_STREQ(@"denied", [messageHandler receivedMessages][0]);
        break;
    }

    bool addedEventListener = false;
    [webView evaluateJavaScript:@"addEventListener('deviceorientation', (e) => { webkit.messageHandlers.testHandler.postMessage('received-event') });" completionHandler: [&] (id result, NSError *error) {
        addedEventListener = true;
    }];

    TestWebKitAPI::Util::run(&addedEventListener);
    addedEventListener = false;

    [webView _simulateDeviceOrientationChangeWithAlpha:1.0 beta:2.0 gamma:3.0];

    if (deviceOrientationPermission == DeviceOrientationPermission::GrantedByClient || deviceOrientationPermission == DeviceOrientationPermission::GrantedByUser) {
        TestWebKitAPI::Util::run(&didReceiveMessage);
        EXPECT_WK_STREQ(@"received-event", [messageHandler receivedMessages][1]);
    } else {
        TestWebKitAPI::Util::runFor(0.1_s);
        EXPECT_FALSE(didReceiveMessage);
    }
}

TEST(DeviceOrientation, PermissionGrantedByUser)
{
    runDeviceOrientationTest(DeviceOrientationPermission::GrantedByUser);
}

TEST(DeviceOrientation, PermissionDeniedByUser)
{
    runDeviceOrientationTest(DeviceOrientationPermission::DeniedByUser);
}

TEST(DeviceOrientation, PermissionGrantedByClient)
{
    runDeviceOrientationTest(DeviceOrientationPermission::GrantedByClient);
}

TEST(DeviceOrientation, PermissionDeniedByClient)
{
    runDeviceOrientationTest(DeviceOrientationPermission::DeniedByClient);
}

TEST(DeviceOrientation, RememberPermissionForSession)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = [WKWebsiteDataStore defaultDataStore];

    bool didReceiveMessage = false;
    RetainPtr messageHandler = addTestMessageHandler(configuration.get(), &didReceiveMessage);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<DeviceOrientationPermissionUIDelegate> uiDelegate = adoptNS([[DeviceOrientationPermissionUIDelegate alloc] initWithHandler:[] { return true; }]);
    [webView setUIDelegate:uiDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"DeviceOrientationEvent.requestPermission().then((granted) => { webkit.messageHandlers.testHandler.postMessage(granted) });" completionHandler: [&] (id result, NSError *error) { }];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_TRUE(askedClientForPermission);
    askedClientForPermission = false;
    EXPECT_WK_STREQ(@"granted", [messageHandler receivedMessages][0]);

    // Load the same origin again in a new WebView, it should not ask the client.
    webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"DeviceOrientationEvent.requestPermission().then((granted) => { webkit.messageHandlers.testHandler.postMessage(granted) }, (error) => { webkit.messageHandlers.testHandler.postMessage('error'); });" completionHandler: [&] (id result, NSError *error) { }];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_WK_STREQ(@"granted", [messageHandler receivedMessages][1]);
    EXPECT_FALSE(askedClientForPermission);
    askedClientForPermission = false;

    // Load the same origin again in a new WebView but this time with a different data store, it should ask the client again.
    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto preferences = [configuration preferences];
    [preferences _setSecureContextChecksEnabled: NO];

    webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"DeviceOrientationEvent.requestPermission().then((granted) => { webkit.messageHandlers.testHandler.postMessage(granted) });" completionHandler: [&] (id result, NSError *error) { }];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_TRUE(askedClientForPermission);
    askedClientForPermission = false;
    EXPECT_WK_STREQ(@"granted", [messageHandler receivedMessages][2]);

    // Now go to a different origin, it should ask the client again.
    NSURLRequest *request2 = [NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]];
    [webView loadRequest:request2];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"DeviceOrientationEvent.requestPermission().then((granted) => { webkit.messageHandlers.testHandler.postMessage(granted) });" completionHandler: [&] (id result, NSError *error) { }];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_TRUE(askedClientForPermission);
    askedClientForPermission = false;
    EXPECT_WK_STREQ(@"granted", [messageHandler receivedMessages][3]);

    // Go back to the first origin in a new WebView (same data store) and make sure it does not ask the client.
    webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView _evaluateJavaScriptWithoutUserGesture:@"DeviceOrientationEvent.requestPermission().then((granted) => { webkit.messageHandlers.testHandler.postMessage(granted) }, (error) => { webkit.messageHandlers.testHandler.postMessage('error'); });" completionHandler: [&] (id result, NSError *error) { }];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_WK_STREQ(@"granted", [messageHandler receivedMessages][4]);
    EXPECT_FALSE(askedClientForPermission);
    askedClientForPermission = false;
}

TEST(DeviceOrientation, FireOrientationEventsRightAwayIfPermissionAlreadyGranted)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = [WKWebsiteDataStore defaultDataStore];

    bool didReceiveMessage = false;
    RetainPtr messageHandler = addTestMessageHandler(configuration.get(), &didReceiveMessage);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<DeviceOrientationPermissionUIDelegate> uiDelegate = adoptNS([[DeviceOrientationPermissionUIDelegate alloc] initWithHandler:[] { return true; }]);
    [webView setUIDelegate:uiDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Request permission.
    [webView evaluateJavaScript:@"DeviceOrientationEvent.requestPermission().then((granted) => { webkit.messageHandlers.testHandler.postMessage(granted) });" completionHandler: [&] (id result, NSError *error) { }];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_TRUE(askedClientForPermission);
    askedClientForPermission = false;
    EXPECT_WK_STREQ(@"granted", [messageHandler receivedMessages][0]);

    // Go to the same origin again in a new view.
    webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // This time, we do not request permission but set our event listener.
    bool addedEventListener = false;
    [webView evaluateJavaScript:@"addEventListener('deviceorientation', (e) => { webkit.messageHandlers.testHandler.postMessage('received-event') });" completionHandler: [&] (id result, NSError *error) {
        addedEventListener = true;
    }];

    TestWebKitAPI::Util::run(&addedEventListener);
    addedEventListener = false;

    // Simulate a device orientation event. The page's event listener should get called even though it did not request permission,
    // because it was previously granted permission during this browsing session.
    [webView _simulateDeviceOrientationChangeWithAlpha:1.0 beta:2.0 gamma:3.0];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    EXPECT_WK_STREQ(@"received-event", [messageHandler receivedMessages][1]);
}

static constexpr auto mainBytes = R"TESTRESOURCE(
<script>

function log(msg)
{
    webkit.messageHandlers.testHandler.postMessage(msg);
}

function requestPermission() {
    DeviceOrientationEvent.requestPermission().then((result) => {
        log(result);
    });
}

</script>
)TESTRESOURCE"_s;

TEST(DeviceOrientation, PermissionSecureContextCheck)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } }
    });
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = [WKWebsiteDataStore defaultDataStore];
    
    bool didReceiveMessage = false;
    RetainPtr messageHandler = addTestMessageHandler(configuration.get(), &didReceiveMessage);
    
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<DeviceOrientationPermissionUIDelegate> uiDelegate = adoptNS([[DeviceOrientationPermissionUIDelegate alloc] initWithHandler:[] { return true; }]);
    [webView setUIDelegate:uiDelegate.get()];
    
    [webView loadRequest:server.request()];
    [webView _test_waitForDidFinishNavigation];
    
    [webView evaluateJavaScript:@"requestPermission();" completionHandler:nil];
    
    TestWebKitAPI::Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    EXPECT_WK_STREQ(@"granted", [messageHandler receivedMessages][0]);
    
    bool addedEventListener = false;
    [webView evaluateJavaScript:@"addEventListener('deviceorientation', (e) => { webkit.messageHandlers.testHandler.postMessage('received-event') });" completionHandler: [&] (id result, NSError *error) {
        addedEventListener = true;
    }];

    TestWebKitAPI::Util::run(&addedEventListener);
    addedEventListener = false;

    // Simulate a device orientation event. The page's event listener should get called even though it did not request permission,
    // because it was previously granted permission during this browsing session.
    [webView _simulateDeviceOrientationChangeWithAlpha:1.0 beta:2.0 gamma:3.0];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    EXPECT_WK_STREQ(@"received-event", [messageHandler receivedMessages][1]);
}

@interface DeviceOrientationPermissionValidationDelegate : NSObject <WKUIDelegatePrivate>
- (void)setValidationHandler:(Function<void(WKSecurityOrigin*, WKFrameInfo*)>&&)validationHandler;
@end

@implementation DeviceOrientationPermissionValidationDelegate {
Function<void(WKSecurityOrigin*, WKFrameInfo*)> _validationHandler;
}
- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    askedClientForPermission = false;
    return self;
}

- (void)setValidationHandler:(Function<void(WKSecurityOrigin*, WKFrameInfo*)>&&)validationHandler {
    _validationHandler = WTF::move(validationHandler);
}

- (void)webView:(WKWebView *)webView requestDeviceOrientationAndMotionPermissionForOrigin:(WKSecurityOrigin*)origin initiatedByFrame:(WKFrameInfo *)frame decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler {
    if (_validationHandler)
        _validationHandler(origin, frame);

    askedClientForPermission  = true;
    decisionHandler(WKPermissionDecisionGrant);
}
@end

static constexpr auto mainFrameText = R"DOCDOCDOC(
<html><body>
<iframe src='https://127.0.0.1:9091/frame' allow='accelerometer:https://127.0.0.1:9091;gyroscope:https://127.0.0.1:9091;magnetometer:https://127.0.0.1:9091'></iframe>
</body></html>
)DOCDOCDOC"_s;
static constexpr auto frameText = R"DOCDOCDOC(
<html><body></body></html>
)DOCDOCDOC"_s;

TEST(WebKit, DeviceOrientationPermissionInIFrame)
{
    TestWebKitAPI::HTTPServer server1({
        { "/"_s, { mainFrameText } }
    }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, nullptr, 9090);

    TestWebKitAPI::HTTPServer server2({
        { "/frame"_s, { frameText } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, nullptr, 9091);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr permissionDelegate = adoptNS([[DeviceOrientationPermissionValidationDelegate alloc] init]);
    [webView setUIDelegate:permissionDelegate.get()];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    __block bool didFinishNavigation = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];
    RetainPtr<WKFrameInfo> frame;
    [navigationDelegate setDecidePolicyForNavigationAction:[&](WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if (action.targetFrame && !action.targetFrame.isMainFrame)
            frame = action.targetFrame;
        decisionHandler(WKNavigationActionPolicyAllow);
    }];

    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server1.request()];
    TestWebKitAPI::Util::run(&didFinishNavigation);

    [permissionDelegate setValidationHandler:[&webView](WKSecurityOrigin *origin, WKFrameInfo *frame) {
        EXPECT_WK_STREQ(origin.protocol, @"https");
        EXPECT_WK_STREQ(origin.host, @"127.0.0.1");
        EXPECT_EQ(origin.port, 9090);

        EXPECT_WK_STREQ(frame.securityOrigin.protocol, @"https");
        EXPECT_WK_STREQ(frame.securityOrigin.host, @"127.0.0.1");
        EXPECT_EQ(frame.securityOrigin.port, 9091);
        EXPECT_FALSE(frame.isMainFrame);
        EXPECT_TRUE(frame.webView == webView);
    }];

    [webView evaluateJavaScript:@"DeviceOrientationEvent.requestPermission()" inFrame:frame.get() inContentWorld:WKContentWorld.pageWorld completionHandler: [&] (id result, NSError *error) { }];

    TestWebKitAPI::Util::run(&askedClientForPermission);

}

static constexpr auto requestPermissionPageBytes = R"TESTRESOURCE(
<script>
function requestPermission() {
    DeviceOrientationEvent.requestPermission().then((result) => {
        webkit.messageHandlers.testHandler.postMessage(result);
    }).catch(() => {
        webkit.messageHandlers.testHandler.postMessage('error');
    });
}
</script>
)TESTRESOURCE"_s;

TEST(DeviceOrientation, PermissionOriginDuringPendingNavigation)
{
    TestWebKitAPI::HTTPServer server({
        { "/source"_s, { requestPermissionPageBytes } },
        { "/target"_s, { "hi"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    bool didReceiveMessage = false;
    RetainPtr messageHandler = addTestMessageHandler(configuration.get(), &didReceiveMessage);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr permissionDelegate = adoptNS([[DeviceOrientationPermissionValidationDelegate alloc] init]);
    [webView setUIDelegate:permissionDelegate.get()];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example1.com/source"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Start permission request on example1.com after navigation to example2.com starts
    // and active URL is changed to example2.com.
    navigationDelegate.get().didStartProvisionalNavigation = ^(WKWebView *view, WKNavigation *) {
        [view evaluateJavaScript:@"requestPermission();" completionHandler:nil];
    };
    bool receivedPermissionRequest = false;
    [permissionDelegate setValidationHandler:[&](WKSecurityOrigin *origin, WKFrameInfo *frame) {
        EXPECT_WK_STREQ(origin.protocol, @"https");
        EXPECT_WK_STREQ(origin.host, @"example1.com");
        EXPECT_WK_STREQ(frame.securityOrigin.protocol, @"https");
        EXPECT_WK_STREQ(frame.securityOrigin.host, @"example1.com");
        receivedPermissionRequest = true;
    }];
    [webView evaluateJavaScript:@"location.href = 'https://example2.com/target'" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedPermissionRequest);

    TestWebKitAPI::Util::run(&didReceiveMessage);
    EXPECT_WK_STREQ(@"granted", [messageHandler receivedMessages][0]);
}

#endif // ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
