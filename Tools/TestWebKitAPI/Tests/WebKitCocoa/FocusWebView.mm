/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <wtf/RetainPtr.h>
#import <wtf/darwin/DispatchExtras.h>

#if PLATFORM(MAC)
@interface NSApplication ()
- (void)_setKeyWindow:(NSWindow *)newKeyWindow;
@end

@interface TestTextView : NSTextView
#else
@interface TestTextView : UITextView
#endif
@property (readonly) BOOL didBecomeFirstResponder;
@property (readonly) BOOL didSeeKeyDownEvent;
@end

@implementation TestTextView {
    BOOL _isBecomingFirstResponder;
}

#if PLATFORM(MAC)
- (void)keyDown:(NSEvent *)event
{
    _didSeeKeyDownEvent = YES;
    [super keyDown:event];
}
#endif

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    _didBecomeFirstResponder = YES;
    [super becomeFirstResponder];
    return YES;
}

- (BOOL)resignFirstResponder
{
    return NO;
}

@end

static bool didFocusMainFrameInput = false;
static bool didFocusInnerFrameInput = false;

@interface FocusEventMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation FocusEventMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    didFocusMainFrameInput = [[message body] isEqualToString:@"input-focused"];
    didFocusInnerFrameInput = [[message body] isEqualToString:@"iframeInput-focused"];
}
@end

namespace TestWebKitAPI {

static auto advanceFocusRelinquishToChrome = R"FOCUSRESOURCE(
<div id="div1" contenteditable="true" tabindex="1">Main 1</div><br>
)FOCUSRESOURCE"_s;

static std::tuple<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>, RetainPtr<TestUIDelegate>> makeWebViewAndDelegates(RetainPtr<WKWebViewConfiguration> configuration)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];

    return {
        WTF::move(webView),
        WTF::move(navigationDelegate),
        WTF::move(uiDelegate)
    };
};

static std::tuple<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>, RetainPtr<TestUIDelegate>> makeWebViewAndDelegates(HTTPServer& server)
{
    return makeWebViewAndDelegates(server.httpsProxyConfiguration());
}

TEST(FocusWebView, AdvanceFocusRelinquishToChrome)
{
    HTTPServer server({
        { "/example"_s, { advanceFocusRelinquishToChrome } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, uiDelegate] = makeWebViewAndDelegates(server);

#if PLATFORM(MAC)
    NSRect newWindowFrame = NSMakeRect(0, 0, 400, 500);
#endif
    NSRect textFieldFrame = NSMakeRect(0, 400, 400, 100);

    __block RetainPtr textField = adoptNS([[TestTextView alloc] initWithFrame:textFieldFrame]);
    textField.get().editable = YES;
    textField.get().selectable = YES;
#if PLATFORM(MAC)
    [textField setString:@"Hello world"];
    [[[webView window] contentView] addSubview:textField.get()];
    [[webView window] setFrame:newWindowFrame display:YES];
    [[webView window] makeKeyWindow];
    [NSApp _setKeyWindow:[webView window]];
    [[webView window] makeFirstResponder:webView.get()];
#else
    [webView becomeFirstResponder];
#endif

    __block bool takeFocusCalled = false;
    uiDelegate.get().takeFocus = ^(WKWebView *view, _WKFocusDirection) {
        takeFocusCalled = true;
#if PLATFORM(MAC)
        [view.window makeFirstResponder:textField.get()];
#endif
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView typeCharacter:'\t'];
    [webView typeCharacter:'\t'];
    Util::run(&takeFocusCalled);

    // Bounce some JavaScript off the WebContent process to flush any lingering IPC messages
    // that might cause the key event to continue processing.
    __block bool jsDone = false;
    [webView evaluateJavaScript:@"" completionHandler:^(id, NSError *) {
        jsDone = true;
    }];
    Util::run(&jsDone);

#if PLATFORM(MAC)
    EXPECT_TRUE(textField.get().didBecomeFirstResponder);
#endif
    EXPECT_FALSE(textField.get().didSeeKeyDownEvent);
}

TEST(FocusWebView, MultipleFrames)
{
    auto exampleHTML = "<body>"
        "<input id='input'>"
        "<iframe src='https://webkit.org/webkitframe'></iframe>"
        "<iframe src='https://apple.com/appleframe'></iframe>"
        "</body>"_s;

    auto iframeHTML = "<script>"
        "onload = () => {"
        "    document.getElementById('iframeInput').addEventListener('focusin', (e) => {"
        "        alert(window.origin + ' focused');"
        "    });"
        "};"
        "</script>"
        "<input id='iframeInput' type='text' value='Iframe Input'>"
        "</body>"_s;

    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkitframe"_s, { iframeHTML } },
        { "/appleframe"_s, { iframeHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    // FIXME: Enable site isolation here and make the test behave the same.
    auto [webView, navigationDelegate, uiDelegate] = makeWebViewAndDelegates(WTF::move(configuration));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@""
        "let i = document.getElementById('input');"
        "i.addEventListener('focusin', (e) => {"
        "    alert('main frame focused');"
        "});"
        "i.focus()" completionHandler:nil];

    EXPECT_WK_STREQ([uiDelegate waitForAlert], "main frame focused");
    [webView typeCharacter:'\t'];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "https://webkit.org focused");
    [webView typeCharacter:'\t'];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "https://apple.com focused");
}

TEST(FocusWebView, DoNotFocusWebViewWhenUnparented)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"open-in-new-tab"];

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];

    __block bool calledFocusWebView = false;
    [uiDelegate setFocusWebView:^(WKWebView *viewToFocus) {
        EXPECT_EQ(viewToFocus, webView.get());
        calledFocusWebView = true;
    }];

    [webView objectByEvaluatingJavaScript:@"openNewTab()"];
    EXPECT_TRUE(calledFocusWebView);

    [webView removeFromSuperview];
    [CATransaction flush];
    __block bool doneUnparenting = false;
    dispatch_async(mainDispatchQueueSingleton(), ^{
        doneUnparenting = true;
    });
    Util::run(&doneUnparenting);

    calledFocusWebView = false;
    [webView objectByEvaluatingJavaScript:@"openNewTab()"];
    EXPECT_FALSE(calledFocusWebView);
}

