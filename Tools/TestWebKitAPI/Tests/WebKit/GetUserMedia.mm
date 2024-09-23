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
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestWKWebView.h"
#import "UserMediaCaptureUIDelegate.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKSecurityOriginRef.h>
#import <WebKit/WKString.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <pal/spi/mac/MediaRemoteSPI.h>
#import <notify.h>
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
        string.append("_WKMediaCaptureStateDeprecatedActiveMicrophone + "_s);
    if (flags & _WKMediaCaptureStateDeprecatedActiveCamera)
        string.append("_WKMediaCaptureStateDeprecatedActiveCamera + "_s);
    if (flags & _WKMediaCaptureStateDeprecatedMutedMicrophone)
        string.append("_WKMediaCaptureStateDeprecatedMutedMicrophone + "_s);
    if (flags & _WKMediaCaptureStateDeprecatedMutedCamera)
        string.append("_WKMediaCaptureStateDeprecatedMutedCamera + "_s);
    if (string.isEmpty())
        string.append("_WKMediaCaptureStateDeprecatedNone"_s);
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

static void initializeMediaCaptureConfiguration(WKWebViewConfiguration* configuration)
{
    auto preferences = [configuration preferences];

    configuration._mediaCaptureEnabled = YES;
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mockCaptureDevicesEnabled = YES;
    preferences._getUserMediaRequiresFocus = NO;
}

void doCaptureMuteTest(const Function<void(TestWKWebView*, _WKMediaMutedState)>& setPageMuted)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());
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
    initializeMediaCaptureConfiguration(configuration.get());
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
    initializeMediaCaptureConfiguration(configuration.get());

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

TEST(WebKit2, CloseWKWebViewShortlyAfterStartingToCapture)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMedia"];
    [delegate waitUntilPrompted];

    // Wait a bit before closing so that WebPage is instructed to start capturing.
    TestWebKitAPI::Util::spinRunLoop(5);

    [webView _close];

    // Wait a little bit to verify everything is fine.
    TestWebKitAPI::Util::spinRunLoop(100);
}

TEST(WebKit2, CaptureIndicatorDelay)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    [webView _setMediaCaptureReportingDelayForTesting:2];

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

    int retryCount = 1000;
    while (--retryCount && ![webView stringByEvaluatingJavaScript:@"haveStream()"].boolValue)
        TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_TRUE(!!retryCount);

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
    initializeMediaCaptureConfiguration(configuration.get());

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
    initializeMediaCaptureConfiguration(configuration.get());

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

    initializeMediaCaptureConfiguration(configuration.get());

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    done = false;
    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    webView1.get().UIDelegate = delegate.get();
    [webView1 loadTestPageNamed:@"getUserMedia2"];
    TestWebKitAPI::Util::run(&done);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    configuration.get()._relatedWebView = webView1.get();
    ALLOW_DEPRECATED_DECLARATIONS_END

    done = false;
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    webView2.get().UIDelegate = delegate.get();
    [webView2 loadTestPageNamed:@"getUserMedia2"];
    TestWebKitAPI::Util::run(&done);

    done = false;
#if PLATFORM(IOS) || PLATFORM(VISION)
    [webView1 stringByEvaluatingJavaScript:@"checkIsNotPlaying()"];
#else
    [webView1 stringByEvaluatingJavaScript:@"checkIsPlaying()"];
#endif
    TestWebKitAPI::Util::run(&done);
}

static constexpr auto transferTrackBetweenSameProcessPagesText = R"DOCDOCDOC(
<html><body onload='start()'><script>
function captureAndTransferAudioTrack()
{
    navigator.mediaDevices.getUserMedia({audio:true}).then(stream => {
        const track = stream.getAudioTracks()[0];
        self.opener.postMessage({ state : "PASS", track }, [track]);
    });
}

function captureAndTransferVideoTrack()
{
    navigator.mediaDevices.getUserMedia({video:true}).then(stream => {
        const track = stream.getVideoTracks()[0];
        self.opener.postMessage({ state : "PASS", track }, [track]);
    });
}

