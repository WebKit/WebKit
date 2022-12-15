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

#import "config.h"

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKContentRuleListStorePrivate.h>
#import <WebKit/WKMutableDictionary.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKString.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebsitePolicies.h>
#import <WebKit/_WKCustomHeaderFields.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/Function.h>
#import <wtf/HashMap.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <WebKit/WKWebViewConfigurationPrivate.h>
#endif

@interface WKWebView ()
- (WKPageRef)_pageForTesting;
@end

static bool doneCompiling;
static bool receivedAlert;
static bool finishedNavigation;

static std::optional<_WKAutoplayEvent> receivedAutoplayEvent;
static std::optional<_WKAutoplayEventFlags> receivedAutoplayEventFlags;

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

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
    switch (alertCount) {
    case 0:
        // Verify the content blockers behave correctly with the default behavior.
        break;
    case 1:
        {
            // Verify disabling content blockers works correctly.
            [websitePolicies _setContentBlockersEnabled:false];
            
            // Verify calling the decisionHandler asynchronously works correctly.
            auto decisionHandlerCopy = Block_copy(decisionHandler);
            callOnMainThread([decisionHandlerCopy, websitePolicies] {
                decisionHandlerCopy(WKNavigationActionPolicyAllow, websitePolicies.get());
                Block_release(decisionHandlerCopy);
            });
        }
        return;
    case 2:
        // Verify enabling content blockers has no effect when reloading without content blockers.
        [websitePolicies _setContentBlockersEnabled:true];
        break;
    default:
        EXPECT_TRUE(false);
    }
    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
}

@end

TEST(WebpagePreferences, WebsitePoliciesContentBlockersEnabled)
{
    [[WKContentRuleListStore defaultStore] _removeAllContentRuleLists];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    doneCompiling = false;
    NSString* contentBlocker = @"[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]";
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"WebsitePoliciesTest" encodedContentRuleList:contentBlocker completionHandler:^(WKContentRuleList *list, NSError *error) {
        EXPECT_TRUE(error == nil);
        [[configuration userContentController] addContentRuleList:list];
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

    [[WKContentRuleListStore defaultStore] _removeAllContentRuleLists];
}

@interface AutoplayPoliciesDelegate : TestNavigationDelegate <WKNavigationDelegate, WKUIDelegatePrivate>
@property (nonatomic, copy) _WKWebsiteAutoplayPolicy(^autoplayPolicyForURL)(NSURL *);
@property (nonatomic, copy) _WKWebsiteAutoplayQuirk(^allowedAutoplayQuirksForURL)(NSURL *);
@end

@implementation AutoplayPoliciesDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    // _webView:decidePolicyForNavigationAction:decisionHandler: should be called instead if it is implemented.
    EXPECT_TRUE(false);
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
    if (_allowedAutoplayQuirksForURL)
        [websitePolicies _setAllowedAutoplayQuirks:_allowedAutoplayQuirksForURL(navigationAction.request.URL)];
    if (_autoplayPolicyForURL)
        [websitePolicies _setAutoplayPolicy:_autoplayPolicyForURL(navigationAction.request.URL)];
    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
}

- (void)_webView:(WKWebView *)webView handleAutoplayEvent:(_WKAutoplayEvent)event withFlags:(_WKAutoplayEventFlags)flags
{
    receivedAutoplayEventFlags = flags;
    receivedAutoplayEvent = event;
}

@end

@interface AsyncAutoplayPoliciesDelegate : NSObject <WKNavigationDelegate, WKUIDelegatePrivate>
@property (nonatomic, copy) _WKWebsiteAutoplayPolicy(^autoplayPolicyForURL)(NSURL *);
@property (nonatomic, copy) _WKWebsiteAutoplayQuirk(^allowedAutoplayQuirksForURL)(NSURL *);
@end

@implementation AsyncAutoplayPoliciesDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    // _webView:decidePolicyForNavigationAction:decisionHandler: should be called instead if it is implemented.
    EXPECT_TRUE(false);
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
        if (_allowedAutoplayQuirksForURL)
            [websitePolicies _setAllowedAutoplayQuirks:_allowedAutoplayQuirksForURL(navigationAction.request.URL)];
        if (_autoplayPolicyForURL)
            [websitePolicies _setAutoplayPolicy:_autoplayPolicyForURL(navigationAction.request.URL)];
        decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
    });
}

- (void)_webView:(WKWebView *)webView handleAutoplayEvent:(_WKAutoplayEvent)event withFlags:(_WKAutoplayEventFlags)flags
{
    receivedAutoplayEventFlags = flags;
    receivedAutoplayEvent = event;
}

@end

@interface WebsitePoliciesNavigationDelegate : TestNavigationDelegate <WKNavigationDelegatePrivate>
@property (nonatomic, copy) void (^decidePolicyForNavigationActionWithWebsitePolicies)(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *));
@end

@implementation WebsitePoliciesNavigationDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    if (_decidePolicyForNavigationActionWithWebsitePolicies)
        _decidePolicyForNavigationActionWithWebsitePolicies(navigationAction, preferences, decisionHandler);
    else
        decisionHandler(WKNavigationActionPolicyAllow, nil);
}

@end

TEST(WebpagePreferences, WebsitePoliciesAutoplayEnabled)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

#if PLATFORM(IOS_FAMILY)
    configuration.get().allowsInlineMediaPlayback = YES;
    configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
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
    auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
    [websitePolicies _setAutoplayPolicy:_WKWebsiteAutoplayPolicyAllow];
    [webView _updateWebpagePreferences:websitePolicies.get()];
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

static void runUntilReceivesAutoplayEvent(_WKAutoplayEvent event)
{
    while (!receivedAutoplayEvent || *receivedAutoplayEvent != event)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
}

#if PLATFORM(MAC)
TEST(WebpagePreferences, WebsitePoliciesPlayAfterPreventedAutoplay)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 336, 276) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    NSPoint playButtonClickPoint = NSMakePoint(20, 256);

    receivedAutoplayEvent = std::nullopt;
    NSURLRequest *jsPlayRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"js-play-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"loaded"];
    runUntilReceivesAutoplayEvent(_WKAutoplayEventDidPreventFromAutoplaying);

    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
    runUntilReceivesAutoplayEvent(_WKAutoplayEventDidPlayMediaWithUserGesture);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsHasAudio);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsPlaybackWasPrevented);

    receivedAutoplayEvent = std::nullopt;
    [webView loadHTMLString:@"" baseURL:nil];

    NSURLRequest *autoplayRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:autoplayRequest];
    [webView waitForMessage:@"loaded"];
    runUntilReceivesAutoplayEvent(_WKAutoplayEventDidPreventFromAutoplaying);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsHasAudio);

    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
    runUntilReceivesAutoplayEvent(_WKAutoplayEventDidPlayMediaWithUserGesture);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsHasAudio);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsPlaybackWasPrevented);

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
    runUntilReceivesAutoplayEvent(_WKAutoplayEventDidPreventFromAutoplaying);

    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
    runUntilReceivesAutoplayEvent(_WKAutoplayEventDidPlayMediaWithUserGesture);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsHasAudio);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsPlaybackWasPrevented);
}
#endif

