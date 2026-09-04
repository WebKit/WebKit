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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

// Without the fix the retry loop never gets a working context and the GPU process PID
// never changes, so both expectations below FAIL. With the fix the GPU process is reset
// after repeated creation failures, a fresh one serves a working context, and both PASS.

static void enableWebGLInGPUProcess(WKWebViewConfiguration *configuration)
{
    WKPreferencesRef preferences = (__bridge WKPreferencesRef)[configuration preferences];
    WKPreferencesSetBoolValueForKeyForTesting(preferences, true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting(preferences, true, WKStringCreateWithUTF8CString("UseGPUProcessForWebGLEnabled"));
}

// Returns true if a WebGL context that actually works (not lost, no GL error) can be
// obtained. Retries for a few seconds because recovery involves an asynchronous GPU
// process termination + relaunch. Uses setTimeout (not requestAnimationFrame) so it
// makes progress even in an off-window test WebView.
static NSString *retryUntilWorkingWebGLContextJS()
{
    return @"const sleep = ms => new Promise(r => setTimeout(r, ms));"
        "for (let i = 0; i < 80; ++i) {"
        "    const c = document.createElement('canvas');"
        "    c.width = 64; c.height = 64;"
        "    const gl = c.getContext('webgl');"
        "    if (gl) {"
        "        await sleep(50);" // Let an async creation failure surface as context loss.
        "        if (!gl.isContextLost()) {"
        "            gl.clear(gl.COLOR_BUFFER_BIT);"
        "            if (gl.getError() === 0 && !gl.isContextLost())"
        "                return true;"
        "        }"
        "    }"
        "    await sleep(50);"
        "}"
        "return false;";
}

TEST(GPUProcess, WebGLRecoversWhenContextCreationKeepsFailing)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    enableWebGLInGPUProcess(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<html><body></body></html>"];

    // Bring up a healthy GPU process by creating one working WebGL context.
    __block bool done = false;
    [webView callAsyncJavaScript:@"const c = document.createElement('canvas'); c.width = 64; c.height = 64; return !!c.getContext('webgl');" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *processPool = configuration.get().processPool;
    unsigned timeout = 0;
    while (![processPool _gpuProcessIdentifier] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);

    auto initialGPUProcessPID = [processPool _gpuProcessIdentifier];
    EXPECT_NE(initialGPUProcessPID, 0);
    if (!initialGPUProcessPID)
        return;

    // Arm the GPU process so every subsequent GraphicsContextGL comes up with a null backend.
    done = false;
    [webView _setGPUProcessCreatesFailingWebGLContextsForTesting:YES completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Keep trying to obtain a working WebGL context. Without the recovery fix this never
    // succeeds; with the fix the poisoned GPU process is reset and a fresh one serves a
    // working context.
    __block bool gotContext = false;
    done = false;
    [webView callAsyncJavaScript:retryUntilWorkingWebGLContextJS() arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        gotContext = [result boolValue];
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(gotContext);

    // Recovery must have replaced the poisoned GPU process with a fresh one.
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], initialGPUProcessPID);
}

} // namespace TestWebKitAPI
