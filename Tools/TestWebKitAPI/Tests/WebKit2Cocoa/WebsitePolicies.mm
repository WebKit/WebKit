/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKUserContentExtensionStorePrivate.h>
#import <WebKit/_WKWebsitePolicies.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import <WebKit/WKWebViewConfigurationPrivate.h>
#endif

#if WK_API_ENABLED

@interface WKWebView ()
- (WKPageRef)_pageForTesting;
@end

static bool doneCompiling;
static bool receivedAlert;

#if PLATFORM(MAC)
static std::optional<WKAutoplayEvent> receivedAutoplayEvent;
static std::optional<WKAutoplayEventFlags> receivedAutoplayEventFlags;
#endif

static size_t alertCount;

@interface ContentBlockingWebsitePoliciesDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation ContentBlockingWebsitePoliciesDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    // _webView:decidePolicyForNavigationAction:decisionHandler: should be called instead if it is implemented.
    EXPECT_TRUE(false);
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    switch (alertCount++) {
    case 0:
        // Default behavior.
        EXPECT_STREQ("content blockers enabled", message.UTF8String);
        break;
    case 1:
        // After having set websitePolicies.contentBlockersEnabled to false.
        EXPECT_STREQ("content blockers disabled", message.UTF8String);
        break;
    case 2:
        // After having reloaded without content blockers.
        EXPECT_STREQ("content blockers disabled", message.UTF8String);
        break;
    default:
        EXPECT_TRUE(false);
    }
    receivedAlert = true;
    completionHandler();
}

- (void)_webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy, _WKWebsitePolicies *))decisionHandler
{
    _WKWebsitePolicies *websitePolicies = [[[_WKWebsitePolicies alloc] init] autorelease];
    switch (alertCount) {
    case 0:
        // Verify the content blockers behave correctly with the default behavior.
        break;
    case 1:
        {
            // Verify disabling content blockers works correctly.
            websitePolicies.contentBlockersEnabled = false;
            
            // Verify calling the decisionHandler asynchronously works correctly.
            auto decisionHandlerCopy = Block_copy(decisionHandler);
            callOnMainThread([decisionHandlerCopy, websitePolicies = RetainPtr<_WKWebsitePolicies>(websitePolicies)] {
                decisionHandlerCopy(WKNavigationActionPolicyAllow, websitePolicies.get());
                Block_release(decisionHandlerCopy);
            });
        }
        return;
    case 2:
        // Verify enabling content blockers has no effect when reloading without content blockers.
        websitePolicies.contentBlockersEnabled = true;
        break;
    default:
        EXPECT_TRUE(false);
    }
    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies);
}

@end

TEST(WebKit2, WebsitePoliciesContentBlockersEnabled)
{
    [[_WKUserContentExtensionStore defaultStore] _removeAllContentExtensions];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    doneCompiling = false;
    NSString* contentBlocker = @"[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]";
    [[_WKUserContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"WebsitePoliciesTest" encodedContentExtension:contentBlocker completionHandler:^(_WKUserContentFilter *filter, NSError *error) {
        EXPECT_TRUE(error == nil);
        [[configuration userContentController] _addUserContentFilter:filter];
        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[ContentBlockingWebsitePoliciesDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"contentBlockerCheck" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    alertCount = 0;
    receivedAlert = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&receivedAlert);

    receivedAlert = false;
    [webView reload];
    TestWebKitAPI::Util::run(&receivedAlert);

    receivedAlert = false;
    [webView _reloadWithoutContentBlockers];
    TestWebKitAPI::Util::run(&receivedAlert);

    [[_WKUserContentExtensionStore defaultStore] _removeAllContentExtensions];
}

@interface AutoplayPoliciesDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@property (nonatomic, copy) _WKWebsiteAutoplayPolicy(^autoplayPolicyForURL)(NSURL *);
@property (nonatomic, copy) BOOL(^allowsAutoplayQuirksForURL)(NSURL *);
@end

@implementation AutoplayPoliciesDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    // _webView:decidePolicyForNavigationAction:decisionHandler: should be called instead if it is implemented.
    EXPECT_TRUE(false);
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)_webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy, _WKWebsitePolicies *))decisionHandler
{
    _WKWebsitePolicies *websitePolicies = [[[_WKWebsitePolicies alloc] init] autorelease];
    if (_allowsAutoplayQuirksForURL)
        websitePolicies.allowsAutoplayQuirks = _allowsAutoplayQuirksForURL(navigationAction.request.URL);
    if (_autoplayPolicyForURL)
        websitePolicies.autoplayPolicy = _autoplayPolicyForURL(navigationAction.request.URL);
    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies);
}

@end

TEST(WebKit2, WebsitePoliciesAutoplayEnabled)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

#if PLATFORM(IOS)
    [configuration setAllowsInlineMediaPlayback:YES];
#endif

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *requestWithAudio = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-check" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    NSURLRequest *requestWithoutAudio = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-no-audio-check" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    // iOS does not support volume changes to media elements.
#if PLATFORM(MAC)
    NSURLRequest *requestWithoutVolume = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-zero-volume-check" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
