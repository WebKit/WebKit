/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

#if PLATFORM(IOS_FAMILY)
#include <pal/system/ios/UserInterfaceIdiom.h>
#endif

@interface GetUserMediaRepromptTestView : TestWKWebView
- (BOOL)haveStream:(BOOL)expected;
@end

@implementation GetUserMediaRepromptTestView
- (BOOL)haveStream:(BOOL)expected
{
    int retryCount = 1000;
    while (retryCount--) {
        auto result = [self stringByEvaluatingJavaScript:@"haveStream()"];
        if (result.boolValue == expected)
            return YES;

        TestWebKitAPI::Util::spinRunLoop(10);
    }

    return NO;
}
@end

namespace TestWebKitAPI {

static void initializeMediaCaptureConfiguration(WKWebViewConfiguration* configuration)
{
    auto preferences = [configuration preferences];

    configuration._mediaCaptureEnabled = YES;
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mockCaptureDevicesEnabled = YES;
    preferences._getUserMediaRequiresFocus = NO;
}

TEST(WebKit2, GetUserMediaReprompt)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    initializeMediaCaptureConfiguration(configuration.get());
    auto webView = adoptNS([[GetUserMediaRepromptTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMedia"];
    [delegate waitUntilPrompted];

    EXPECT_TRUE([webView haveStream:YES]);

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE([webView haveStream:NO]);

    EXPECT_FALSE([delegate wasPrompted]);
    [webView stringByEvaluatingJavaScript:@"promptForCapture()"];
    EXPECT_TRUE([webView haveStream:YES]);
    EXPECT_FALSE([delegate wasPrompted]);

    preferences._inactiveMediaCaptureStreamRepromptIntervalInMinutes = .5 / 60;
    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE([webView haveStream:NO]);

    // Sleep long enough for the reprompt timer to fire and clear cached state.
    Util::runFor(1_s);

    [webView stringByEvaluatingJavaScript:@"promptForCapture()"];
    [delegate waitUntilPrompted];
}

TEST(WebKit2, GetUserMediaRepromptWithoutUserGesture)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._inactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes = 0.5 / 60;

    initializeMediaCaptureConfiguration(configuration.get());
    auto webView = adoptNS([[GetUserMediaRepromptTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMedia"];
    [delegate waitUntilPrompted];

    EXPECT_TRUE([webView haveStream:YES]);

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE([webView haveStream:NO]);

    // Sleep a bit but not long enough for reprompting capture without user gesture.
    Util::runFor(0.1_s);

    EXPECT_FALSE([delegate wasPrompted]);
    [webView objectByEvaluatingJavaScript:@"promptForCapture()"];
    EXPECT_TRUE([webView haveStream:YES]);
    EXPECT_FALSE([delegate wasPrompted]);

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE([webView haveStream:NO]);

    // Sleep long enough for the time without capture to be above 0.5s.
    Util::runFor(1_s);

    // Start capture without user gesture.
    [webView objectByEvaluatingJavaScript:@"promptForCapture()"];
    [delegate waitUntilPrompted];
}

TEST(WebKit2, GetUserMediaRepromptAfterAudioVideoBeingDenied)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());
    auto webView = adoptNS([[GetUserMediaRepromptTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [delegate setAudioDecision:WKPermissionDecisionGrant];
    [delegate setVideoDecision:WKPermissionDecisionDeny];
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMediaAudioVideoCapture"];
    [delegate waitUntilPrompted];
    EXPECT_TRUE([webView haveStream:NO]);

    [webView stringByEvaluatingJavaScript:@"promptForAudioOnly()"];
    [delegate waitUntilPrompted];
    EXPECT_TRUE([webView haveStream:YES]);
}

TEST(WebKit2, MultipleGetUserMediaSynchronously)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());
    auto webView = adoptNS([[GetUserMediaRepromptTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMedia"];
    [delegate waitUntilPrompted];
    EXPECT_EQ([delegate numberOfPrompts], 1);

    [webView stringByEvaluatingJavaScript:@"doMultipleGetUserMediaSynchronously()"];
    [delegate waitUntilPrompted];
    EXPECT_EQ([delegate numberOfPrompts], 2);
}

TEST(WebKit2, GetUserMediaDefaultInactiveCaptureRepromptInterval)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    initializeMediaCaptureConfiguration(configuration.get());
    auto webView = adoptNS([[GetUserMediaRepromptTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"getUserMedia"];
    [delegate waitUntilPrompted];

    EXPECT_TRUE([webView haveStream:YES]);

    auto expectedInactiveMediaCaptureStreamRepromptInterval = []() -> double {
#if PLATFORM(IOS_FAMILY)
        if (!PAL::currentUserInterfaceIdiomIsDesktop())
            return 1;
#endif
        return 10;
    };
    EXPECT_EQ(preferences._inactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes, expectedInactiveMediaCaptureStreamRepromptInterval());
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
