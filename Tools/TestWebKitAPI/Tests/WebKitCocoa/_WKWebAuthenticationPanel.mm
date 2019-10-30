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
#import "Test.h"

#if ENABLE(WEB_AUTHN)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKWebAuthenticationPanel.h>
#import <wtf/BlockPtr.h>

static bool webAuthenticationPanelRan = false;
static bool webAuthenticationPanelFailed = false;
static bool webAuthenticationPanelSucceded = false;

@interface TestWebAuthenticationPanelDelegate : NSObject <_WKWebAuthenticationPanelDelegate>
@end

@implementation TestWebAuthenticationPanelDelegate

- (void)panel:(_WKWebAuthenticationPanel *)panel dismissWebAuthenticationPanelWithResult:(_WKWebAuthenticationResult)result
{
    ASSERT_NE(panel, nil);
    if (result == _WKWebAuthenticationResultFailed) {
        webAuthenticationPanelFailed = true;
        return;
    }
    if (result == _WKWebAuthenticationResultSucceeded) {
        webAuthenticationPanelSucceded = true;
        return;
    }
}

@end

@interface TestWebAuthenticationPanelUIDelegate : NSObject <WKUIDelegatePrivate>
@property bool isRacy;
- (instancetype)init;
@end

@implementation TestWebAuthenticationPanelUIDelegate {
    RetainPtr<_WKWebAuthenticationPanel> _panel;
    RetainPtr<TestWebAuthenticationPanelDelegate> _delegate;
    BlockPtr<void(_WKWebAuthenticationPanelResult)> _callback;
}

- (instancetype)init
{
    if (self = [super init])
        self.isRacy = false;
    return self;
}

- (void)_webView:(WKWebView *)webView runWebAuthenticationPanel:(_WKWebAuthenticationPanel *)panel initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(_WKWebAuthenticationPanelResult))completionHandler
{
    webAuthenticationPanelRan = true;

    _delegate = adoptNS([[TestWebAuthenticationPanelDelegate alloc] init]);
    ASSERT_NE(panel, nil);
    _panel = panel;
    [_panel setDelegate:_delegate.get()];

    EXPECT_WK_STREQ([_panel relyingPartyID], "");

    if (_isRacy) {
        if (!_callback) {
            _callback = makeBlockPtr(completionHandler);
            return;
        }
        _callback(_WKWebAuthenticationPanelResultUnavailable);
    }
    completionHandler(_WKWebAuthenticationPanelResultPresented);
}

@end

namespace TestWebKitAPI {

namespace {

static _WKExperimentalFeature *webAuthenticationExperimentalFeature()
{
    static RetainPtr<_WKExperimentalFeature> theFeature;
    if (theFeature)
        return theFeature.get();

    NSArray *features = [WKPreferences _experimentalFeatures];
    for (_WKExperimentalFeature *feature in features) {
        if ([feature.key isEqual:@"WebAuthenticationEnabled"]) {
            theFeature = feature;
            break;
        }
    }
    return theFeature.get();
}

// Only focused documents can trigger WebAuthn.
static void focus(TestWKWebView *webView)
{
#if PLATFORM(MAC)
    [[webView hostWindow] makeFirstResponder:webView];
#elif PLATFORM(IOS)
    [webView becomeFirstResponder];
#endif
}

static void reset()
{
    webAuthenticationPanelRan = false;
    webAuthenticationPanelFailed = false;
    webAuthenticationPanelSucceded = false;
}

} // namesapce;

TEST(WebAuthenticationPanel, NoPanelTimeout)
{
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    focus(webView.get());

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Operation timed out."];
}

TEST(WebAuthenticationPanel, NoPanelHidSuccess)
{
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    focus(webView.get());

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, PanelTimeout)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    focus(webView.get());

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, PanelHidSuccess)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    focus(webView.get());

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);
}

#if HAVE(NEAR_FIELD)
// This test aims to see if the callback for the first ceremony could affect the second one.
// Therefore, the first callback will be held to return at the time when the second arrives.
// The first callback will return _WKWebAuthenticationPanelResultUnavailable which leads to timeout for NFC.
// The second callback will return _WKWebAuthenticationPanelResultPresented which leads to success.
TEST(WebAuthenticationPanel, PanelRacy1)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsRacy:true];
    [webView setUIDelegate:delegate.get()];
    focus(webView.get());

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

// Unlike the previous one, this one focuses on the order of the delegate callbacks.
TEST(WebAuthenticationPanel, PanelRacy2)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsRacy:true];
    [webView setUIDelegate:delegate.get()];
    focus(webView.get());

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    webAuthenticationPanelRan = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelFailed);
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);
}
#endif // HAVE(NEAR_FIELD)

TEST(WebAuthenticationPanel, PanelTwice)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    focus(webView.get());

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);

    reset();
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