function start()
{
    if (self.location.search == "") {
        const popupWindow = window.open("http://127.0.0.1:9090?no-open", "newWindow");
        if (!popupWindow)
            window.webkit.messageHandlers.gum.postMessage("FAIL, no popup window");
        window.onmessage = (event) => {
            const eventData = event.data;
            if (eventData.track) {
                window.webkit.messageHandlers.gum.postMessage(eventData.track instanceof MediaStreamTrack ? "PASS" : "FAIL");
                return;
            }
            window.webkit.messageHandlers.gum.postMessage(eventData.state);
        };
        return;
    }
    self.opener.postMessage({ state : "PASS" });
}
</script></body></html>
)DOCDOCDOC"_s;

TEST(WebKit, TransferTrackBetweenSameProcessPages)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { transferTrackBetweenSameProcessPagesText } },
        { "/?no-open"_s, { transferTrackBetweenSameProcessPagesText } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 9090);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    __block RetainPtr<TestWKWebView> webView2;
    auto observer = adoptNS([[MediaCaptureObserver alloc] init]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    delegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration addToWindow:YES]);
        [webView2 addObserver:observer.get() forKeyPath:@"microphoneCaptureState" options:NSKeyValueObservingOptionNew context:nil];
        [webView2 addObserver:observer.get() forKeyPath:@"cameraCaptureState" options:NSKeyValueObservingOptionNew context:nil];
        [webView2 _setMediaCaptureReportingDelayForTesting:0];
        webView2.get().UIDelegate = delegate.get();
        return webView2.get();
    };
    webView1.get().UIDelegate = delegate.get();
#if PLATFORM(IOS_FAMILY)
    webView1.get().navigationDelegate = delegate.get();
#endif

    [webView1 _setMediaCaptureReportingDelayForTesting:0];

    done = false;
    [webView1 loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);

    // webView2 captures audio and transfers track to webView1.
    done = false;
    microphoneCaptureStateChange = false;
    [webView2 stringByEvaluatingJavaScript:@"captureAndTransferAudioTrack()"];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(waitUntilMicrophoneState(webView2.get(), WKMediaCaptureStateActive));

    // webView2 captures video and transfers track to webView1.
    done = false;
    cameraCaptureStateChange = false;
    [webView2 stringByEvaluatingJavaScript:@"captureAndTransferVideoTrack()"];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(waitUntilCameraState(webView2.get(), WKMediaCaptureStateActive));

    EXPECT_EQ(webView1.get().microphoneCaptureState, WKMediaCaptureStateNone);
    EXPECT_EQ(webView1.get().cameraCaptureState, WKMediaCaptureStateNone);

    EXPECT_EQ(webView2.get().microphoneCaptureState, WKMediaCaptureStateActive);
    EXPECT_EQ(webView2.get().cameraCaptureState, WKMediaCaptureStateActive);
}

#if PLATFORM(MAC)
TEST(WebKit, InterruptionBetweenGetDisplayMediaAndGetUserMedia)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));

    configuration.get().processPool = (WKProcessPool *)context.get();

    initializeMediaCaptureConfiguration(configuration.get());

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    webView1.get().UIDelegate = delegate.get();

    auto observer = adoptNS([[MediaCaptureObserver alloc] init]);
    [webView1 addObserver:observer.get() forKeyPath:@"microphoneCaptureState" options:NSKeyValueObservingOptionNew context:nil];
    [webView1 addObserver:observer.get() forKeyPath:@"cameraCaptureState" options:NSKeyValueObservingOptionNew context:nil];

    cameraCaptureStateChange = false;
    [webView1 loadTestPageNamed:@"getUserMedia"];

    EXPECT_TRUE(waitUntilCameraState(webView1.get(), WKMediaCaptureStateActive));

    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    webView2.get().UIDelegate = delegate.get();

    [delegate setGetDisplayMediaDecision:WKDisplayCapturePermissionDecisionScreenPrompt];
    [webView2 synchronouslyLoadTestPageNamed:@"getDisplayMedia"];
    [webView2 stringByEvaluatingJavaScript:@"promptForCapture({ video : true })"];

    cameraCaptureStateChange = false;
    EXPECT_TRUE(waitUntilCameraState(webView1.get(), WKMediaCaptureStateMuted));
}
#endif // PLATFORM(MAC)

