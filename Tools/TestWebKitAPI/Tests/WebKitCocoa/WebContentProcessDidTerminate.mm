/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

static bool didCrash;
static _WKProcessTerminationReason expectedCrashReason;
static bool startedLoad;
static bool finishedLoad;
static bool shouldLoadAgainOnCrash;
static bool receivedScriptMessage;
static bool calledAllCallbacks;
static unsigned callbackCount;
static unsigned crashHandlerCount;
static unsigned loadCount;
static unsigned expectedLoadCount;

static NSString *testHTML = @"<script>window.webkit.messageHandlers.testHandler.postMessage('LOADED');</script>";

@interface CrashOnStartNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation CrashOnStartNavigationDelegate

- (void)_webView:(WKWebView *)webView webContentProcessDidTerminateWithReason:(_WKProcessTerminationReason)reason
{
    EXPECT_FALSE(didCrash);
    didCrash = true;
    EXPECT_EQ(expectedCrashReason, reason);
    EXPECT_EQ(0, webView._webProcessIdentifier);

    // Attempt the load again.
    if (shouldLoadAgainOnCrash)
        [webView loadHTMLString:testHTML baseURL:nil];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    finishedLoad = true;
}

@end

@interface BasicNavigationDelegateWithoutCrashHandler : NSObject <WKNavigationDelegate>
@end

@implementation BasicNavigationDelegateWithoutCrashHandler

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(null_unspecified WKNavigation *)navigation
{
    startedLoad = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    finishedLoad = true;
}
@end

@interface CrashRecoveryScriptMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation CrashRecoveryScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_STREQ([(NSString *)[message body] UTF8String], "LOADED");
    receivedScriptMessage = true;
}

@end

