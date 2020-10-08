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
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/WTFString.h>

static bool done;
static bool wasPrompted;

@interface GetUserMediaCaptureUIDelegate : NSObject<WKUIDelegate>
- (void)_webView:(WKWebView *)webView requestMediaCaptureAuthorization: (_WKCaptureDevices)devices decisionHandler:(void (^)(BOOL))decisionHandler;
- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler;
@end

@implementation GetUserMediaCaptureUIDelegate
- (void)_webView:(WKWebView *)webView requestMediaCaptureAuthorization: (_WKCaptureDevices)devices decisionHandler:(void (^)(BOOL))decisionHandler
{
    decisionHandler(YES);
    wasPrompted = true;
}

- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler
{
    decisionHandler(@"0x9876543210", YES);
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
    preferences._mediaDevicesEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;
    auto webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()];
    auto delegate = adoptNS([[GetUserMediaCaptureUIDelegate alloc] init]);
    webView.UIDelegate = delegate.get();

    webView._mediaCaptureReportingDelayForTesting = 0;

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateActiveCamera));

    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateMutedCamera));
    [webView _setPageMuted: _WKMediaNoneMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateActiveCamera));

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateNone));

    [webView stringByEvaluatingJavaScript:@"captureAudio()"];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateActiveMicrophone));
    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateMutedMicrophone));
    [webView _setPageMuted: _WKMediaNoneMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateActiveMicrophone));

    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateMutedMicrophone));

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateNone));

    [webView stringByEvaluatingJavaScript:@"captureAudioAndVideo()"];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateActiveCamera | _WKMediaCaptureStateActiveMicrophone));
    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateMutedCamera | _WKMediaCaptureStateMutedMicrophone));
    [webView _setPageMuted: _WKMediaNoneMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateActiveCamera | _WKMediaCaptureStateActiveMicrophone));

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateNone));
}

TEST(WebKit2, CaptureStop)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mediaDevicesEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()];
    auto delegate = adoptNS([[GetUserMediaCaptureUIDelegate alloc] init]);
    webView.UIDelegate = delegate.get();

    wasPrompted = false;

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateActiveCamera));

    TestWebKitAPI::Util::run(&wasPrompted);
    wasPrompted = false;

    [webView _setPageMuted: _WKMediaCaptureDevicesMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateMutedCamera));
    [webView _setPageMuted: _WKMediaNoneMuted];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateActiveCamera));

    [webView stringByEvaluatingJavaScript:@"notifyEndedEvent()"];
    [webView _stopMediaCapture];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateNone));

    [webView stringByEvaluatingJavaScript:@"promptForCapture()"];
    TestWebKitAPI::Util::run(&wasPrompted);
    wasPrompted = false;
}

TEST(WebKit2, CaptureIndicatorDelay)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mediaDevicesEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()];
    webView._mediaCaptureReportingDelayForTesting = 2;

    auto delegate = adoptNS([[GetUserMediaCaptureUIDelegate alloc] init]);
    webView.UIDelegate = delegate.get();

    wasPrompted = false;

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateActiveCamera));

    TestWebKitAPI::Util::run(&wasPrompted);
    wasPrompted = false;

    [webView stringByEvaluatingJavaScript:@"stop()"];

    // We wait 1 second, we should still see camera be reported.
    sleep(1_s);
    EXPECT_EQ([webView _mediaCaptureState], _WKMediaCaptureStateActiveCamera);

    // One additional second should allow us to go back to no capture being reported.
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateNone));

}

TEST(WebKit2, GetCapabilities)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mediaDevicesEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()];
    auto delegate = adoptNS([[GetUserMediaCaptureUIDelegate alloc] init]);
    webView.UIDelegate = delegate.get();

    wasPrompted = false;

    [webView loadTestPageNamed:@"getUserMedia"];
    EXPECT_TRUE(waitUntilCaptureState(webView, _WKMediaCaptureStateActiveCamera));

    TestWebKitAPI::Util::run(&wasPrompted);

    done = false;
    [webView stringByEvaluatingJavaScript:@"checkGetCapabilities()"];

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
    preferences._mediaDevicesEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;

    auto messageHandler = adoptNS([[GUMMessageHandler alloc] init]);
    [[configuration.get() userContentController] addScriptMessageHandler:messageHandler.get() name:@"gum"];

    auto webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()];

    auto delegate = adoptNS([[GetUserMediaCaptureUIDelegate alloc] init]);
    webView.UIDelegate = delegate.get();

    auto url = adoptWK(Util::createURLForResource("getUserMedia-webaudio", "html"));
    [webView loadTestPageNamed:@"getUserMedia-webaudio"];

    TestWebKitAPI::Util::run(&done);
    done = false;
}
#endif

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