TEST(WebpagePreferences, WebsitePoliciesPlayingWithUserGesture)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
#if PLATFORM(IOS_FAMILY)
    configuration.get().allowsInlineMediaPlayback = YES;
    configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
#endif

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyAllow;
    }];
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    receivedAutoplayEvent = std::nullopt;

#if PLATFORM(MAC)
    NSPoint playButtonClickPoint = NSMakePoint(20, 580);
#endif

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"audio-with-play-button" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView waitForMessage:@"loaded"];

#if PLATFORM(MAC)
    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
#else
    [webView clickOnElementID:@"playButton"];
#endif

    runUntilReceivesAutoplayEvent(_WKAutoplayEventDidPlayMediaWithUserGesture);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsHasAudio);
    ASSERT_FALSE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsMediaIsMainContent);

    receivedAutoplayEvent = std::nullopt;

    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"video-with-play-button" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView waitForMessage:@"loaded"];

#if PLATFORM(MAC)
    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
#else
    [webView clickOnElementID:@"playButton"];
#endif

    runUntilReceivesAutoplayEvent(_WKAutoplayEventDidPlayMediaWithUserGesture);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsHasAudio);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsMediaIsMainContent);
}

TEST(WebpagePreferences, WebsitePoliciesPlayingWithoutInterference)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
#if PLATFORM(IOS_FAMILY)
    configuration.get().allowsInlineMediaPlayback = YES;
    configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
#endif

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 336, 276) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyAllow;
    }];
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    receivedAutoplayEvent = std::nullopt;
    NSURLRequest *jsPlayRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"js-autoplay-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:jsPlayRequest];
    runUntilReceivesAutoplayEvent(_WKAutoplayEventDidAutoplayMediaPastThresholdWithoutUserInterference);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsHasAudio);
}

TEST(WebpagePreferences, WebsitePoliciesUserInterferenceWithPlaying)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
#if PLATFORM(IOS_FAMILY)
    configuration.get().allowsInlineMediaPlayback = YES;
    configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
#endif

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 336, 276) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyAllow;
    }];
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    receivedAutoplayEvent = std::nullopt;
    NSURLRequest *jsPlayRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"js-play-with-controls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"playing"];
    ASSERT_TRUE(receivedAutoplayEvent == std::nullopt);

    WKPageSetMuted([webView _pageForTesting], _WKMediaAudioMuted);
    runUntilReceivesAutoplayEvent(_WKAutoplayEventUserDidInterfereWithPlayback);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsHasAudio);

    receivedAutoplayEvent = std::nullopt;
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"playing"];
    ASSERT_TRUE(receivedAutoplayEvent == std::nullopt);

#if PLATFORM(MAC)
    const NSPoint muteButtonClickPoint = NSMakePoint(80, 256);
    [webView mouseDownAtPoint:muteButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:muteButtonClickPoint];
#else
    [webView clickOnElementID:@"muteButton"];
#endif

    runUntilReceivesAutoplayEvent(_WKAutoplayEventUserDidInterfereWithPlayback);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsHasAudio);

    receivedAutoplayEvent = std::nullopt;
    [webView loadRequest:jsPlayRequest];
    [webView waitForMessage:@"playing"];
    ASSERT_TRUE(receivedAutoplayEvent == std::nullopt);

#if PLATFORM(MAC)
    const NSPoint playButtonClickPoint = NSMakePoint(20, 256);
    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
#else
    [webView clickOnElementID:@"playButton"];
#endif

    runUntilReceivesAutoplayEvent(_WKAutoplayEventUserDidInterfereWithPlayback);
    ASSERT_TRUE(*receivedAutoplayEventFlags & _WKAutoplayEventFlagsHasAudio);
}

#if PLATFORM(MAC)
TEST(WebpagePreferences, WebsitePoliciesPerDocumentAutoplayBehaviorMediaLoading)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool receivedLoadedEvent = false;
    __block bool receivedSuspendEvent = false;

    [webView performAfterReceivingMessage:@"loadeddata" action:^{ receivedLoadedEvent = true; }];
    [webView performAfterReceivingMessage:@"suspend" action:^{ receivedSuspendEvent = true; }];

    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];

    [webView loadTestPageNamed:@"media-loading"];

    TestWebKitAPI::Util::run(&receivedSuspendEvent);
    EXPECT_FALSE(receivedLoadedEvent);

    [webView loadHTMLString:@"" baseURL:nil];
    [delegate waitForDidFinishNavigation];

    [webView setNavigationDelegate:delegate.get()];

    receivedLoadedEvent = false;
    receivedSuspendEvent = false;

    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyAllow;
    }];

    [webView loadTestPageNamed:@"media-loading"];

    TestWebKitAPI::Util::run(&receivedLoadedEvent);
}

TEST(WebpagePreferences, WebsitePoliciesWithBridgingCast)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 336, 276) configuration:configuration.get()]);
    auto delegate = adoptNS([[WebsitePoliciesNavigationDelegate alloc] init]);

    __block bool didInvokeDecisionHandler = false;
    [delegate setDecidePolicyForNavigationActionWithWebsitePolicies:^(WKNavigationAction *, WKWebpagePreferences *, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        auto policies = adoptWK(WKWebsitePoliciesCreate());
        decisionHandler(WKNavigationActionPolicyAllow, (__bridge WKWebpagePreferences *)policies.get());
        didInvokeDecisionHandler = true;
    }];

    [webView setNavigationDelegate:delegate.get()];
    [webView loadTestPageNamed:@"simple"];
    [delegate waitForDidFinishNavigation];
    EXPECT_TRUE(didInvokeDecisionHandler);
}