static constexpr auto mainFrameText = R"DOCDOCDOC(
<html><body>
<iframe src='http://127.0.0.1:9091/frame' allow='camera:http://127.0.0.1:9091'></iframe>
</body></html>
)DOCDOCDOC"_s;
static constexpr auto frameText = R"DOCDOCDOC(
<html><body><script>
navigator.mediaDevices.getUserMedia({video:true});
</script></body></html>
)DOCDOCDOC"_s;

TEST(WebKit, PermissionDelegateParameters)
{
    TestWebKitAPI::HTTPServer server1({
        { "/"_s, { mainFrameText } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 9090);

    TestWebKitAPI::HTTPServer server2({
        { "/frame"_s, { frameText } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 9091);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

#if PLATFORM(IOS_FAMILY)
    [configuration setAllowsInlineMediaPlayback:YES];
#endif

    initializeMediaCaptureConfiguration(configuration.get());

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

    initializeMediaCaptureConfiguration(configuration.get());

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

    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"CaptureAudioInGPUProcessEnabled"])
            [preferences _setEnabled:YES forFeature:feature];
        if ([feature.key isEqualToString:@"CaptureAudioInUIProcessEnabled"])
            [preferences _setEnabled:NO forFeature:feature];
        if ([feature.key isEqualToString:@"CaptureVideoInGPUProcessEnabled"])
            [preferences _setEnabled:YES forFeature:feature];
    }

    initializeMediaCaptureConfiguration(configuration.get());

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
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_NE([processPool _gpuProcessIdentifier], 0);
    if (![processPool _gpuProcessIdentifier])
        return;
    auto gpuProcessPID = [processPool _gpuProcessIdentifier];

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

    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"CaptureAudioInGPUProcessEnabled"])
            [preferences _setEnabled:YES forFeature:feature];
        if ([feature.key isEqualToString:@"CaptureAudioInUIProcessEnabled"])
            [preferences _setEnabled:NO forFeature:feature];
        if ([feature.key isEqualToString:@"CaptureVideoInGPUProcessEnabled"])
            [preferences _setEnabled:YES forFeature:feature];
    }
    initializeMediaCaptureConfiguration(configuration.get());

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

    done = false;
    [webView stringByEvaluatingJavaScript:@"applyBadConstraintsToAudio()"];
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

    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"CaptureAudioInGPUProcessEnabled"])
            [preferences _setEnabled:YES forFeature:feature];
        if ([feature.key isEqualToString:@"CaptureAudioInUIProcessEnabled"])
            [preferences _setEnabled:NO forFeature:feature];
        if ([feature.key isEqualToString:@"CaptureVideoInGPUProcessEnabled"])
            [preferences _setEnabled:YES forFeature:feature];
        if ([feature.key isEqualToString:@"WebRTCPlatformCodecsInGPUProcessEnabled"])
            [preferences _setEnabled:YES forFeature:feature];
    }

    initializeMediaCaptureConfiguration(configuration.get());

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

    done = false;
    [webView stringByEvaluatingJavaScript:@"checkDecodingVideo('first')"];
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

    done = false;
    [webView stringByEvaluatingJavaScript:@"checkVideoStatus()"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView stringByEvaluatingJavaScript:@"checkAudioStatus()"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView stringByEvaluatingJavaScript:@"checkDecodingVideo('second')"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(gpuProcessPID, [processPool _gpuProcessIdentifier]);
    EXPECT_EQ(webViewPID, [webView _webProcessIdentifier]);
}
#endif // ENABLE(GPU_PROCESS)

#if PLATFORM(MAC)
static constexpr auto visibilityTestText = R"DOCDOCDOC(
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

function hasSleepDisabler()
{
    return window.internals ? internals.hasSleepDisabler() : false;
}

function stop()
{
    if (localVideo.srcObject)
        localVideo.srcObject.getVideoTracks()[0].stop();
    window.webkit.messageHandlers.gum.postMessage("PASS");
}
</script>
</body></html>
)DOCDOCDOC"_s;

