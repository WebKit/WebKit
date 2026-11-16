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

#import "Helpers/cocoa/ModelLoadingMessageHandler.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestNSBundleExtras.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
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
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAllowTestOnlyIPC:YES];
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelElementEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelProcessEnabled"));

    RetainPtr messageHandler = adoptNS([[ModelLoadingMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"modelLoading"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
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
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAllowTestOnlyIPC:YES];
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelElementEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelProcessEnabled"));

    RetainPtr messageHandler = adoptNS([[ModelLoadingMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"modelLoading"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
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
    [configuration _setAllowTestOnlyIPC:YES];
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)configuration.preferences, true, WKStringCreateWithUTF8CString("ModelElementEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)configuration.preferences, true, WKStringCreateWithUTF8CString("ModelProcessEnabled"));

    RetainPtr messageHandler = adoptNS([[ModelLoadingMessageHandler alloc] init]);
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"modelLoading"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration]);
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

#if ENABLE(SPATIAL_PORTAL)

TEST(ModelProcess, WebProcessTerminationAfterTooManyModelProcessCrashesWithSpatialPortals)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAllowTestOnlyIPC:YES];
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelElementEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("SpatialPortalEnabled"));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);

    // A spatial portal recreates its model player synchronously when the model process exits. With two portals,
    // the WebProcess's model player count never drops to zero, so it never sends StartedPlayingModels again.
    [webView synchronouslyLoadHTMLString:@"<style>.portal { spatial: portal; width: 300px; height: 150px; }</style>"
        "<div class='portal'><model><source src='cube.usdz'></model></div>"
        "<div class='portal'><model><source src='cube.usdz'></model></div>"
        baseURL:NSBundle.test_resourcesBundle.resourceURL];

    __block bool done = false;
    [webView callAsyncJavaScript:@"await Promise.all([...document.querySelectorAll('model')].map(model => model.ready))" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    Util::run(&done);

    EXPECT_EQ([webView modelProcessModelPlayerCount], 2u);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    auto terminationReason = std::make_shared<std::optional<_WKProcessTerminationReason>>();
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *, _WKProcessTerminationReason reason) {
        *terminationReason = reason;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto webProcessPID = [webView _webProcessIdentifier];
    auto modelProcessPID = [webView _modelProcessIdentifier];
    ASSERT_NE(modelProcessPID, 0);

    // The first two model process crashes are below the limit, so the WebProcess must survive them.
    for (unsigned i = 0; i < 2; ++i) {
        kill(modelProcessPID, SIGKILL);

        ASSERT_TRUE(Util::waitFor([&] {
            auto relaunchedModelProcessPID = [webView _modelProcessIdentifier];
            return relaunchedModelProcessPID && relaunchedModelProcessPID != modelProcessPID;
        }));
        modelProcessPID = [webView _modelProcessIdentifier];

        EXPECT_FALSE(terminationReason->has_value());
        EXPECT_EQ(webProcessPID, [webView _webProcessIdentifier]);
    }

    // The third crash exceeds the limit. The WebProcess is still using models, so it must be terminated.
    kill(modelProcessPID, SIGKILL);

    ASSERT_TRUE(Util::waitFor([&] {
        return terminationReason->has_value();
    }, 50));
    EXPECT_EQ(terminationReason->value(), _WKProcessTerminationReasonExceededSharedProcessCrashLimit);
    EXPECT_EQ([webView _webProcessIdentifier], 0);
}

#endif // ENABLE(SPATIAL_PORTAL)

} // namespace TestWebKitAPI

#endif