struct ParsedRange {
    ParsedRange(String string)
    {
        // This is a strict and unsafe Range header parser adequate only for tests.
        bool parsingMin = true;
        size_t min = 0;
        size_t max = 0;
        ASSERT(string.length() > 6);
        ASSERT(string.startsWith("bytes="_s));
        for (size_t i = 6; i < string.length(); ++i) {
            if (isASCIIDigit(string[i])) {
                if (parsingMin)
                    min = min * 10 + string[i] - '0';
                else
                    max = max * 10 + string[i] - '0';
            } else if (string[i] == '-') {
                if (parsingMin)
                    parsingMin = false;
                else
                    return;
            } else
                return;
        }
        if (min <= max)
            range = std::make_pair(min, max);
    }
    std::optional<std::pair<size_t, size_t>> range;
};

@interface TestSchemeHandler : NSObject <WKURLSchemeHandler>
- (instancetype)initWithVideoData:(RetainPtr<NSData>&&)data;
@end

@implementation TestSchemeHandler {
    RetainPtr<NSData> videoData;
}

- (instancetype)initWithVideoData:(RetainPtr<NSData>&&)data
{
    self = [super init];
    if (!self)
        return nil;
    
    videoData = WTFMove(data);
    
    return self;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    if ([task.request.URL.path isEqualToString:@"/should-redirect"]) {
        [(id<WKURLSchemeTaskPrivate>)task _didPerformRedirection:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:nil expectedContentLength:0 textEncodingName:nil]).get() newRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///autoplay-check.html"]]];
        
        NSData *data = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"autoplay-check" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
        [task didReceiveData:data];
        [task didFinish];
        return;
    }
    
    ASSERT_TRUE([task.request.URL.path isEqualToString:@"/test.mp4"]);
    ParsedRange parsedRange([task.request valueForHTTPHeaderField:@"Range"]);
    ASSERT_TRUE(!!parsedRange.range);
    auto& range = *parsedRange.range;
    
    NSDictionary *headerFields = @{ @"Content-Length": [@(range.second - range.first + 1) stringValue], @"Content-Range": [NSString stringWithFormat:@"bytes %lu-%lu/%lu", range.first, range.second, [videoData length]] };
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:task.request.URL statusCode:200 HTTPVersion:(NSString *)kCFHTTPVersion1_1 headerFields:headerFields]);
    [task didReceiveResponse:response.get()];
    [task didReceiveData:[videoData subdataWithRange:NSMakeRange(range.first, range.second - range.first + 1)]];
    [task didFinish];
    
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

TEST(WebpagePreferences, WebsitePoliciesDuringRedirect)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto videoData = adoptNS([[NSData alloc] initWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"test" withExtension:@"mp4" subdirectory:@"TestWebKitAPI.resources"]]);
    [configuration setURLSchemeHandler:adoptNS([[TestSchemeHandler alloc] initWithVideoData:WTFMove(videoData)]).get() forURLScheme:@"test"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [delegate setAutoplayPolicyForURL:^(NSURL *url) {
        if ([url.path isEqualToString:@"/should-redirect"])
            return _WKWebsiteAutoplayPolicyDeny;
        return _WKWebsiteAutoplayPolicyAllow;
    }];
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///should-redirect"]]];
    [webView waitForMessage:@"autoplayed"];
}
#endif // PLATFORM(MAC)

TEST(WebpagePreferences, WebsitePoliciesUpdates)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
#if PLATFORM(IOS_FAMILY)
    configuration.get().allowsInlineMediaPlayback = YES;
    configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
#endif

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    NSURLRequest *requestWithAudio = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-check" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];
    [webView loadRequest:requestWithAudio];
    [webView waitForMessage:@"did-not-play"];

    auto policies = adoptNS([[WKWebpagePreferences alloc] init]);
    [policies _setAutoplayPolicy:_WKWebsiteAutoplayPolicyAllow];
    [webView _updateWebpagePreferences:policies.get()];

    // Now that we updated our policies, a script should be able to autoplay media.
    [webView stringByEvaluatingJavaScript:@"playVideo()"];
    [webView waitForMessage:@"autoplayed"];

    [webView stringByEvaluatingJavaScript:@"pauseVideo()"];

    auto preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences _setAutoplayPolicy:_WKWebsiteAutoplayPolicyDeny];
    [webView _updateWebpagePreferences:preferences.get()];

    // A script should no longer be able to autoplay media.
    receivedAutoplayEvent = std::nullopt;
    [webView stringByEvaluatingJavaScript:@"playVideo()"];
    runUntilReceivesAutoplayEvent(_WKAutoplayEventDidPreventFromAutoplaying);
}

#if PLATFORM(MAC)
TEST(WebpagePreferences, WebsitePoliciesArbitraryUserGestureQuirk)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    WKRetainPtr<WKPreferencesRef> preferences = adoptWK(WKPreferencesCreate());
    WKPreferencesSetNeedsSiteSpecificQuirks(preferences.get(), true);
    WKPageGroupSetPreferences(WKPageGetPageGroup([webView _pageForTesting]), preferences.get());

    [delegate setAllowedAutoplayQuirksForURL:^_WKWebsiteAutoplayQuirk(NSURL *url)
    {
        return _WKWebsiteAutoplayQuirkArbitraryUserGestures;
    }];
    [delegate setAutoplayPolicyForURL:^(NSURL *)
    {
        return _WKWebsiteAutoplayPolicyDeny;
    }];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-check" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView waitForMessage:@"did-not-play"];

    const NSPoint clickPoint = NSMakePoint(760, 560);
    [webView mouseDownAtPoint:clickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:clickPoint];

    [webView stringByEvaluatingJavaScript:@"playVideo()"];
    [webView waitForMessage:@"autoplayed"];
}

TEST(WebpagePreferences, WebsitePoliciesAutoplayQuirks)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    WKRetainPtr<WKPreferencesRef> preferences = adoptWK(WKPreferencesCreate());
    WKPreferencesSetNeedsSiteSpecificQuirks(preferences.get(), true);
    WKPageGroupSetPreferences(WKPageGetPageGroup([webView _pageForTesting]), preferences.get());

    NSURLRequest *requestWithAudio = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-check" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    [delegate setAllowedAutoplayQuirksForURL:^_WKWebsiteAutoplayQuirk(NSURL *url) {
        return _WKWebsiteAutoplayQuirkSynthesizedPauseEvents;
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

    [delegate setAllowedAutoplayQuirksForURL:^_WKWebsiteAutoplayQuirk(NSURL *url) {
        if ([url.lastPathComponent isEqualToString:@"autoplay-check-frame.html"])
            return _WKWebsiteAutoplayQuirkInheritedUserGestures;
        
        return _WKWebsiteAutoplayQuirkSynthesizedPauseEvents | _WKWebsiteAutoplayQuirkInheritedUserGestures;
    }];
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];
    [webView loadRequest:requestWithAudioInFrame];
    [webView waitForMessage:@"did-not-play"];
    [webView waitForMessage:@"on-pause"];

    receivedAutoplayEvent = std::nullopt;
    [webView loadHTMLString:@"" baseURL:nil];

    NSURLRequest *requestThatInheritsGesture = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-inherits-gesture-from-document" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:requestThatInheritsGesture];
    [webView waitForMessage:@"loaded"];

    // Click in the document, but not in the media element.
    const NSPoint clickPoint = NSMakePoint(760, 560);
    [webView mouseDownAtPoint:clickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:clickPoint];

    [webView stringByEvaluatingJavaScript:@"play()"];
    [webView waitForMessage:@"playing"];
}

