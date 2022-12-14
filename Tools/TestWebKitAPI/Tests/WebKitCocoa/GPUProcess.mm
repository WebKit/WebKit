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
#import "UserMediaCaptureUIDelegate.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <notify.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)
typedef NSImage *PlatformImage;
typedef NSWindow *PlatformWindow;

static RetainPtr<CGImageRef> convertToCGImage(NSImage *image)
{
    return [image CGImageForProposedRect:nil context:nil hints:nil];
}

#else
typedef UIImage *PlatformImage;
typedef UIWindow *PlatformWindow;

static RetainPtr<CGImageRef> convertToCGImage(UIImage *image)
{
    return image.CGImage;
}
#endif

static NSInteger getPixelIndex(NSInteger x, NSInteger y, NSInteger width)
{
    return (y * width + x) * 4;
}

TEST(GPUProcess, RelaunchOnCrash)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));

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
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));

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
    [webView _test_waitForWebContentProcessDidTerminate];

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
    [webView evaluateJavaScript:@"startPlaying()" completionHandler:^(id result, NSError *error) {
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

TEST(GPUProcess, OnlyLaunchesGPUProcessWhenNecessary)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("CaptureVideoInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], false, WKStringCreateWithUTF8CString("UseGPUProcessForDOMRenderingEnabled"));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ([configuration.get().processPool _gpuProcessIdentifier], 0);
}

TEST(GPUProcess, OnlyLaunchesGPUProcessWhenNecessarySVG)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("CaptureVideoInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], false, WKStringCreateWithUTF8CString("UseGPUProcessForDOMRenderingEnabled"));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<img src='enormous.svg'></img>"];

    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ([configuration.get().processPool _gpuProcessIdentifier], 0);
}

TEST(GPUProcess, OnlyLaunchesGPUProcessWhenNecessaryMediaFeatureDetection)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("CaptureVideoInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], false, WKStringCreateWithUTF8CString("UseGPUProcessForDOMRenderingEnabled"));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    __block bool done = false;
    [webView evaluateJavaScript:@"!!document.createElement('audio').canPlayType" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"!!document.createElement('video').canPlayType" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    TestWebKitAPI::Util::spinRunLoop(10);

    // This should not have launched a GPUProcess.
    EXPECT_EQ([configuration.get().processPool _gpuProcessIdentifier], 0);
}

TEST(GPUProcess, DoNotLeakConnectionAfterClosingWebPage)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], false, WKStringCreateWithUTF8CString("UseGPUProcessForDOMRenderingEnabled"));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"canvas-image-data"];
    EXPECT_EQ(1U, [webView gpuToWebProcessConnectionCount]);
    [webView _close];

    while ([webView gpuToWebProcessConnectionCount])
        TestWebKitAPI::Util::runFor(0.1_s);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
TEST(GPUProcess, LegacyCDM)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("CaptureVideoInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], false, WKStringCreateWithUTF8CString("UseGPUProcessForDOMRenderingEnabled"));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
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
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
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
            if (fabs(currentTime - initialTime) > 0.01)
                break;
        } while (timeout++ < 100);
        return fabs(currentTime - initialTime) > 0.01;
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
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
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
#if !PLATFORM(IOS)
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
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));

    NSInteger viewWidth = 400;
    NSInteger viewHeight = 400;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, viewWidth, viewHeight) configuration:configuration.get() addToWindow:NO]);

    RetainPtr<PlatformWindow> window;
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

    auto snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, 150, 150)];
    [snapshotConfiguration setSnapshotWidth:@(150)];
    [snapshotConfiguration setAfterScreenUpdates:YES];

    // Make sure a red square is painted.
    done = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(PlatformImage snapshotImage, NSError *error) {
        EXPECT_TRUE(!error);

        RetainPtr<CGImageRef> cgImage = convertToCGImage(snapshotImage);
        RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());

        NSInteger viewWidthInPixels = viewWidth * backingScaleFactor;
        NSInteger viewHeightInPixels = viewHeight * backingScaleFactor;

        uint8_t *rgba = (unsigned char *)calloc(viewWidthInPixels * viewHeightInPixels * 4, sizeof(unsigned char));
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
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
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(PlatformImage snapshotImage, NSError *error) {
        EXPECT_TRUE(!error);

        RetainPtr<CGImageRef> cgImage = convertToCGImage(snapshotImage);
        RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());

        NSInteger viewWidthInPixels = viewWidth * backingScaleFactor;
        NSInteger viewHeightInPixels = viewHeight * backingScaleFactor;

        uint8_t *rgba = (unsigned char *)calloc(viewWidthInPixels * viewHeightInPixels * 4, sizeof(unsigned char));
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
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

