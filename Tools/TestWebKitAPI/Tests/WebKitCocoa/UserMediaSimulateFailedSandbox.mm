/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestWKWebView.h"

#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool wasPrompted = false;

static bool receivedScriptMessage = false;
static RetainPtr<WKScriptMessage> lastScriptMessage;

@interface SimulateFailedSandboxMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation SimulateFailedSandboxMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    lastScriptMessage = message;
    receivedScriptMessage = true;
}
@end

@interface SimulateFailedSandboxUIDelegate : NSObject <WKUIDelegate>
@end

@implementation SimulateFailedSandboxUIDelegate

- (void)_webView:(WKWebView *)webView requestMediaCaptureAuthorization: (_WKCaptureDevices)devices decisionHandler:(void (^)(BOOL))decisionHandler
{
    wasPrompted = true;

    BOOL needsMicrophoneAuthorized = devices & _WKCaptureDeviceMicrophone;
    BOOL needsCameraAuthorized = devices & _WKCaptureDeviceCamera;
    if (!needsMicrophoneAuthorized && !needsCameraAuthorized) {
        decisionHandler(NO);
        return;
    }

    decisionHandler(YES);
}

- (void)_webView:(WKWebView *)webView includeSensitiveMediaDeviceDetails:(void (^)(BOOL includeSensitiveDetails))decisionHandler
{
    decisionHandler(NO);
}
@end

class MediaCaptureSimulateFailedSandbox : public testing::Test {
public:
    virtual void SetUp()
    {
        m_configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        RetainPtr<SimulateFailedSandboxMessageHandler> handler = adoptNS([[SimulateFailedSandboxMessageHandler alloc] init]);
        [[m_configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

        m_webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:m_configuration.get()]);

        auto preferences = [m_webView configuration].preferences;
        preferences._mediaDevicesEnabled = YES;
        preferences._mockCaptureDevicesEnabled = YES;
        preferences._mediaCaptureRequiresSecureConnection = NO;

        m_uiDelegate = adoptNS([[SimulateFailedSandboxUIDelegate alloc] init]);
        [m_webView setUIDelegate:m_uiDelegate.get()];
    }

    void loadTestAndWaitForMessage(const char* message)
    {
        wasPrompted = false;
        receivedScriptMessage = false;
        [m_webView loadTestPageNamed:@"disableGetUserMedia"];
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        EXPECT_STREQ([(NSString *)[lastScriptMessage body] UTF8String], message);
    }

    RetainPtr<WKWebViewConfiguration> m_configuration;
    RetainPtr<SimulateFailedSandboxUIDelegate> m_uiDelegate;
    RetainPtr<TestWKWebView> m_webView;
};

TEST_F(MediaCaptureSimulateFailedSandbox, Test)
{
    EXPECT_TRUE(m_webView.get()._mediaCaptureEnabled);
    loadTestAndWaitForMessage("allowed");
    EXPECT_TRUE(wasPrompted);

    [m_webView _denyNextUserMediaRequest];
    loadTestAndWaitForMessage("denied");
    EXPECT_TRUE(wasPrompted);
    auto* error = [m_webView stringByEvaluatingJavaScript:@"lastError()"];
    EXPECT_STREQ([error UTF8String], "AbortError,Unable to extend sandbox.");

    loadTestAndWaitForMessage("allowed");
    EXPECT_TRUE(wasPrompted);
}
#endif