#endif

    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyAllowWithoutSound;
    }];
    [webView loadRequest:requestWithAudio];
    [webView waitForMessage:@"did-not-play"];

    [webView loadRequest:requestWithoutAudio];
    [webView waitForMessage:@"autoplayed"];

#if PLATFORM(MAC)
    [webView loadRequest:requestWithoutVolume];
    [webView waitForMessage:@"autoplayed"];
#endif

    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];

#if PLATFORM(MAC)
    [webView loadRequest:requestWithoutVolume];
    [webView waitForMessage:@"did-not-play"];
#endif

    [webView loadRequest:requestWithAudio];
    [webView waitForMessage:@"did-not-play"];

    // Test updating website policies.
    _WKWebsitePolicies *websitePolicies = [[[_WKWebsitePolicies alloc] init] autorelease];
    websitePolicies.autoplayPolicy = _WKWebsiteAutoplayPolicyAllow;
    [webView _updateWebsitePolicies:websitePolicies];
    [webView stringByEvaluatingJavaScript:@"playVideo()"];
    [webView waitForMessage:@"autoplayed"];

    [webView loadRequest:requestWithoutAudio];
    [webView waitForMessage:@"did-not-play"];

    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyAllow;
    }];
    [webView loadRequest:requestWithAudio];
    [webView waitForMessage:@"autoplayed"];

    [webView loadRequest:requestWithoutAudio];
    [webView waitForMessage:@"autoplayed"];

#if PLATFORM(MAC)
    [webView loadRequest:requestWithoutVolume];
    [webView waitForMessage:@"autoplayed"];
#endif

    NSURLRequest *requestWithAudioInIFrame = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-check-in-iframe" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    // If the top-level document allows autoplay, any iframes should also autoplay.
    [delegate setAutoplayPolicyForURL:^(NSURL *url) {
        if ([url.lastPathComponent isEqualToString:@"autoplay-check-in-iframe.html"])
            return _WKWebsiteAutoplayPolicyAllow;
        return _WKWebsiteAutoplayPolicyDeny;
    }];

    [webView loadRequest:requestWithAudioInIFrame];
    [webView waitForMessage:@"autoplayed"];

    // If the top-level document disallows autoplay, any iframes should also not autoplay.
    [delegate setAutoplayPolicyForURL:^(NSURL *url) {
        if ([url.lastPathComponent isEqualToString:@"autoplay-check-in-iframe.html"])
            return _WKWebsiteAutoplayPolicyDeny;
        return _WKWebsiteAutoplayPolicyAllow;
    }];

    [webView loadRequest:requestWithAudioInIFrame];
    [webView waitForMessage:@"did-not-play"];
}

#if PLATFORM(MAC)
static void handleAutoplayEvent(WKPageRef page, WKAutoplayEvent event, WKAutoplayEventFlags flags, const void* clientInfo)
{
    receivedAutoplayEventFlags = flags;
    receivedAutoplayEvent = event;
}

static void runUntilReceivesAutoplayEvent(WKAutoplayEvent event)
{
    while (!receivedAutoplayEvent || *receivedAutoplayEvent != event)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
}

TEST(WebKit2, WebsitePoliciesPlayAfterPreventedAutoplay)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 336, 276) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];
    [webView setNavigationDelegate:delegate.get()];

    WKPageUIClientV9 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 9;
    uiClient.handleAutoplayEvent = handleAutoplayEvent;

    WKPageSetPageUIClient([webView _pageForTesting], &uiClient.base);
    NSPoint playButtonClickPoint = NSMakePoint(20, 256);

    receivedAutoplayEvent = std::nullopt;
    NSURLRequest *jsPlayRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"js-play-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"loaded"];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventDidPreventFromAutoplaying);

    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventDidPlayMediaPreventedFromAutoplaying);
    ASSERT_TRUE(*receivedAutoplayEventFlags & kWKAutoplayEventFlagsHasAudio);

    receivedAutoplayEvent = std::nullopt;
    [webView loadHTMLString:@"" baseURL:nil];

    NSURLRequest *autoplayRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:autoplayRequest];
    [webView waitForMessage:@"loaded"];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventDidPreventFromAutoplaying);
    ASSERT_TRUE(*receivedAutoplayEventFlags & kWKAutoplayEventFlagsHasAudio);

    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventDidPlayMediaPreventedFromAutoplaying);
    ASSERT_TRUE(*receivedAutoplayEventFlags & kWKAutoplayEventFlagsHasAudio);

    receivedAutoplayEvent = std::nullopt;
    [webView loadHTMLString:@"" baseURL:nil];

    NSURLRequest *noAutoplayRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"no-autoplay-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:noAutoplayRequest];
    [webView waitForMessage:@"loaded"];
    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
    [webView waitForMessage:@"played"];
    ASSERT_TRUE(receivedAutoplayEvent == std::nullopt);

    receivedAutoplayEvent = std::nullopt;
    [webView loadHTMLString:@"" baseURL:nil];

    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyAllowWithoutSound;
    }];

    NSURLRequest *autoplayMutedRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-muted-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:autoplayMutedRequest];
    [webView waitForMessage:@"loaded"];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventDidPreventFromAutoplaying);

    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventDidPlayMediaPreventedFromAutoplaying);
    ASSERT_TRUE(*receivedAutoplayEventFlags & kWKAutoplayEventFlagsHasAudio);
}