TEST(WebpagePreferences, WebsitePoliciesPerDocumentAutoplayBehaviorQuirks)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[AutoplayPoliciesDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    WKRetainPtr<WKPreferencesRef> preferences = adoptWK(WKPreferencesCreate());
    WKPreferencesSetNeedsSiteSpecificQuirks(preferences.get(), true);
    WKPageGroupSetPreferences(WKPageGetPageGroup([webView _pageForTesting]), preferences.get());

    receivedAutoplayEvent = std::nullopt;

    NSURLRequest *requestWithMultipleMediaElements = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplaying-multiple-media-elements" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    [delegate setAllowedAutoplayQuirksForURL:^_WKWebsiteAutoplayQuirk(NSURL *url) {
        return _WKWebsiteAutoplayQuirkPerDocumentAutoplayBehavior;
    }];
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];
    [webView loadRequest:requestWithMultipleMediaElements];
    [webView waitForMessage:@"loaded"];

    // We should not be allowed to play without a user gesture.
    [webView _evaluateJavaScriptWithoutUserGesture:@"playVideo('video1')" completionHandler:nil];
    [webView waitForMessage:@"did-not-play-video1"];

    // Now try again with a user gesture...
    const NSPoint playButtonClickPoint = NSMakePoint(20, 580);
    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
    [webView waitForMessage:@"did-play-video1"];

    // Now video2 should also be allowed to autoplay without a user gesture because of the quirk.
    [webView _evaluateJavaScriptWithoutUserGesture:@"playVideo('video2')" completionHandler:nil];
    [webView waitForMessage:@"did-play-video2"];

    // Now let's test this without the quirk.
    [webView loadHTMLString:@"" baseURL:nil];
    receivedAutoplayEvent = std::nullopt;

    [delegate setAllowedAutoplayQuirksForURL:^_WKWebsiteAutoplayQuirk(NSURL *url) {
        return 0;
    }];
    [webView loadRequest:requestWithMultipleMediaElements];
    [webView waitForMessage:@"loaded"];

    // We should not be allowed to play without a user gesture.
    [webView _evaluateJavaScriptWithoutUserGesture:@"playVideo('video1')" completionHandler:nil];
    [webView waitForMessage:@"did-not-play-video1"];

    // Now try again with a user gesture...
    [webView mouseDownAtPoint:playButtonClickPoint simulatePressure:NO];
    [webView mouseUpAtPoint:playButtonClickPoint];
    [webView waitForMessage:@"did-play-video1"];

    // Now video2 should not be allowed to autoplay without a user gesture.
    [webView _evaluateJavaScriptWithoutUserGesture:@"playVideo('video2')" completionHandler:nil];
    [webView waitForMessage:@"did-not-play-video2"];
}
#endif

// FIXME: Re-enable this test once webkit.org/b/230494 is resolved.
TEST(WebpagePreferences, DISABLED_WebsitePoliciesAutoplayQuirksAsyncPolicyDelegate)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
#if PLATFORM(IOS_FAMILY)
    configuration.get().allowsInlineMediaPlayback = YES;
    configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
#endif
    [configuration preferences].siteSpecificQuirksModeEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[AsyncAutoplayPoliciesDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *requestWithAudio = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplay-check" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    [delegate setAllowedAutoplayQuirksForURL:^_WKWebsiteAutoplayQuirk(NSURL *url) {
        return _WKWebsiteAutoplayQuirkSynthesizedPauseEvents;
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

    [delegate setAllowedAutoplayQuirksForURL:^_WKWebsiteAutoplayQuirk(NSURL *url) {
        if ([url.lastPathComponent isEqualToString:@"autoplay-check-frame.html"])
            return _WKWebsiteAutoplayQuirkInheritedUserGestures;

        return _WKWebsiteAutoplayQuirkSynthesizedPauseEvents | _WKWebsiteAutoplayQuirkInheritedUserGestures;
    }];
    [delegate setAutoplayPolicyForURL:^(NSURL *) {
        return _WKWebsiteAutoplayPolicyDeny;
    }];
    [webView loadRequest:requestWithAudioInFrame];
    [webView waitForMessage:@"did-not-play"];
    [webView waitForMessage:@"on-pause"];
}

TEST(WebpagePreferences, InvalidCustomHeaders)
{
    auto customHeaderFields = adoptNS([[_WKCustomHeaderFields alloc] init]);
    [customHeaderFields setFields:@{@"invalidheader" : @"", @"noncustom" : @"header", @"    x-Custom ":@"  Needs Canonicalization\t ", @"x-other" : @"other value"}];
    NSDictionary<NSString *, NSString *> *canonicalized = [customHeaderFields fields];
    EXPECT_EQ(canonicalized.count, 2u);
    EXPECT_STREQ([canonicalized objectForKey:@"x-Custom"].UTF8String, "Needs Canonicalization");
    EXPECT_STREQ([canonicalized objectForKey:@"x-other"].UTF8String, "other value");
}

static bool firstTestDone;
static bool secondTestDone;
static bool thirdTestDone;
static bool fourthTestDone;
static bool fifthTestDone;

static void expectHeaders(id <WKURLSchemeTask> task, bool expected)
{
    NSURLRequest *request = task.request;
    if (expected) {
        EXPECT_STREQ([[request valueForHTTPHeaderField:@"X-key3"] UTF8String], "value3");
        EXPECT_STREQ([[request valueForHTTPHeaderField:@"X-key4"] UTF8String], "value4");
    } else {
        EXPECT_TRUE([request valueForHTTPHeaderField:@"X-key3"] == nil);
        EXPECT_TRUE([request valueForHTTPHeaderField:@"X-key4"] == nil);
    }
}