TEST(WebKit, AutoplayOnVisibilityChange)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { visibilityTestText } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 9090);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    initializeMediaCaptureConfiguration(configuration.get());

    done = false;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    webView.get().UIDelegate = delegate.get();

    NSLog(@"AutoplayOnVisibilityChange 1\n");

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);

    NSLog(@"AutoplayOnVisibilityChange 2\n");
    done = false;
    auto *hostWindow = [webView hostWindow];
    [hostWindow miniaturize:hostWindow];
    TestWebKitAPI::Util::run(&done);

    NSLog(@"AutoplayOnVisibilityChange 3\n");
    done = false;
    [webView stringByEvaluatingJavaScript:@"capture()"];
    TestWebKitAPI::Util::run(&done);

    bool hasSleepDisabler = [webView stringByEvaluatingJavaScript:@"hasSleepDisabler()"].boolValue;
    EXPECT_TRUE(hasSleepDisabler);

    NSLog(@"AutoplayOnVisibilityChange 4\n");
    done = false;
    [hostWindow deminiaturize:hostWindow];
    TestWebKitAPI::Util::run(&done);

    NSLog(@"AutoplayOnVisibilityChange 5\n");
    done = false;
    [webView stringByEvaluatingJavaScript:@"doTest()"];
    TestWebKitAPI::Util::run(&done);

    NSLog(@"AutoplayOnVisibilityChange 6\n");
    done = false;
    [webView stringByEvaluatingJavaScript:@"stop()"];
    TestWebKitAPI::Util::run(&done);

    NSLog(@"AutoplayOnVisibilityChange 7\n");
    hasSleepDisabler = [webView stringByEvaluatingJavaScript:@"hasSleepDisabler()"].boolValue;
    EXPECT_FALSE(hasSleepDisabler);
}

static constexpr auto getUserMediaFocusText = R"DOCDOCDOC(
<html><body>
<script>
onload = () => {
    document.onvisibilitychange = () => window.webkit.messageHandlers.gum.postMessage("PASS");
    window.webkit.messageHandlers.gum.postMessage("PASS");
}

function capture()
{
    navigator.mediaDevices.getUserMedia({video : true}).then(stream => {
        window.webkit.messageHandlers.gum.postMessage(document.hasFocus() ? "PASS" : "FAIL");
    });
}
</script>
</body></html>
)DOCDOCDOC"_s;

TEST(WebKit, GetUserMediaFocus)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { getUserMediaFocusText } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 9090);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    initializeMediaCaptureConfiguration(configuration.get());
    auto preferences = [configuration preferences];
    preferences._getUserMediaRequiresFocus = YES;

    done = false;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    webView.get().UIDelegate = delegate.get();

    // Load page.
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);

    // Minimize
    done = false;
    auto *hostWindow = [webView hostWindow];
    [hostWindow miniaturize:hostWindow];
    TestWebKitAPI::Util::run(&done);

    // We call capture while minimizing.
    done = false;
    [webView stringByEvaluatingJavaScript:@"capture()"];

    // Make sure that getUserMedia does not resolve too soon.
    TestWebKitAPI::Util::spinRunLoop(100);
    EXPECT_FALSE(done);

    [hostWindow deminiaturize:hostWindow];
    TestWebKitAPI::Util::run(&done);
}
#endif // PLATFORM(MAC)

TEST(WebKit, InvalidDeviceIdHashSalts)
{
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"InvalidDeviceIdHashSaltsPathsTest"] isDirectory:YES];
    NSURL *hashSaltLocation = [tempDir URLByAppendingPathComponent:@"1"];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager createDirectoryAtURL:hashSaltLocation withIntermediateDirectories:YES attributes:nil error:nil];

    NSString *fileName = @"CBF3087F32B7E19D13F683C9973060819F869CA9298B405E";
    NSURL *filePath = [hashSaltLocation URLByAppendingPathComponent:fileName];

    // Prepare invalid data.
    if ([fileManager fileExistsAtPath:filePath.path])
        [fileManager removeItemAtPath:filePath.path error:nil];
    auto *fileData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"invalidDeviceIDHashSalts" withExtension:@""]];
    EXPECT_TRUE([fileManager createFileAtPath:filePath.path contents:fileData attributes: nil]);
    while (![fileManager fileExistsAtPath:filePath.path])
        Util::spinRunLoop();

    // Prepare web view to use the invalid data
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [websiteDataStoreConfiguration setDeviceIdHashSaltsStorageDirectory:tempDir];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
    initializeMediaCaptureConfiguration(configuration.get());
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    // Wait for invalid data to be loaded.
    [webView loadTestPageNamed:@"getUserMedia"];
    [delegate waitUntilPrompted];
}