TEST(WebKit2, WebsitePoliciesPlayingWithoutInterference)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 336, 276) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyAllow;
    }];
    [webView setNavigationDelegate:delegate.get()];

    WKPageUIClientV9 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 9;
    uiClient.handleAutoplayEvent = handleAutoplayEvent;

    WKPageSetPageUIClient([webView _pageForTesting], &uiClient.base);

    receivedAutoplayEvent = std::nullopt;
    NSURLRequest *jsPlayRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"js-play-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"playing"];

    ASSERT_TRUE(receivedAutoplayEvent == std::nullopt);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventDidEndMediaPlaybackWithoutUserInterference);
    ASSERT_TRUE(*receivedAutoplayEventFlags & kWKAutoplayEventFlagsHasAudio);

    receivedAutoplayEvent = std::nullopt;
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"ended"];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventDidEndMediaPlaybackWithoutUserInterference);
    ASSERT_TRUE(*receivedAutoplayEventFlags & kWKAutoplayEventFlagsHasAudio);
}

TEST(WebKit2, WebsitePoliciesUserInterferenceWithPlaying)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 336, 276) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyAllow;
    }];
    [webView setNavigationDelegate:delegate.get()];

    WKPageUIClientV9 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 9;
    uiClient.handleAutoplayEvent = handleAutoplayEvent;

    WKPageSetPageUIClient([webView _pageForTesting], &uiClient.base);

    receivedAutoplayEvent = std::nullopt;
    NSURLRequest *jsPlayRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"js-play-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"playing"];
    ASSERT_TRUE(receivedAutoplayEvent == std::nullopt);

    WKPageSetMuted([webView _pageForTesting], kWKMediaAudioMuted);
    runUntilReceivesAutoplayEvent(kWKAutoplayEventUserDidInterfereWithPlayback);
    ASSERT_TRUE(*receivedAutoplayEventFlags & kWKAutoplayEventFlagsHasAudio);

    receivedAutoplayEvent = std::nullopt;
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"playing"];
    ASSERT_TRUE(receivedAutoplayEvent == std::nullopt);

    const NSPoint muteButtonClickPoint = NSMakePoint(80, 256);
    [webView mouseDownAtPoint:muteButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:muteButtonClickPoint];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventUserDidInterfereWithPlayback);
    ASSERT_TRUE(*receivedAutoplayEventFlags & kWKAutoplayEventFlagsHasAudio);

    receivedAutoplayEvent = std::nullopt;
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"playing"];
    ASSERT_TRUE(receivedAutoplayEvent == std::nullopt);

    const NSPoint playButtonClickPoint = NSMakePoint(20, 256);
    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventUserDidInterfereWithPlayback);
    ASSERT_TRUE(*receivedAutoplayEventFlags & kWKAutoplayEventFlagsHasAudio);
}

TEST(WebKit2, WebsitePoliciesUserNeverPlayedMediaPreventedFromPlaying)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 336, 276) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];
    [webView setNavigationDelegate:delegate.get()];

    WKPageUIClientV9 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 9;
    uiClient.handleAutoplayEvent = handleAutoplayEvent;

    WKPageSetPageUIClient([webView _pageForTesting], &uiClient.base);

    receivedAutoplayEvent = std::nullopt;
    NSURLRequest *jsPlayRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"js-play-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"loaded"];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventDidPreventFromAutoplaying);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    runUntilReceivesAutoplayEvent(kWKAutoplayEventUserNeverPlayedMediaPreventedFromPlaying);
    ASSERT_TRUE(*receivedAutoplayEventFlags & kWKAutoplayEventFlagsHasAudio);
}

TEST(WebKit2, WebsitePoliciesAutoplayQuirks)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    WKRetainPtr<WKPreferencesRef> preferences(AdoptWK, WKPreferencesCreate());
    WKPreferencesSetNeedsSiteSpecificQuirks(preferences.get(), true);
    WKPageGroupSetPreferences(WKPageGetPageGroup([webView _pageForTesting]), preferences.get());

    NSURLRequest *requestWithAudio = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-check" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    [delegate setAllowsAutoplayQuirksForURL:^(NSURL *url) {
        return YES;
    }];
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];
    [webView loadRequest:requestWithAudio];
    [webView waitForMessage:@"did-not-play"];
    [webView waitForMessage:@"on-pause"];

    receivedAutoplayEvent = std::nullopt;
    [webView loadHTMLString:@"" baseURL:nil];

    NSURLRequest *requestWithAudioInFrame = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-check-in-iframe" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    [delegate setAllowsAutoplayQuirksForURL:^(NSURL *url) {
        return [url.lastPathComponent isEqualToString:@"autoplay-check-frame.html"];
    }];
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];
    [webView loadRequest:requestWithAudioInFrame];
    [webView waitForMessage:@"did-not-play"];
    [webView waitForMessage:@"on-pause"];
}
#endif

#endif
