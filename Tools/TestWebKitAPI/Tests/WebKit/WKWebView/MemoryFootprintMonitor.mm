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

#if ENABLE(UIPROCESS_PERIODIC_MEMORY_MONITOR)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/ProcessTerminationTestUtilities.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

namespace TestWebKitAPI {

static constexpr auto touchMegabytesHTML = R"HTML(
<script>
function touchMegabytes(mb)
{
    const array = new Uint8Array(mb * 1024 * 1024);
    for (let i = 0; i < array.length; i += 4096)
        array[i] = Math.random() * 256;
    return array.fill(42);
}
</script>
)HTML"_s;

TEST(MemoryFootprintMonitor, PageMemoryLimit)
{
    HTTPServer server({ { "/"_s, { touchMegabytesHTML } } }, HTTPServer::Protocol::Http);

    static constexpr size_t pageMemoryLimit = 200 * MB;

    RetainPtr configuration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [configuration setMemoryFootprintPollIntervalForTesting:0.1];
    [configuration setMemoryLimitForTesting:pageMemoryLimit];

    RetainPtr webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [webViewConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] _initWithConfiguration:configuration.get()]).get()];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get() addToWindow:YES]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    __block bool terminated = false;
    __block _WKProcessTerminationReason terminationReason = _WKProcessTerminationReasonCrash;
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *, _WKProcessTerminationReason reason) {
        terminated = true;
        terminationReason = reason;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView synchronouslyLoadRequest:server.request()];

    auto pid = [webView _webProcessIdentifier];
    EXPECT_NE(pid, 0);

    [webView evaluateJavaScript:@"touchMegabytes(50)" completionHandler:nil];
    Util::runFor(2_s);
    EXPECT_FALSE(terminated);
    EXPECT_EQ([webView _webProcessIdentifier], pid);

    [webView evaluateJavaScript:@"touchMegabytes(400)" completionHandler:nil];
    bool didTerminate = Util::runFor(&terminated, 2_s);
    ASSERT_TRUE(didTerminate);
    EXPECT_EQ(terminationReason, _WKProcessTerminationReasonExceededMemoryLimit);
}

TEST(MemoryFootprintMonitor, SiteIsolatedPageMemoryLimit)
{
    HTTPServer server({
        { "/example"_s, { makeString("<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe>"_s, touchMegabytesHTML) } },
        { "/webkit"_s, { touchMegabytesHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    // The page's processes exceed this limit together, but neither of them does on its own.
    static constexpr size_t memoryLimit = 300 * MB;

    RetainPtr configuration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [configuration setMemoryFootprintPollIntervalForTesting:0.1];
    [configuration setMemoryLimitForTesting:memoryLimit];
    [configuration setUsesWebProcessCache:YES];

    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:adoptNS([[WKProcessPool alloc] _initWithConfiguration:configuration.get()]).get()];
    enableSiteIsolation(webViewConfiguration.get());
    setFeatureEnabled(webViewConfiguration.get(), @"SiteIsolationSharedProcessEnabled", false);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get() addToWindow:YES]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    __block Vector<_WKProcessTerminationReason> terminationReasons;
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *, _WKProcessTerminationReason reason) {
        terminationReasons.append(reason);
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    pid_t mainFrameProcess = [webView _webProcessIdentifier];
    pid_t iframeProcess = [webView firstChildFrame]._processIdentifier;
    EXPECT_NE(iframeProcess, mainFrameProcess);

    [webView evaluateJavaScript:@"window.mainFrameMemory = touchMegabytes(150)" completionHandler:nil];
    Util::runFor(1_s);
    EXPECT_TRUE(terminationReasons.isEmpty());

    [webView evaluateJavaScript:@"window.iframeMemory = touchMegabytes(150)" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_TRUE(Util::waitFor(^{
        return !terminationReasons.isEmpty();
    }));
    ASSERT_EQ(terminationReasons.size(), 1u);
    EXPECT_EQ(terminationReasons[0], _WKProcessTerminationReasonExceededMemoryLimit);

    EXPECT_TRUE(Util::waitFor([&] {
        return !processStillRunning(iframeProcess);
    }));
    [webView reload];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_NE([webView firstChildFrame]._processIdentifier, iframeProcess);
    EXPECT_EQ(terminationReasons.size(), 1u);
}

} // namespace TestWebKitAPI

#endif // ENABLE(UIPROCESS_PERIODIC_MEMORY_MONITOR)
