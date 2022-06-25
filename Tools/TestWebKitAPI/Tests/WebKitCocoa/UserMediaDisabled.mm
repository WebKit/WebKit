/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "DeprecatedGlobalValues.h"
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

@interface UserMediaMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation UserMediaMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    lastScriptMessage = message;
    receivedScriptMessage = true;
}
@end

@interface UserMediaUIDelegate : NSObject <WKUIDelegate>
@end

@implementation UserMediaUIDelegate

- (void)webView:(WKWebView *)webView requestMediaCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame type:(WKMediaCaptureType)type decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler
{
    wasPrompted = true;

    decisionHandler(WKPermissionDecisionGrant);
}

- (void)_webView:(WKWebView *)webView includeSensitiveMediaDeviceDetails:(void (^)(BOOL includeSensitiveDetails))decisionHandler
{
    decisionHandler(NO);
}
@end

class MediaCaptureDisabledTest : public testing::Test {
public:
    virtual void SetUp()
    {
        m_configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        m_configuration.get()._mediaCaptureEnabled = YES;

        RetainPtr<UserMediaMessageHandler> handler = adoptNS([[UserMediaMessageHandler alloc] init]);
        [[m_configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

        m_webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:m_configuration.get()]);

        auto preferences = [m_webView configuration].preferences;
        preferences._mockCaptureDevicesEnabled = YES;
        preferences._mediaCaptureRequiresSecureConnection = NO;
        preferences._getUserMediaRequiresFocus = NO;

        m_uiDelegate = adoptNS([[UserMediaUIDelegate alloc] init]);
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
    RetainPtr<UserMediaUIDelegate> m_uiDelegate;
    RetainPtr<TestWKWebView> m_webView;
};

TEST_F(MediaCaptureDisabledTest, EnableAndDisable)
{
    EXPECT_TRUE(m_webView.get()._mediaCaptureEnabled);
    loadTestAndWaitForMessage("allowed");
    EXPECT_TRUE(wasPrompted);

    m_webView.get()._mediaCaptureEnabled = NO;
    EXPECT_FALSE(m_webView.get()._mediaCaptureEnabled);
    loadTestAndWaitForMessage("denied");
    EXPECT_FALSE(wasPrompted);

    m_webView.get()._mediaCaptureEnabled = YES;
    EXPECT_TRUE(m_webView.get()._mediaCaptureEnabled);
    loadTestAndWaitForMessage("allowed");
    EXPECT_TRUE(wasPrompted);
}

TEST_F(MediaCaptureDisabledTest, UnsecureContext)
{
    auto preferences = [m_webView configuration].preferences;
    preferences._mediaCaptureRequiresSecureConnection = YES;

    receivedScriptMessage = false;
    [m_webView loadHTMLString:@"<html><body><script>window.webkit.messageHandlers.testHandler.postMessage(Navigator.prototype.hasOwnProperty('mediaDevices') ? 'has' : 'none');</script></body></html>" baseURL:adoptNS([[NSURL alloc] initWithString:@"http://test.org"]).get()];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_STREQ([(NSString *)[lastScriptMessage body] UTF8String], "none");
}