static _WKFeature *permissionsAPIEnabledExperimentalFeature()
{
    static RetainPtr<_WKFeature> theFeature;
    if (theFeature)
        return theFeature.get();

    NSArray *features = [WKPreferences _features];
    for (_WKFeature *feature in features) {
        if ([feature.key isEqual:@"PermissionsAPIEnabled"]) {
            theFeature = feature;
            break;
        }
    }
    return theFeature.get();
}

TEST(WebKit2, CapturePermission)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());
    [[configuration preferences] _setEnabled:YES forFeature:permissionsAPIEnabledExperimentalFeature()];

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView loadTestPageNamed:@"getUserMediaPermission"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [delegate setAudioDecision:WKPermissionDecisionPrompt];
    [delegate setVideoDecision:WKPermissionDecisionPrompt];
    [webView stringByEvaluatingJavaScript:@"checkPermission('microphone', 'prompt')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"checkPermission('camera', 'prompt')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [delegate setAudioDecision:WKPermissionDecisionGrant];
    [delegate setVideoDecision:WKPermissionDecisionGrant];
    [webView stringByEvaluatingJavaScript:@"checkPermission('microphone', 'granted')"];
    TestWebKitAPI::Util::run(&done);
    done = false;
    [webView stringByEvaluatingJavaScript:@"checkPermission('camera', 'granted')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Although Deny, we expect prompt is exposed since we do not trust the page to call getUserMedia.
    [delegate setAudioDecision:WKPermissionDecisionDeny];
    [delegate setVideoDecision:WKPermissionDecisionDeny];
    [webView stringByEvaluatingJavaScript:@"checkPermission('microphone', 'prompt')"];
    TestWebKitAPI::Util::run(&done);
    done = false;
    [webView stringByEvaluatingJavaScript:@"checkPermission('camera', 'prompt')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Now that we getUserMedia has been called, we can go with deny.
    [webView stringByEvaluatingJavaScript:@"captureVideo()"];
    [delegate waitUntilPrompted];
    [webView stringByEvaluatingJavaScript:@"checkPermission('microphone', 'prompt')"];
    TestWebKitAPI::Util::run(&done);
    done = false;
    [webView stringByEvaluatingJavaScript:@"checkPermission('camera', 'denied')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"captureAudio()"];
    [delegate waitUntilPrompted];
    [webView stringByEvaluatingJavaScript:@"checkPermission('microphone', 'denied')"];
    TestWebKitAPI::Util::run(&done);
    done = false;
    [webView stringByEvaluatingJavaScript:@"checkPermission('camera', 'denied')"];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

static constexpr auto capturePermissionChangedWebPage = R"DOCDOCDOC(
<!DOCTYPE html>
<html>
<body onload="window.webkit.messageHandlers.gum.postMessage('PASS')">
<script>
function callGetUserMedia(audio, video, successMessage, failureMessage)
{
    navigator.mediaDevices.getUserMedia({audio: audio, video:video}).then(() => {
        window.webkit.messageHandlers.gum.postMessage(successMessage);
    }, () => {
        window.webkit.messageHandlers.gum.postMessage(failureMessage);
    });
}
</script>
</body>
</html>
)DOCDOCDOC"_s;

TEST(WebKit2, CapturePermissionChangedFromGrantToDeny)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { capturePermissionChangedWebPage } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [delegate setAudioDecision:WKPermissionDecisionGrant];
    [delegate setVideoDecision:WKPermissionDecisionGrant];

    done = false;
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"callGetUserMedia(false, true, 'PASS', 'Camera initial failure unexpected')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"callGetUserMedia(true, false, 'PASS', 'Microphone initial failure unexpected')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [delegate setVideoDecision:WKPermissionDecisionDeny];

    auto originString = adoptWK(WKStringCreateWithUTF8CString([server.request().URL.absoluteString cStringUsingEncoding:NSUTF8StringEncoding]));
    auto origin = adoptWK(WKSecurityOriginCreateFromString(originString.get()));
    [WKWebView _permissionChanged:@"camera" forOrigin:(__bridge WKSecurityOrigin *)origin.get()];

    [webView stringByEvaluatingJavaScript:@"callGetUserMedia(false, true, 'Camera second success unexpected', 'PASS')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"callGetUserMedia(true, false, 'PASS', 'Microphone second failure unexpected')"];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WebKit2, CapturePermissionChangedFromDenyToGrant)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { capturePermissionChangedWebPage } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [delegate setAudioDecision:WKPermissionDecisionDeny];
    [delegate setVideoDecision:WKPermissionDecisionDeny];

    done = false;
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"callGetUserMedia(false, true, 'Camera initial success unexpected', 'PASS')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"callGetUserMedia(true, false, 'Microphone initial success unexpected', 'PASS')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [delegate setAudioDecision:WKPermissionDecisionGrant];

    auto originString = adoptWK(WKStringCreateWithUTF8CString([server.request().URL.absoluteString cStringUsingEncoding:NSUTF8StringEncoding]));
    auto origin = adoptWK(WKSecurityOriginCreateFromString(originString.get()));
    [WKWebView _permissionChanged:@"microphone" forOrigin:(__bridge WKSecurityOrigin *)origin.get()];

    [webView stringByEvaluatingJavaScript:@"callGetUserMedia(false, true, 'Camera second success unexpected', 'PASS')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"callGetUserMedia(true, false, 'PASS', 'Microphone second failure unexpected')"];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WebKit2, EnumerateDevicesAfterMuting)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._inactiveMediaCaptureStreamRepromptIntervalInMinutes = .5 / 60;

    initializeMediaCaptureConfiguration(configuration.get());

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

    [webView setCameraCaptureState:WKMediaCaptureStateMuted completionHandler:nil];
    [webView setMicrophoneCaptureState:WKMediaCaptureStateMuted completionHandler:nil];

    // Sleep long enough for the reprompt timer to fire and clear cached state.
    Util::runFor(1_s);

    // Verify enumerateDevices still works fine.
    done = false;
    [webView stringByEvaluatingJavaScript:@"checkEnumerateDevicesDoesNotFilter()"];
    TestWebKitAPI::Util::run(&done);
}

#if PLATFORM(IOS_FAMILY)
TEST(WebKit2, CapturePermissionWithSystemBlocking)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._mediaCaptureEnabled = YES;

    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mockCaptureDevicesEnabled = NO;
    preferences._getUserMediaRequiresFocus = NO;
    [preferences _setEnabled:YES forFeature:permissionsAPIEnabledExperimentalFeature()];

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView loadTestPageNamed:@"getUserMediaPermission"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [delegate setAudioDecision:WKPermissionDecisionGrant];
    [delegate setVideoDecision:WKPermissionDecisionGrant];

    // Given mock capture is not enabled, system is forbidding access to camera and microphone.
    // Permission API should not show granted even if our callback grants access.
    [webView stringByEvaluatingJavaScript:@"checkPermission('microphone', 'prompt')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"checkPermission('camera', 'prompt')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Permission API should show denied now that getUserMedia was called.
    [webView stringByEvaluatingJavaScript:@"callGetUserMedia(true, false)"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"checkPermission('microphone', 'denied')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"checkPermission('camera', 'prompt')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // We cannot start camera on iOS simulator as there might not be any camera available.
}
#endif

#if PLATFORM(MAC)
TEST(WebKit2, ConnectedToHardwareConsole)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());
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
    [webView _setConnectedToHardwareConsoleForTesting:NO];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));

    // It should not be possible to unmute while detached.
    cameraCaptureStateChange = false;
    [webView setCameraCaptureState:WKMediaCaptureStateActive completionHandler:nil];
    int retryCount = 1000;
    while (--retryCount && cameraCaptureState == WKMediaCaptureStateMuted)
        TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_TRUE(cameraCaptureState == WKMediaCaptureStateMuted);

    // Capture should be unmuted if it was active when the disconnect happened.
    cameraCaptureStateChange = false;
    [webView _setConnectedToHardwareConsoleForTesting:YES];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));

    cameraCaptureStateChange = false;
    [webView setCameraCaptureState:WKMediaCaptureStateMuted completionHandler:nil];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));

    // Reconnecting should not unmute if capture if it was already muted when the disconnect happened.
    [webView _setConnectedToHardwareConsoleForTesting:NO];
    retryCount = 1000;
    while (--retryCount)
        TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_TRUE(cameraCaptureState == WKMediaCaptureStateMuted);

    [webView _setConnectedToHardwareConsoleForTesting:YES];
    retryCount = 1000;
    while (--retryCount)
        TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_TRUE(cameraCaptureState == WKMediaCaptureStateMuted);
}

