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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "UserMediaCaptureUIDelegate.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPagePrivate.h>
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

@interface UserMediaCaptureUIDelegateForParameters : NSObject<WKUIDelegate>
// WKUIDelegate
- (void)webView:(WKWebView *)webView requestMediaCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame type:(WKMediaCaptureType)type decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler;
@end

@implementation UserMediaCaptureUIDelegateForParameters
- (void)webView:(WKWebView *)webView requestMediaCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame type:(WKMediaCaptureType)type decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler {
    EXPECT_WK_STREQ(origin.protocol, @"http");
    EXPECT_WK_STREQ(origin.host, @"127.0.0.1");
    EXPECT_EQ(origin.port, 9090U);

    EXPECT_WK_STREQ(frame.securityOrigin.protocol, @"http");
    EXPECT_WK_STREQ(frame.securityOrigin.host, @"127.0.0.1");
    EXPECT_EQ(frame.securityOrigin.port, 9091U);
    EXPECT_FALSE(frame.isMainFrame);
    EXPECT_TRUE(frame.webView == webView);

    EXPECT_EQ(type, WKMediaCaptureTypeCamera);

    done = true;
    decisionHandler(WKPermissionDecisionGrant);
}
@end

@interface GUMMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation GUMMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(@"PASS", [message body]);
    done = true;
}
@end

@interface WKWebView ()
- (WKPageRef)_pageForTesting;
@end

static bool cameraCaptureStateChange;
static bool microphoneCaptureStateChange;
static WKMediaCaptureState cameraCaptureState;
static WKMediaCaptureState microphoneCaptureState;

@interface MediaCaptureObserver : NSObject
@end

@implementation MediaCaptureObserver
- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void *)context
{
    EXPECT_TRUE([keyPath isEqualToString:NSStringFromSelector(@selector(cameraCaptureState))] || [keyPath isEqualToString:NSStringFromSelector(@selector(microphoneCaptureState))]);
    EXPECT_TRUE([[object class] isEqual:[TestWKWebView class]]);
    if ([keyPath isEqualToString:NSStringFromSelector(@selector(cameraCaptureState))]) {
        cameraCaptureState = (WKMediaCaptureState)[[change objectForKey:NSKeyValueChangeNewKey] unsignedIntegerValue];
        cameraCaptureStateChange = true;
        return;
    }
    microphoneCaptureState = (WKMediaCaptureState)[[change objectForKey:NSKeyValueChangeNewKey] unsignedIntegerValue];
    microphoneCaptureStateChange = true;
}
@end

namespace TestWebKitAPI {

static String wkMediaCaptureStateString(_WKMediaCaptureStateDeprecated flags)
{
    StringBuilder string;
    if (flags & _WKMediaCaptureStateDeprecatedActiveMicrophone)
        string.append("_WKMediaCaptureStateDeprecatedActiveMicrophone + ");
    if (flags & _WKMediaCaptureStateDeprecatedActiveCamera)
        string.append("_WKMediaCaptureStateDeprecatedActiveCamera + ");
    if (flags & _WKMediaCaptureStateDeprecatedMutedMicrophone)
        string.append("_WKMediaCaptureStateDeprecatedMutedMicrophone + ");
    if (flags & _WKMediaCaptureStateDeprecatedMutedCamera)
        string.append("_WKMediaCaptureStateDeprecatedMutedCamera + ");
    if (string.isEmpty())
        string.append("_WKMediaCaptureStateDeprecatedNone");
    else
        string.shrink(string.length() - 2);
    return string.toString();
}

bool waitUntilCaptureState(WKWebView *webView, _WKMediaCaptureStateDeprecated expectedState)
{
    NSTimeInterval end = [[NSDate date] timeIntervalSinceReferenceDate] + 10;
    _WKMediaCaptureStateDeprecated state;
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

void doCaptureMuteTest(const Function<void(TestWKWebView*, _WKMediaMutedState)>& setPageMuted)
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
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

    setPageMuted(webView.get(), _WKMediaCaptureDevicesMuted);
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedMutedCamera));
    setPageMuted(webView.get(), _WKMediaNoneMuted);
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedNone));

    [webView stringByEvaluatingJavaScript:@"captureAudio()"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveMicrophone));
    setPageMuted(webView.get(), _WKMediaCaptureDevicesMuted);
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedMutedMicrophone));
    setPageMuted(webView.get(), _WKMediaNoneMuted);
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveMicrophone));

    setPageMuted(webView.get(), _WKMediaCaptureDevicesMuted);
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedMutedMicrophone));

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedNone));

    [webView stringByEvaluatingJavaScript:@"captureAudioAndVideo()"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera | _WKMediaCaptureStateDeprecatedActiveMicrophone));
    setPageMuted(webView.get(), _WKMediaCaptureDevicesMuted);
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedMutedCamera | _WKMediaCaptureStateDeprecatedMutedMicrophone));
    setPageMuted(webView.get(), _WKMediaNoneMuted);
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera | _WKMediaCaptureStateDeprecatedActiveMicrophone));

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedNone));
}

