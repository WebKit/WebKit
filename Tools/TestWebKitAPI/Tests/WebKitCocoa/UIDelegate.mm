/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "IOSMouseEventTestHarness.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKContext.h>
#import <WebKit/WKContextPrivateMac.h>
#import <WebKit/WKGeolocationManager.h>
#import <WebKit/WKGeolocationPosition.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebViewPrivateForTestingMac.h>
#import <wtf/darwin/DispatchExtras.h>

#if ENABLE(POINTER_LOCK)
#import <GameController/GameController.h>

@interface GCMouse ()
- (instancetype)initWithName:(NSString *)name additionalButtons:(uint32_t)additionalButtons;
@end

@interface GCMouseInput ()
- (void)handleMouseMovementEventWithDelta:(CGPoint)delta;
@end
#endif

#import <WebKit/_WKFeature.h>
#import <WebKit/_WKHitTestResult.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#if PLATFORM(MAC)
#import <Carbon/Carbon.h>
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)
#import "UIKitSPIForTesting.h"
#import "UIKitUtilities.h"
#endif

static bool didReceiveMessage;

@interface AudioObserver : NSObject
@end

@implementation AudioObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void *)context
{
    EXPECT_TRUE([keyPath isEqualToString:NSStringFromSelector(@selector(_isPlayingAudio))]);
    EXPECT_TRUE([[object class] isEqual:[TestWKWebView class]]);
    EXPECT_FALSE([[change objectForKey:NSKeyValueChangeOldKey] boolValue]);
    EXPECT_TRUE([[change objectForKey:NSKeyValueChangeNewKey] boolValue]);
    EXPECT_TRUE(context == nullptr);
    done = true;
}

@end