TEST(WebKit2, DoNotUnmuteWhenTakingAThumbnail)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());
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

    [webView _setThumbnailView:nil];
    int retryCount = 1000;
    while (--retryCount && cameraCaptureState == WKMediaCaptureStateMuted) {
        TestWebKitAPI::Util::spinRunLoop(10);
    }
    EXPECT_TRUE(cameraCaptureState == WKMediaCaptureStateMuted);
}
#endif

#if WK_HAVE_C_SPI
TEST(WebKit2, WebRTCAndRemoteCommands)
{
    [TestProtocol registerWithScheme:@"https"];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    configuration.get().processPool._configuration.shouldCaptureAudioInUIProcess = NO;

    initializeMediaCaptureConfiguration(configuration.get());

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView _setMediaCaptureReportingDelayForTesting:0];

    auto observer = adoptNS([[MediaCaptureObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:@"microphoneCaptureState" options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:observer.get() forKeyPath:@"cameraCaptureState" options:NSKeyValueObservingOptionNew context:nil];

    cameraCaptureStateChange = false;
    microphoneCaptureStateChange = false;

    done = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/webrtc-remote.html"]]];
    [webView _test_waitForDidFinishNavigation];

    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));

    done = false;
    [webView stringByEvaluatingJavaScript:@"startTest()"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    cameraCaptureStateChange = false;
    microphoneCaptureStateChange = false;
    [webView stringByEvaluatingJavaScript:@"sendCommand('pause')"];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateMuted));

    [webView stringByEvaluatingJavaScript:@"sendCommand('play')"];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));

    [webView stringByEvaluatingJavaScript:@"sendCommand('toggleplaypause')"];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateMuted));

    [webView stringByEvaluatingJavaScript:@"sendCommand('toggleplaypause')"];
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));

    done = false;
    // register handlers will catch commands, so capture should not muted.
    [webView stringByEvaluatingJavaScript:@"registerHandlers()"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView stringByEvaluatingJavaScript:@"sendCommand('pause')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));

    [webView stringByEvaluatingJavaScript:@"sendCommand('play')"];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));
}

