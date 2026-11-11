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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/UserMediaCaptureUIDelegate.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <objc/runtime.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static NSInteger getPixelIndex(NSInteger x, NSInteger y, NSInteger width)
{
    return (y * width + x) * 4;
}

TEST(GPUProcess, RelaunchOnCrash)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"audio-context-playing"];

    // evaluateJavaScript gives us the user gesture we need to reliably start audio playback on all platforms.
    __block bool done = false;
    [webView callAsyncJavaScript:@"return startPlaying()" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto webViewPID = [webView _webProcessIdentifier];

    auto* processPool = configuration.get().processPool;
    unsigned timeout = 0;
    while (![processPool _gpuProcessIdentifier] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    if (![processPool _gpuProcessIdentifier])
        return;

    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_TRUE([webView _isPlayingAudio]);

    auto initialGPUProcessPID = [processPool _gpuProcessIdentifier];
    kill(initialGPUProcessPID, 9);

    // Make sure the GPU process gets relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == initialGPUProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], initialGPUProcessPID);

    // Make sure the WebView's WebProcess did not crash or get terminated.
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);

    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_TRUE([webView _isPlayingAudio]);
}

TEST(GPUProcess, WebProcessTerminationAfterTooManyGPUProcessCrashes)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"audio-context-playing"];

    // evaluateJavaScript gives us the user gesture we need to reliably start audio playback on all platforms.
    __block bool done = false;
    [webView callAsyncJavaScript:@"return startPlaying()" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto webViewPID = [webView _webProcessIdentifier];

    auto* processPool = configuration.get().processPool;
    unsigned timeout = 0;
    while (![processPool _gpuProcessIdentifier] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    if (![processPool _gpuProcessIdentifier])
        return;

    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_TRUE([webView _isPlayingAudio]);

    // First GPUProcess kill.
    auto gpuProcessPID = [processPool _gpuProcessIdentifier];
    kill(gpuProcessPID, 9);

    // Wait for GPU process to get relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == gpuProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    ASSERT_NE([processPool _gpuProcessIdentifier], 0);
    ASSERT_NE([processPool _gpuProcessIdentifier], gpuProcessPID);
    gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Make sure the WebView's WebProcess did not crash or get terminated.
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);

    // Second GPUProcess kill.
    kill(gpuProcessPID, 9);

    // Wait for GPU process to get relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == gpuProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    ASSERT_NE([processPool _gpuProcessIdentifier], 0);
    ASSERT_NE([processPool _gpuProcessIdentifier], gpuProcessPID);
    gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Make sure the WebView's WebProcess did not crash or get terminated.
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);

    // Third GPUProcess kill.
    kill(gpuProcessPID, 9);

    // The WebView's WebProcess should get killed this time.
    auto crashReason = [webView _test_waitForWebContentProcessDidTerminate];
    EXPECT_EQ(crashReason, _WKProcessTerminationReasonExceededSharedProcessCrashLimit);

    EXPECT_EQ(0, [webView _webProcessIdentifier]);

    // Reload the test page.
    [webView synchronouslyLoadTestPageNamed:@"audio-context-playing"];

    // Audio should no longer be playing.
    timeout = 0;
    while ([webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_FALSE([webView _isPlayingAudio]);

    // Manually start audio playback again.
    // evaluateJavaScript gives us the user gesture we need to reliably start audio playback on all platforms.
    done = false;
    [webView callAsyncJavaScript:@"return startPlaying()" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // GPU Process should get relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == gpuProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], gpuProcessPID);
    gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Audio should be playing again.
    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_TRUE([webView _isPlayingAudio]);
}

TEST(GPUProcess, DoNotLeakConnectionAfterClosingWebPage)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"canvas-image-data"];
    EXPECT_EQ(1U, [webView gpuToWebProcessConnectionCount]);
    [webView _close];

    while ([webView gpuToWebProcessConnectionCount])
        TestWebKitAPI::Util::runFor(0.1_s);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
TEST(GPUProcess, LegacyCDM)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("CaptureVideoInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    __block bool done = false;
    [webView evaluateJavaScript:@"WebKitMediaKeys.isTypeSupported('com.apple.fps.1_0')" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // This should not have launched a GPUProcess.
    while (![configuration.get().processPool _gpuProcessIdentifier])
        TestWebKitAPI::Util::spinRunLoop(1);
}
#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)

TEST(GPUProcess, CrashWhilePlayingVideo)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"large-video-with-audio"];

    [webView objectByEvaluatingJavaScript:@"document.getElementsByTagName('video')[0].loop = true;"];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"play()"];

    auto webViewPID = [webView _webProcessIdentifier];

    // The GPU process should get launched.
    auto* processPool = configuration.get().processPool;
    unsigned timeout = 0;
    while (![processPool _gpuProcessIdentifier] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    if (![processPool _gpuProcessIdentifier])
        return;
    auto gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Video should be playing.
    auto ensureIsPlaying = [&] {
        double initialTime = [[webView objectByEvaluatingJavaScript:@"document.getElementsByTagName('video')[0].currentTime"] doubleValue];
        timeout = 0;
        double currentTime = initialTime;
        do {
            TestWebKitAPI::Util::runFor(0.1_s);
            currentTime = [[webView objectByEvaluatingJavaScript:@"document.getElementsByTagName('video')[0].currentTime"] doubleValue];
            if (std::abs(currentTime - initialTime) > 0.01)
                break;
        } while (timeout++ < 100);
        return std::abs(currentTime - initialTime) > 0.01;
    };
    EXPECT_TRUE(ensureIsPlaying());

    // Kill the GPU Process.
    kill(gpuProcessPID, 9);

    // GPU Process should get relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == gpuProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], gpuProcessPID);
    gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Make sure the WebProcess did not crash.
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);

    // Audio should resume playing.
    EXPECT_TRUE(ensureIsPlaying());

    EXPECT_EQ(gpuProcessPID, [processPool _gpuProcessIdentifier]);
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);
}