TEST(WKNavigation, FailureToStartWebProcessRecovery)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[CrashRecoveryScriptMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    [configuration.get().processPool _makeNextWebProcessLaunchFailForTesting];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[CrashOnStartNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    finishedLoad = false;
    didCrash = false;
    receivedScriptMessage = false;
    expectedCrashReason = _WKProcessTerminationReasonCrash;
    shouldLoadAgainOnCrash = true;

    [webView loadHTMLString:testHTML baseURL:nil];
    TestWebKitAPI::Util::run(&finishedLoad);

    EXPECT_TRUE(didCrash);
    EXPECT_TRUE(!!webView.get()._webProcessIdentifier);
    EXPECT_TRUE(receivedScriptMessage);
}

TEST(WKNavigation, FailureToStartWebProcessAfterCrashRecovery)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[CrashRecoveryScriptMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[CrashOnStartNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    receivedScriptMessage = false;
    finishedLoad = false;
    didCrash = false;

    [webView loadHTMLString:testHTML baseURL:nil];
    TestWebKitAPI::Util::run(&finishedLoad);

    EXPECT_FALSE(didCrash);
    EXPECT_TRUE(!!webView.get()._webProcessIdentifier);
    EXPECT_TRUE(receivedScriptMessage);

    receivedScriptMessage = false;
    shouldLoadAgainOnCrash = false;
    expectedCrashReason = _WKProcessTerminationReasonRequestedByClient;
    [webView _killWebContentProcessAndResetState];

    TestWebKitAPI::Util::run(&didCrash);
    EXPECT_TRUE(!webView.get()._webProcessIdentifier);
    EXPECT_FALSE(receivedScriptMessage);

    expectedCrashReason = _WKProcessTerminationReasonCrash;
    didCrash = false;
    finishedLoad = false;
    receivedScriptMessage = false;
    shouldLoadAgainOnCrash = true;

    [configuration.get().processPool _makeNextWebProcessLaunchFailForTesting];
    [webView loadHTMLString:testHTML baseURL:nil];

    TestWebKitAPI::Util::run(&finishedLoad);

    EXPECT_TRUE(didCrash);
    EXPECT_TRUE(!!webView.get()._webProcessIdentifier);
    EXPECT_TRUE(receivedScriptMessage);
}

TEST(WKNavigation, AutomaticViewReloadAfterWebProcessCrash)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);

    auto delegate = adoptNS([[BasicNavigationDelegateWithoutCrashHandler alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    startedLoad = false;
    finishedLoad = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"rich-and-plain-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    TestWebKitAPI::Util::run(&finishedLoad);

    startedLoad = false;
    finishedLoad = false;

    // Simulate crash.
    [webView _killWebContentProcess];

    // Since we do not deal with the crash, WebKit should attempt a reload.
    TestWebKitAPI::Util::run(&finishedLoad);

    startedLoad = false;
    finishedLoad = false;

    // Simulate another crash.
    [webView _killWebContentProcess];

    // WebKit should not attempt to reload again.
    EXPECT_FALSE(startedLoad);
    TestWebKitAPI::Util::sleep(0.5);
    EXPECT_FALSE(startedLoad);
}

TEST(WKNavigation, ProcessCrashDuringCallback)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto delegate = adoptNS([[BasicNavigationDelegateWithoutCrashHandler alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    startedLoad = false;
    finishedLoad = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"rich-and-plain-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    TestWebKitAPI::Util::run(&finishedLoad);

    startedLoad = false;
    finishedLoad = false;

    callbackCount = 0;
    calledAllCallbacks = false;

    __block WKWebView *view = webView.get();
    [webView _getContentsAsStringWithCompletionHandler:^(NSString *contents, NSError *error) {
        if (!!error)
            EXPECT_TRUE(error.code == WKErrorWebContentProcessTerminated || error.code == WKErrorWebViewInvalidated);
        ++callbackCount;
        if (callbackCount == 6)
            calledAllCallbacks = true;
    }];
    [webView _getContentsAsStringWithCompletionHandler:^(NSString *contents, NSError *error) {
        if (!!error)
            EXPECT_TRUE(error.code == WKErrorWebContentProcessTerminated || error.code == WKErrorWebViewInvalidated);
        ++callbackCount;
        if (callbackCount == 6)
            calledAllCallbacks = true;
    }];
    [webView _getContentsAsStringWithCompletionHandler:^(NSString *contents, NSError *error) {
        if (!!error)
            EXPECT_TRUE(error.code == WKErrorWebContentProcessTerminated || error.code == WKErrorWebViewInvalidated);
        [view _close]; // Calling _close will also invalidate all callbacks.
        ++callbackCount;
        if (callbackCount == 6)
            calledAllCallbacks = true;
    }];
    [webView _getContentsAsStringWithCompletionHandler:^(NSString *contents, NSError *error) {
        if (!!error)
            EXPECT_TRUE(error.code == WKErrorWebContentProcessTerminated || error.code == WKErrorWebViewInvalidated);
        ++callbackCount;
        if (callbackCount == 6)
            calledAllCallbacks = true;
    }];
    [webView _getContentsAsStringWithCompletionHandler:^(NSString *contents, NSError *error) {
        if (!!error)
            EXPECT_TRUE(error.code == WKErrorWebContentProcessTerminated || error.code == WKErrorWebViewInvalidated);
        ++callbackCount;
        if (callbackCount == 6)
            calledAllCallbacks = true;
    }];
    [webView _getContentsAsStringWithCompletionHandler:^(NSString *contents, NSError *error) {
        if (!!error)
            EXPECT_TRUE(error.code == WKErrorWebContentProcessTerminated || error.code == WKErrorWebViewInvalidated);
        ++callbackCount;
        if (callbackCount == 6)
            calledAllCallbacks = true;
    }];

    // Simulate a crash, which should invalidate all pending callbacks.
    [webView _killWebContentProcess];

    TestWebKitAPI::Util::run(&calledAllCallbacks);
    TestWebKitAPI::Util::sleep(0.5);
    EXPECT_EQ(6U, callbackCount);
}

@interface NavigationDelegateWithCrashHandlerThatLoadsAgain : NSObject <WKNavigationDelegate>
@end

@implementation NavigationDelegateWithCrashHandlerThatLoadsAgain

- (void)_webView:(WKWebView *)webView webContentProcessDidTerminateWithReason:(_WKProcessTerminationReason)reason
{
    ++crashHandlerCount;

    // Attempt the load again synchronously.
    [webView loadHTMLString:@"foo" baseURL:nil];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    if (++loadCount == expectedLoadCount)
        finishedLoad = true;
}

@end

TEST(WKNavigation, ReloadRelatedViewsInProcessDidTerminate)
{
    const unsigned numberOfViews = 20;
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);

    Vector<RetainPtr<WKWebView>> webViews;
    webViews.append(webView1);

    configuration.get()._relatedWebView = webView1.get();
    for (unsigned i = 0; i < numberOfViews - 1; ++i) {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
        webViews.append(webView);
    }
    auto delegate = adoptNS([[NavigationDelegateWithCrashHandlerThatLoadsAgain alloc] init]);
    for (auto& webView : webViews)
        [webView setNavigationDelegate:delegate.get()];

    crashHandlerCount = 0;
    loadCount = 0;
    expectedLoadCount = numberOfViews;
    finishedLoad = false;

    for (auto& webView : webViews)
        [webView loadHTMLString:@"foo" baseURL:nil];

    TestWebKitAPI::Util::run(&finishedLoad);
    EXPECT_EQ(0U, crashHandlerCount);

    auto pidBefore = [webView1 _webProcessIdentifier];
    EXPECT_TRUE(!!pidBefore);
    for (auto& webView : webViews)
        EXPECT_EQ(pidBefore, [webView _webProcessIdentifier]);

    loadCount = 0;
    finishedLoad = false;

    // Kill the WebContent process. The crash handler should reload all views.
    kill(pidBefore, 9);

    TestWebKitAPI::Util::run(&finishedLoad);
    EXPECT_EQ(numberOfViews, crashHandlerCount);

    auto pidAfter = [webView1 _webProcessIdentifier];
    EXPECT_TRUE(!!pidAfter);
    for (auto& webView : webViews)
        EXPECT_EQ(pidAfter, [webView _webProcessIdentifier]);
}

TEST(WKNavigation, WebViewURLInProcessDidTerminate)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    NSString *viewURL = [webView URL].absoluteString;
    EXPECT_TRUE(!!viewURL);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    __block bool done = false;
    navigationDelegate.get().webContentProcessDidTerminate = ^(WKWebView *view) {
        EXPECT_EQ(view, webView.get());
        EXPECT_WK_STREQ(view.URL.absoluteString, viewURL);
        done = true;
    };
    kill([webView _webProcessIdentifier], 9);
    TestWebKitAPI::Util::run(&done);
}

TEST(WKNavigation, WebProcessLimit)
{
    constexpr unsigned maxProcessCount = 10;
    [WKProcessPool _setWebProcessCountLimit:maxProcessCount];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        finishedLoad = true;
    }];
    auto createWebView = [&] {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        finishedLoad = false;
        [webView loadTestPageNamed:@"simple"];
        TestWebKitAPI::Util::run(&finishedLoad);
        return webView;
    };

    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *) {
        didCrash = true;
    }];

    Vector<RetainPtr<WKWebView>> views;
    for (unsigned i = 0; i < maxProcessCount; ++i)
        views.append(createWebView());
    EXPECT_FALSE(didCrash);
    for (auto& view : views)
        EXPECT_NE([view _webProcessIdentifier], 0);

    // We have now reached the WebProcess cap, let's try and launch a new one.
    __block unsigned crashCount = 0;
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView * view) {
        EXPECT_EQ(views[0], view);
        ++crashCount;
    }];
    views.append(createWebView());

    EXPECT_EQ(crashCount, 1U);
    for (unsigned i = 0; i < views.size(); ++i) {
        if (!i)
            EXPECT_EQ([views[i] _webProcessIdentifier], 0);
        else
            EXPECT_NE([views[i] _webProcessIdentifier], 0);
    }

    crashCount = 0;
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView * view) {
        EXPECT_EQ(views[1], view);
        ++crashCount;
    }];
    views.append(createWebView());

    EXPECT_EQ(crashCount, 1U);
    for (unsigned i = 0; i < views.size(); ++i) {
        if (i < 2)
            EXPECT_EQ([views[i] _webProcessIdentifier], 0);
        else
            EXPECT_NE([views[i] _webProcessIdentifier], 0);
    }

    [WKProcessPool _setWebProcessCountLimit:400];
}
