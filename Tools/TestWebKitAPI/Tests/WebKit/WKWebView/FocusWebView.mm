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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestProtocol.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
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
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate;

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate];

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
    [[[webView window] contentView] addSubview:textField];
    [[webView window] setFrame:newWindowFrame display:YES];
    [[webView window] makeKeyWindow];
    [NSApp _setKeyWindow:[webView window]];
    [[webView window] makeFirstResponder:webView];
#else
    [webView becomeFirstResponder];
#endif

    __block bool takeFocusCalled = false;
    uiDelegate.get().takeFocus = ^(WKWebView *view, _WKFocusDirection) {
        takeFocusCalled = true;
#if PLATFORM(MAC)
        [view.window makeFirstResponder:textField];
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

static void focusView(TestWKWebView *webView)
{
#if PLATFORM(IOS_FAMILY)
    [webView focusInWindow];
#else
    [webView focus];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.hasFocus()"] boolValue];
    }));
#endif
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

    auto [webView, navigationDelegate, uiDelegate] = makeWebViewAndDelegates(WTF::move(configuration));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    focusView(webView);
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

static void runFocusNavigationIntoFrameWithNestedRemoteFrameTest(bool siteIsolationEnabled)
{
    auto mainHTML = "<body>"
        "<input id='mainInput'>"
        "<iframe id='outerFrame' src='https://webkit.org/outerframe'></iframe>"
        "</body>"_s;

    auto outerFrameHTML = "<body>"
        "<iframe src='https://apple.com/innerframe'></iframe>"
        "<input id='outerInput'>"
        "<script>document.getElementById('outerInput').addEventListener('focusin', () => alert('outerInput focused'));</script>"
        "</body>"_s;

    auto innerFrameHTML = "<body>"
        "<input id='innerInput'>"
        "<script>document.getElementById('innerInput').addEventListener('focusin', () => alert('innerInput focused'));</script>"
        "</body>"_s;

    HTTPServer server({
        { "/main"_s, { mainHTML } },
        { "/outerframe"_s, { outerFrameHTML } },
        { "/innerframe"_s, { innerFrameHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    if (siteIsolationEnabled)
        [[configuration preferences] _setSiteIsolationEnabled:YES];

    auto [webView, navigationDelegate, uiDelegate] = makeWebViewAndDelegates(WTF::move(configuration));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    focusView(webView);

    [webView evaluateJavaScript:@""
        "document.getElementById('mainInput').addEventListener('focusin', () => alert('mainInput focused'));"
        "document.getElementById('mainInput').focus();" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "mainInput focused");

    [webView typeCharacter:'\t'];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "innerInput focused");

    [webView typeCharacter:'\t'];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "outerInput focused");
}

TEST(FocusWebView, FocusNavigationIntoFrameWithNestedRemoteFrame)
{
    runFocusNavigationIntoFrameWithNestedRemoteFrameTest(false);
}

TEST(FocusWebView, FocusNavigationIntoFrameWithNestedRemoteFrameSiteIsolation)
{
    runFocusNavigationIntoFrameWithNestedRemoteFrameTest(true);
}

TEST(FocusWebView, MultipleFramesSomeUnfocusable)
{
    auto exampleHTML = "<body>"
        "<input id='input'>"
        "<iframe src='https://webkit.org/webkitframe'></iframe>"
        "<iframe src='https://apple.com/appleframe'></iframe>"
        "</body>"_s;

    auto focusableIframeHTML = "<script>"
        "onload = () => {"
        "    document.getElementById('iframeInput').addEventListener('focusin', (e) => {"
        "        alert(window.origin + ' focused');"
        "    });"
        "};"
        "</script>"
        "<input id='iframeInput' type='text' value='Iframe Input'>"
        "</body>"_s;

    auto unfocusableIframeHTML = "<script>"
        "</body>"_s;

    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkitframe"_s, { unfocusableIframeHTML } },
        { "/appleframe"_s, { focusableIframeHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();

    auto [webView, navigationDelegate, uiDelegate] = makeWebViewAndDelegates(WTF::move(configuration));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    focusView(webView);
    [webView evaluateJavaScript:@""
        "let i = document.getElementById('input');"
        "i.addEventListener('focusin', (e) => {"
        "    alert('main frame focused');"
        "});"
        "i.focus()" completionHandler:nil];

    EXPECT_WK_STREQ([uiDelegate waitForAlert], "main frame focused");
    // First tab enters the empty webkit.org iframe (no focusable elements, but
    // the frame itself is a valid focus target). Second tab advances past it.
    [webView typeCharacter:'\t'];
    [webView typeCharacter:'\t'];

    EXPECT_WK_STREQ([uiDelegate waitForAlert], "https://apple.com focused");
}

TEST(FocusWebView, DoNotFocusWebViewWhenUnparented)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"open-in-new-tab"];

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate];

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

TEST(FocusWebView, NoFocusEventsForBackgroundWindow)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);

    NSString *html = @"<!DOCTYPE html>"
        "<html>"
        "<body>"
        "<input id='input' type='text'>"
        "<script>"
        "let focusEventCount = 0;"
        "let focusInEventCount = 0;"
        "input.addEventListener('focus', () => {"
        "    focusEventCount++;"
        "});"
        "input.addEventListener('focusin', () => {"
        "    focusInEventCount++;"
        "});"
        "function getFocusEventCount() { return focusEventCount; }"
        "function getFocusInEventCount() { return focusInEventCount; }"
        "</script>"
        "</body>"
        "</html>";
    [webView synchronouslyLoadHTMLString:html];

    [[webView window] resignKeyWindow];
    [webView objectByEvaluatingJavaScript:@"input.focus()"];
    [[webView window] makeKeyWindow];
#if PLATFORM(IOS_FAMILY)
    // iOS requires that the window manually be made first responder to be focused.
    [webView becomeFirstResponder];
#endif
    [webView waitForNextPresentationUpdate];

    NSNumber *focusEventCount = [webView objectByEvaluatingJavaScript:@"getFocusEventCount()"];
    EXPECT_EQ([focusEventCount intValue], 1);

    NSNumber *focusInEventCount = [webView objectByEvaluatingJavaScript:@"getFocusInEventCount()"];
    EXPECT_EQ([focusInEventCount intValue], 1);
}