static void respond(id <WKURLSchemeTask>task, NSString *html = nil)
{
    [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:html.length textEncodingName:nil]).get()];
    [task didReceiveData:[html dataUsingEncoding:NSUTF8StringEncoding]];
    [task didFinish];
}

@interface CustomHeaderFieldsDelegate : NSObject <WKNavigationDelegatePrivate, WKURLSchemeHandler>
@end

@implementation CustomHeaderFieldsDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    auto headerFields = adoptNS([[_WKCustomHeaderFields alloc] init]);
    [headerFields setFields:@{@"X-key3": @"value3", @"X-key4": @"value4"}];
    [headerFields setThirdPartyDomains:@[
        @"*.hostwithasterisk.com",
        @"hostwithoutasterisk.com",
        @"*.com" // should be ignored.
    ]];
    
    [preferences _setCustomHeaderFields:@[headerFields.get()]];
    
    if ([navigationAction.request.URL.path isEqualToString:@"/mainresource"]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            decisionHandler(WKNavigationActionPolicyAllow, preferences);
        });
    } else
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    NSString *path = urlSchemeTask.request.URL.path;
    if ([path isEqualToString:@"/mainresource"]) {
        respond(urlSchemeTask, @"<script>fetch('subresource').then(function(response){fetch('test://differentsecurityorigin/crossoriginsubresource',{mode:'no-cors'})})</script>");
    } else if ([path isEqualToString:@"/subresource"]) {
        respond(urlSchemeTask);
    } else if ([path isEqualToString:@"/crossoriginsubresource"]) {
        respond(urlSchemeTask);
        firstTestDone = true;
    } else if ([path isEqualToString:@"/mainresourcewithiframe"]) {
        respond(urlSchemeTask, @"<iframe src='test://iframeorigin/iframemainresource'></iframe>");
    } else if ([path isEqualToString:@"/iframemainresource"]) {
        respond(urlSchemeTask, @"<script>fetch('iframesubresource').then(function(response){fetch('test://mainframeorigin/originaloriginsubresource',{mode:'no-cors'})})</script>");
    } else if ([path isEqualToString:@"/iframesubresource"]) {
        respond(urlSchemeTask);
    } else if ([path isEqualToString:@"/originaloriginsubresource"]) {
        respond(urlSchemeTask);
        secondTestDone = true;
    } else if ([path isEqualToString:@"/nestedtop"]) {
        respond(urlSchemeTask, @"<iframe src='test://otherorigin/nestedmid'></iframe>");
    } else if ([path isEqualToString:@"/nestedmid"]) {
        respond(urlSchemeTask, @"<iframe src='test://toporigin/nestedbottom'></iframe>");
    } else if ([path isEqualToString:@"/nestedbottom"]) {
        respond(urlSchemeTask);
        thirdTestDone = true;
    } else if ([path isEqualToString:@"/requestfromaboutblank"]) {
        respond(urlSchemeTask);
        fourthTestDone = true;
    } else if ([path isEqualToString:@"/testcustomheaderfieldhosts"]) {
        expectHeaders(urlSchemeTask, true);
        NSString *html = @"<script>fetch('test://a.b.c.sub.hostwithasterisk.com/hosttest1',{mode:'no-cors'})"
            ".then(function(response){fetch('test://subhostwithasterisk.com/hosttest2',{mode:'no-cors'})})"
            ".then(function(response){fetch('test://hostwithoutasterisk.com/hosttest3',{mode:'no-cors'})})"
            ".then(function(response){fetch('test://a.b.c.sub.hostwithoutasterisk.com/hosttest4',{mode:'no-cors'})})</script>";
        respond(urlSchemeTask, html);
    } else if ([path isEqualToString:@"/hosttest1"]) {
        expectHeaders(urlSchemeTask, true);
        respond(urlSchemeTask);
    } else if ([path isEqualToString:@"/hosttest2"]) {
        expectHeaders(urlSchemeTask, false);
        respond(urlSchemeTask);
    } else if ([path isEqualToString:@"/hosttest3"]) {
        expectHeaders(urlSchemeTask, true);
        respond(urlSchemeTask);
    } else if ([path isEqualToString:@"/hosttest4"]) {
        expectHeaders(urlSchemeTask, false);
        respond(urlSchemeTask);
        fifthTestDone = true;
    } else if ([path isEqualToString:@"/testcustomheaderfieldhosts"])
        EXPECT_TRUE(false);
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
}

@end