TEST(WebKit2, ToggleCameraCaptureWhenRestarting)
{
    [TestProtocol registerWithScheme:@"https"];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("MediaSessionCaptureToggleAPIEnabled"));

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    configuration.get().processPool._configuration.shouldCaptureAudioInUIProcess = NO;

    initializeMediaCaptureConfiguration(configuration.get());

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView _setMediaCaptureReportingDelayForTesting:0];

    auto observer = adoptNS([[MediaCaptureObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:@"microphoneCaptureState" options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:observer.get() forKeyPath:@"cameraCaptureState" options:NSKeyValueObservingOptionNew context:nil];

    done = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/media-session-capture.html"]]];
    TestWebKitAPI::Util::run(&done);

    cameraCaptureStateChange = false;
    microphoneCaptureStateChange = false;
    done = false;
    [webView stringByEvaluatingJavaScript:@"startCapture()"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));

    // Mute capture.
    cameraCaptureStateChange = false;
    microphoneCaptureStateChange = false;

    [webView setCameraCaptureState:WKMediaCaptureStateMuted completionHandler:nil];
    [webView setMicrophoneCaptureState:WKMediaCaptureStateMuted completionHandler:nil];

    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateMuted));

    // Unmute via MediaSession.
    cameraCaptureStateChange = false;
    done = false;
    [webView stringByEvaluatingJavaScript:@"setCameraActive(true)"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));

    // Validate handlers/events order.
    done = false;
    [webView stringByEvaluatingJavaScript:@"validateActionState('deactivating camera, muting camera, deactivating microphone, muting microphone, setCameraActive successful, unmuting camera, end')"];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit2, ToggleMicrophoneCaptureWhenRestarting)
{
    [TestProtocol registerWithScheme:@"https"];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("MediaSessionCaptureToggleAPIEnabled"));

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    configuration.get().processPool._configuration.shouldCaptureAudioInUIProcess = NO;

    initializeMediaCaptureConfiguration(configuration.get());

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView _setMediaCaptureReportingDelayForTesting:0];

    auto observer = adoptNS([[MediaCaptureObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:@"microphoneCaptureState" options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:observer.get() forKeyPath:@"cameraCaptureState" options:NSKeyValueObservingOptionNew context:nil];

    done = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/media-session-capture.html"]]];
    TestWebKitAPI::Util::run(&done);

    cameraCaptureStateChange = false;
    microphoneCaptureStateChange = false;
    done = false;
    [webView stringByEvaluatingJavaScript:@"startCapture()"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateActive));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));

    // Mute capture.
    cameraCaptureStateChange = false;
    microphoneCaptureStateChange = false;

    [webView setCameraCaptureState:WKMediaCaptureStateMuted completionHandler:nil];
    [webView setMicrophoneCaptureState:WKMediaCaptureStateMuted completionHandler:nil];

    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));
    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateMuted));

    // Unmute via MediaSession.
    cameraCaptureStateChange = false;
    microphoneCaptureStateChange = false;
    done = false;
    [webView stringByEvaluatingJavaScript:@"setMicrophoneActive(true)"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(waitUntilMicrophoneState(webView.get(), WKMediaCaptureStateActive));

    sleep(0.5_s);
    EXPECT_TRUE(waitUntilCameraState(webView.get(), WKMediaCaptureStateMuted));

    // Validate handlers/events order.
    done = false;
    [webView stringByEvaluatingJavaScript:@"validateActionState('deactivating camera, muting camera, deactivating microphone, muting microphone, setMicrophoneActive successful, unmuting microphone, end')"];
    TestWebKitAPI::Util::run(&done);
}
#endif // WK_HAVE_C_SPI