TEST(WebKit2, CaptureMute)
{
    doCaptureMuteTest([](auto* webView, auto state) {
        [webView _setPageMuted: state];
    });
}

TEST(WebKit2, CaptureMute2)
{
    doCaptureMuteTest([](auto* webView, auto state) {
        WKPageSetMuted([webView _pageForTesting], state);
    });
}

bool waitUntilCameraState(WKWebView *webView, WKMediaCaptureState expectedState)
{
    if (expectedState == cameraCaptureState)
        return true;
    TestWebKitAPI::Util::run(&cameraCaptureStateChange);
    return expectedState == cameraCaptureState;
}

bool waitUntilMicrophoneState(WKWebView *webView, WKMediaCaptureState expectedState)
{
    if (expectedState == microphoneCaptureState)
        return true;
    TestWebKitAPI::Util::run(&microphoneCaptureStateChange);
    return expectedState == microphoneCaptureState;
}

TEST(WebKit2, CaptureMuteAPI)
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

    auto observer = adoptNS([[MediaCaptureObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:@"microphoneCaptureState" options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:observer.get() forKeyPath:@"cameraCaptureState" options:NSKeyValueObservingOptionNew context:nil];

    cameraCaptureStateChange = false;
    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));

    cameraCaptureStateChange = false;
    [webView setCameraCaptureState:WKMediaCaptureStateMuted completionHandler:nil];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));

    cameraCaptureStateChange = false;
    [webView setCameraCaptureState:WKMediaCaptureStateActive completionHandler:nil];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));

    cameraCaptureStateChange = false;
    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateNone));

    microphoneCaptureStateChange = false;
    [webView stringByEvaluatingJavaScript:@"captureAudio()"];
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));

    microphoneCaptureStateChange = false;
    [webView setMicrophoneCaptureState:WKMediaCaptureStateMuted completionHandler:nil];
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateMuted));

    microphoneCaptureStateChange = false;
    [webView setMicrophoneCaptureState:WKMediaCaptureStateActive completionHandler:nil];
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));

    microphoneCaptureStateChange = false;
    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateNone));

    // Mute camera, then microphone
    microphoneCaptureStateChange = false;
    cameraCaptureStateChange = false;
    [webView stringByEvaluatingJavaScript:@"captureAudioAndVideo()"];
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));

    cameraCaptureStateChange = false;
    [webView setCameraCaptureState:WKMediaCaptureStateMuted completionHandler:nil];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));

    microphoneCaptureStateChange = false;
    [webView setMicrophoneCaptureState:WKMediaCaptureStateMuted completionHandler:nil];
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateMuted));
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));

    microphoneCaptureStateChange = false;
    cameraCaptureStateChange = false;
    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateNone));
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateNone));

    // Mute microphone, then camera
    microphoneCaptureStateChange = false;
    cameraCaptureStateChange = false;
    [webView stringByEvaluatingJavaScript:@"captureAudioAndVideo()"];
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));

    microphoneCaptureStateChange = false;
    [webView setMicrophoneCaptureState:WKMediaCaptureStateMuted completionHandler:nil];
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateMuted));
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));

    cameraCaptureStateChange = false;
    [webView setCameraCaptureState:WKMediaCaptureStateMuted completionHandler:nil];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateMuted));

    // Stop microphone, then stop camera
    microphoneCaptureStateChange = false;
    [webView setMicrophoneCaptureState:WKMediaCaptureStateNone completionHandler:nil];
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateNone));
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));

    cameraCaptureStateChange = false;
    [webView setCameraCaptureState:WKMediaCaptureStateNone completionHandler:nil];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateNone));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateNone));

    [webView removeObserver:observer.get() forKeyPath:@"microphoneCaptureState"];
    [webView removeObserver:observer.get() forKeyPath:@"cameraCaptureState"];
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
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

    [delegate waitUntilPrompted];

    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedMutedCamera));
    [webView _setPageMuted: _WKMediaNoneMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

    [webView stringByEvaluatingJavaScript:@"notifyEndedEvent()"];
    [webView _stopMediaCapture];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedNone));

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
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

    [delegate waitUntilPrompted];

    [webView stringByEvaluatingJavaScript:@"stop()"];

    // We wait 1 second, we should still see camera be reported.
    sleep(1_s);
    EXPECT_EQ([webView _mediaCaptureState], _WKMediaCaptureStateDeprecatedActiveCamera);

    // One additional second should allow us to go back to no capture being reported.
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedNone));
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
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

    [delegate waitUntilPrompted];

    [webView _close];

    EXPECT_EQ([webView _mediaCaptureState], _WKMediaCaptureStateDeprecatedNone);
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
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

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