static void runMemoryPressureExitTest(Function<void(WKWebView *)>&& loadTestPageSynchronously, Function<void(WKWebViewConfiguration *)>&& updateConfiguration = [](WKWebViewConfiguration *) { })
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("CaptureVideoInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("CaptureAudioInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("WebRTCPlatformCodecsInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], false, WKStringCreateWithUTF8CString("CaptureAudioInUIProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], false, WKStringCreateWithUTF8CString("UseGPUProcessForDOMRenderingEnabled"));

    updateConfiguration(configuration.get());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    loadTestPageSynchronously(webView.get());

    // A GPUProcess should get launched.
    while (![configuration.get().processPool _gpuProcessIdentifier])
        TestWebKitAPI::Util::runFor(0.1_s);
    auto gpuProcessPID = [configuration.get().processPool _gpuProcessIdentifier];

    // Simulate memory pressure (notifyutil -p org.WebKit.lowMemory).
    notify_post("org.WebKit.lowMemory");

    // Make sure the GPUProcess does not exit since it is still needed.
    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_EQ(gpuProcessPID, [configuration.get().processPool _gpuProcessIdentifier]);

    // Navigate to another page that no longer requires the GPUProcess.
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    TestWebKitAPI::Util::runFor(0.5_s);

    // The GPUProcess should exit on memory pressure.
    do {
        // Simulate memory pressure (notifyutil -p org.WebKit.lowMemory).
        notify_post("org.WebKit.lowMemory");
        TestWebKitAPI::Util::runFor(0.1_s);
    } while ([configuration.get().processPool _gpuProcessIdentifier]);

    // The GPUProcess should not relaunch.
    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_EQ(0, [configuration.get().processPool _gpuProcessIdentifier]);
}

TEST(GPUProcess, ExitsUnderMemoryPressureCanvasCase)
{
    runMemoryPressureExitTest([](WKWebView *webView) {
        [webView synchronouslyLoadHTMLString:testCanvasPage];

        __block bool done = false;
        [webView evaluateJavaScript:@"context.fillStyle = '#00FF00'; context.fillRect(0, 0, 400, 400);" completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE(!error);
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        done = false;
        [webView evaluateJavaScript:@"context.getImageData(0, 0, 400, 400).width" completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE(!error);
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    });
}

TEST(GPUProcess, ExitsUnderMemoryPressureVideoCase)
{
    runMemoryPressureExitTest([](WKWebView *webView) {
        [webView synchronouslyLoadTestPageNamed:@"large-videos-with-audio"];

        __block bool done = false;
        [webView evaluateJavaScript:@"document.getElementsByTagName('video')[0].play() && true" completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE(!error);
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    });
}

#if ENABLE(MEDIA_STREAM)
static bool waitUntilCaptureState(WKWebView *webView, _WKMediaCaptureStateDeprecated expectedState)
{
    NSTimeInterval end = [[NSDate date] timeIntervalSinceReferenceDate] + 10;
    do {
        if ([webView _mediaCaptureState] == expectedState)
            return true;

        TestWebKitAPI::Util::spinRunLoop(1);

        if ([[NSDate date] timeIntervalSinceReferenceDate] > end)
            break;
    } while (true);

    return false;
}

#if PLATFORM(MAC)
// FIXME: https://bugs.webkit.org/show_bug.cgi?id=237854 is disabled for IOS
TEST(GPUProcess, ExitsUnderMemoryPressureGetUserMediaAudioCase)
{
    runMemoryPressureExitTest([](WKWebView *webView) {
        auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
        webView.UIDelegate = delegate.get();

        [webView loadTestPageNamed:@"getUserMedia"];
        EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateDeprecatedActiveCamera));
        [webView stringByEvaluatingJavaScript:@"captureAudio(true)"];
    }, [](WKWebViewConfiguration* configuration) {
        auto preferences = configuration.preferences;
        preferences._mediaCaptureRequiresSecureConnection = NO;
        configuration._mediaCaptureEnabled = YES;
        preferences._mockCaptureDevicesEnabled = YES;
        preferences._getUserMediaRequiresFocus = NO;
    });
}
#endif

TEST(GPUProcess, ExitsUnderMemoryPressureGetUserMediaVideoCase)
{
    runMemoryPressureExitTest([](WKWebView *webView) {
        auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
        webView.UIDelegate = delegate.get();

        [webView loadTestPageNamed:@"getUserMedia"];
        EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateDeprecatedActiveCamera));
        [webView stringByEvaluatingJavaScript:@"captureVideo(true)"];
    }, [](WKWebViewConfiguration* configuration) {
        auto preferences = configuration.preferences;
        preferences._mediaCaptureRequiresSecureConnection = NO;
        configuration._mediaCaptureEnabled = YES;
        preferences._mockCaptureDevicesEnabled = YES;
        preferences._getUserMediaRequiresFocus = NO;
    });
}