TEST(GPUProcess, CrashWhilePlayingAudioViaCreateMediaElementSource)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"webaudio-createMediaElementSource"];

    __block bool done = false;
    [webView evaluateJavaScript:@"document.getElementById('testButton').click()" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto webViewPID = [webView _webProcessIdentifier];

    // The GPU process should get launched.
    auto* processPool = configuration.get().processPool;
    unsigned timeout = 0;
    while (![processPool _gpuProcessIdentifier] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    if (![processPool _gpuProcessIdentifier])
        return;
    auto gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Audio should be playing.
    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_TRUE([webView _isPlayingAudio]);

    // Kill the GPU Process.
    kill(gpuProcessPID, 9);

    // GPU Process should get relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == gpuProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], gpuProcessPID);
    gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Make sure the WebProcess did not crash.
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);

    // FIXME: On iOS, video resumes after the GPU process crash but audio does not.
#if !(PLATFORM(IOS) || PLATFORM(VISION))
    // Audio should resume playing.
    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_TRUE([webView _isPlayingAudio]);
#endif

    EXPECT_EQ(gpuProcessPID, [processPool _gpuProcessIdentifier]);
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);
}

static NSString *testCanvasPage = @"<body> \n"
    "<canvas id='myCanvas' width='400px' height='400px'>\n"
    "<script> \n"
    "var context = document.getElementById('myCanvas').getContext('2d'); \n"
    "</script> \n"
    "</body>";

TEST(GPUProcess, CanvasBasicCrashHandling)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));

    NSInteger viewWidth = 400;
    NSInteger viewHeight = 400;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, viewWidth, viewHeight) configuration:configuration.get() addToWindow:NO]);

    RetainPtr<Util::PlatformWindow> window;
    CGFloat backingScaleFactor;

#if PLATFORM(MAC)
    window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    backingScaleFactor = [window backingScaleFactor];
#elif PLATFORM(IOS_FAMILY)
    window = adoptNS([[UIWindow alloc] initWithFrame:[webView frame]]);
    [window addSubview:webView.get()];
    backingScaleFactor = [[window screen] scale];
