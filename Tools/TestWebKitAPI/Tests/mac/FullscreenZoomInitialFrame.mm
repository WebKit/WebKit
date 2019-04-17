/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "JavaScriptTest.h"
#import "Test.h"
#import "WebKitAgnosticTest.h"
#import <Carbon/Carbon.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WKViewPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>

@interface NSWindowController (WebKitFullScreenAdditions)
- (NSRect)initialFrame;
- (NSRect)finalFrame;
@end

static bool isWaitingForPageSignalToContinue = false;
static bool didGetPageSignalToContinue = false;

@interface FullscreenStateDelegate : NSObject <WebUIDelegate>
@end

@implementation FullscreenStateDelegate
- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    EXPECT_TRUE(isWaitingForPageSignalToContinue);
    isWaitingForPageSignalToContinue = false;
    didGetPageSignalToContinue = true;
}
@end

// WebKit2 WKPageUIClient

static void runJavaScriptAlert(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void* clientInfo)
{
    EXPECT_TRUE(isWaitingForPageSignalToContinue);
    isWaitingForPageSignalToContinue = false;
    didGetPageSignalToContinue = true;
}

// WebKitAgnosticTest

namespace TestWebKitAPI {

class FullscreenZoomInitialFrame : public WebKitAgnosticTest {
public:
    template <typename View> void runTest(View);

    // WebKitAgnosticTest
    NSURL *url() const override { return [[NSBundle mainBundle] URLForResource:@"FullscreenZoomInitialFrame" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]; }
    void didLoadURL(WebView *webView) override { runTest(webView); }
    void didLoadURL(WKView *wkView) override { runTest(wkView); }

    // Setup and teardown the UIDelegate which gets alert() signals from the page.
    void initializeView(WebView *) override;
    void initializeView(WKView *) override;
    void teardownView(WebView *) override;
    void teardownView(WKView *) override;

    void setPageScale(WebView *, double);
    void setPageScale(WKView *, double);
    void sendMouseDownEvent(WebView *, NSEvent *);
    void sendMouseDownEvent(WKView *, NSEvent *);

private:
    RetainPtr<id <WebUIDelegate>> m_delegate;
};

void FullscreenZoomInitialFrame::initializeView(WebView *webView)
{
    m_delegate = adoptNS([[FullscreenStateDelegate alloc] init]);
    webView.UIDelegate = m_delegate.get();

    RetainPtr<WebPreferences> customPreferences = adoptNS([[WebPreferences alloc] initWithIdentifier:@"FullscreenZoomInitialFramePreferences"]);
    [customPreferences setFullScreenEnabled:YES];
    webView.preferences = customPreferences.get();
}

void FullscreenZoomInitialFrame::teardownView(WebView *webView)
{
    EXPECT_TRUE(webView.UIDelegate == m_delegate.get());
    webView.UIDelegate = nil;
    m_delegate = nil;
}

void FullscreenZoomInitialFrame::initializeView(WKView *wkView)
{
    WKPageUIClientV0 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 0;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;

    WKPageSetPageUIClient(wkView.pageRef, &uiClient.base);

    WKRetainPtr<WKStringRef> identifier = adoptWK(WKStringCreateWithUTF8CString("FullscreenZoomInitialFramePreferences"));
    WKRetainPtr<WKPreferencesRef> customPreferences = adoptWK(WKPreferencesCreateWithIdentifier(identifier.get()));
    WKPreferencesSetFullScreenEnabled(customPreferences.get(), true);
    WKPageGroupSetPreferences(WKPageGetPageGroup(wkView.pageRef), customPreferences.get());
}

void FullscreenZoomInitialFrame::teardownView(WKView *wkView)
{
    // We do not need to teardown the WKPageUIClient.
}

void FullscreenZoomInitialFrame::setPageScale(WebView *webView, double scale)
{
    [webView _scaleWebView:scale atOrigin:NSMakePoint(0, 0)];
}

void FullscreenZoomInitialFrame::setPageScale(WKView *wkView, double scale)
{
    WKPageSetScaleFactor(wkView.pageRef, scale, WKPointMake(0, 0));
}

void FullscreenZoomInitialFrame::sendMouseDownEvent(WebView *webView, NSEvent *event)
{
    [webView.mainFrame.frameView.documentView mouseDown:event];
}

void FullscreenZoomInitialFrame::sendMouseDownEvent(WKView *wkView, NSEvent *event)
{
    [wkView mouseDown:event];
}

template <typename View>
void FullscreenZoomInitialFrame::runTest(View view)
{
    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:view.frame styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [window.get().contentView addSubview:view];
    [window makeKeyAndOrderFront:view];

    setPageScale(view, 2);

    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(5, 5) modifierFlags:0 timestamp:0 windowNumber:window.get().windowNumber context:0 eventNumber:0 clickCount:0 pressure:0];

    isWaitingForPageSignalToContinue = true;
    didGetPageSignalToContinue = false;
    sendMouseDownEvent(view, event);
    Util::run(&didGetPageSignalToContinue);

    id windowController = [[view window] windowController];
    EXPECT_TRUE([windowController respondsToSelector:@selector(initialFrame)]);
    EXPECT_TRUE([windowController respondsToSelector:@selector(finalFrame)]);

    NSRect initialFrame = [windowController initialFrame];
    NSRect finalFrame = [windowController finalFrame];

    EXPECT_EQ(300, initialFrame.size.width);
    EXPECT_EQ(300, initialFrame.size.height);
    EXPECT_EQ(400, finalFrame.size.width);
    EXPECT_EQ(400, finalFrame.size.height);

    isWaitingForPageSignalToContinue = true;
    didGetPageSignalToContinue = false;
    sendMouseDownEvent(view, event);
    Util::run(&didGetPageSignalToContinue);
}

TEST_F(FullscreenZoomInitialFrame, WebKit)
{
    runWebKit1Test();
}

// FIXME:<rdar://problem/20504403>
TEST_F(FullscreenZoomInitialFrame, DISABLED_WebKit2)
{
    runWebKit2Test();
}

} // namespace TestWebKitAPI
