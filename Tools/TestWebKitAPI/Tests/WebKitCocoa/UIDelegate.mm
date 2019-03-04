/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKContext.h>
#import <WebKit/WKContextPrivateMac.h>
#import <WebKit/WKGeolocationManager.h>
#import <WebKit/WKGeolocationPosition.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKHitTestResult.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#if PLATFORM(MAC)
#import <Carbon/Carbon.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>
#endif

static bool done;

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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:[[[WKWebViewConfiguration alloc] init] autorelease]]);
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
}

- (id)initWithAllowGeolocation:(bool)allowGeolocation;

@end

@implementation GeolocationDelegate

- (id)initWithAllowGeolocation:(bool)allowGeolocation
{
    if (!(self = [super init]))
        return nil;
    _allowGeolocation = allowGeolocation;
    return self;
}

- (void)_webView:(WKWebView *)webView requestGeolocationPermissionForFrame:(WKFrameInfo *)frame decisionHandler:(void (^)(BOOL allowed))decisionHandler
{
    EXPECT_TRUE(frame.isMainFrame);
    EXPECT_STREQ(frame.request.URL.absoluteString.UTF8String, _allowGeolocation ? "https://example.org/" : "https://example.com/");
    EXPECT_EQ(frame.securityOrigin.port, 0);
    EXPECT_STREQ(frame.securityOrigin.protocol.UTF8String, "https");
    EXPECT_STREQ(frame.securityOrigin.host.UTF8String, _allowGeolocation ? "example.org" : "example.com");
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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate1 = adoptNS([[GeolocationDelegate alloc] initWithAllowGeolocation:false]);
    [webView setUIDelegate:delegate1.get()];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://example.com/"]];
    TestWebKitAPI::Util::run(&done);

    done = false;
    auto delegate2 = adoptNS([[GeolocationDelegate alloc] initWithAllowGeolocation:true]);
    [webView setUIDelegate:delegate2.get()];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://example.org/"]];
    TestWebKitAPI::Util::run(&done);
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
    TestWebKitAPI::Util::run(&done);
}

#if PLATFORM(MAC)

@class UITestDelegate;

static RetainPtr<WKWebView> webViewFromDelegateCallback;
static RetainPtr<WKWebView> createdWebView;
static RetainPtr<UITestDelegate> delegate;

@interface UITestDelegate : NSObject <WKUIDelegatePrivate, WKURLSchemeHandler>
@end

@implementation UITestDelegate

- (nullable WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
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
    [urlSchemeTask didReceiveResponse:[[[NSURLResponse alloc] initWithURL:urlSchemeTask.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
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

static bool resizableSet;

@interface ModalDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation ModalDelegate

- (void)_webViewRunModal:(WKWebView *)webView
{
    EXPECT_TRUE(resizableSet);
    EXPECT_EQ(webView, createdWebView.get());
    done = true;
}

- (void)_webView:(WKWebView *)webView setResizable:(BOOL)isResizable
{
    EXPECT_FALSE(isResizable);
    resizableSet = true;
}

- (nullable WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    createdWebView = [[[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration] autorelease];
    [createdWebView setUIDelegate:self];
    return createdWebView.get();
}

@end

TEST(WebKit, RunModal)
{
    auto delegate = adoptNS([[ModalDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView setUIDelegate:delegate.get()];
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSString *html = [NSString stringWithFormat:@"%@%@%@", @"<script> function openModal() { window.showModalDialog('", url, @"'); } </script> <input type='button' value='Click to open modal' onclick='openModal();'>"];
    [webView synchronouslyLoadHTMLString:html];
    [webView sendClicksAtPoint:NSMakePoint(20, 600 - 20) numberOfClicks:1];
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
    [webView loadHTMLString:@"<head><title>test_title</title></head><body onload='print()'>hello world!</body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];
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
    NSString *html = @"<script>Notification.requestPermission(function(p){alert('permission '+p)})</script>";
    auto webView = adoptNS([[WKWebView alloc] init]);
    [webView setUIDelegate:[[[NotificationDelegate alloc] initWithAllowNotifications:YES] autorelease]];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"http://example.org"]];
    TestWebKitAPI::Util::run(&done);
    done = false;
    [webView setUIDelegate:[[[NotificationDelegate alloc] initWithAllowNotifications:NO] autorelease]];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"http://example.com"]];
    TestWebKitAPI::Util::run(&done);
}

@interface PlugInDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation PlugInDelegate

- (void)_webView:(WKWebView *)webView unavailablePlugInButtonClickedWithReason:(_WKPlugInUnavailabilityReason)reason plugInInfo:(NSDictionary *)plugInInfo
{
    ASSERT_EQ(_WKPlugInUnavailabilityReasonPluginMissing, reason);
    ASSERT_TRUE([@"application/x-shockwave-flash" isEqualToString:[plugInInfo objectForKey:@"PluginInformationMIMEType"]]);
    done = true;
}

@end

TEST(WebKit, UnavailablePlugIn)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] setPlugInsEnabled:YES];
    auto delegate = adoptNS([[PlugInDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<object type='application/x-shockwave-flash'/>"];
    [webView sendClicksAtPoint:NSMakePoint(210, 600 - 80) numberOfClicks:1];
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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:[[[WKWebViewConfiguration alloc] init] autorelease]]);
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
    [webView setUIDelegate:[[[MouseMoveOverElementDelegate alloc] init] autorelease]];
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
    TestWebKitAPI::Util::run(&readyForClick);
    NSPoint buttonLocation = NSMakePoint(130, 575);
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

static _WKFocusDirection takenDirection;

@interface FocusDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation FocusDelegate

- (void)_webView:(WKWebView *)webView takeFocus:(_WKFocusDirection)direction
{
    takenDirection = direction;
    done = true;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    completionHandler();
    synthesizeTab([webView window], webView, true);
}

@end

TEST(WebKit, Focus)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto delegate = adoptNS([[FocusDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    NSString *html = @"<script>function loaded() { document.getElementById('in').focus(); alert('ready'); }</script>"
    "<body onload='loaded()'><input type='text' id='in'></body>";
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&done);
    ASSERT_EQ(takenDirection, _WKFocusDirectionBackward);
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