#endif

    [webView synchronouslyLoadHTMLString:testCanvasPage];

    auto webViewPID = [webView _webProcessIdentifier];

    // Try painting the canvas red.
    __block bool done = false;
    [webView evaluateJavaScript:@"context.fillStyle = '#FF0000'; context.fillRect(0, 0, 400, 400);" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [webView waitForNextPresentationUpdate];

    // The GPU process should have been launched.
    auto* processPool = configuration.get().processPool;
    unsigned timeout = 0;
    while (![processPool _gpuProcessIdentifier] && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    auto gpuProcessPID = [processPool _gpuProcessIdentifier];
    EXPECT_NE(0, gpuProcessPID);

    RetainPtr snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, 150, 150)];
    [snapshotConfiguration setSnapshotWidth:@(150)];
    [snapshotConfiguration setAfterScreenUpdates:YES];

    // Make sure a red square is painted.
    done = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_TRUE(!error);

        RetainPtr cgImage = Util::convertToCGImage(snapshotImage);
        RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());

        NSInteger viewWidthInPixels = viewWidth * backingScaleFactor;
        NSInteger viewHeightInPixels = viewHeight * backingScaleFactor;

        uint8_t *rgba = (unsigned char *)calloc(viewWidthInPixels * viewHeightInPixels * 4, sizeof(unsigned char));
        RetainPtr context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, viewWidthInPixels, viewHeightInPixels), cgImage.get());

        NSInteger pixelIndex = getPixelIndex(50, 50, viewWidthInPixels);
        EXPECT_EQ(255, rgba[pixelIndex]);
        EXPECT_EQ(0, rgba[pixelIndex + 1]);
        EXPECT_EQ(0, rgba[pixelIndex + 2]);

        pixelIndex = getPixelIndex(100, 100, viewWidthInPixels);
        EXPECT_EQ(255, rgba[pixelIndex]);
        EXPECT_EQ(0, rgba[pixelIndex + 1]);
        EXPECT_EQ(0, rgba[pixelIndex + 2]);

        free(rgba);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Kill the GPUProcess.
    kill(gpuProcessPID, 9);

    // GPU Process should get relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == gpuProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], gpuProcessPID);
    gpuProcessPID = [processPool _gpuProcessIdentifier];

    // WebProcess should not have crashed.
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);

    // Try painting the canvas green.
    done = false;
    [webView evaluateJavaScript:@"context.fillStyle = '#00FF00'; context.fillRect(0, 0, 400, 400);" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [webView waitForNextPresentationUpdate];

    // Make sure a green square is painted.
    done = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_TRUE(!error);

        RetainPtr cgImage = Util::convertToCGImage(snapshotImage);
        RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());

        NSInteger viewWidthInPixels = viewWidth * backingScaleFactor;
        NSInteger viewHeightInPixels = viewHeight * backingScaleFactor;

        uint8_t *rgba = (unsigned char *)calloc(viewWidthInPixels * viewHeightInPixels * 4, sizeof(unsigned char));
        RetainPtr context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, viewWidthInPixels, viewHeightInPixels), cgImage.get());

        NSInteger pixelIndex = getPixelIndex(50, 50, viewWidthInPixels);
        EXPECT_EQ(0, rgba[pixelIndex]);
        EXPECT_EQ(255, rgba[pixelIndex + 1]);
        EXPECT_EQ(0, rgba[pixelIndex + 2]);

        pixelIndex = getPixelIndex(100, 100, viewWidthInPixels);
        EXPECT_EQ(0, rgba[pixelIndex]);
        EXPECT_EQ(255, rgba[pixelIndex + 1]);
        EXPECT_EQ(0, rgba[pixelIndex + 2]);

        free(rgba);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(GPUProcess, ValidateWebAudioMediaProcessingAssertion)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"audio-context-playing"];

    // evaluateJavaScript gives us the user gesture we need to reliably start audio playback on all platforms.
    __block bool done = false;
    [webView evaluateJavaScript:@"generateAudioInMediaStreamTrack()" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // A GPUProcess should get launched.
    while (![configuration.get().processPool _gpuProcessIdentifier])
        TestWebKitAPI::Util::runFor(0.1_s);

    // There should be no audible activity.
    EXPECT_FALSE([configuration.get().processPool _hasAudibleMediaActivity]);

    done = false;
    [webView evaluateJavaScript:@"transitionAudioToSpeakers()" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // There should be audible activity.
    int counter = 20;
    while (--counter && ![configuration.get().processPool _hasAudibleMediaActivity])
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_TRUE([configuration.get().processPool _hasAudibleMediaActivity]);
}

TEST(GPUProcess, ReuseBetweenProcessPools)
{
    auto loadBlankViewAndWaitForGPUProcess = []() -> pid_t {
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
        while (![webView.get().configuration.processPool _gpuProcessIdentifier])
            TestWebKitAPI::Util::spinRunLoop();
        return [webView.get().configuration.processPool _gpuProcessIdentifier];
    };

    pid_t firstGPUProcessPID = 0;
    @autoreleasepool {
        firstGPUProcessPID = loadBlankViewAndWaitForGPUProcess();
    }

    TestWebKitAPI::Util::runFor(0.1_s);

    pid_t secondGPUProcessPID = 0;
    @autoreleasepool {
        secondGPUProcessPID = loadBlankViewAndWaitForGPUProcess();
    }

    EXPECT_EQ(firstGPUProcessPID, secondGPUProcessPID);
}

} // namespace TestWebKitAPI