TEST(FocusWebView, FocusEventDispatchedForUserGestureInBackgroundWindow)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);

    NSString *html = @"<!DOCTYPE html>"
        "<html>"
        "<body>"
        "<input id='input' type='text'>"
        "<script>"
        "let focusEventCount = 0;"
        "input.addEventListener('focus', () => {"
        "    focusEventCount++;"
        "});"
        "function getFocusEventCount() { return focusEventCount; }"
        "</script>"
        "</body>"
        "</html>";
    [webView synchronouslyLoadHTMLString:html];

    [[webView window] resignKeyWindow];
    NSNumber *focusEventCountWithoutGesture = [webView objectByEvaluatingJavaScript:@"input.focus(); getFocusEventCount()"];
    EXPECT_EQ([focusEventCountWithoutGesture intValue], 0);

    [webView objectByEvaluatingJavaScript:@"input.blur()"];

    NSNumber *focusEventCountAfterFocus = [webView objectByEvaluatingJavaScriptWithUserGesture:@"input.focus(); getFocusEventCount()"];
    EXPECT_EQ([focusEventCountAfterFocus intValue], 1);
}

TEST(FocusWebView, SingleBlurEventWhenNoLongerActiveAndFocused)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);

    NSString *html = @"<!DOCTYPE html>"
        "<html>"
        "<body>"
        "<input id='input' type='text'>"
        "<script>"
        "let blurEventCount = 0;"
        "let focusOutEventCount = 0;"
        "input.addEventListener('blur', () => {"
        "    blurEventCount++;"
        "});"
        "input.addEventListener('focusout', () => {"
        "    focusOutEventCount++;"
        "});"
        "function getBlurEventCount() { return blurEventCount; }"
        "function getFocusOutEventCount() { return focusOutEventCount; }"
        "</script>"
        "</body>"
        "</html>";
    [webView synchronouslyLoadHTMLString:html];

    [[webView window] makeKeyWindow];
