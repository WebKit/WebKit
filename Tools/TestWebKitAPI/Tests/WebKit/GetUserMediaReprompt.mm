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
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

static bool wasPrompted = false;

@interface GetUserMediaRepromptUIDelegate : NSObject<WKUIDelegate>
- (void)_webView:(WKWebView *)webView requestMediaCaptureAuthorization: (_WKCaptureDevices)devices decisionHandler:(void (^)(BOOL))decisionHandler;
- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler;
@end

@implementation GetUserMediaRepromptUIDelegate
- (void)_webView:(WKWebView *)webView requestMediaCaptureAuthorization: (_WKCaptureDevices)devices decisionHandler:(void (^)(BOOL))decisionHandler
{
    wasPrompted = true;
    decisionHandler(YES);
}

- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler
{
    decisionHandler(@"0x987654321", YES);
}
@end

@interface GetUserMediaRepromptTestView : TestWKWebView
- (BOOL)haveStream:(BOOL)expected;
@end

@implementation GetUserMediaRepromptTestView
- (BOOL)haveStream:(BOOL)expected
{
    int retryCount = 10;
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

TEST(WebKit2, GetUserMediaReprompt)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mediaDevicesEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;
    auto webView = [[GetUserMediaRepromptTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()];
    auto delegate = adoptNS([[GetUserMediaRepromptUIDelegate alloc] init]);
    webView.UIDelegate = delegate.get();

    wasPrompted = false;
    [webView loadTestPageNamed:@"getUserMedia"];
    TestWebKitAPI::Util::run(&wasPrompted);

    EXPECT_TRUE([webView haveStream:YES]);

    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE([webView haveStream:NO]);

    wasPrompted = false;
    [webView stringByEvaluatingJavaScript:@"promptForCapture()"];
    EXPECT_TRUE([webView haveStream:YES]);
    EXPECT_FALSE(wasPrompted);

    preferences._inactiveMediaCaptureSteamRepromptIntervalInMinutes = .5 / 60;
    [webView stringByEvaluatingJavaScript:@"stop()"];
    EXPECT_TRUE([webView haveStream:NO]);

    // Sleep long enough for the reprompt timer to fire and clear cached state.
    Util::sleep(1);

    wasPrompted = false;
    [webView stringByEvaluatingJavaScript:@"promptForCapture()"];
    TestWebKitAPI::Util::run(&wasPrompted);
    EXPECT_TRUE([webView haveStream:YES]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