TEST(GPUProcess, ExitsUnderMemoryPressureWebRTCCase)
{
    runMemoryPressureExitTest([](WKWebView *webView) {
        auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
        webView.UIDelegate = delegate.get();

        [webView loadTestPageNamed:@"getUserMedia"];
        EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateDeprecatedActiveCamera));
        [webView stringByEvaluatingJavaScript:@"captureVideo(true)"];
        [webView stringByEvaluatingJavaScript:@"createConnection()"];
    }, [](WKWebViewConfiguration* configuration) {
        auto preferences = configuration.preferences;
        preferences._mediaCaptureRequiresSecureConnection = NO;
        configuration._mediaCaptureEnabled = YES;
        preferences._mockCaptureDevicesEnabled = YES;
        preferences._getUserMediaRequiresFocus = NO;
    });
}
#endif // ENABLE(MEDIA_STREAM)

TEST(GPUProcess, ExitsUnderMemoryPressureWebAudioCase)
{
    runMemoryPressureExitTest([](WKWebView *webView) {
        [webView synchronouslyLoadTestPageNamed:@"audio-context-playing"];

        // evaluateJavaScript gives us the user gesture we need to reliably start audio playback on all platforms.
        __block bool done = false;
        [webView evaluateJavaScript:@"startPlaying()" completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE(!error);
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    });
}

TEST(GPUProcess, ExitsUnderMemoryPressureWebAudioNonRenderingAudioContext)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("CaptureVideoInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], false, WKStringCreateWithUTF8CString("UseGPUProcessForDOMRenderingEnabled"));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"audio-context-playing"];

    // evaluateJavaScript gives us the user gesture we need to reliably start audio playback on all platforms.
    __block bool done = false;
    [webView evaluateJavaScript:@"startPlaying()" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // A GPUProcess should get launched.
    while (![configuration.get().processPool _gpuProcessIdentifier])
        TestWebKitAPI::Util::runFor(0.1_s);
    auto gpuProcessPID = [configuration.get().processPool _gpuProcessIdentifier];

    // Simulate memory pressure (notifyutil -p org.WebKit.lowMemory).
    notify_post("org.WebKit.lowMemory");

    // Make sure the GPUProcess does not exit since it is still needed.
    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_EQ(gpuProcessPID, [configuration.get().processPool _gpuProcessIdentifier]);

    // Suspend audio rendering.
    [webView evaluateJavaScript:@"context.suspend() && true" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];

    // The GPUProcess should exit on memory pressure.
    do {
        // Simulate memory pressure (notifyutil -p org.WebKit.lowMemory).
        notify_post("org.WebKit.lowMemory");
        TestWebKitAPI::Util::runFor(0.1_s);
    } while ([configuration.get().processPool _gpuProcessIdentifier]);

    // The GPUProcess should not relaunch.
    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_EQ(0, [configuration.get().processPool _gpuProcessIdentifier]);
}

TEST(GPUProcess, ValidateWebAudioMediaProcessingAssertion)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
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
