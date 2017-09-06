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

#import "config.h"

#if WK_API_ENABLED

#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKContextPrivateMac.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKHitTestResult.h>
#import <wtf/RetainPtr.h>

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
    [[configuration preferences] _setPlugInsEnabled:YES];
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
    done = true;
}

@end

TEST(WebKit, MouseMoveOverElement)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
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
    ASSERT_TRUE([(id<NSObject>)userInfo isKindOfClass:[NSString class]]);
    ASSERT_STREQ([(NSString*)userInfo UTF8String], "user data string");
    done = true;
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

#endif // WK_API_ENABLED
