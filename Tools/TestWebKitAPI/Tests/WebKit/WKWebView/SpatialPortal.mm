/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_PROCESS) && ENABLE(SPATIAL_PORTAL)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/ModelLoadingMessageHandler.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> loadTwoModelPortalWebView(bool spatialPortalEnabled)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAllowTestOnlyIPC:YES];
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelElementEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("ModelProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], spatialPortalEnabled, WKStringCreateWithUTF8CString("SpatialPortalEnabled"));

    RetainPtr messageHandler = adoptNS([[ModelLoadingMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"modelLoading"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"spatial-portal-two-models-page"];

    // The page posts READY only after every model has settled, so the player
    // count is stable by the time we read it.
    while (![messageHandler modelIsReady])
        Util::spinRunLoop();

    return webView;
}

// Two <model>s in a spatial portal share the portal's single ModelPlayer when
// the feature is enabled, but render as two independent standalone players when
// it is disabled. The differing counts prove the preference actually gates
// delegation (a single model would yield 1 either way and wouldn't distinguish).
TEST(SpatialPortal, EnabledSharesOnePlayerAcrossModels)
{
    RetainPtr webView = loadTwoModelPortalWebView(true);
    EXPECT_EQ([webView modelProcessModelPlayerCount], 1u);
}

TEST(SpatialPortal, DisabledRendersModelsStandalone)
{
    RetainPtr webView = loadTwoModelPortalWebView(false);
    EXPECT_EQ([webView modelProcessModelPlayerCount], 2u);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MODEL_PROCESS) && ENABLE(SPATIAL_PORTAL)
