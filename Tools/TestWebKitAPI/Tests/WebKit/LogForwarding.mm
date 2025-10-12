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

#if WK_HAVE_C_SPI && ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)

#import "LogStream.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>

TEST(WebKit, LogForwarding)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView2 synchronouslyLoadTestPageNamed:@"simple"];

    auto emitWebCoreLogsFromMainThread = [&](WKWebView *webView, unsigned logCount) {
        auto jsString = [NSString stringWithFormat:@"window.internals.emitWebCoreLogs(%d, true)", logCount];
        return [webView stringByEvaluatingJavaScript:jsString].boolValue;
    };

    auto emitWebCoreLogsFromBackgroundThread = [&](WKWebView *webView, unsigned logCount) {
        auto jsString = [NSString stringWithFormat:@"window.internals.emitWebCoreLogs(%d, false)", logCount];
        return [webView stringByEvaluatingJavaScript:jsString].boolValue;
    };

    auto emitLogsFromBackgroundThread = [&](WKWebView *webView, ASCIILiteral logString, unsigned logCount) {
        auto jsString = [NSString stringWithFormat:@"window.internals.emitLogs('%s', %d, false)", logString.characters(), logCount];
        return [webView stringByEvaluatingJavaScript:jsString].boolValue;
    };

    auto emitLogsFromMainThread = [&](WKWebView *webView, ASCIILiteral logString, unsigned logCount) {
        auto jsString = [NSString stringWithFormat:@"window.internals.emitLogs('%s', %d, true)", logString.characters(), logCount];
        return [webView stringByEvaluatingJavaScript:jsString].boolValue;
    };

    constexpr auto backgroundThreadLogCount = 1024;
    constexpr auto mainThreadLogCount = 1024;

    constexpr ASCIILiteral logMessageOnBackgroundThread = "Log message on background thread"_s;
    constexpr ASCIILiteral logMessageOnMainThread = "Log message on main thread"_s;

    EXPECT_TRUE(emitWebCoreLogsFromMainThread(webView.get(), mainThreadLogCount));
    EXPECT_TRUE(emitWebCoreLogsFromMainThread(webView2.get(), mainThreadLogCount));

    EXPECT_TRUE(emitWebCoreLogsFromBackgroundThread(webView.get(), mainThreadLogCount));
    EXPECT_TRUE(emitWebCoreLogsFromBackgroundThread(webView2.get(), mainThreadLogCount));

    EXPECT_TRUE(emitLogsFromBackgroundThread(webView.get(), logMessageOnBackgroundThread, backgroundThreadLogCount));
    EXPECT_TRUE(emitLogsFromBackgroundThread(webView2.get(), logMessageOnBackgroundThread, backgroundThreadLogCount));

    EXPECT_TRUE(emitLogsFromMainThread(webView.get(), logMessageOnMainThread, mainThreadLogCount));
    EXPECT_TRUE(emitLogsFromMainThread(webView2.get(), logMessageOnMainThread, mainThreadLogCount));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]]];
    [webView2 loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]]];

    unsigned expectedLogCount = (backgroundThreadLogCount + mainThreadLogCount) * 4;
    while ([webView _forwardedLogsCountForTesting] < expectedLogCount)
        TestWebKitAPI::Util::spinRunLoop(1);
}

TEST(WebKit, LaunchLogs)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    EXPECT_TRUE([webView _receivedLogsDuringLaunchForTesting]);
}
#endif // WK_HAVE_C_SPI