#if PLATFORM(IOS_FAMILY)

TEST(WebKit2, OrientationNotAffectedByCSSOrientation)
{
    auto runTestForOrientation = ^(UIInterfaceOrientation orientation) {
        done = false;

        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        initializeMediaCaptureConfiguration(configuration.get());

        auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
        [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
        auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
        [webView setUIDelegate:delegate.get()];

        [webView _setInterfaceOrientationOverride:orientation];

        [webView loadTestPageNamed:@"getUserMedia"];
        EXPECT_TRUE(waitUntilCaptureState(webView.get(), _WKMediaCaptureStateDeprecatedActiveCamera));

        [delegate waitUntilPrompted];

        done = false;
        [webView stringByEvaluatingJavaScript:@"captureVideo(true)"];
        TestWebKitAPI::Util::run(&done);

        NSString *actualOrientation = [webView stringByEvaluatingJavaScript:@"captureOrientation()"];

        // Capture orientation should be landscape regardless of the
        // CSS orientation override.
        EXPECT_WK_STREQ(actualOrientation, "landscape");
    };

    runTestForOrientation(UIInterfaceOrientationLandscapeLeft);
    runTestForOrientation(UIInterfaceOrientationLandscapeRight);
    runTestForOrientation(UIInterfaceOrientationPortrait);
    runTestForOrientation(UIInterfaceOrientationPortraitUpsideDown);
}

#endif

TEST(GetUserMedia, ClearRemoteVideoFrameObjectHeapPixelConformerUnderMemoryPressure)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("CaptureAudioInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("CaptureVideoInGPUProcessEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, WKStringCreateWithUTF8CString("UseGPUProcessForCanvasRenderingEnabled"));
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], false, WKStringCreateWithUTF8CString("UseGPUProcessForDOMRenderingEnabled"));

    configuration.get()._mediaCaptureEnabled = YES;
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mockCaptureDevicesEnabled = YES;
    preferences._getUserMediaRequiresFocus = NO;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    done = false;
    [webView synchronouslyLoadTestPageNamed:@"camera-to-canvas"];
    TestWebKitAPI::Util::run(&done);

    // trigger canvas painting to use the pixel conformer.
    done = false;
    [webView callAsyncJavaScript:@"paintCameraInCanvas()" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
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

    done = false;
    // trigger again canvas painting and verify this works ok again
    [webView callAsyncJavaScript:@"paintCameraInCanvas()" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
    }];
    TestWebKitAPI::Util::run(&done);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