TEST(WebKit, WKWebViewIsPlayingAudio)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:adoptNS([[WKWebViewConfiguration alloc] init]).get()]);
    auto observer = adoptNS([[AudioObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:@"_isPlayingAudio" options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld context:nil];
    [webView synchronouslyLoadTestPageNamed:@"file-with-video"];
    [webView evaluateJavaScript:@"playVideo()" completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
    [webView removeObserver:observer.get() forKeyPath:@"_isPlayingAudio"];
}

@interface NoUIDelegate : NSObject <WKNavigationDelegate>
@end

@implementation NoUIDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if ([navigationAction.request.URL.absoluteString isEqualToString:[[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"] absoluteString]])
        done = true;
    decisionHandler(WKNavigationActionPolicyAllow);
}

@end

TEST(WebKit, WindowOpenWithoutUIDelegate)
{
    done = false;
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto delegate = adoptNS([[NoUIDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadHTMLString:@"<script>window.open('simple2.html');window.location='simple.html'</script>" baseURL:[NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"]];
    TestWebKitAPI::Util::run(&done);
}

@interface GeolocationDelegate : NSObject <WKUIDelegatePrivate> {
    bool _allowGeolocation;
    Function<void(WKFrameInfo*)> _validationHandler;
}

- (id)initWithAllowGeolocation:(bool)allowGeolocation;
- (void)setValidationHandler:(Function<void(WKFrameInfo*)>&&)validationHandler;
@end

@implementation GeolocationDelegate

- (id)initWithAllowGeolocation:(bool)allowGeolocation
{
    if (!(self = [super init]))
        return nil;
    _allowGeolocation = allowGeolocation;
    return self;
}

- (void)setValidationHandler:(Function<void(WKFrameInfo*)>&&)validationHandler {
    _validationHandler = WTFMove(validationHandler);
}

- (void)_webView:(WKWebView *)webView requestGeolocationPermissionForFrame:(WKFrameInfo *)frame decisionHandler:(void (^)(BOOL allowed))decisionHandler
{
    if (_validationHandler)
        _validationHandler(frame);
    decisionHandler(_allowGeolocation);
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    if (_allowGeolocation)
        EXPECT_STREQ(message.UTF8String, "position 50.644358 3.345453");
    else
        EXPECT_STREQ(message.UTF8String, "error 1 User denied Geolocation");
    completionHandler();
    done = true;
}

@end

TEST(WebKit, GeolocationPermission)
{
    NSString *html = @"<script>navigator.geolocation.watchPosition("
        "function(p) { alert('position ' + p.coords.latitude + ' ' + p.coords.longitude) },"
        "function(e) { alert('error ' + e.code + ' ' + e.message) })"
    "</script>";

    auto pool = adoptNS([[WKProcessPool alloc] init]);
    
    WKGeolocationProviderV1 providerCallback;
    zeroBytes(providerCallback);
    providerCallback.base.version = 1;
    providerCallback.startUpdating = [] (WKGeolocationManagerRef manager, const void*) {
        WKGeolocationManagerProviderDidChangePosition(manager, adoptWK(WKGeolocationPositionCreate(0, 50.644358, 3.345453, 2.53)).get());
    };
    WKGeolocationManagerSetProvider(WKContextGetGeolocationManager((WKContextRef)pool.get()), &providerCallback.base);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = pool.get();

    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSURL *requestURL = [task request].URL;
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:@"text/html" expectedContentLength:[html length] textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[html dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"custom"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate1 = adoptNS([[GeolocationDelegate alloc] initWithAllowGeolocation:false]);
    [webView setUIDelegate:delegate1.get()];

    done = false;
    [delegate1 setValidationHandler:[](WKFrameInfo *frame) {
        EXPECT_TRUE(frame.isMainFrame);
        EXPECT_STREQ(frame.request.URL.absoluteString.UTF8String, "https://example.com/");
        EXPECT_EQ(frame.securityOrigin.port, 0);
        EXPECT_STREQ(frame.securityOrigin.protocol.UTF8String, "https");
        EXPECT_STREQ(frame.securityOrigin.host.UTF8String, "example.com");
    }];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://example.com/"]];
    TestWebKitAPI::Util::run(&done);

    done = false;
    auto delegate2 = adoptNS([[GeolocationDelegate alloc] initWithAllowGeolocation:true]);
    [delegate2 setValidationHandler:[](WKFrameInfo *frame) {
        EXPECT_TRUE(frame.isMainFrame);
        EXPECT_STREQ(frame.request.URL.absoluteString.UTF8String, "https://example.org/");
        EXPECT_EQ(frame.securityOrigin.port, 0);
        EXPECT_STREQ(frame.securityOrigin.protocol.UTF8String, "https");
        EXPECT_STREQ(frame.securityOrigin.host.UTF8String, "example.org");
    }];
    [webView setUIDelegate:delegate2.get()];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://example.org/"]];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [delegate2 setValidationHandler:[](WKFrameInfo *frame) {
        EXPECT_TRUE(frame.isMainFrame);
        EXPECT_STREQ(frame.request.URL.absoluteString.UTF8String, "custom://localhost/mainframe.html");
        EXPECT_EQ(frame.securityOrigin.port, 0);
        EXPECT_STREQ(frame.securityOrigin.protocol.UTF8String, "custom");
        EXPECT_STREQ(frame.securityOrigin.host.UTF8String, "localhost");
    }];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"custom://localhost/mainframe.html"]];
    TestWebKitAPI::Util::run(&done);
}

@interface GeolocationDelegateNew : NSObject <WKUIDelegatePrivate>
- (void)setValidationHandler:(Function<void(WKSecurityOrigin*, WKFrameInfo*)>&&)validationHandler;
@end

@implementation GeolocationDelegateNew {
    Function<void(WKSecurityOrigin*, WKFrameInfo*)> _validationHandler;
}
- (void)setValidationHandler:(Function<void(WKSecurityOrigin*, WKFrameInfo*)>&&)validationHandler {
    _validationHandler = WTFMove(validationHandler);
}

- (void)_webView:(WKWebView *)webView requestGeolocationPermissionForOrigin:(WKSecurityOrigin*)origin initiatedByFrame:(WKFrameInfo *)frame decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler {
    if (_validationHandler)
        _validationHandler(origin, frame);

    done  = true;
    decisionHandler(WKPermissionDecisionGrant);
}
@end
 
@interface GeolocationPermissionMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation GeolocationPermissionMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    didReceiveMessage = true;
}
@end

static constexpr auto mainFrameText = R"DOCDOCDOC(
<html><body>
<iframe src='https://127.0.0.1:9091/frame' allow='geolocation:https://127.0.0.1:9091'></iframe>
</body></html>
)DOCDOCDOC"_s;
static constexpr auto frameText = R"DOCDOCDOC(
<html><body><script>
navigator.geolocation.getCurrentPosition(() => { webkit.messageHandlers.testHandler.postMessage("ok") }, () => { webkit.messageHandlers.testHandler.postMessage("ko") });
</script></body></html>
)DOCDOCDOC"_s;

TEST(WebKit, GeolocationPermissionInIFrame)
{
    TestWebKitAPI::HTTPServer server1({
        { "/"_s, { mainFrameText } }
    }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, nullptr, 9090);

    TestWebKitAPI::HTTPServer server2({
        { "/frame"_s, { frameText } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, nullptr, 9091);

    auto pool = adoptNS([[WKProcessPool alloc] init]);

    WKGeolocationProviderV1 providerCallback;
    zeroBytes(providerCallback);
    providerCallback.base.version = 1;
    providerCallback.startUpdating = [] (WKGeolocationManagerRef manager, const void*) {
        WKGeolocationManagerProviderDidChangePosition(manager, adoptWK(WKGeolocationPositionCreate(0, 50.644358, 3.345453, 2.53)).get());
    };
    WKGeolocationManagerSetProvider(WKContextGetGeolocationManager((WKContextRef)pool.get()), &providerCallback.base);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = pool.get();

    auto messageHandler = adoptNS([[GeolocationPermissionMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    auto permissionDelegate = adoptNS([[GeolocationDelegateNew alloc] init]);
    [webView setUIDelegate:permissionDelegate.get()];

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    webView.get().navigationDelegate = navigationDelegate.get();

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

    done = false;
    didReceiveMessage = false;
    [webView loadRequest:server1.request()];
    TestWebKitAPI::Util::run(&didReceiveMessage);
    EXPECT_TRUE(done);
}

static constexpr auto notAllowingMainFrameText = R"DOCDOCDOC(
<html><body>
<iframe src='https://127.0.0.1:9091/frame' allow='geolocation:https://127.0.0.1:9092'></iframe>
</body></html>
)DOCDOCDOC"_s;

TEST(WebKit, GeolocationPermissionInDisallowedIFrame)
{
    TestWebKitAPI::HTTPServer server1({
        { "/"_s, { notAllowingMainFrameText } }
    }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, nullptr, 9090);

    TestWebKitAPI::HTTPServer server2({
        { "/frame"_s, { frameText } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, nullptr, 9091);

    auto pool = adoptNS([[WKProcessPool alloc] init]);

    WKGeolocationProviderV1 providerCallback;
    zeroBytes(providerCallback);
    providerCallback.base.version = 1;
    providerCallback.startUpdating = [] (WKGeolocationManagerRef manager, const void*) {
        WKGeolocationManagerProviderDidChangePosition(manager, adoptWK(WKGeolocationPositionCreate(0, 50.644358, 3.345453, 2.53)).get());
    };
    WKGeolocationManagerSetProvider(WKContextGetGeolocationManager((WKContextRef)pool.get()), &providerCallback.base);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = pool.get();

    auto messageHandler = adoptNS([[GeolocationPermissionMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    auto permissionDelegate = adoptNS([[GeolocationDelegateNew alloc] init]);
    [webView setUIDelegate:permissionDelegate.get()];

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    webView.get().navigationDelegate = navigationDelegate.get();

    done = false;
    didReceiveMessage = false;
    [webView loadRequest:server1.request()];
    TestWebKitAPI::Util::run(&didReceiveMessage);
    EXPECT_FALSE(done);
}

@interface InjectedBundleNodeHandleIsSelectElementDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation InjectedBundleNodeHandleIsSelectElementDelegate

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)())completionHandler
{
    completionHandler();
    done = true;
    ASSERT_STREQ(message.UTF8String, "isSelectElement success");
}

@end

TEST(WebKit, InjectedBundleNodeHandleIsSelectElement)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"InjectedBundleNodeHandleIsSelectElement"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    auto delegate = adoptNS([[InjectedBundleNodeHandleIsSelectElementDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    TestWebKitAPI::Util::run(&done);
}

#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 160000) || PLATFORM(VISION)

static int presentViewControllerCallCount = 0;

static UIViewController *overrideViewControllerForFullscreenPresentation()
{
    ++presentViewControllerCallCount;
    return nil;
}

// Note: Use the legacy 'CaptivePortal' string to avoid losing users choice from earlier releases.
constexpr auto WebKitLockdownModeAlertShownKey = @"WebKitCaptivePortalModeAlertShown";

TEST(WebKit, LockdownModeDefaultFirstUseMessage)
{
    InstanceMethodSwizzler swizzler(UIView.class, @selector(_wk_viewControllerForFullScreenPresentation), reinterpret_cast<IMP>(overrideViewControllerForFullscreenPresentation));

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_FALSE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get() addToWindow:NO]);

    [[NSUserDefaults standardUserDefaults] removeObjectForKey:WebKitLockdownModeAlertShownKey];
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];
    [WKWebView _resetPresentLockdownModeMessage];

    presentViewControllerCallCount = 0;

    [webView addToTestWindow];

    EXPECT_EQ(presentViewControllerCallCount, 0);
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(presentViewControllerCallCount, 1);

    EXPECT_TRUE([[NSUserDefaults standardUserDefaults] boolForKey:WebKitLockdownModeAlertShownKey]);
    
    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:WebKitLockdownModeAlertShownKey];
}

static bool showedNoFirstUseMessage;

@interface NoLockdownFirstUseMessage : NSObject <WKUIDelegatePrivate>
@end

@implementation NoLockdownFirstUseMessage
- (void)webView:(WKWebView *)webView showLockdownModeFirstUseMessage:(NSString *)message completionHandler:(void (^)(WKDialogResult))completionHandler
{
    showedNoFirstUseMessage = true;
    completionHandler(WKDialogResultHandled);
}
@end

TEST(WebKit, LockdownModeNoFirstUseMessage)
{
    InstanceMethodSwizzler swizzler(UIView.class, @selector(_wk_viewControllerForFullScreenPresentation), reinterpret_cast<IMP>(overrideViewControllerForFullscreenPresentation));

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_FALSE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get() addToWindow:NO]);

    [[NSUserDefaults standardUserDefaults] removeObjectForKey:WebKitLockdownModeAlertShownKey];
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];
    [WKWebView _resetPresentLockdownModeMessage];

    presentViewControllerCallCount = 0;
    showedNoFirstUseMessage = false;

    auto delegate = adoptNS([[NoLockdownFirstUseMessage alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView addToTestWindow];

    EXPECT_TRUE(showedNoFirstUseMessage);
    EXPECT_EQ(presentViewControllerCallCount, 0);
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(presentViewControllerCallCount, 0);

    EXPECT_TRUE([[NSUserDefaults standardUserDefaults] boolForKey:WebKitLockdownModeAlertShownKey]);
    
    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:WebKitLockdownModeAlertShownKey];
}

static bool showedCustomFirstUseMessage;
static bool requestFutureFirstUseMessage;

@interface AskAgainFirstUseMessage : NSObject <WKUIDelegatePrivate>
@end

@implementation AskAgainFirstUseMessage
- (void)webView:(WKWebView *)webView showLockdownModeFirstUseMessage:(NSString *)message completionHandler:(void (^)(WKDialogResult))completionHandler
{
    if (requestFutureFirstUseMessage) {
        requestFutureFirstUseMessage = false;
        showedCustomFirstUseMessage = false;
        completionHandler(WKDialogResultShowDefault);
        return;
    }
        
    requestFutureFirstUseMessage = true;
    showedCustomFirstUseMessage = true;
    completionHandler(WKDialogResultAskAgain);
}
@end

TEST(WebKit, LockdownModeAskAgainFirstUseMessage)
{
    InstanceMethodSwizzler swizzler(UIView.class, @selector(_wk_viewControllerForFullScreenPresentation), reinterpret_cast<IMP>(overrideViewControllerForFullscreenPresentation));

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_FALSE(webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled);
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get() addToWindow:NO]);

    [[NSUserDefaults standardUserDefaults] removeObjectForKey:WebKitLockdownModeAlertShownKey];
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];
    [WKWebView _resetPresentLockdownModeMessage];

    presentViewControllerCallCount = 0;
    showedCustomFirstUseMessage = false;
    requestFutureFirstUseMessage = false;

    auto delegate = adoptNS([[AskAgainFirstUseMessage alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView addToTestWindow];

    EXPECT_EQ(presentViewControllerCallCount, 0);
    EXPECT_TRUE(showedCustomFirstUseMessage);
    EXPECT_TRUE(requestFutureFirstUseMessage);

    EXPECT_FALSE([[NSUserDefaults standardUserDefaults] boolForKey:WebKitLockdownModeAlertShownKey]);

    // Load a new view and ask again:
    auto secondWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get() addToWindow:NO]);

    [secondWebView setUIDelegate:delegate.get()];
    [secondWebView addToTestWindow];

    EXPECT_EQ(presentViewControllerCallCount, 0);
    [secondWebView waitForNextPresentationUpdate];
    EXPECT_EQ(presentViewControllerCallCount, 1);

    EXPECT_FALSE(showedCustomFirstUseMessage);
    EXPECT_FALSE(requestFutureFirstUseMessage);

    EXPECT_TRUE([[NSUserDefaults standardUserDefaults] boolForKey:WebKitLockdownModeAlertShownKey]);

    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:WebKitLockdownModeAlertShownKey];
}

#endif // (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 160000) || PLATFORM(VISION)

#if PLATFORM(IOS_FAMILY)

static bool gShouldKeepScreenAwake = false;

@interface SetShouldKeepScreenAwakeDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation SetShouldKeepScreenAwakeDelegate

- (void)_webView:(WKWebView *)webView setShouldKeepScreenAwake:(BOOL)shouldKeepScreenAwake
{
    EXPECT_NE(gShouldKeepScreenAwake, shouldKeepScreenAwake);
    gShouldKeepScreenAwake = shouldKeepScreenAwake;
}

@end

TEST(WebKit, SetShouldKeepScreenAwake)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([SetShouldKeepScreenAwakeDelegate new]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView evaluateJavaScript:@"navigator.wakeLock.request('screen').then((wakeLock) => { window.lock = wakeLock; }); true;" completionHandler:nil];
    TestWebKitAPI::Util::run(&gShouldKeepScreenAwake);
    [webView evaluateJavaScript:@"window.lock.release(); true;" completionHandler:nil];
    while (gShouldKeepScreenAwake)
        TestWebKitAPI::Util::spinRunLoop(10);
}

TEST(WebKit, SetShouldKeepScreenAwakeLastPageIsClosed)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([SetShouldKeepScreenAwakeDelegate new]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView evaluateJavaScript:@"navigator.wakeLock.request('screen').then((wakeLock) => { window.lock = wakeLock; }); true;" completionHandler:nil];
    TestWebKitAPI::Util::run(&gShouldKeepScreenAwake);

    [webView evaluateJavaScript:@"navigator.wakeLock.request('screen').then((wakeLock) => { window.lock = wakeLock; }); true;" completionHandler:nil];
    TestWebKitAPI::Util::run(&gShouldKeepScreenAwake);

    // Close the last page while holding the lock.
    [webView _close];
    webView = nil;

    while (gShouldKeepScreenAwake)
        TestWebKitAPI::Util::spinRunLoop(10);
}

