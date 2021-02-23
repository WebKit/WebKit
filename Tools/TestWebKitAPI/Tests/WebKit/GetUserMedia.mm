/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "UserMediaCaptureUIDelegate.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKInternalDebugFeature.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/WTFString.h>

static bool done;

@interface GUMMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation GUMMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(@"PASS", [message body]);
    done = true;
}
@end

namespace TestWebKitAPI {

static String wkMediaCaptureStateString(_WKMediaCaptureState flags)
{
    StringBuilder string;
    if (flags & _WKMediaCaptureStateActiveMicrophone)
        string.append("_WKMediaCaptureStateActiveMicrophone + ");
    if (flags & _WKMediaCaptureStateActiveCamera)
        string.append("_WKMediaCaptureStateActiveCamera + ");
    if (flags & _WKMediaCaptureStateMutedMicrophone)
        string.append("_WKMediaCaptureStateMutedMicrophone + ");
    if (flags & _WKMediaCaptureStateMutedCamera)
        string.append("_WKMediaCaptureStateMutedCamera + ");
    if (string.isEmpty())
        string.append("_WKMediaCaptureStateNone");
    else
        string.resize(string.length() - 2);

    return string.toString();
}

bool waitUntilCaptureState(WKWebView *webView, _WKMediaCaptureState expectedState)
{
    NSTimeInterval end = [[NSDate date] timeIntervalSinceReferenceDate] + 10;
    _WKMediaCaptureState state;
    do {
        state = [webView _mediaCaptureState];
        if (state == expectedState)
            return true;

        TestWebKitAPI::Util::spinRunLoop(1);
        
        if ([[NSDate date] timeIntervalSinceReferenceDate] > end)
            break;
    } while (true);

    NSLog(@"Expected state %s, but after 10 seconds state is %s", wkMediaCaptureStateString(expectedState).utf8().data(), wkMediaCaptureStateString(state).utf8().data());
    return false;
}

TEST(WebKit2, CaptureMute)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView _setMediaCaptureReportingDelayForTesting:0];

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera));

    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateMutedCamera));
    [webView _setPageMuted: _WKMediaNoneMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera));

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateNone));

    [webView stringByEvaluatingJavaScript:@"captureAudio()"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveMicrophone));
    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateMutedMicrophone));
    [webView _setPageMuted: _WKMediaNoneMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveMicrophone));

    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateMutedMicrophone));

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateNone));

    [webView stringByEvaluatingJavaScript:@"captureAudioAndVideo()"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera | _WKMediaCaptureStateActiveMicrophone));
    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateMutedCamera | _WKMediaCaptureStateMutedMicrophone));
    [webView _setPageMuted: _WKMediaNoneMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera | _WKMediaCaptureStateActiveMicrophone));

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateNone));
}

TEST(WebKit2, CaptureStop)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera));

    [delegate waitUntilPrompted];

    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateMutedCamera));
    [webView _setPageMuted: _WKMediaNoneMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera));

    [webView stringByEvaluatingJavaScript:@"notifyEndedEvent()"];
    [webView _stopMediaCapture];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateNone));

    [webView stringByEvaluatingJavaScript:@"promptForCapture()"];
    [delegate waitUntilPrompted];
}

TEST(WebKit2, CaptureIndicatorDelay)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    [webView _setMediaCaptureReportingDelayForTesting:2];

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera));

    [delegate waitUntilPrompted];

    [webView stringByEvaluatingJavaScript:@"stop()"];

    // We wait 1 second, we should still see camera be reported.
    sleep(1_s);
    EXPECT_EQ([webView _mediaCaptureState], _WKMediaCaptureStateActiveCamera);

    // One additional second should allow us to go back to no capture being reported.
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateNone));
}

TEST(WebKit2, CaptureIndicatorDelayWhenClosed)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    [webView _setMediaCaptureReportingDelayForTesting:2];

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera));

    [delegate waitUntilPrompted];

    [webView _close];

    EXPECT_EQ([webView _mediaCaptureState], _WKMediaCaptureStateNone);
}