TEST(WebpagePreferences, CustomHeaderFields)
{
    auto delegate = adoptNS([[CustomHeaderFieldsDelegate alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:delegate.get() forURLScheme:@"test"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///mainresource"]]];
    TestWebKitAPI::Util::run(&firstTestDone);
    
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test://mainframeorigin/mainresourcewithiframe"]]];
    TestWebKitAPI::Util::run(&secondTestDone);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test://toporigin/nestedtop"]]];
    TestWebKitAPI::Util::run(&thirdTestDone);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test://host/testcustomheaderfieldhosts"]]];
    TestWebKitAPI::Util::run(&fifthTestDone);
}

static unsigned loadCount;

@interface DataMappingSchemeHandler : NSObject <WKURLSchemeHandler> {
    HashMap<String, RetainPtr<NSData>> _dataMappings;
    Function<void(id <WKURLSchemeTask>)> _taskHandler;
}
- (void)addMappingFromURLString:(NSString *)urlString toData:(const char*)data;
- (void)setTaskHandler:(Function<void(id <WKURLSchemeTask>)>&&)handler;
@end

@implementation DataMappingSchemeHandler

- (void)addMappingFromURLString:(NSString *)urlString toData:(const char*)data
{
    _dataMappings.set(urlString, [NSData dataWithBytesNoCopy:(void*)data length:strlen(data) freeWhenDone:NO]);
}

- (void)setTaskHandler:(Function<void(id <WKURLSchemeTask>)>&&)handler
{
    _taskHandler = WTFMove(handler);
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    NSURL *finalURL = task.request.URL;

    ++loadCount;
    if (_taskHandler)
        _taskHandler(task);

    auto response = adoptNS([[NSURLResponse alloc] initWithURL:finalURL MIMEType:@"text/html" expectedContentLength:1 textEncodingName:nil]);
    [task didReceiveResponse:response.get()];

    if (auto data = _dataMappings.get([finalURL absoluteString]))
        [task didReceiveData:data.get()];
    else
        [task didReceiveData:[@"Hello" dataUsingEncoding:NSUTF8StringEncoding]];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

@interface CustomUserAgentDelegate : NSObject <WKNavigationDelegate> {
}
@end

@implementation CustomUserAgentDelegate

- (void)_webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences userInfo:(id <NSSecureCoding>)userInfo decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
    if (navigationAction.targetFrame.mainFrame)
        [websitePolicies _setCustomUserAgent:@"Foo Custom UserAgent"];

    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    finishedNavigation = true;
}

@end

static const char* customUserAgentMainFrameTestBytes = R"TESTRESOURCE(
<script src="test://www.webkit.org/script.js"></script>
<img src="test://www.webkit.org/image.png"></img>
<iframe src="test://www.apple.com/subframe.html"></iframe>
<script>
onload = () => {
    setTimeout(() => {
        fetch("test://www.webkit.org/fetchResource.html");
    }, 0);
}
onmessage = (event) => {
    window.subframeUserAgent = event.data;
}
</script>
)TESTRESOURCE";

static const char* customUserAgentSubFrameTestBytes = R"TESTRESOURCE(
<script src="test://www.apple.com/script.js"></script>
<img src="test://www.apple.com/image.png"></img>
<iframe src="test://www.apple.com/subframe2.html"></iframe>
<script>
onload = () => {
    setTimeout(() => {
        fetch("test://www.apple.com/fetchResource.html");
    }, 0);
    top.postMessage(navigator.userAgent, '*');
}
</script>
)TESTRESOURCE";

TEST(WebpagePreferences, WebsitePoliciesCustomUserAgent)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[DataMappingSchemeHandler alloc] init]);
    [schemeHandler addMappingFromURLString:@"test://www.webkit.org/main.html" toData:customUserAgentMainFrameTestBytes];
    [schemeHandler addMappingFromURLString:@"test://www.apple.com/subframe.html" toData:customUserAgentSubFrameTestBytes];
    [schemeHandler setTaskHandler:[](id <WKURLSchemeTask> task) {
        EXPECT_STREQ("Foo Custom UserAgent", [[task.request valueForHTTPHeaderField:@"User-Agent"] UTF8String]);
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[CustomUserAgentDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    loadCount = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"test://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    while (loadCount != 9U)
        TestWebKitAPI::Util::spinRunLoop();
    loadCount = 0;

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"test://www.google.com/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    EXPECT_EQ(1U, loadCount);
    loadCount = 0;
}

@interface CustomJavaScriptUserAgentDelegate : NSObject <WKNavigationDelegate>
@property (nonatomic) BOOL setCustomUserAgent;
@end

@implementation CustomJavaScriptUserAgentDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
    if (navigationAction.targetFrame.mainFrame) {
        [websitePolicies _setCustomUserAgentAsSiteSpecificQuirks:@"Foo Site Specific Quriks UserAgent"];
        if (_setCustomUserAgent)
            [websitePolicies _setCustomUserAgent:@"Foo Custom Request UserAgent"];
    }

    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    finishedNavigation = true;
}

@end

TEST(WebpagePreferences, WebsitePoliciesCustomUserAgentAsSiteSpecificQuirksDisabled)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[DataMappingSchemeHandler alloc] init]);
    [schemeHandler addMappingFromURLString:@"test://www.webkit.org/main.html" toData:customUserAgentMainFrameTestBytes];
    [schemeHandler addMappingFromURLString:@"test://www.apple.com/subframe.html" toData:customUserAgentSubFrameTestBytes];
    [schemeHandler setTaskHandler:[](id <WKURLSchemeTask> task) {
        auto* userAgentString = [task.request valueForHTTPHeaderField:@"User-Agent"];
        EXPECT_TRUE([userAgentString hasPrefix:@"Mozilla/5.0 "]);
        EXPECT_TRUE([userAgentString containsString:@"(KHTML, like Gecko)"]);
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];
    [configuration preferences].siteSpecificQuirksModeEnabled = NO;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[CustomJavaScriptUserAgentDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    loadCount = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"test://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    while (loadCount != 9U)
        TestWebKitAPI::Util::spinRunLoop();

    auto* userAgentString = [webView stringByEvaluatingJavaScript:@"navigator.userAgent"];
    EXPECT_TRUE([userAgentString hasPrefix:@"Mozilla/5.0 "]);
    EXPECT_TRUE([userAgentString containsString:@"(KHTML, like Gecko)"]);

    userAgentString = [webView stringByEvaluatingJavaScript:@"navigator.userAgent"];
    EXPECT_TRUE([userAgentString hasPrefix:@"Mozilla/5.0 "]);
    EXPECT_TRUE([userAgentString containsString:@"(KHTML, like Gecko)"]);
}

TEST(WebpagePreferences, WebsitePoliciesCustomUserAgentAsSiteSpecificQuirks)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[DataMappingSchemeHandler alloc] init]);
    [schemeHandler addMappingFromURLString:@"test://www.webkit.org/main.html" toData:customUserAgentMainFrameTestBytes];
    [schemeHandler addMappingFromURLString:@"test://www.apple.com/subframe.html" toData:customUserAgentSubFrameTestBytes];
    [schemeHandler setTaskHandler:[](id <WKURLSchemeTask> task) {
        EXPECT_STREQ("Foo Site Specific Quriks UserAgent", [[task.request valueForHTTPHeaderField:@"User-Agent"] UTF8String]);
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];
    [configuration preferences].siteSpecificQuirksModeEnabled = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[CustomJavaScriptUserAgentDelegate alloc] init]);
    delegate.get().setCustomUserAgent = YES;
    [webView setNavigationDelegate:delegate.get()];

    loadCount = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"test://www.webkit.org/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    while (loadCount != 9U)
        TestWebKitAPI::Util::spinRunLoop();

    EXPECT_STREQ("Foo Site Specific Quriks UserAgent", [[webView stringByEvaluatingJavaScript:@"navigator.userAgent"] UTF8String]);
    EXPECT_STREQ("Foo Site Specific Quriks UserAgent", [[webView stringByEvaluatingJavaScript:@"subframeUserAgent"] UTF8String]);
}

@interface CustomNavigatorPlatformDelegate : NSObject <WKNavigationDelegate> {
}
@end

@implementation CustomNavigatorPlatformDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
    if (navigationAction.targetFrame.mainFrame)
        [websitePolicies _setCustomNavigatorPlatform:@"Test Custom Platform"];

    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    finishedNavigation = true;
}

@end

