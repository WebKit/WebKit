/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_PROCESS)

#import "ModelLoadingMessageHandler.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <notify.h>
#import <objc/runtime.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(ModelProcess, CleanUpOnReload)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelElementEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelProcessEnabled"));

    RetainPtr messageHandler = adoptNS([[ModelLoadingMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"modelLoading"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple-model-page"];

    while (![messageHandler modelIsReady])
        Util::spinRunLoop();

    EXPECT_EQ([webView modelProcessModelPlayerCount], 1u);

    [messageHandler setModelIsReady:NO];
    [webView reload];

    while (![messageHandler modelIsReady])
        Util::spinRunLoop();

    EXPECT_EQ([webView modelProcessModelPlayerCount], 1u);
}

TEST(ModelProcess, CleanUpOnNavigate)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelElementEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelProcessEnabled"));

    RetainPtr messageHandler = adoptNS([[ModelLoadingMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"modelLoading"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple-model-page"];

    while (![messageHandler modelIsReady])
        Util::spinRunLoop();

    EXPECT_EQ([webView modelProcessModelPlayerCount], 1u);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView modelProcessModelPlayerCount], 0u);
}

TEST(ModelProcess, CleanUpOnHide)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)configuration.preferences, true, WKStringCreateWithUTF8CString("ModelElementEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)configuration.preferences, true, WKStringCreateWithUTF8CString("ModelProcessEnabled"));

    RetainPtr messageHandler = adoptNS([[ModelLoadingMessageHandler alloc] init]);
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"modelLoading"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"simple-model-page"];

    bool isHidden = false;
    [webView performAfterReceivingMessage:@"hidden" action:[&] { isHidden = true; }];
    [webView objectByEvaluatingJavaScript:@"document.addEventListener('visibilitychange', event => { if (document.hidden) window.webkit.messageHandlers.testHandler.postMessage('hidden') })"];

    while (![messageHandler modelIsReady])
        Util::spinRunLoop();

    EXPECT_EQ([webView modelProcessModelPlayerCount], 1u);

    [webView objectByEvaluatingJavaScript:@"window.internals.setPageVisibility(false)"];
    TestWebKitAPI::Util::run(&isHidden);

    EXPECT_EQ([webView modelProcessModelPlayerCount], 0u);
}

} // namespace TestWebKitAPI

#endif