TEST(WebKit, SetShouldKeepScreenAwakeWebProcessCrash)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([SetShouldKeepScreenAwakeDelegate new]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView evaluateJavaScript:@"navigator.wakeLock.request('screen').then((wakeLock) => { window.lock = wakeLock; }); true;" completionHandler:nil];
    TestWebKitAPI::Util::run(&gShouldKeepScreenAwake);

    [webView evaluateJavaScript:@"navigator.wakeLock.request('screen').then((wakeLock) => { window.lock = wakeLock; }); true;" completionHandler:nil];
    TestWebKitAPI::Util::run(&gShouldKeepScreenAwake);

    // Kill the WebProcess while holding the screen lock.
    kill([webView _webProcessIdentifier], 9);

    while (gShouldKeepScreenAwake)
        TestWebKitAPI::Util::spinRunLoop(10);
}

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

@class UITestDelegate;

static RetainPtr<WKWebView> webViewFromDelegateCallback;
static RetainPtr<WKWebView> createdWebView;
static RetainPtr<UITestDelegate> delegate;

@interface UITestDelegate : NSObject <WKUIDelegatePrivate, WKURLSchemeHandler>
@end

@implementation UITestDelegate

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    createdWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    [createdWebView setUIDelegate:delegate.get()];
    return createdWebView.get();
}

