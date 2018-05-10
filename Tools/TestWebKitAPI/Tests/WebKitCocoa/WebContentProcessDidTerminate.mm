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

#include "config.h"

#if WK_API_ENABLED

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>

static bool didCrash;
static _WKProcessTerminationReason expectedCrashReason;
static bool finishedLoad;
static bool shouldLoadAgainOnCrash;
static bool receivedScriptMessage;

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

#endif // WK_API_ENABLED
