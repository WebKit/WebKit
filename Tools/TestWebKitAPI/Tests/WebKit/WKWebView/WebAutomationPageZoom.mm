/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(REMOTE_INSPECTOR)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKAutomationSession.h>
#import <WebKit/_WKAutomationSessionConfiguration.h>
#import <WebKit/_WKAutomationSessionPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

// Sends a command with id 1 and returns the JSON text of the reply, which holds either a "result" or an "error".
static RetainPtr<NSString> dispatchAutomationCommand(_WKAutomationSession *session, NSString *command)
{
    __block bool gotReply = false;
    __block RetainPtr<NSString> reply;
    [session _setMessageToFrontendHandlerForTesting:^(NSString *message) {
        if (![message containsString:@"\"id\":1"])
            return;
        reply = message;
        gotReply = true;
    }];
    [session _dispatchMessageFromRemoteForTesting:command];
    Util::run(&gotReply);
    [session _setMessageToFrontendHandlerForTesting:nil];
    return reply;
}

class WebAutomationPageZoom : public testing::Test {
public:
    void SetUp() override
    {
        RetainPtr sessionConfiguration = adoptNS([_WKAutomationSessionConfiguration new]);
        m_session = adoptNS([[_WKAutomationSession alloc] initWithConfiguration:sessionConfiguration.get()]);
        [m_session setSessionIdentifier:@"PageZoomTest"];

        m_processPool = adoptNS([WKProcessPool new]);
        [m_processPool _setAutomationSession:m_session.get()];

        RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
        [configuration setProcessPool:m_processPool.get()];

        m_webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [m_webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><body>page zoom</body>"];

        m_handle = [m_session _registerWebViewForTesting:m_webView.get()];
        ASSERT_NOT_NULL(m_handle.get());
    }

    void TearDown() override
    {
        [m_processPool _setAutomationSession:nil];
    }

    RetainPtr<NSString> setPageZoomFactor(NSString *handle, NSString *zoomFactor)
    {
        return dispatchAutomationCommand(m_session.get(), [NSString stringWithFormat:
            @"{\"id\":1,\"method\":\"Automation.setPageZoomFactorOfBrowsingContext\",\"params\":{\"handle\":\"%@\",\"zoomFactor\":%@}}", handle, zoomFactor]);
    }

    double devicePixelRatio() { return [[m_webView objectByEvaluatingJavaScript:@"window.devicePixelRatio"] doubleValue]; }

    RetainPtr<_WKAutomationSession> m_session;
    RetainPtr<WKProcessPool> m_processPool;
    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<NSString> m_handle;
};

TEST_F(WebAutomationPageZoom, SetsPageZoomFactor)
{
    double baseDevicePixelRatio = devicePixelRatio();
    EXPECT_DOUBLE_EQ([m_webView _pageZoomFactor], 1.0);

    RetainPtr reply = setPageZoomFactor(m_handle.get(), @"1.5");
    EXPECT_FALSE([reply containsString:@"\"error\""]) << [reply UTF8String];
    EXPECT_DOUBLE_EQ([m_webView _pageZoomFactor], 1.5);
    EXPECT_DOUBLE_EQ(devicePixelRatio(), baseDevicePixelRatio * 1.5);

    reply = setPageZoomFactor(m_handle.get(), @"1");
    EXPECT_FALSE([reply containsString:@"\"error\""]) << [reply UTF8String];
    EXPECT_DOUBLE_EQ([m_webView _pageZoomFactor], 1.0);
    EXPECT_DOUBLE_EQ(devicePixelRatio(), baseDevicePixelRatio);
}

TEST_F(WebAutomationPageZoom, RejectsNonPositiveZoomFactor)
{
    for (NSString *zoomFactor in @[ @"0", @"-1", @"-0.5" ]) {
        RetainPtr reply = setPageZoomFactor(m_handle.get(), zoomFactor);
        EXPECT_TRUE([reply containsString:@"InvalidParameter"]) << [zoomFactor UTF8String] << ": " << [reply UTF8String];
        EXPECT_DOUBLE_EQ([m_webView _pageZoomFactor], 1.0);
    }
}

TEST_F(WebAutomationPageZoom, RejectsUnknownBrowsingContext)
{
    RetainPtr reply = setPageZoomFactor(@"no-such-browsing-context", @"1.5");
    EXPECT_TRUE([reply containsString:@"WindowNotFound"]) << [reply UTF8String];
    EXPECT_DOUBLE_EQ([m_webView _pageZoomFactor], 1.0);
}

TEST_F(WebAutomationPageZoom, RejectsMissingZoomFactor)
{
    RetainPtr reply = dispatchAutomationCommand(m_session.get(), [NSString stringWithFormat:
        @"{\"id\":1,\"method\":\"Automation.setPageZoomFactorOfBrowsingContext\",\"params\":{\"handle\":\"%@\"}}", m_handle.get()]);
    EXPECT_TRUE([reply containsString:@"\"error\""]) << [reply UTF8String];
    EXPECT_DOUBLE_EQ([m_webView _pageZoomFactor], 1.0);
}

} // namespace TestWebKitAPI

#endif // ENABLE(REMOTE_INSPECTOR)