TEST(WebKit2, GetCapabilities)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera));

    [delegate waitUntilPrompted];

    done = false;
    [webView stringByEvaluatingJavaScript:@"checkGetCapabilities()"];

    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, InterruptionBetweenSameProcessPages)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

#if PLATFORM(IOS_FAMILY)
    [configuration setAllowsInlineMediaPlayback:YES];
#endif

    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    done = false;
    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    webView1.get().UIDelegate = delegate.get();
    [webView1 loadTestPageNamed:@"getUserMedia2"];
    TestWebKitAPI::Util::run(&done);

    configuration.get()._relatedWebView = webView1.get();

    done = false;
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    webView2.get().UIDelegate = delegate.get();
    [webView2 loadTestPageNamed:@"getUserMedia2"];
    TestWebKitAPI::Util::run(&done);

    done = false;
#if PLATFORM(IOS)
    [webView1 stringByEvaluatingJavaScript:@"checkIsNotPlaying()"];
#else
    [webView1 stringByEvaluatingJavaScript:@"checkIsPlaying()"];
#endif
    TestWebKitAPI::Util::run(&done);
}

#if WK_HAVE_C_SPI
TEST(WebKit, WebAudioAndGetUserMedia)
{
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    configuration.get().processPool._configuration.shouldCaptureAudioInUIProcess = NO;

    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    auto url = adoptWK(Util::createURLForResource("getUserMedia-webaudio", "html"));
    [webView loadTestPageNamed:@"getUserMedia-webaudio"];

    TestWebKitAPI::Util::run(&done);
    done = false;
}
#endif

#if ENABLE(GPU_PROCESS)
TEST(WebKit2, CrashGPUProcessWhileCapturing)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto preferences = [configuration preferences];

    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"CaptureAudioInGPUProcessEnabled"])
            [preferences _setEnabled:YES forInternalDebugFeature:feature];
        if ([feature.key isEqualToString:@"CaptureAudioInUIProcessEnabled"])
            [preferences _setEnabled:NO forInternalDebugFeature:feature];
        if ([feature.key isEqualToString:@"CaptureVideoInGPUProcessEnabled"])
            [preferences _setEnabled:YES forInternalDebugFeature:feature];
    }

    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    webView.get().UIDelegate = delegate.get();

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera));

    done = false;
    [webView stringByEvaluatingJavaScript:@"captureAudioAndVideo(true)"];
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

    done = false;
    [webView stringByEvaluatingJavaScript:@"createConnection()"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView stringByEvaluatingJavaScript:@"checkVideoStatus()"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView stringByEvaluatingJavaScript:@"checkAudioStatus()"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(gpuProcessPID, [processPool _gpuProcessIdentifier]);
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);
}

TEST(WebKit2, CrashGPUProcessWhileCapturingAndCalling)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto preferences = [configuration preferences];

    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"CaptureAudioInGPUProcessEnabled"])
            [preferences _setEnabled:YES forInternalDebugFeature:feature];
        if ([feature.key isEqualToString:@"CaptureAudioInUIProcessEnabled"])
            [preferences _setEnabled:NO forInternalDebugFeature:feature];
        if ([feature.key isEqualToString:@"CaptureVideoInGPUProcessEnabled"])
            [preferences _setEnabled:YES forInternalDebugFeature:feature];
    }
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
        if ([feature.key isEqualToString:@"WebRTCPlatformCodecsInGPUProcessEnabled"])
            [preferences _setEnabled:YES forFeature:feature];
    }

    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    webView.get().UIDelegate = delegate.get();

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateActiveCamera));

    done = false;
    [webView stringByEvaluatingJavaScript:@"captureAudioAndVideo(true)"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView stringByEvaluatingJavaScript:@"createConnection()"];
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

    done = false;
    [webView stringByEvaluatingJavaScript:@"checkDecodingVideo()"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(gpuProcessPID, [processPool _gpuProcessIdentifier]);
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);
}
#endif // ENABLE(GPU_PROCESS)

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
