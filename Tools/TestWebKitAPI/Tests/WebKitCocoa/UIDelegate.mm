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

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
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
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKHitTestResult.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#if PLATFORM(MAC)
#import <Carbon/Carbon.h>
#endif

#if PLATFORM(IOS)
#import "ClassMethodSwizzler.h"
#import "UIKitSPI.h"
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
}

@interface NoUIDelegate : NSObject <WKNavigationDelegate>
@end

@implementation NoUIDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if ([navigationAction.request.URL.absoluteString isEqualToString:[[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"] absoluteString]])
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
    [webView loadHTMLString:@"<script>window.open('simple2.html');window.location='simple.html'</script>" baseURL:[[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
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
    memset(&providerCallback, 0, sizeof(WKGeolocationProviderV1));
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
    memset(&providerCallback, 0, sizeof(WKGeolocationProviderV1));
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
    memset(&providerCallback, 0, sizeof(WKGeolocationProviderV1));
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

#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 160000

static int presentViewControllerCallCount = 0;

static UIViewController *overrideViewControllerForFullscreenPresentation()
{
    ++presentViewControllerCallCount;
    return nil;
}

constexpr auto WebKitLockdownModeAlertShownKey = @"WebKitLockdownModeAlertShown";

TEST(WebKit, LockdownModeDefaultFirstUseMessage)
{
    ClassMethodSwizzler swizzler(UIViewController.class, @selector(_viewControllerForFullScreenPresentationFromView:), reinterpret_cast<IMP>(overrideViewControllerForFullscreenPresentation));

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
    ClassMethodSwizzler swizzler(UIViewController.class, @selector(_viewControllerForFullScreenPresentationFromView:), reinterpret_cast<IMP>(overrideViewControllerForFullscreenPresentation));

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
    ClassMethodSwizzler swizzler(UIViewController.class, @selector(_viewControllerForFullScreenPresentationFromView:), reinterpret_cast<IMP>(overrideViewControllerForFullscreenPresentation));

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

#endif // PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 160000

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

@interface PointerLockDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation PointerLockDelegate

- (void)_webViewDidRequestPointerLock:(WKWebView *)webView completionHandler:(void (^)(BOOL))completionHandler
{
    completionHandler(YES);
    done = true;
}

@end

TEST(WebKit, PointerLock)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto delegate = adoptNS([[PointerLockDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:
        @"<canvas width='800' height='600'></canvas><script>"
        @"var canvas = document.querySelector('canvas');"
        @"canvas.onclick = ()=>{canvas.requestPointerLock()};"
        @"</script>"
    ];
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
    TestWebKitAPI::Util::run(&done);
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
    EXPECT_TRUE(fabs(rect.origin.y - 698.858398) < .00001);
    EXPECT_TRUE(fabs(rect.size.height - 3.141590) < .00001);
    EXPECT_EQ(rect.size.width, 468.000000);
    EXPECT_STREQ(title.UTF8String, "test_title");
    EXPECT_STREQ(url.absoluteString.UTF8String, "http://example.com/");
    drawHeaderCalled = true;
}

- (void)_webView:(WKWebView *)webView drawFooterInRect:(CGRect)rect forPageWithTitle:(NSString *)title URL:(NSURL *)url
{
    EXPECT_EQ(rect.origin.x, 72);
    EXPECT_EQ(rect.origin.y, 90);
    EXPECT_TRUE(fabs(rect.size.height - 2.718280) < .00001);
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

bool firstToolbarDone;

@interface ToolbarDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation ToolbarDelegate

- (void)_webView:(WKWebView *)webView getToolbarsAreVisibleWithCompletionHandler:(void(^)(BOOL))completionHandler
{
    completionHandler(firstToolbarDone);
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    if (firstToolbarDone) {
        EXPECT_STREQ(message.UTF8String, "visible:true");
        done = true;
    } else {
        EXPECT_STREQ(message.UTF8String, "visible:false");
        firstToolbarDone = true;
    }
    completionHandler();
}

@end

TEST(WebKit, ToolbarVisible)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:adoptNS([[WKWebViewConfiguration alloc] init]).get()]);
    auto delegate = adoptNS([[ToolbarDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<script>alert('visible:' + window.toolbar.visible);alert('visible:' + window.toolbar.visible)</script>"];
    TestWebKitAPI::Util::run(&done);
}

@interface MouseMoveOverElementDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation MouseMoveOverElementDelegate

- (void)_webView:(WKWebView *)webview mouseDidMoveOverElement:(_WKHitTestResult *)hitTestResult withFlags:(NSEventModifierFlags)flags userInfo:(id <NSSecureCoding>)userInfo
{
    EXPECT_STREQ(hitTestResult.absoluteLinkURL.absoluteString.UTF8String, "http://example.com/path");
    EXPECT_STREQ(hitTestResult.linkLabel.UTF8String, "link label");
    EXPECT_STREQ(hitTestResult.linkTitle.UTF8String, "link title");
    EXPECT_EQ(flags, NSEventModifierFlagShift);
    EXPECT_STREQ(NSStringFromClass([(NSObject *)userInfo class]).UTF8String, "_WKFrameHandle");
    done = true;
}

@end

TEST(WebKit, MouseMoveOverElement)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"FrameHandleSerialization"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    auto uiDelegate = adoptNS([[MouseMoveOverElementDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<a href='http://example.com/path' title='link title'>link label</a>"];
    [webView mouseMoveToPoint:NSMakePoint(20, 600 - 20) withFlags:NSEventModifierFlagShift];
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

static void testDidResignInputElementStrongPasswordAppearanceAfterEvaluatingJavaScript(NSString *script)
{
    done = false;
    readytoResign = false;
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"DidResignInputElementStrongPasswordAppearance"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    auto delegate = adoptNS([[DidResignInputElementStrongPasswordAppearanceDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView evaluateJavaScript:@"" completionHandler:nil]; // Make sure WebProcess and injected bundle are running.
    TestWebKitAPI::Util::run(&readytoResign);
    [webView evaluateJavaScript:script completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, DidResignInputElementStrongPasswordAppearanceWhenTypeDidChange)
{
    testDidResignInputElementStrongPasswordAppearanceAfterEvaluatingJavaScript(@"document.querySelector('input').type = 'text'");
}

TEST(WebKit, DidResignInputElementStrongPasswordAppearanceWhenValueDidChange)
{
    testDidResignInputElementStrongPasswordAppearanceAfterEvaluatingJavaScript(@"document.querySelector('input').value = ''");
}

TEST(WebKit, DidResignInputElementStrongPasswordAppearanceWhenFormIsReset)
{
    testDidResignInputElementStrongPasswordAppearanceAfterEvaluatingJavaScript(@"document.forms[0].reset()");
}

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

#define MOUSE_EVENT_CAUSES_DOWNLOAD 0
// FIXME: At least on El Capitan, sending a mouse event does not cause the PDFPlugin to think the download button has been clicked.
// This test works on High Sierra, but it should be investigated on older platforms.
#if MOUSE_EVENT_CAUSES_DOWNLOAD

@interface SaveDataToFileDelegate : NSObject <WKUIDelegatePrivate, WKNavigationDelegate>
@end

@implementation SaveDataToFileDelegate

- (void)_webView:(WKWebView *)webView saveDataToFile:(NSData *)data suggestedFilename:(NSString *)suggestedFilename mimeType:(NSString *)mimeType originatingURL:(NSURL *)url
{
    NSURL *pdfURL = [[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"];
    EXPECT_TRUE([data isEqualToData:[NSData dataWithContentsOfURL:pdfURL]]);
    EXPECT_STREQ([suggestedFilename UTF8String], "test.pdf");
    EXPECT_STREQ([mimeType UTF8String], "application/pdf");
    EXPECT_STREQ([[url absoluteString] UTF8String], [[pdfURL absoluteString] UTF8String]);
    done = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    NSPoint location = NSMakePoint(490, 70); // Location of button to download the pdf.
    [(TestWKWebView *)webView mouseDownAtPoint:location simulatePressure:NO];
    [(TestWKWebView *)webView mouseUpAtPoint:location];
}

@end

TEST(WebKit, SaveDataToFile)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto delegate = adoptNS([[SaveDataToFileDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView setNavigationDelegate:delegate.get()];
    NSURL *pdfURL = [[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:pdfURL]];
    TestWebKitAPI::Util::run(&done);
}

#endif // MOUSE_EVENT_CAUSES_DOWNLOAD

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
    dispatch_async(dispatch_get_main_queue(), ^ {
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