TEST(WebpagePreferences, WebsitePoliciesCustomNavigatorPlatform)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[CustomNavigatorPlatformDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,hello"]];
    [webView loadRequest:request];
    
    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    EXPECT_STREQ("Test Custom Platform", [[webView stringByEvaluatingJavaScript:@"navigator.platform"] UTF8String]);
}

#if PLATFORM(IOS_FAMILY)

static const char* deviceOrientationEventTestBytes = R"TESTRESOURCE(
<script>
addEventListener("deviceorientation", (event) => {
    webkit.messageHandlers.testHandler.postMessage("received-device-orientation-event");
});
</script>
)TESTRESOURCE";

@interface WebsitePoliciesDeviceOrientationDelegate : NSObject <WKNavigationDelegate> {
    _WKWebsiteDeviceOrientationAndMotionAccessPolicy _accessPolicy;
}
- (instancetype)initWithDeviceOrientationAccessPolicy:(_WKWebsiteDeviceOrientationAndMotionAccessPolicy)accessPolicy;
@end

@implementation WebsitePoliciesDeviceOrientationDelegate

- (instancetype)initWithDeviceOrientationAccessPolicy:(_WKWebsiteDeviceOrientationAndMotionAccessPolicy)accessPolicy
{
    self = [super init];
    _accessPolicy = accessPolicy;
    return self;
}

- (void)_webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences userInfo:(id <NSSecureCoding>)userInfo decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
    [websitePolicies _setDeviceOrientationAndMotionAccessPolicy:_accessPolicy];

    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    finishedNavigation = true;
}

@end

static bool calledShouldAllowDeviceOrientationAndMotionAccessDelegate = false;

@interface WebsitePoliciesDeviceOrientationUIDelegate : NSObject <WKUIDelegatePrivate> {
}
@end

@implementation WebsitePoliciesDeviceOrientationUIDelegate

- (void)webView:(WKWebView *)webView requestDeviceOrientationAndMotionPermissionForOrigin:(WKSecurityOrigin*)origin initiatedByFrame:(WKFrameInfo *)requestingFrame decisionHandler:(void (^)(WKPermissionDecision))decisionHandler
{
    calledShouldAllowDeviceOrientationAndMotionAccessDelegate = true;
    decisionHandler(WKPermissionDecisionGrant);
}

@end

