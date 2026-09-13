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
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
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

} // namespace TestWebKitAPI

#endif // ENABLE(UIPROCESS_PERIODIC_MEMORY_MONITOR)