class CrossOriginIframeRelinquishToChromeTests : public ::testing::TestWithParam<bool> {
public:
    bool siteIsolationEnabled() const { return GetParam(); }
    static std::string testNameGenerator(testing::TestParamInfo<bool> info)
    {
        return std::string { "siteIsolation_is_" } + (info.param ? "enabled" : "disabled");
    }
    void runTest();
};

void CrossOriginIframeRelinquishToChromeTests::runTest()
{
    auto exampleHTML = "<script>"
        "window.addEventListener('load', () => {"
        "    document.getElementById('input').addEventListener('focus', () => {"
        "        window.webkit.messageHandlers.focusEvent.postMessage('input-focused');"
        "    });"
        "});"
        "</script>"
        "<body id=mainFrameBody>"
        "<div id='input' contenteditable='true' tabindex='1'>Main Frame</div>"
        "<iframe id='iframe' src='https://webkit.org/iframe'></iframe>"
        "</body>"_s;

    auto iframeHTML = "<script>"
        "window.addEventListener('load', () => {"
        "    document.getElementById('iframeInput').addEventListener('focus', () => {"
        "        window.webkit.messageHandlers.focusEvent.postMessage('iframeInput-focused');"
        "    });"
        "});"
        "</script>"
        "<body id=iframeBody>"
        "<input id='iframeInput' type='text' value='Iframe Input'>"
        "</body>"_s;

    HTTPServer server { {
        { "/example"_s, { exampleHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy };

    RetainPtr configuration = server.httpsProxyConfiguration();
    [[configuration preferences] _setSiteIsolationEnabled:siteIsolationEnabled()];

    RetainPtr focusHandler = adoptNS([[FocusEventMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:focusHandler.get() name:@"focusEvent"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

#if PLATFORM(MAC)
    NSRect newWindowFrame = NSMakeRect(0, 0, 400, 500);
#endif
    NSRect textFieldFrame = NSMakeRect(0, 400, 400, 100);
    RetainPtr textField = adoptNS([[TestTextView alloc] initWithFrame:textFieldFrame]);
    [textField setEditable:YES];
    [textField setSelectable:YES];

#if PLATFORM(MAC)
    [[[webView window] contentView] addSubview:textField.get()];
    [[webView window] setFrame:newWindowFrame display:YES];
    [[webView window] makeKeyWindow];
    [NSApp _setKeyWindow:[webView window]];
    [[webView window] makeFirstResponder:webView.get()];
#else
    [webView becomeFirstResponder];
#endif

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    bool takeFocusCalled = false;
    [uiDelegate setTakeFocus:[&takeFocusCalled, textField](WKWebView *view, _WKFocusDirection) {
        takeFocusCalled = true;
#if PLATFORM(MAC)
        [view.window makeFirstResponder:textField.get()];
#endif
    }];
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    struct FrameFocusState {
        const char* activeElementID;
        bool hasFocus;
    };
    auto checkFocusState = [webView](FrameFocusState mainFrame, FrameFocusState innerFrame) {
        // FIXME: <rdar://161283373> This IPC round trip should not be necessary.

        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.activeElement.id"], mainFrame.activeElementID);

        EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"document.hasFocus()"] boolValue], mainFrame.hasFocus);

        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.activeElement.id" inFrame:[webView firstChildFrame]], innerFrame.activeElementID);

        EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"document.hasFocus()" inFrame:[webView firstChildFrame]] boolValue], innerFrame.hasFocus);
    };

    checkFocusState({ "mainFrameBody", true }, { "iframeBody", false });

    [webView typeCharacter:'\t'];
    Util::run(&didFocusMainFrameInput);
    checkFocusState({ "input", true }, { "iframeBody", false });

    [webView typeCharacter:'\t'];
    Util::run(&didFocusInnerFrameInput);
    checkFocusState({ "iframe", true }, { "iframeInput", true });

    [webView typeCharacter:'\t'];
    Util::run(&takeFocusCalled);
    checkFocusState({ "mainFrameBody", false }, { "iframeBody", false });
#if PLATFORM(MAC)
    EXPECT_TRUE([textField didBecomeFirstResponder]);
#endif
    EXPECT_FALSE([textField didSeeKeyDownEvent]);
}

TEST_P(CrossOriginIframeRelinquishToChromeTests, Test)
{
    runTest();
}

INSTANTIATE_TEST_SUITE_P(FocusWebView, CrossOriginIframeRelinquishToChromeTests, testing::Bool(), &TestWebKitAPI::CrossOriginIframeRelinquishToChromeTests::testNameGenerator);

} // namespace TestWebKitAPI