static void runWebsitePoliciesDeviceOrientationEventTest(_WKWebsiteDeviceOrientationAndMotionAccessPolicy accessPolicy)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[DataMappingSchemeHandler alloc] init]);
    [schemeHandler addMappingFromURLString:@"test://localhost/main.html" toData:deviceOrientationEventTestBytes];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[WebsitePoliciesDeviceOrientationDelegate alloc] initWithDeviceOrientationAccessPolicy:accessPolicy]);
    [webView setNavigationDelegate:delegate.get()];
    auto uiDelegate = adoptNS([[WebsitePoliciesDeviceOrientationUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"test://localhost/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    bool askedForPermission = false;
    [webView evaluateJavaScript:@"DeviceOrientationEvent.requestPermission()" completionHandler: [&] (id result, NSError *error) {
        askedForPermission = true;
    }];
    TestWebKitAPI::Util::run(&askedForPermission);

    __block bool didReceiveMessage = false;
    [webView performAfterReceivingMessage:@"received-device-orientation-event" action:^{
        didReceiveMessage = true;
    }];

    [webView _simulateDeviceOrientationChangeWithAlpha:1.0 beta:2.0 gamma:3.0];

    if (accessPolicy == _WKWebsiteDeviceOrientationAndMotionAccessPolicyAsk)
        EXPECT_TRUE(calledShouldAllowDeviceOrientationAndMotionAccessDelegate);
    else
        EXPECT_FALSE(calledShouldAllowDeviceOrientationAndMotionAccessDelegate);

    if (accessPolicy != _WKWebsiteDeviceOrientationAndMotionAccessPolicyDeny)
        TestWebKitAPI::Util::run(&didReceiveMessage);
    else {
        TestWebKitAPI::Util::runFor(0.1_s);
        EXPECT_FALSE(didReceiveMessage);
    }
}

TEST(WebpagePreferences, WebsitePoliciesDeviceOrientationGrantAccess)
{
    runWebsitePoliciesDeviceOrientationEventTest(_WKWebsiteDeviceOrientationAndMotionAccessPolicyGrant);
}

TEST(WebpagePreferences, WebsitePoliciesDeviceOrientationDenyAccess)
{
    runWebsitePoliciesDeviceOrientationEventTest(_WKWebsiteDeviceOrientationAndMotionAccessPolicyDeny);
}

TEST(WebpagePreferences, WebsitePoliciesDeviceOrientationAskAccess)
{
    runWebsitePoliciesDeviceOrientationEventTest(_WKWebsiteDeviceOrientationAndMotionAccessPolicyAsk);
}

#endif // PLATFORM(IOS_FAMILY)

@interface PopUpPoliciesDelegate : NSObject <WKNavigationDelegate, WKUIDelegatePrivate>
@property (nonatomic, copy) _WKWebsitePopUpPolicy(^popUpPolicyForURL)(NSURL *);
@end

@implementation PopUpPoliciesDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    // webView:decidePolicyForNavigationAction:preferences:decisionHandler: should be called instead if it is implemented.
    EXPECT_TRUE(false);
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
    if (_popUpPolicyForURL)
        [websitePolicies _setPopUpPolicy:_popUpPolicyForURL(navigationAction.request.URL)];
    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    return [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration];
}

@end

TEST(WebpagePreferences, WebsitePoliciesPopUp)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[PopUpPoliciesDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"pop-up-check" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    [delegate setPopUpPolicyForURL:^_WKWebsitePopUpPolicy(NSURL *) {
        return _WKWebsitePopUpPolicyBlock;
    }];

    [webView loadRequest:request];
    [webView waitForMessage:@"pop-up-blocked"];

    [delegate setPopUpPolicyForURL:^_WKWebsitePopUpPolicy(NSURL *) {
        return _WKWebsitePopUpPolicyAllow;
    }];

    [webView loadRequest:request];
    [webView waitForMessage:@"pop-up-allowed"];
}

@interface WebsitePoliciesWebsiteDataStoreDelegate : NSObject <WKNavigationDelegatePrivate, WKURLSchemeHandler, WKUIDelegate>
@end

@implementation WebsitePoliciesWebsiteDataStoreDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    NSURL *url = navigationAction.request.URL;
    if ([url.path isEqualToString:@"/invalid"]) {
        auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
        [websitePolicies _setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]).get()]).get()];

        bool sawException = false;
        @try {
            decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
        } @catch (NSException *exception) {
            sawException = true;
        }
        EXPECT_FALSE(sawException);

        done = true;
    }
    if ([url.path isEqualToString:@"/checkStorage"]
        || [url.path isEqualToString:@"/checkCookies"]
        || [url.path isEqualToString:@"/mainFrame"]) {
        auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
        [websitePolicies _setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
        decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
    }
    if ([url.path isEqualToString:@"/subFrame"]) {
        auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
        [websitePolicies _setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
        bool sawException = false;
        @try {
            decisionHandler(WKNavigationActionPolicyCancel, websitePolicies.get());
        } @catch (NSException *exception) {
            sawException = true;
        }
        EXPECT_TRUE(sawException);
        done = true;
    }
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    NSURL *url = task.request.URL;
    if ([url.path isEqualToString:@"/checkStorage"]) {
        NSString *html = @"<script>var oldValue = window.sessionStorage['storageKey']; window.sessionStorage['storageKey'] = 'value'; alert('old value: <' + (oldValue ? 'fail' : '') + '>');</script>";
        [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:url MIMEType:@"text/html" expectedContentLength:html.length textEncodingName:nil]).get()];
        [task didReceiveData:[html dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    EXPECT_STREQ(message.UTF8String, "old value: <>");
    completionHandler();
    done = true;
}

@end

RetainPtr<WKWebView> websiteDataStoreTestWebView()
{
    auto delegate = adoptNS([[WebsitePoliciesWebsiteDataStoreDelegate alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:delegate.get() forURLScheme:@"test"];
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    return webView;
}

TEST(WebpagePreferences, UpdateWebsitePoliciesInvalid)
{
    auto webView = websiteDataStoreTestWebView();
    auto policies = adoptNS([[WKWebpagePreferences alloc] init]);
    [policies _setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    bool sawException = false;
    @try {
        [webView _updateWebpagePreferences:policies.get()];
    } @catch (NSException *exception) {
        sawException = true;
    }
    EXPECT_TRUE(sawException);
    
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///invalid"]]];
    TestWebKitAPI::Util::run(&done);
    
    done = false;
    [webView loadHTMLString:@"<iframe src='subFrame'></iframe>" baseURL:[NSURL URLWithString:@"http://webkit.org/mainFrame"]];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebpagePreferences, WebsitePoliciesDataStore)
{
    auto cookieWebView = websiteDataStoreTestWebView();
    NSString *alertOldCookie = @"<script>var oldCookie = document.cookie; document.cookie = 'key=value'; alert('old value: <' + oldCookie + '>');</script>";
    [cookieWebView loadHTMLString:alertOldCookie baseURL:[NSURL URLWithString:@"http://example.com/checkCookies"]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [cookieWebView _webProcessIdentifier];

    [cookieWebView loadHTMLString:alertOldCookie baseURL:[NSURL URLWithString:@"http://example.com/checkCookies"]];
    TestWebKitAPI::Util::run(&done);

    EXPECT_NE(pid1, [cookieWebView _webProcessIdentifier]);
}

TEST(WebpagePreferences, WebsitePoliciesUserContentController)
{
    auto makeScript = [] (NSString *script) {
        return adoptNS([[WKUserScript alloc] initWithSource:script injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);
    };
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addUserScript:makeScript(@"alert('testAlert1')").get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    __block bool iframeExceptionThrown = false;
    __block RetainPtr<WKUserContentController> replacementUserContentController;
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if ([action.request.URL.path hasSuffix:@"/simple-iframe.html"])
            return completionHandler(WKNavigationActionPolicyAllow, nil);
        if ([action.request.URL.path hasSuffix:@"/simple.html"]) {
            @try {
                auto preferences = adoptNS([WKWebpagePreferences new]);
                [preferences _setUserContentController:adoptNS([WKUserContentController new]).get()];
                return completionHandler(WKNavigationActionPolicyAllow, preferences.get());
            } @catch (NSException *exception) {
                iframeExceptionThrown = true;
            }
            return;
        }
        
        EXPECT_TRUE([action.request.URL.path hasSuffix:@"/simple2.html"]);
        auto preferences = adoptNS([WKWebpagePreferences new]);
        replacementUserContentController = adoptNS([WKUserContentController new]);
        [replacementUserContentController addUserScript:makeScript(@"alert('testAlert2')").get()];
        [preferences _setUserContentController:replacementUserContentController.get()];
        completionHandler(WKNavigationActionPolicyAllow, preferences.get());
    };
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadTestPageNamed:@"simple-iframe"];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "testAlert1");
    TestWebKitAPI::Util::run(&iframeExceptionThrown);

    [webView loadTestPageNamed:@"simple2"];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "testAlert2");

    bool caughtException = false;
    @try {
        auto preferences = adoptNS([WKWebpagePreferences new]);
        [preferences _setUserContentController:adoptNS([WKUserContentController new]).get()];
        [webView _updateWebpagePreferences:preferences.get()];
    } @catch (NSException *exception) {
        caughtException = true;
    }
    EXPECT_TRUE(caughtException);

    [replacementUserContentController _addUserScriptImmediately:makeScript(@"alert('testAlert3');").get()];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "testAlert3");
}

TEST(WebpagePreferences, UserExplicitlyPrefersColorSchemeLight)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    configuration.get().defaultWebpagePreferences._colorSchemePreference = _WKWebsiteColorSchemePreferenceLight;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadTestPageNamed:@"color-scheme"];
    [webView waitForMessage:@"light-detected"];
}

TEST(WebpagePreferences, UserExplicitlyPrefersColorSchemeDark)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    configuration.get().defaultWebpagePreferences._colorSchemePreference = _WKWebsiteColorSchemePreferenceDark;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadTestPageNamed:@"color-scheme"];
    [webView waitForMessage:@"dark-detected"];
}

TEST(WebpagePreferences, ToggleNetworkConnectionIntegrity)
{
    auto preferences = adoptNS([WKWebpagePreferences new]);
    EXPECT_FALSE([preferences _networkConnectionIntegrityEnabled]);
    [preferences _setNetworkConnectionIntegrityEnabled:YES];
    EXPECT_TRUE([preferences _networkConnectionIntegrityEnabled]);
    [preferences _setNetworkConnectionIntegrityEnabled:NO];
    EXPECT_FALSE([preferences _networkConnectionIntegrityEnabled]);
    [preferences _setNetworkConnectionIntegrityEnabled:YES];
    EXPECT_TRUE([preferences _networkConnectionIntegrityEnabled]);
}
