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

#if ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>

static bool wasPrompted = false;
static bool shouldDeny = false;
static _WKCaptureDevices requestedDevices = 0;
static bool receivedScriptMessage = false;
static RetainPtr<WKScriptMessage> lastScriptMessage;

@interface GetDisplayMediaMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation GetDisplayMediaMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    lastScriptMessage = message;
    receivedScriptMessage = true;
}
@end

@interface GetDisplayMediaUIDelegate : NSObject<WKUIDelegate>
- (void)_webView:(WKWebView *)webView requestUserMediaAuthorizationForDevices:(_WKCaptureDevices)devices url:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL decisionHandler:(void (^)(BOOL authorized))decisionHandler;
- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler;
@end

@implementation GetDisplayMediaUIDelegate
- (void)_webView:(WKWebView *)webView requestUserMediaAuthorizationForDevices:(_WKCaptureDevices)devices url:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL decisionHandler:(void (^)(BOOL authorized))decisionHandler
{
    wasPrompted = true;

    if (shouldDeny) {
        decisionHandler(NO);
        return;
    }

    requestedDevices = devices;
    BOOL needsMicrophoneAuthorization = !!(requestedDevices & _WKCaptureDeviceMicrophone);
    BOOL needsCameraAuthorization = !!(requestedDevices & _WKCaptureDeviceCamera);
    BOOL needsDisplayCaptureAuthorization = !!(requestedDevices & _WKCaptureDeviceDisplay);
    if (!needsMicrophoneAuthorization && !needsCameraAuthorization && !needsDisplayCaptureAuthorization) {
        decisionHandler(NO);
        return;
    }

    decisionHandler(YES);
}

- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler
{
    decisionHandler(@"0x987654321", YES);
}
@end

namespace TestWebKitAPI {

class GetDisplayMediaTest : public testing::Test {
public:
    virtual void SetUp()
    {
        m_configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        auto handler = adoptNS([[GetDisplayMediaMessageHandler alloc] init]);
        [[m_configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

        auto preferences = [m_configuration preferences];
        preferences._mediaCaptureRequiresSecureConnection = NO;
        preferences._mediaDevicesEnabled = YES;
        preferences._mockCaptureDevicesEnabled = YES;
        preferences._screenCaptureEnabled = YES;

        m_webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:m_configuration.get()]);

        m_uiDelegate = adoptNS([[GetDisplayMediaUIDelegate alloc] init]);
        m_webView.get().UIDelegate = m_uiDelegate.get();

        [m_webView synchronouslyLoadTestPageNamed:@"getDisplayMedia"];
    }

    bool haveStream(bool expected)
    {
        int retryCount = 10;
        while (retryCount--) {
            auto result = [m_webView stringByEvaluatingJavaScript:@"haveStream()"];
            if (result.boolValue == expected)
                return YES;

            TestWebKitAPI::Util::spinRunLoop(10);
        }

        return NO;
    }

    void promptForCapture(NSString* constraints, bool shouldSucceed)
    {
        [m_webView stringByEvaluatingJavaScript:@"stop()"];
        EXPECT_TRUE(haveStream(false));

        wasPrompted = false;
        receivedScriptMessage = false;

        NSString *script = [NSString stringWithFormat:@"promptForCapture(%@)", constraints];
        [m_webView stringByEvaluatingJavaScript:script];

        TestWebKitAPI::Util::run(&receivedScriptMessage);
        if (shouldSucceed) {
            EXPECT_STREQ([(NSString *)[lastScriptMessage body] UTF8String], "allowed");
            EXPECT_TRUE(haveStream(true));
            EXPECT_TRUE(wasPrompted);
        } else {
            EXPECT_STREQ([(NSString *)[lastScriptMessage body] UTF8String], "denied");
            EXPECT_TRUE(haveStream(false));
            if (shouldDeny)
                EXPECT_TRUE(wasPrompted);
            else
                EXPECT_FALSE(wasPrompted);
        }
    }

    RetainPtr<WKWebViewConfiguration> m_configuration;
    RetainPtr<GetDisplayMediaUIDelegate> m_uiDelegate;
    RetainPtr<TestWKWebView> m_webView;
};

TEST_F(GetDisplayMediaTest, BasicPrompt)
{
    promptForCapture(@"{ audio: true, video: true }", true);
    promptForCapture(@"{ audio: true, video: false }", false);
    promptForCapture(@"{ audio: false, video: true }", true);
    promptForCapture(@"{ audio: false, video: false }", false);
}

TEST_F(GetDisplayMediaTest, Constraints)
{
    promptForCapture(@"{ video: {width: 640} }", true);
    promptForCapture(@"{ video: true, audio: { volume: 0.5 } }", true);
    promptForCapture(@"{ video: {height: 480}, audio: true }", true);
    promptForCapture(@"{ video: {width: { exact: 640} } }", false);
}

TEST_F(GetDisplayMediaTest, PromptOnceAfterDenial)
{
    promptForCapture(@"{ video: true }", true);
    shouldDeny = true;
    promptForCapture(@"{ video: true }", false);
    shouldDeny = false;
    promptForCapture(@"{ video: true }", false);
    promptForCapture(@"{ video: true }", false);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
