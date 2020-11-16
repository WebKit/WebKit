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

TEST(GPUProcess, CrashWhilePlayingVideo)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"UseGPUProcessForMediaEnabled"]) {
            [[configuration preferences] _setEnabled:YES forInternalDebugFeature:feature];
            break;
        }
    }

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"large-videos-with-audio"];

    __block bool done = false;
    [webView evaluateJavaScript:@"document.getElementsByTagName('video')[0].play() && true" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto webViewPID = [webView _webProcessIdentifier];

    // The GPU process should get launched.
    auto* processPool = configuration.get().processPool;
    unsigned timeout = 0;
    while (![processPool _gpuProcessIdentifier] && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);

    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    if (![processPool _gpuProcessIdentifier])
        return;
    auto gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Audio should be playing.
    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_TRUE([webView _isPlayingAudio]);

    // Kill the GPU Process.
    kill(gpuProcessPID, 9);

    // GPU Process should get relaunched.
    timeout = 0;
    while ((![processPool _gpuProcessIdentifier] || [processPool _gpuProcessIdentifier] == gpuProcessPID) && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    EXPECT_NE([processPool _gpuProcessIdentifier], gpuProcessPID);
    gpuProcessPID = [processPool _gpuProcessIdentifier];

    // Make sure the WebProcess did not crash.
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);

    // Audio should resume playing.
    timeout = 0;
    while (![webView _isPlayingAudio] && timeout++ < 100)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_TRUE([webView _isPlayingAudio]);

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
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"UseGPUProcessForCanvasRenderingEnabled"]) {
            [[configuration preferences] _setEnabled:YES forInternalDebugFeature:feature];
            break;
        }
    }

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
        TestWebKitAPI::Util::sleep(0.1);
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
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big));
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
        TestWebKitAPI::Util::sleep(0.1);
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
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big));
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
