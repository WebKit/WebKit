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

#if PLATFORM(MAC)

#import "TestWKWebView.h"
#import "Utilities.h"
#import <Carbon/Carbon.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>

@class UITestDelegate;

static RetainPtr<WKWebView> webViewFromDelegateCallback;
static RetainPtr<WKWebView> createdWebView;
static RetainPtr<UITestDelegate> delegate;
static bool done;

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

TEST(WebKit2, ShowWebView)
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

static NSEvent *tabEvent(NSWindow *window, NSEventType type, NSEventModifierFlags flags)
{
    return [NSEvent keyEventWithType:type location:NSMakePoint(5, 5) modifierFlags:flags timestamp:GetCurrentEventTime() windowNumber:[window windowNumber] context:[NSGraphicsContext currentContext] characters:@"\t" charactersIgnoringModifiers:@"\t" isARepeat:NO keyCode:0];
}

static void synthesizeTab(NSWindow *window, NSView *view, bool withShiftDown)
{
    [view keyDown:tabEvent(window, NSEventTypeKeyDown, withShiftDown ? NSEventModifierFlagShift : 0)];
    [view keyUp:tabEvent(window, NSEventTypeKeyUp, withShiftDown ? NSEventModifierFlagShift : 0)];
}

static RetainPtr<NSWindow> window;
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
    synthesizeTab(window.get(), webView, true);
}

@end

TEST(WebKit2, Focus)
{
    window = adoptNS([[NSWindow alloc] initWithContentRect:CGRectMake(0, 0, 800, 600) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [[window contentView] addSubview:webView.get()];
    auto delegate = adoptNS([[FocusDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    NSString *html = @"<script>function loaded() { document.getElementById('in').focus(); alert('ready'); }</script>"
    "<body onload='loaded()'><input type='text' id='in'></body>";
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&done);
    ASSERT_EQ(takenDirection, _WKFocusDirectionBackward);
}

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

TEST(WebKit2, DidNotHandleWheelEvent)
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