- (void)_showWebView:(WKWebView *)webView
{
    webViewFromDelegateCallback = webView;
    done = true;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    NSString *data = @"<script>window.open('other.html');</script>";
    [urlSchemeTask didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:urlSchemeTask.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
    [urlSchemeTask didReceiveData:[data dataUsingEncoding:NSUTF8StringEncoding]];
    [urlSchemeTask didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
}

@end

TEST(WebKit, ShowWebView)
{
    delegate = adoptNS([[UITestDelegate alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:delegate.get() forURLScheme:@"test"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///first"]]];
    TestWebKitAPI::Util::run(&done);
    
    ASSERT_EQ(webViewFromDelegateCallback, createdWebView);
}

static bool receivedWindowFrame;

@interface WindowFrameDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation WindowFrameDelegate

- (void)_webView:(WKWebView *)webView setWindowFrame:(CGRect)frame
{
    EXPECT_EQ(frame.origin.x, 160);
    EXPECT_EQ(frame.origin.y, 230);
    EXPECT_EQ(frame.size.width, 350);
    EXPECT_EQ(frame.size.height, 450);
    receivedWindowFrame = true;
}

- (void)_webView:(WKWebView *)webView getWindowFrameWithCompletionHandler:(void (^)(CGRect))completionHandler
{
    completionHandler(CGRectMake(150, 250, 350, 450));
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    EXPECT_STREQ("350", message.UTF8String);
    completionHandler();
    done = true;
}

@end

TEST(WebKit, WindowFrame)
{
    auto delegate = adoptNS([[WindowFrameDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:@"<script>moveBy(10,20);alert(outerWidth);</script>" baseURL:nil];
    TestWebKitAPI::Util::run(&receivedWindowFrame);
    TestWebKitAPI::Util::run(&done);
}

static bool headerHeightCalled;
static bool footerHeightCalled;
static bool drawHeaderCalled;
static bool drawFooterCalled;

@interface PrintDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation PrintDelegate

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame
{
    done = true;
}

- (CGFloat)_webViewHeaderHeight:(WKWebView *)webView
{
    headerHeightCalled = true;
    return 3.14159;
}

- (CGFloat)_webViewFooterHeight:(WKWebView *)webView
{
    footerHeightCalled = true;
    return 2.71828;
}

- (void)_webView:(WKWebView *)webView drawHeaderInRect:(CGRect)rect forPageWithTitle:(NSString *)title URL:(NSURL *)url
{
    EXPECT_EQ(rect.origin.x, 72);
    EXPECT_TRUE(std::abs(rect.origin.y - 698.858398) < .00001);
    EXPECT_TRUE(std::abs(rect.size.height - 3.141590) < .00001);
    EXPECT_EQ(rect.size.width, 468.000000);
    EXPECT_STREQ(title.UTF8String, "test_title");
    EXPECT_STREQ(url.absoluteString.UTF8String, "http://example.com/");
    drawHeaderCalled = true;
}

- (void)_webView:(WKWebView *)webView drawFooterInRect:(CGRect)rect forPageWithTitle:(NSString *)title URL:(NSURL *)url
{
    EXPECT_EQ(rect.origin.x, 72);
    EXPECT_EQ(rect.origin.y, 90);
    EXPECT_TRUE(std::abs(rect.size.height - 2.718280) < .00001);
    EXPECT_EQ(rect.size.width, 468.000000);
    EXPECT_STREQ(url.absoluteString.UTF8String, "http://example.com/");
    drawFooterCalled = true;
}

@end

TEST(WebKit, PrintFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto delegate = adoptNS([[PrintDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:@"<head><title>test_title</title></head><body onload='setTimeout(function() { print() });'>hello world!</body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&done);

    NSPrintOperation *operation = [webView _printOperationWithPrintInfo:[NSPrintInfo sharedPrintInfo]];
    EXPECT_TRUE(operation.canSpawnSeparateThread);
    EXPECT_STREQ(operation.jobTitle.UTF8String, "test_title");

    [operation runOperationModalForWindow:[webView hostWindow] delegate:nil didRunSelector:nil contextInfo:nil];
    TestWebKitAPI::Util::run(&headerHeightCalled);
    TestWebKitAPI::Util::run(&footerHeightCalled);
    TestWebKitAPI::Util::run(&drawHeaderCalled);
    TestWebKitAPI::Util::run(&drawFooterCalled);
}

TEST(WebKit, PrintPreview)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto delegate = adoptNS([[PrintDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:@"<head><title>test_title</title></head><body onload='print()'>hello world!</body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&done);

    NSPrintOperation *operation = [webView _printOperationWithPrintInfo:[NSPrintInfo sharedPrintInfo]];
    NSPrintOperation.currentOperation = operation;
    auto previewView = [operation view];
    [webView _close];
    [previewView drawRect:CGRectMake(0, 0, 10, 10)];
}

@interface PrintDelegateWithCompletionHandler : NSObject <WKUIDelegatePrivate>
- (void)waitForPrintFrameCall;
@end

@implementation PrintDelegateWithCompletionHandler {
    bool _done;
}

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame pdfFirstPageSize:(CGSize)size completionHandler:(void (^)(void))completionHandler
{
    completionHandler();
    _done = true;
}

- (void)waitForPrintFrameCall
{
    while (!_done)
        TestWebKitAPI::Util::spinRunLoop();
}

@end

TEST(WebKit, PrintWithCompletionHandler)
{
    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([PrintDelegateWithCompletionHandler new]);
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:@"<head><title>test_title</title></head><body onload='print()'>hello world!</body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    [delegate waitForPrintFrameCall];
}

@interface NotificationDelegate : NSObject <WKUIDelegatePrivate> {
    bool _allowNotifications;
}
- (id)initWithAllowNotifications:(bool)allowNotifications;
@end

@implementation NotificationDelegate

- (id)initWithAllowNotifications:(bool)allowNotifications
{
    if (!(self = [super init]))
        return nil;
    _allowNotifications = allowNotifications;
    return self;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    if (_allowNotifications)
        EXPECT_STREQ(message.UTF8String, "permission granted");
    else
        EXPECT_STREQ(message.UTF8String, "permission denied");
    completionHandler();
    done = true;
}

- (void)_webView:(WKWebView *)webView requestNotificationPermissionForSecurityOrigin:(WKSecurityOrigin *)securityOrigin decisionHandler:(void (^)(BOOL))decisionHandler
{
    if (_allowNotifications)
        EXPECT_STREQ(securityOrigin.host.UTF8String, "example.org");
    else
        EXPECT_STREQ(securityOrigin.host.UTF8String, "example.com");
    decisionHandler(_allowNotifications);
}

@end

TEST(WebKit, NotificationPermission)
{
    NSString *html = @"<script>function requestPermission() { Notification.requestPermission(function(p){alert('permission '+p)}); }</script>";
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:adoptNS([[WKWebViewConfiguration alloc] init]).get()]);
    auto uiDelegate = adoptNS([[NotificationDelegate alloc] initWithAllowNotifications:YES]);
    [webView setUIDelegate:uiDelegate.get()];
    [webView synchronouslyLoadHTMLString:html baseURL:[NSURL URLWithString:@"https://example.org"]];
    [webView evaluateJavaScript:@"requestPermission()" completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
    done = false;
    uiDelegate = adoptNS([[NotificationDelegate alloc] initWithAllowNotifications:NO]);
    [webView setUIDelegate:uiDelegate.get()];
    [webView synchronouslyLoadHTMLString:html baseURL:[NSURL URLWithString:@"https://example.com"]];
    [webView evaluateJavaScript:@"requestPermission()" completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, ToolbarVisible)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:adoptNS([[WKWebViewConfiguration alloc] init]).get()]);
    [webView loadHTMLString:@"<script>alert('visible:' + window.toolbar.visible)</script>" baseURL:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "visible:true");
    webView.get()._toolbarsAreVisible = NO;
    [webView evaluateJavaScript:@"alert('visible:' + window.toolbar.visible)" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "visible:false");
}

@interface MouseMoveOverElementDelegate : NSObject <WKUIDelegatePrivate>
@property (nonatomic, copy) void (^mouseDidMoveOverElement)(_WKHitTestResult *, NSEventModifierFlags, id<NSSecureCoding>);
@property (nonatomic, copy) WKWebView* (^createWebViewWithConfiguration)(WKWebViewConfiguration *, WKNavigationAction *, WKWindowFeatures *);
@end

@implementation MouseMoveOverElementDelegate

- (void)_webView:(WKWebView *)webview mouseDidMoveOverElement:(_WKHitTestResult *)hitTestResult withFlags:(NSEventModifierFlags)flags userInfo:(id <NSSecureCoding>)userInfo
{
    _mouseDidMoveOverElement(hitTestResult, flags, userInfo);
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    return _createWebViewWithConfiguration(configuration, navigationAction, windowFeatures);
}

@end

TEST(WebKit, MouseMoveOverElement)
{
    __block bool done { false };
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr uiDelegate = adoptNS([MouseMoveOverElementDelegate new]);
    uiDelegate.get().mouseDidMoveOverElement = ^(_WKHitTestResult *hitTestResult, NSEventModifierFlags flags, id<NSSecureCoding> userInfo) {
        EXPECT_STREQ(hitTestResult.absoluteLinkURL.absoluteString.UTF8String, "http://example.com/path");
        EXPECT_STREQ(hitTestResult.linkLabel.UTF8String, "link label");
        EXPECT_STREQ(hitTestResult.linkTitle.UTF8String, "link title");
        EXPECT_EQ(flags, NSEventModifierFlagShift);
        EXPECT_NULL(userInfo);
        EXPECT_TRUE(hitTestResult.linkTargetFrameIsSameAsLinkFrame);
        EXPECT_TRUE(hitTestResult.linkHasTargetFrame);
        done = true;
    };
    [webView setUIDelegate:uiDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<a href='http://example.com/path' title='link title'>link label</a>"];
    [webView mouseMoveToPoint:NSMakePoint(20, 600 - 20) withFlags:NSEventModifierFlagShift];
    TestWebKitAPI::Util::run(&done);

    done = false;
    uiDelegate.get().mouseDidMoveOverElement = ^(_WKHitTestResult *hitTestResult, NSEventModifierFlags, id<NSSecureCoding>) {
        EXPECT_FALSE(hitTestResult.linkTargetFrameIsSameAsLinkFrame);
        EXPECT_TRUE(hitTestResult.linkHasTargetFrame);
        EXPECT_FALSE(hitTestResult.linkTargetFrameIsInDifferentWebView);
        done = true;
    };
    [webView synchronouslyLoadHTMLString:@"<a href='http://example.com/path' title='link title' target='testiframe'>link label</a><iframe name='testiframe' style='height:1px;width:1px'></iframe>"];
    [webView mouseMoveToPoint:NSMakePoint(20, 600 - 20) withFlags:NSEventModifierFlagShift];
    TestWebKitAPI::Util::run(&done);

    done = false;
    uiDelegate.get().mouseDidMoveOverElement = ^(_WKHitTestResult *hitTestResult, NSEventModifierFlags, id<NSSecureCoding>) {
        EXPECT_FALSE(hitTestResult.linkTargetFrameIsSameAsLinkFrame);
        EXPECT_FALSE(hitTestResult.linkHasTargetFrame);
        done = true;
    };
    [webView mouseMoveToPoint:NSMakePoint(300, 300) withFlags:NSEventModifierFlagShift];
    TestWebKitAPI::Util::run(&done);

    done = false;
    uiDelegate.get().mouseDidMoveOverElement = ^(_WKHitTestResult *hitTestResult, NSEventModifierFlags, id<NSSecureCoding>) {
        EXPECT_FALSE(hitTestResult.linkTargetFrameIsSameAsLinkFrame);
        EXPECT_TRUE(hitTestResult.linkHasTargetFrame);
        EXPECT_TRUE(hitTestResult.linkTargetFrameIsInDifferentWebView);
        done = true;
    };
    __block bool opened { false };
    uiDelegate.get().createWebViewWithConfiguration = ^WKWebView *(WKWebViewConfiguration * configuration, WKNavigationAction *navigationAction, WKWindowFeatures *) {
        static RetainPtr<WKWebView> openedView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened = true;
        return openedView.get();
    };
    [webView synchronouslyLoadHTMLString:@"<a href='http://example.com/path' title='link title' target='testtarget'>link label</a><script>window.open('about:blank', 'testtarget')</script>"];
    TestWebKitAPI::Util::run(&opened);
    [webView mouseMoveToPoint:NSMakePoint(20, 600 - 20) withFlags:NSEventModifierFlagShift];
    TestWebKitAPI::Util::run(&done);
}

static BlockPtr<NSEvent*(NSEvent*)> gEventMonitorHandler;

@interface TestLocalEventObserver : NSObject {
    NSEventMask _mask;
    id _block;
    BOOL _isAdditive;
}
+ (void)initialize;
- (instancetype)initMatchingEvents:(NSEventMask)mask handler:(NSEvent *(^)(NSEvent *))block;
- (void)invalidate;
- (void)dealloc;
- (void)recomputeObserverMask;
@end

@implementation TestLocalEventObserver
+ (void)initialize
{
}

- (instancetype)initMatchingEvents:(NSEventMask)mask handler:(NSEvent *(^)(NSEvent *))block
{
    self = [super init];
    return self;
}

- (void)dealloc
{
    [super dealloc];
}

- (void)invalidate
{
}

- (void)recomputeObserverMask
{
}
@end

@interface TestEventMonitor : NSObject

+ (id)addLocalMonitorForEventsMatchingMask:(NSEventMask)mask handler:(NSEvent* (^)(NSEvent *event))block;

@end

@implementation TestEventMonitor

+ (id)addLocalMonitorForEventsMatchingMask:(NSEventMask)mask handler:(NSEvent* (^)(NSEvent *event))block
{
    gEventMonitorHandler = makeBlockPtr(block);
    return adoptNS([[TestLocalEventObserver alloc] initMatchingEvents:mask handler:block]).leakRef();
}

@end

TEST(WebKit, MouseMoveOverElementWithClosedWebView)
{
    auto linkLocation = NSMakePoint(200, 150);

    ClassMethodSwizzler localMonitorSwizzler(NSEvent.class, @selector(addLocalMonitorForEventsMatchingMask:handler:), [TestEventMonitor methodForSelector:@selector(addLocalMonitorForEventsMatchingMask:handler:)]);

    // We swizzle `NSWindow.mouseLocationOutsideOfEventStream` because manually calling the handler intercepted
    // from the swizzled `+[NSEvent addLocalMonitorForEventsMatchingMask:handler:]` with a fake event means we
    // skip over the bookkeeping required for this SPI to provide the correct mouse location.
    InstanceMethodSwizzler mouseLocationSwizzler(NSWindow.class, @selector(mouseLocationOutsideOfEventStream), imp_implementationWithBlock(^{
        return linkLocation;
    }));

    __block bool done { false };
    @autoreleasepool {
        RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:configuration.get()]);
        [webView removeFromSuperview];
        [webView addToTestWindow];
        RetainPtr uiDelegate = adoptNS([MouseMoveOverElementDelegate new]);
        uiDelegate.get().mouseDidMoveOverElement = ^(_WKHitTestResult *hitTestResult, NSEventModifierFlags flags, id<NSSecureCoding> userInfo) {
            EXPECT_STREQ(hitTestResult.absoluteLinkURL.absoluteString.UTF8String, "http://example.com/path");
            EXPECT_STREQ(hitTestResult.linkLabel.UTF8String, "link label");
            EXPECT_STREQ(hitTestResult.linkTitle.UTF8String, "link title");
            EXPECT_EQ(flags, NSEventModifierFlagShift);
            EXPECT_NULL(userInfo);
            EXPECT_TRUE(hitTestResult.linkTargetFrameIsSameAsLinkFrame);
            EXPECT_TRUE(hitTestResult.linkHasTargetFrame);
            done = true;
        };
        [webView setUIDelegate:uiDelegate.get()];
        [webView synchronouslyLoadHTMLString:@"<a id='link' href='http://example.com/path' style='font-size: 300px;' title='link title'>link label</a>"];

        [webView _createFlagsChangedEventMonitorForTesting];

        [webView mouseMoveToPoint:linkLocation withFlags:NSEventModifierFlagShift];
        [webView waitForNextPresentationUpdate];
        // This test just verifies that attempting to asynchronously dispatch a mouseDidMoveOverElement
        // update when the WKWebView and its page client have been destructed does not trigger a crash.
        gEventMonitorHandler([NSEvent mouseEventWithType:NSEventTypeMouseMoved location:linkLocation modifierFlags:0 timestamp:0 windowNumber:[[webView hostWindow] windowNumber] context:nil eventNumber:0 clickCount:0 pressure:0]);
        [webView removeFromSuperview];

        EXPECT_FALSE([webView _hasFlagsChangedEventMonitorForTesting]);
    }

    TestWebKitAPI::Util::runFor(10_ms);
    TestWebKitAPI::Util::run(&done);
}

static bool readyForClick;

@interface AutoFillDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation AutoFillDelegate

- (void)_webView:(WKWebView *)webView didClickAutoFillButtonWithUserInfo:(id <NSSecureCoding>)userInfo
{
    done = true;
    ASSERT_TRUE([(id<NSObject>)userInfo isKindOfClass:[NSString class]]);
    ASSERT_STREQ([(NSString*)userInfo UTF8String], "user data string");
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    completionHandler();
    ASSERT_STREQ(message.UTF8String, "ready for click!");
    readyForClick = true;
}

@end

TEST(WebKit, ClickAutoFillButton)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ClickAutoFillButton"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    auto delegate = adoptNS([[AutoFillDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView evaluateJavaScript:@"" completionHandler: nil]; // Ensure the WebProcess and injected bundle are running.
    TestWebKitAPI::Util::run(&readyForClick);
    NSPoint buttonLocation = NSMakePoint(130, 577);
    [webView mouseDownAtPoint:buttonLocation simulatePressure:NO];
    [webView mouseUpAtPoint:buttonLocation];
    TestWebKitAPI::Util::run(&done);
}

static bool readytoResign;

@interface DidResignInputElementStrongPasswordAppearanceDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation DidResignInputElementStrongPasswordAppearanceDelegate

- (void)_webView:(WKWebView *)webView didResignInputElementStrongPasswordAppearanceWithUserInfo:(id <NSSecureCoding>)userInfo
{
    done = true;
    ASSERT_TRUE([(id<NSObject>)userInfo isKindOfClass:[NSString class]]);
    ASSERT_STREQ([(NSString*)userInfo UTF8String], "user data string");
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    completionHandler();
    ASSERT_STREQ(message.UTF8String, "ready to resign!");
    readytoResign = true;
}

@end

@interface AutoFillAvailableDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation AutoFillAvailableDelegate

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)())completionHandler
{
    completionHandler();
    done = true;
    ASSERT_STREQ(message.UTF8String, "autofill available");
}

@end

TEST(WebKit, AutoFillAvailable)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"AutoFillAvailable"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    auto delegate = adoptNS([[AutoFillAvailableDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView evaluateJavaScript:@"" completionHandler: nil]; // Ensure the WebProcess and injected bundle are running.
    TestWebKitAPI::Util::run(&done);
}

@interface InjectedBundleNodeHandleIsTextFieldDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation InjectedBundleNodeHandleIsTextFieldDelegate

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)())completionHandler
{
    completionHandler();
    done = true;
    ASSERT_STREQ(message.UTF8String, "isTextField success");
}

@end

TEST(WebKit, InjectedBundleNodeHandleIsTextField)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"InjectedBundleNodeHandleIsTextField"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    auto delegate = adoptNS([[InjectedBundleNodeHandleIsTextFieldDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    TestWebKitAPI::Util::run(&done);
}

@interface PinnedStateObserver : NSObject
@end

@implementation PinnedStateObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void *)context
{
    EXPECT_TRUE([keyPath isEqualToString:NSStringFromSelector(@selector(_pinnedState))]);
    EXPECT_TRUE([[object class] isEqual:[TestWKWebView class]]);
    EXPECT_EQ([[change objectForKey:NSKeyValueChangeOldKey] unsignedIntegerValue], _WKRectEdgeAll);
    EXPECT_EQ([[change objectForKey:NSKeyValueChangeNewKey] unsignedIntegerValue], _WKRectEdgeLeft | _WKRectEdgeRight);
    EXPECT_TRUE(context == nullptr);
    done = true;
}

@end

TEST(WebKit, PinnedState)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto observer = adoptNS([[PinnedStateObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:@"_pinnedState" options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld context:nil];
    [webView loadHTMLString:@"<body onload='scroll(100, 100)' style='height:10000vh;'/>" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&done);
    [webView removeObserver:observer.get() forKeyPath:@"_pinnedState"];
}

@interface DidScrollDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation DidScrollDelegate

- (void)_webViewDidScroll:(WKWebView *)webView
{
    done = true;
}

@end

TEST(WebKit, DidScroll)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto delegate = adoptNS([[DidScrollDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:@"<body onload='scroll(100, 100)' style='height:10000vh;'/>" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&done);
}

static NSEvent *tabEvent(NSWindow *window, NSEventType type, NSEventModifierFlags flags)
{
    return [NSEvent keyEventWithType:type location:NSMakePoint(5, 5) modifierFlags:flags timestamp:GetCurrentEventTime() windowNumber:[window windowNumber] context:[NSGraphicsContext currentContext] characters:@"\t" charactersIgnoringModifiers:@"\t" isARepeat:NO keyCode:0];
}

static void synthesizeTab(NSWindow *window, NSView *view, bool withShiftDown)
{
    [view keyDown:tabEvent(window, NSEventTypeKeyDown, withShiftDown ? NSEventModifierFlagShift : 0)];
    [view keyUp:tabEvent(window, NSEventTypeKeyUp, withShiftDown ? NSEventModifierFlagShift : 0)];
}

@interface FocusDelegate : NSObject <WKUIDelegatePrivate>
@property (nonatomic) _WKFocusDirection takenDirection;
@property (nonatomic) BOOL useShiftTab;
@end

@implementation FocusDelegate {
@package
    bool _done;
    bool _didSendKeyEvent;
}

- (void)_webView:(WKWebView *)webView takeFocus:(_WKFocusDirection)direction
{
    _takenDirection = direction;
    _done = true;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    completionHandler();
    _didSendKeyEvent = true;
    synthesizeTab([webView window], webView, _useShiftTab);
}

@end

TEST(WebKit, Focus)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto delegate = adoptNS([[FocusDelegate alloc] init]);
    [delegate setUseShiftTab:YES];
    [webView setUIDelegate:delegate.get()];
    NSString *html = @"<script>function loaded() { document.getElementById('in').focus(); alert('ready'); }</script>"
    "<body onload='loaded()'><input type='text' id='in'></body>";
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&delegate->_done);
    ASSERT_EQ([delegate takenDirection], _WKFocusDirectionBackward);
}

TEST(WebKit, ShiftTabTakesFocusFromEditableWebView)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView _setEditable:YES];

    auto delegate = adoptNS([[FocusDelegate alloc] init]);
    [delegate setUseShiftTab:YES];
    [webView setUIDelegate:delegate.get()];
    NSString *html = @"<script>function loaded() { document.body.focus(); alert('ready'); }</script>"
    "<body onload='loaded()'></body>";
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&delegate->_done);
    ASSERT_EQ([delegate takenDirection], _WKFocusDirectionBackward);
}

TEST(WebKit, ShiftTabDoesNotTakeFocusFromEditableWebViewWhenPreventingKeyPress)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView _setEditable:YES];

    auto delegate = adoptNS([[FocusDelegate alloc] init]);
    [delegate setUseShiftTab:YES];
    [webView setUIDelegate:delegate.get()];
    NSString *markup = @"<script>"
        "    function loaded() {"
        "        document.body.focus();"
        "        document.body.addEventListener('keypress', e => e.preventDefault());"
        "        document.body.addEventListener('keyup', () => webkit.messageHandlers.testHandler.postMessage('keyup'));"
        "        alert('ready');"
        "    }"
        "</script>"
        "<body onload='loaded()'></body>";

    __block bool handledKeyUp = false;
    [webView performAfterReceivingMessage:@"keyup" action:^{
        handledKeyUp = true;
    }];
    [webView synchronouslyLoadHTMLString:markup];

    TestWebKitAPI::Util::run(&handledKeyUp);
    EXPECT_FALSE(delegate->_done);
}