static const char* mainFrameText = R"DOCDOCDOC(
<html><body>
<iframe src='http://127.0.0.1:9091/frame' allow='camera:http://127.0.0.1:9091'></iframe>
</body></html>
)DOCDOCDOC";
static const char* frameText = R"DOCDOCDOC(
<html><body><script>
navigator.mediaDevices.getUserMedia({video:true});
</script></body></html>
)DOCDOCDOC";

TEST(WebKit, PermissionDelegateParameters)
{
    TestWebKitAPI::HTTPServer server1({
        { "/", { mainFrameText } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 9090);

    TestWebKitAPI::HTTPServer server2({
        { "/frame", { frameText } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 9091);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

#if PLATFORM(IOS_FAMILY)
    [configuration setAllowsInlineMediaPlayback:YES];
#endif

    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    done = false;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegateForParameters alloc] init]);
    webView.get().UIDelegate = delegate.get();
    [webView loadRequest:server1.request()];
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
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

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

TEST(WebKit2, CrashGPUProcessAfterApplyingConstraints)
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

#if PLATFORM(IOS_FAMILY)
    [configuration setAllowsInlineMediaPlayback:YES];
#endif

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    webView.get().UIDelegate = delegate.get();

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

    done = false;
    [webView stringByEvaluatingJavaScript:@"captureAudioAndVideo(true)"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView stringByEvaluatingJavaScript:@"changeConstraints()"];
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
    [webView stringByEvaluatingJavaScript:@"checkConstraintsStatus()"];
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
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

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

#if PLATFORM(MAC)
static const char* visibilityTestText = R"DOCDOCDOC(
<html><body>
<div id='log'></div>
<video id='localVideo' autoplay muted playsInline width=160></video>
<script>
let string = '';
onload = () => {
    for (let i = 0; i < 1000; ++i)
        string += '<br>test';
    if (window.internals)
        internals.setMediaElementRestrictions(localVideo, "invisibleautoplaynotpermitted");
    document.onvisibilitychange = () => window.webkit.messageHandlers.gum.postMessage("PASS");
    window.webkit.messageHandlers.gum.postMessage("PASS");
}

function capture()
{
    navigator.mediaDevices.getUserMedia({video : true}).then(stream => {
        log.innerHTML = string;
        localVideo.srcObject = stream;
        window.webkit.messageHandlers.gum.postMessage("PASS");
    });
}

async function checkLocalVideoPlaying()
{
    let counter = 0;
    while (++counter < 200) {
        if (!localVideo.paused)
            return "PASS";
        await new Promise(resolve => setTimeout(resolve, 50));
    }
    return "FAIL";
}

function doTest()
{
    log.innerHTML = '';
    checkLocalVideoPlaying().then(result => {
        window.webkit.messageHandlers.gum.postMessage(result);
    });
}

</script>
</body></html>
)DOCDOCDOC";

TEST(WebKit, AutoplayOnVisibilityChange)
{
    TestWebKitAPI::HTTPServer server({
        { "/", { visibilityTestText } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 9090);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    done = false;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    webView.get().UIDelegate = delegate.get();

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);

    done = false;
    auto *hostWindow = [webView hostWindow];
    [hostWindow miniaturize:hostWindow];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView stringByEvaluatingJavaScript:@"capture()"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [hostWindow deminiaturize:hostWindow];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView stringByEvaluatingJavaScript:@"doTest()"];
    TestWebKitAPI::Util::run(&done);
}
#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