@interface GPUProcessLaunchObserver : NSObject
@property (nonatomic, copy) void (^gpuProcessDidLaunch)(void);
@end

@implementation GPUProcessLaunchObserver
- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (_gpuProcessDidLaunch)
        _gpuProcessDidLaunch();
}
@end

namespace TestWebKitAPI {

TEST(GPUProcess, NotTerminatedWhenSlowToInitialize)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto* processPool = [webView configuration].processPool;
    ASSERT_EQ([processPool _gpuProcessIdentifier], 0);

    // The notification is sent as soon as the GPU process has launched, before it had a chance to
    // handle the initialization message. Stopping the process here makes its initialization slow.
    __block pid_t gpuProcessPID = 0;
    __block bool didStopGPUProcess = false;
    RetainPtr observer = adoptNS([[GPUProcessLaunchObserver alloc] init]);
    [observer setGpuProcessDidLaunch:^{
        if (gpuProcessPID)
            return;
        gpuProcessPID = [webView _gpuProcessIdentifier];
        if (!gpuProcessPID)
            return;
        kill(gpuProcessPID, SIGSTOP);
        didStopGPUProcess = true;
    }];
    [webView addObserver:observer.get() forKeyPath:@"_gpuProcessIdentifier" options:0 context:nullptr];

    [webView loadHTMLString:@"<canvas id='canvas' width='100' height='100'></canvas><script>canvas.getContext('2d').fillRect(0, 0, 50, 50);</script>" baseURL:nil];
    Util::run(&didStopGPUProcess);

    // Longer than the responsiveness timeout.
    Util::runFor(4_s);
    kill(gpuProcessPID, SIGCONT);

    __block bool done = false;
    [webView callAsyncJavaScript:@"return new Promise(resolve => requestAnimationFrame(() => resolve(canvas.getContext('2d').getImageData(10, 10, 1, 1).data[3])))" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ([result intValue], 255);
        done = true;
    }];
    Util::run(&done);

    // A GPU process which is slow to start is not unresponsive and must not have been terminated.
    EXPECT_EQ([processPool _gpuProcessIdentifier], gpuProcessPID);

    [webView removeObserver:observer.get() forKeyPath:@"_gpuProcessIdentifier"];
}

TEST(GPUProcess, TerminatedWhenHungBeforeInitialization)
{
    constexpr Seconds gpuProcessTimeout = 1_s;
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetDoubleValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], gpuProcessTimeout.seconds(), WKStringCreateWithUTF8CString("WebProcessGPUProcessTimeoutForTesting"));
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto* processPool = [webView configuration].processPool;
    ASSERT_EQ([processPool _gpuProcessIdentifier], 0);

    // Stop the GPU process as soon as it has launched and never resume it.
    __block pid_t gpuProcessPID = 0;
    __block bool didStopGPUProcess = false;
    RetainPtr observer = adoptNS([[GPUProcessLaunchObserver alloc] init]);
    [observer setGpuProcessDidLaunch:^{
        if (gpuProcessPID)
            return;
        gpuProcessPID = [webView _gpuProcessIdentifier];
        if (!gpuProcessPID)
            return;
        kill(gpuProcessPID, SIGSTOP);
        didStopGPUProcess = true;
    }];
    [webView addObserver:observer.get() forKeyPath:@"_gpuProcessIdentifier" options:0 context:nullptr];

    [webView loadHTMLString:@"<canvas id='canvas' width='100' height='100'></canvas><script>canvas.getContext('2d').fillRect(0, 0, 50, 50);</script>" baseURL:nil];
    Util::run(&didStopGPUProcess);
    [webView _test_waitForDidFinishNavigation];

    // The responsiveness checks have not begun, but the web process times out waiting for the GPU
    // process and reports it, which terminates the process.
    __block bool done = false;
    [webView callAsyncJavaScript:@"return new Promise(resolve => requestAnimationFrame(() => resolve(canvas.getContext('2d').getImageData(10, 10, 1, 1).data[3])))" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    Util::run(&done);

    auto newGPUProcessPID = [processPool _gpuProcessIdentifier];
    EXPECT_NE(newGPUProcessPID, 0);
    EXPECT_NE(newGPUProcessPID, gpuProcessPID);

    [webView removeObserver:observer.get() forKeyPath:@"_gpuProcessIdentifier"];
}

} // namespace TestWebKitAPI