TEST(WebKit, TabDoesNotTakeFocusFromEditableWebView)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView _setEditable:YES];

    auto delegate = adoptNS([[FocusDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    NSString *html = @"<script>function loaded() { document.body.focus(); alert('ready'); }</script>"
    "<body onload='loaded()'></body>";
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&delegate->_didSendKeyEvent);
    EXPECT_WK_STREQ("\t", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
    ASSERT_FALSE(delegate->_done);
}

#if ENABLE(PDF_HUD)

@interface SaveDataToFileDelegate : NSObject <WKUIDelegatePrivate, WKNavigationDelegate>
@end

@implementation SaveDataToFileDelegate

- (void)_webView:(WKWebView *)webView saveDataToFile:(NSData *)data suggestedFilename:(NSString *)suggestedFilename mimeType:(NSString *)mimeType originatingURL:(NSURL *)url
{
    NSURL *pdfURL = [NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"];
    EXPECT_TRUE([data isEqualToData:[NSData dataWithContentsOfURL:pdfURL]]);
    EXPECT_STREQ([suggestedFilename UTF8String], "test.pdf");
    EXPECT_STREQ([mimeType UTF8String], "application/pdf");
    EXPECT_STREQ([[url absoluteString] UTF8String], [[pdfURL absoluteString] UTF8String]);
    done = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    [[webView _pdfHUDs].anyObject performSelector:NSSelectorFromString(@"_performActionForControl:") withObject:@"arrow.down.circle"];
}

@end

TEST(WebKit, SaveDataToFile)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"PDFPluginHUDEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setWindowOcclusionDetectionEnabled:NO];
    RetainPtr delegate = adoptNS([[SaveDataToFileDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView setNavigationDelegate:delegate.get()];
    NSURL *pdfURL = [NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"];
    [webView loadRequest:[NSURLRequest requestWithURL:pdfURL]];
    TestWebKitAPI::Util::run(&done);
}

#endif // ENABLE(PDF_HUD)

#define RELIABLE_DID_NOT_HANDLE_WHEEL_EVENT 0
// FIXME: make wheel event handling more reliable.
// https://bugs.webkit.org/show_bug.cgi?id=175967
#if RELIABLE_DID_NOT_HANDLE_WHEEL_EVENT

static void synthesizeWheelEvents(NSView *view, int x, int y)
{
    RetainPtr<CGEventRef> cgScrollEvent = adoptCF(CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 2, y, x));
    NSEvent* event = [NSEvent eventWithCGEvent:cgScrollEvent.get()];
    [view scrollWheel:event];
    
    // Wheel events get coalesced sometimes. Make more events until one is not handled.
    dispatch_async(mainDispatchQueueSingleton(), ^{
        synthesizeWheelEvents(view, x, y);
    });
}

@interface WheelDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation WheelDelegate

- (void)_webView:(WKWebView *)webView didNotHandleWheelEvent:(NSEvent *)event
{
    done = true;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    completionHandler();
    synthesizeWheelEvents(webView, 1, 1);
}

@end

TEST(WebKit, DidNotHandleWheelEvent)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto delegate = adoptNS([[WheelDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:@"<body onload='alert(\"ready\")' onwheel='()=>{}' style='overflow:hidden; height:10000vh;'></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&done);
}

#endif // RELIABLE_DID_NOT_HANDLE_WHEEL_EVENT

#endif // PLATFORM(MAC)

#if ENABLE(POINTER_LOCK)

@interface PointerLockDelegate : NSObject <WKUIDelegatePrivate, WKScriptMessageHandler>
@property (nonatomic, readonly) bool didEngagePointerLock;
@property (nonatomic, readonly) NSArray<NSValue *> *mouseMoveEvents;
- (void)resetState;
- (void)waitForPointerLockEngaged;
- (void)waitForPointerLockLost;
- (void)waitForMouseMoveEvents;
@end

@implementation PointerLockDelegate {
    bool _didLosePointerLock;
    RetainPtr<NSMutableArray<NSValue *>> _mouseMoveEvents;
}

- (instancetype)init
{
    if (self = [super init])
        _mouseMoveEvents = adoptNS([[NSMutableArray alloc] init]);
    return self;
}

- (NSArray<NSValue *> *)mouseMoveEvents
{
    return _mouseMoveEvents.get();
}

- (void)resetState
{
    _didEngagePointerLock = false;
    _didLosePointerLock = false;
    [_mouseMoveEvents removeAllObjects];
}

- (void)waitForPointerLockEngaged
{
    TestWebKitAPI::Util::run(&_didEngagePointerLock);
}

- (void)waitForPointerLockLost
{
    TestWebKitAPI::Util::run(&_didLosePointerLock);
}

- (void)waitForMouseMoveEvents
{
    TestWebKitAPI::Util::waitFor([&] {
        return [_mouseMoveEvents count];
    });
}

- (void)_webViewDidRequestPointerLock:(WKWebView *)webView completionHandler:(void (^)(BOOL))completionHandler
{
    completionHandler(YES);
    _didEngagePointerLock = true;
}

- (void)_webViewDidLosePointerLock:(WKWebView *)webView
{
    _didLosePointerLock = true;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
#if PLATFORM(IOS_FAMILY)
    if ([message.name isEqualToString:@"testHandler"]) {
        NSDictionary *moveData = message.body;
        CGPoint delta = CGPointMake([moveData[@"deltaX"] floatValue], [moveData[@"deltaY"] floatValue]);
        [_mouseMoveEvents addObject:[NSValue valueWithCGPoint:delta]];
    }
#endif
}

@end

#if HAVE(MOUSE_DEVICE_OBSERVATION)

@interface WKMouseDeviceObserver
+ (WKMouseDeviceObserver *)sharedInstance;
- (void)start;
- (void)stop;
- (void)_setHasMouseDeviceForTesting:(BOOL)hasMouseDevice;
@end

#endif

class PointerLockTests : public testing::Test {
public:
    void SetUp() final
    {
#if PLATFORM(IOS_FAMILY)
        TestWebKitAPI::Util::instantiateUIApplicationIfNeeded();
#endif
        setHasMouseDeviceForTesting(true);

#if HAVE(MOUSE_DEVICE_OBSERVATION)
        m_fakeMouse = adoptNS([[GCMouse.class alloc] initWithName:@"TestMouse" additionalButtons:0]);
        m_currentMouseSwizzler = makeUnique<ClassMethodSwizzler>(GCMouse.class, @selector(current),
            imp_implementationWithBlock(^GCMouse *() {
                return m_fakeMouse.get();
            })
        );
        m_miceSwizzler = makeUnique<ClassMethodSwizzler>(GCMouse.class, @selector(mice),
            imp_implementationWithBlock(^NSArray<GCMouse *> *() {
                return @[ m_fakeMouse.get() ];
            })
        );
#endif

        m_webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configurationForWebViewTestingPointerLock().get()]);
        m_delegate = adoptNS([[PointerLockDelegate alloc] init]);
        [m_webView setUIDelegate:m_delegate.get()];
        [[m_webView configuration].userContentController addScriptMessageHandler:m_delegate.get() name:@"testHandler"];
        [m_webView synchronouslyLoadHTMLString:
            @"<canvas width='800' height='600'></canvas><script>"
            @"var canvas = document.querySelector('canvas');"
            @"var mouseMoveEvents = [];"
            @"canvas.onclick = () => canvas.requestPointerLock();"
            @"document.addEventListener('pointermove', (e) => {"
            @"    if (document.pointerLockElement) {"
            @"        mouseMoveEvents.push({deltaX: e.movementX, deltaY: e.movementY, timeStamp: e.timeStamp});"
            @"        window.webkit.messageHandlers.testHandler.postMessage({deltaX: e.movementX, deltaY: e.movementY});"
            @"    }"
            @"});"
            @"</script>"
        ];

        [m_webView focus];
    }

    static void SetUpTestSuite()
    {
#if HAVE(MOUSE_DEVICE_OBSERVATION)
        [sharedMouseDeviceObserver() start];
#endif
    }

    static void TearDownTestSuite()
    {
#if HAVE(MOUSE_DEVICE_OBSERVATION)
        [sharedMouseDeviceObserver() stop];
#endif
    }

    void setHasMouseDeviceForTesting(bool hasMouseDeviceForTesting)
    {
#if HAVE(MOUSE_DEVICE_OBSERVATION)
        [sharedMouseDeviceObserver() _setHasMouseDeviceForTesting:hasMouseDeviceForTesting];
#endif
        UNUSED_PARAM(hasMouseDeviceForTesting);
    }

    void click(int x, int y)
    {
#if PLATFORM(IOS_FAMILY)
        TestWebKitAPI::MouseEventTestHarness testHarness { m_webView.get() };
        testHarness.mouseMove(x, y);
        testHarness.mouseDown();
        testHarness.mouseUp();
#else
        [m_webView sendClickAtPoint:NSMakePoint(x, y)];
#endif
    }

    RetainPtr<TestWKWebView> webView() const { return m_webView.get(); }
    RetainPtr<PointerLockDelegate> pointerLockDelegate() const { return m_delegate.get(); }
    RetainPtr<GCMouse> fakeMouse() const { return m_fakeMouse.get(); }

private:
#if HAVE(MOUSE_DEVICE_OBSERVATION)
    static WKMouseDeviceObserver *sharedMouseDeviceObserver()
    {
        return [NSClassFromString(@"WKMouseDeviceObserver") sharedInstance];
    }
#endif

    static RetainPtr<WKWebViewConfiguration> configurationForWebViewTestingPointerLock()
    {
        RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"PointerLockEnabled"])
                [[configuration preferences] _setEnabled:YES forFeature:feature];
        }

        return configuration;
    }

    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<PointerLockDelegate> m_delegate;
    RetainPtr<GCMouse> m_fakeMouse;
    std::unique_ptr<ClassMethodSwizzler> m_currentMouseSwizzler;
    std::unique_ptr<ClassMethodSwizzler> m_miceSwizzler;
};

TEST_F(PointerLockTests, Simple)
{
    click(200, 200);
    [pointerLockDelegate() waitForPointerLockEngaged];
}

// FIXME: <https://webkit.org/296955> Add test coverage for equivalent flows on iOS.
#if PLATFORM(MAC)
TEST_F(PointerLockTests, ClientDisplaysAlertSheetWhilePointerLockActive)
{
    click(200, 200);

    RetainPtr delegate = pointerLockDelegate();
    [delegate waitForPointerLockEngaged];
    [delegate resetState];

    // Check that pointer lock is lost upon sheet presentation.
    RetainPtr alert = adoptNS([[NSAlert alloc] init]);
    [alert beginSheetModalForWindow:[webView() hostWindow] completionHandler:^(NSModalResponse) { }];
    [delegate waitForPointerLockLost];
    [[webView() hostWindow] endSheet:[alert window]];
    [delegate resetState];

    // Check that pointer lock can be requested again successfully.
    click(200, 200);
    [delegate waitForPointerLockEngaged];
}
#endif

#if HAVE(MOUSE_DEVICE_OBSERVATION)

TEST_F(PointerLockTests, DeniedWithoutMouseDevice)
{
    setHasMouseDeviceForTesting(false);

    click(200, 200);

    __block bool done = false;

    [webView() _doAfterProcessingAllPendingMouseEvents:^{
        EXPECT_FALSE([pointerLockDelegate() didEngagePointerLock]);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

TEST_F(PointerLockTests, MouseDeviceMove)
{
    click(200, 200);
    [pointerLockDelegate() waitForPointerLockEngaged];

    float deltaX = 10.0f;
    float deltaY = 5.0f;
    RetainPtr mouseInput = [fakeMouse() mouseInput];
    [mouseInput handleMouseMovementEventWithDelta:CGPointMake(deltaX, deltaY)];

    [pointerLockDelegate() waitForMouseMoveEvents];
    CGPoint capturedDelta = [[pointerLockDelegate() mouseMoveEvents].firstObject CGPointValue];
    EXPECT_EQ(deltaX, capturedDelta.x);
    EXPECT_EQ(deltaY, capturedDelta.y);
}

TEST_F(PointerLockTests, MouseDeviceDisconnect)
{
    click(200, 200);

    RetainPtr delegate = pointerLockDelegate();
    [delegate waitForPointerLockEngaged];

    [[NSNotificationCenter defaultCenter] postNotificationName:GCMouseDidStopBeingCurrentNotification object:fakeMouse().get() userInfo:nil];
    [delegate waitForPointerLockLost];
}

#endif

#endif
