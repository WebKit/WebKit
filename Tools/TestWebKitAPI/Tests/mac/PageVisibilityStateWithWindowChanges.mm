/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WKViewPrivate.h>
#import <wtf/RetainPtr.h>

static bool isWaitingForPageSignalToContinue = false;
static bool didGetPageSignalToContinue = false;

// WebKit1 WebUIDelegate

@interface PageVisibilityStateDelegate : NSObject <WebUIDelegate>
@end

@implementation PageVisibilityStateDelegate

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

class PageVisibilityStateWithWindowChanges : public WebKitAgnosticTest {
public:
    template <typename View> void runTest(View);

    // WebKitAgnosticTest
    NSURL *url() const override { return [[NSBundle mainBundle] URLForResource:@"PageVisibilityStateWithWindowChanges" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]; }
    void didLoadURL(WebView *webView) override { runTest(webView); }
    void didLoadURL(WKView *wkView) override { runTest(wkView); }

    // Setup and teardown the UIDelegate which gets alert() signals from the page.
    void initializeView(WebView *) override;
    void initializeView(WKView *) override;
    void teardownView(WebView *) override;
    void teardownView(WKView *) override;

private:
    RetainPtr<id <WebUIDelegate>> m_delegate;
};

void PageVisibilityStateWithWindowChanges::initializeView(WebView *webView)
{
    m_delegate = adoptNS([[PageVisibilityStateDelegate alloc] init]);
    webView.UIDelegate = m_delegate.get();

    [webView _setVisibilityState:WebPageVisibilityStatePrerender isInitialState:YES];
}

void PageVisibilityStateWithWindowChanges::teardownView(WebView *webView)
{
    EXPECT_TRUE(webView.UIDelegate == m_delegate.get());
    webView.UIDelegate = nil;
    m_delegate = nil;
}

void PageVisibilityStateWithWindowChanges::initializeView(WKView *wkView)
{
    WKPageUIClientV0 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 0;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;

    WKPageSetPageUIClient(wkView.pageRef, &uiClient.base);
}

void PageVisibilityStateWithWindowChanges::teardownView(WKView *wkView)
{
    // We do not need to teardown the WKPageUIClient.
}


template <typename View>
void PageVisibilityStateWithWindowChanges::runTest(View view)
{
    // This WebView does not have a window and superview. PageVisibility should be "hidden".
    EXPECT_NULL([view window]);
    EXPECT_NULL([view superview]);
    EXPECT_JS_EQ(view, "document.visibilityState", "hidden");
    EXPECT_JS_EQ(view, "document.hidden", "true");

    // Add it to a non-visible window. PageVisibility should still be "hidden".
    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:view.frame styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO]);
    [window.get().contentView addSubview:view];
    EXPECT_NOT_NULL([view window]);
    EXPECT_NOT_NULL([view superview]);
    EXPECT_FALSE([window.get() isVisible]);
    EXPECT_JS_EQ(view, "document.visibilityState", "hidden");
    EXPECT_JS_EQ(view, "document.hidden", "true");

    // Make the window visible. PageVisibility should become "visible".
    isWaitingForPageSignalToContinue = true;
    didGetPageSignalToContinue = false;
    [window.get() makeKeyAndOrderFront:nil];
    EXPECT_TRUE([window.get() isVisible]);
    Util::run(&didGetPageSignalToContinue);
    EXPECT_JS_EQ(view, "document.visibilityState", "visible");
    EXPECT_JS_EQ(view, "document.hidden", "false");

    // Minimize the window. PageVisibility should become "hidden".
    isWaitingForPageSignalToContinue = true;
    didGetPageSignalToContinue = false;
    [window.get() miniaturize:nil];
    Util::run(&didGetPageSignalToContinue);
    EXPECT_JS_EQ(view, "document.visibilityState", "hidden");
    EXPECT_JS_EQ(view, "document.hidden", "true");

    // Deminimize the window. PageVisibility should become "visible".
    isWaitingForPageSignalToContinue = true;
    didGetPageSignalToContinue = false;
    [window.get() deminiaturize:nil];
    Util::run(&didGetPageSignalToContinue);
    EXPECT_JS_EQ(view, "document.visibilityState", "visible");
    EXPECT_JS_EQ(view, "document.hidden", "false");

    // Remove the WebView from its superview. PageVisibility should become "hidden".
    isWaitingForPageSignalToContinue = true;
    didGetPageSignalToContinue = false;
    [view removeFromSuperview];
    EXPECT_NULL([view window]);
    EXPECT_NULL([view superview]);
    EXPECT_TRUE([window.get() isVisible]);
    Util::run(&didGetPageSignalToContinue);
    EXPECT_JS_EQ(view, "document.visibilityState", "hidden");
    EXPECT_JS_EQ(view, "document.hidden", "true");
}
    
TEST_F(PageVisibilityStateWithWindowChanges, WebKit)
{
    runWebKit1Test();
}

TEST_F(PageVisibilityStateWithWindowChanges, WebKit2)
{
    runWebKit2Test();
}

} // namespace TestWebKitAPI
