/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInternalDebugFeature.h>
#import <wtf/RetainPtr.h>

TEST(GPUProcess, RelaunchOnCrash)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"UseGPUProcessForMediaEnabled"]) {
            [[configuration preferences] _setEnabled:YES forInternalDebugFeature:feature];
            break;
        }
    }

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"audio-context-playing"];

    // evaluateJavaScript gives us the user gesture we need to reliably start audio playback on all platforms.
    __block bool done = false;
    [webView evaluateJavaScript:@"startPlaying()" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto webViewPID = [webView _webProcessIdentifier];

    auto* processPool = configuration.get().processPool;
    unsigned timeout = 0;
    while (![processPool _gpuProcessIdentifier] && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);

    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    if (![processPool _gpuProcessIdentifier])
        return;

    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_TRUE([webView _isPlayingAudio]);

    auto initialGPUProcessPID = [processPool _gpuProcessIdentifier];
    kill(initialGPUProcessPID, 9);

    // Make sure the GPU process gets relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == initialGPUProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], initialGPUProcessPID);

    // Make sure the WebView's WebProcess did not crash or get terminated.
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);

    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_TRUE([webView _isPlayingAudio]);
}

TEST(GPUProcess, WebProcessTerminationAfterTooManyGPUProcessCrashes)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"UseGPUProcessForMediaEnabled"]) {
            [[configuration preferences] _setEnabled:YES forInternalDebugFeature:feature];
            break;
        }
    }

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"audio-context-playing"];

    // evaluateJavaScript gives us the user gesture we need to reliably start audio playback on all platforms.
    __block bool done = false;
    [webView evaluateJavaScript:@"startPlaying()" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto webViewPID = [webView _webProcessIdentifier];

    auto* processPool = configuration.get().processPool;
    unsigned timeout = 0;
    while (![processPool _gpuProcessIdentifier] && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);

    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    if (![processPool _gpuProcessIdentifier])
        return;

    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_TRUE([webView _isPlayingAudio]);

    // First GPUProcess kill.
    auto gpuProcessPID = [processPool _gpuProcessIdentifier];
    kill(gpuProcessPID, 9);

    // Wait for GPU process to get relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == gpuProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], gpuProcessPID);
    gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Make sure the WebView's WebProcess did not crash or get terminated.
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);

    // Second GPUProcess kill.
    kill(gpuProcessPID, 9);

    // Wait for GPU process to get relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == gpuProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], gpuProcessPID);
    gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Make sure the WebView's WebProcess did not crash or get terminated.
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);

    // Third GPUProcess kill.
    kill(gpuProcessPID, 9);

    // The WebView's WebProcess should get killed this time.
    [webView _test_waitForWebContentProcessDidTerminate];

    EXPECT_EQ(0, [webView _webProcessIdentifier]);

    // Reload the test page.
    [webView synchronouslyLoadTestPageNamed:@"audio-context-playing"];

    // Audio should no longer be playing.
    timeout = 0;
    while ([webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_FALSE([webView _isPlayingAudio]);

    // Manually start audio playback again.
    // evaluateJavaScript gives us the user gesture we need to reliably start audio playback on all platforms.
    done = false;
    [webView evaluateJavaScript:@"startPlaying()" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // GPU Process should get relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == gpuProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], gpuProcessPID);
    gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Audio should be playing again.
    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_TRUE([webView _isPlayingAudio]);
}