#if PLATFORM(IOS_FAMILY)
    [webView becomeFirstResponder];
#else
    [[webView window] makeFirstResponder:webView];
#endif
    [webView objectByEvaluatingJavaScript:@"input.focus()"];

#if PLATFORM(IOS_FAMILY)
    [webView resignFirstResponder];
#else
    [[webView window] makeFirstResponder:nil];
#endif
    [webView waitForNextPresentationUpdate];

    NSNumber *blurEventCount = [webView objectByEvaluatingJavaScript:@"getBlurEventCount()"];
    EXPECT_EQ([blurEventCount intValue], 1);
    NSNumber *focusOutEventCount = [webView objectByEvaluatingJavaScript:@"getFocusOutEventCount()"];
    EXPECT_EQ([focusOutEventCount intValue], 1);

    [[webView window] resignKeyWindow];
    [webView waitForNextPresentationUpdate];

    blurEventCount = [webView objectByEvaluatingJavaScript:@"getBlurEventCount()"];
    EXPECT_EQ([blurEventCount intValue], 1);
    focusOutEventCount = [webView objectByEvaluatingJavaScript:@"getFocusOutEventCount()"];
    EXPECT_EQ([focusOutEventCount intValue], 1);
}

TEST(FocusWebView, FocusInAndFocusOutEventsAccompanyFocusAndBlurWhenActiveAndFocused)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);

    NSString *html = @"<!DOCTYPE html>"
        "<html>"
        "<body>"
        "<input id='first' type='text' onfocus='record(\"first.focus\")' onfocusin='record(\"first.focusin\")' onblur='record(\"first.blur\")' onfocusout='record(\"first.focusout\")'>"
        "<input id='second' type='text'>"
        "<script>"
        "const counts = { };"
        "function record(key) { counts[key] = (counts[key] || 0) + 1; }"
        "</script>"
        "</body>"
        "</html>";
    [webView synchronouslyLoadHTMLString:html];

    [[webView window] makeKeyWindow];
#if PLATFORM(IOS_FAMILY)
    [webView becomeFirstResponder];
#else
    [[webView window] makeFirstResponder:webView];
#endif

    auto count = [&webView](NSString *key) {
        return [[webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"counts['%@'] || 0", key]] intValue];
    };

    [webView objectByEvaluatingJavaScript:@"first.focus()"];
    EXPECT_EQ(count(@"first.focus"), 1);
    EXPECT_EQ(count(@"first.focusin"), 1);
    [webView objectByEvaluatingJavaScript:@"second.focus()"];
    EXPECT_EQ(count(@"first.blur"), 1);
    EXPECT_EQ(count(@"first.focusout"), 1);
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
    [[configuration userContentController] addScriptMessageHandler:focusHandler name:@"focusEvent"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate];

#if PLATFORM(MAC)
    NSRect newWindowFrame = NSMakeRect(0, 0, 400, 500);
#endif
    NSRect textFieldFrame = NSMakeRect(0, 400, 400, 100);
    RetainPtr textField = adoptNS([[TestTextView alloc] initWithFrame:textFieldFrame]);
    [textField setEditable:YES];
    [textField setSelectable:YES];

#if PLATFORM(MAC)
    [[[webView window] contentView] addSubview:textField];
    [[webView window] setFrame:newWindowFrame display:YES];
    [[webView window] makeKeyWindow];
    [NSApp _setKeyWindow:[webView window]];
    [[webView window] makeFirstResponder:webView];
#else
    [webView becomeFirstResponder];
#endif

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    bool takeFocusCalled = false;
    [uiDelegate setTakeFocus:[&takeFocusCalled, textField](WKWebView *view, _WKFocusDirection) {
        takeFocusCalled = true;
#if PLATFORM(MAC)
        [view.window makeFirstResponder:textField];
#endif
    }];
    [webView setUIDelegate:uiDelegate];

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
