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

#if ENABLE(MEDIA_STREAM) && HAVE(WINDOW_CAPTURE)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>

namespace TestWebKitAPI {

class TabCaptureTest : public testing::Test {
public:
    void SetUp() override
    {
        m_configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        RetainPtr preferences = [m_configuration preferences];
        [preferences _setMediaCaptureRequiresSecureConnection:NO];
        [preferences _setMockCaptureDevicesEnabled:YES];
        [preferences _setScreenCaptureEnabled:YES];
        [m_configuration _setMediaCaptureEnabled:YES];

        m_capturedWebView = createWebView(@"<body style='background-color: green'>Captured tab</body>");
    }

    void TearDown() override
    {
        for (auto& webView : m_capturingWebViews)
            [webView _setTabPickedForGetDisplayMediaForTesting:nil];
    }

protected:
    RetainPtr<TestWKWebView> createWebView(NSString *html)
    {
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300) configuration:m_configuration addToWindow:YES]);
        [webView synchronouslyLoadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        return webView;
    }

    RetainPtr<TestWKWebView> createCapturingWebView()
    {
        RetainPtr webView = createWebView(@"<body>Capturing page</body>");
        [webView _setTabPickedForGetDisplayMediaForTesting:m_capturedWebView];
        m_capturingWebViews.append(webView);
        return webView;
    }

    static void startCapture(TestWKWebView *webView)
    {
        id readyState = [webView objectByCallingAsyncFunction:@"window.stream = await navigator.mediaDevices.getDisplayMedia({ video: true }); return window.stream.getVideoTracks()[0].readyState;" withArguments:nil];
        EXPECT_WK_STREQ(@"live", readyState);
    }

    static void startSecondCapture(TestWKWebView *webView)
    {
        id readyState = [webView objectByCallingAsyncFunction:@"window.stream2 = await navigator.mediaDevices.getDisplayMedia({ video: true }); return window.stream2.getVideoTracks()[0].readyState;" withArguments:nil];
        EXPECT_WK_STREQ(@"live", readyState);
    }

    static NSString *videoTrackProperty(TestWKWebView *webView, NSString *expression)
    {
        return [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"window.stream.getVideoTracks()[0].%@", expression]];
    }

    static NSString *visibilityState(TestWKWebView *webView)
    {
        return [webView stringByEvaluatingJavaScript:@"document.visibilityState"];
    }

    static bool waitForVisibilityState(TestWKWebView *webView, NSString *expectedVisibilityState)
    {
        return Util::waitFor([&] {
            return [visibilityState(webView) isEqualToString:expectedVisibilityState];
        });
    }

    RetainPtr<WKWebViewConfiguration> m_configuration;
    RetainPtr<TestWKWebView> m_capturedWebView;
    Vector<RetainPtr<TestWKWebView>> m_capturingWebViews;
};

TEST_F(TabCaptureTest, PicksTab)
{
    RetainPtr webView = createCapturingWebView();
    startCapture(webView);

    EXPECT_WK_STREQ(@"Mock tab device 1", videoTrackProperty(webView, @"label"));
    EXPECT_WK_STREQ(@"browser", videoTrackProperty(webView, @"getSettings().displaySurface"));
}

TEST_F(TabCaptureTest, CapturedTabStaysVisibleInHiddenWindow)
{
    RetainPtr webView = createCapturingWebView();
    startCapture(webView);

    [m_capturedWebView setVisibility:NO];
    TestWebKitAPI::Util::runFor(0.01_s);
    EXPECT_WK_STREQ(@"visible", visibilityState(m_capturedWebView));

    [webView stringByEvaluatingJavaScript:@"window.stream.getTracks().forEach(track => track.stop())"];
    EXPECT_TRUE(waitForVisibilityState(m_capturedWebView, @"hidden"));
}

TEST_F(TabCaptureTest, CapturedTabBackToHiddenWhenNoLongerCaptured)
{
    RetainPtr webView = createCapturingWebView();
    startCapture(webView);

    [m_capturedWebView setVisibility:NO];
    TestWebKitAPI::Util::runFor(0.01_s);
    EXPECT_WK_STREQ(@"visible", visibilityState(m_capturedWebView));

    [webView _setTabPickedForGetDisplayMediaForTesting:webView];
    startSecondCapture(webView);

    // We stop capturing m_capturedWebView while capturing webView. We expect m_capturedWebView to be hidden.
    [webView stringByEvaluatingJavaScript:@"window.stream.getTracks().forEach(track => track.stop())"];
    EXPECT_TRUE(waitForVisibilityState(m_capturedWebView, @"hidden"));
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM) && HAVE(WINDOW_CAPTURE)
