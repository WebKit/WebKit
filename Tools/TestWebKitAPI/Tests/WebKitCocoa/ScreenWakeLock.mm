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
#import "ScreenWakeLock.h"

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"

#if PLATFORM(IOS_FAMILY)
static bool gShouldKeepScreenAwake = false;

@interface SetShouldKeepScreenAwakeDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation SetShouldKeepScreenAwakeDelegate

- (void)_webView:(WKWebView *)webView setShouldKeepScreenAwake:(BOOL)shouldKeepScreenAwake
{
    EXPECT_NE(gShouldKeepScreenAwake, shouldKeepScreenAwake);
    gShouldKeepScreenAwake = shouldKeepScreenAwake;
}

@end
#endif // PLATFORM(IOS_FAMILY)

TEST_P(ScreenWakeLockTests, SetShouldKeepScreenAwake)
{
    requestLock();
    releaseLock();
    lockShouldBeReleased();
}

TEST_P(ScreenWakeLockTests, SetShouldKeepScreenAwakeTabIsClosed)
{
    requestLock();
    closeTab();
    lockShouldBeReleased();
}

TEST_P(ScreenWakeLockTests, SetShouldKeepScreenAwakeLastPageIsClosed)
{
    requestLock();
    requestLock();
    closeWebPage();
    lockShouldBeReleased();
}

TEST_P(ScreenWakeLockTests, SetShouldKeepScreenAwakeWebProcessCrash)
{
    requestLock();
    requestLock();
    killWebPage();
    lockShouldBeReleased();
}

void ScreenWakeLockTests::SetUp()
{
    if (GetParam().crossSite == ScreenWakeLockTestsConfig::CrossSite::Yes)
        setUpCrossSite();
    else
        setUpSameSite();

#if PLATFORM(IOS_FAMILY)
    m_uiDelegate = adoptNS([SetShouldKeepScreenAwakeDelegate new]);
    [m_webView setUIDelegate:m_uiDelegate.get()];
#endif
}

void ScreenWakeLockTests::requestLock()
{
    __block bool done = false;
    [m_webView callAsyncJavaScript:@"lock = await navigator.wakeLock.request('screen');" arguments:nil inFrame:frameOfConcern() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *err) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    lockShouldNotBeReleased();
}

void ScreenWakeLockTests::releaseLock()
{
    __block bool done = false;
    [m_webView callAsyncJavaScript:@"return await lock.release()" arguments:nil inFrame:frameOfConcern() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *err) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

void ScreenWakeLockTests::killWebPage()
{
    kill([m_webView _webProcessIdentifier], SIGKILL);
    m_webView = nil;
}

void ScreenWakeLockTests::closeWebPage()
{
    [m_webView _close];
    m_webView = nil;
}

void ScreenWakeLockTests::closeTab()
{
    [m_webView removeFromSuperview];
    [m_webView waitUntilActivityStateUpdateDone];
}

void ScreenWakeLockTests::lockShouldBeReleased()
{
#if PLATFORM(IOS_FAMILY)
    while (gShouldKeepScreenAwake)
        TestWebKitAPI::Util::spinRunLoop(10);
#endif // PLATFORM(IOS_FAMILY)

    if (m_webView)
        EXPECT_TRUE([[m_webView objectByEvaluatingJavaScript:@"window.lock.released" inFrame:frameOfConcern()] boolValue]);
}

void ScreenWakeLockTests::lockShouldNotBeReleased()
{
#if PLATFORM(IOS_FAMILY)
    EXPECT_TRUE(gShouldKeepScreenAwake);
#endif // PLATFORM(IOS_FAMILY)

    ASSERT_TRUE(m_webView);
    EXPECT_FALSE([[m_webView objectByEvaluatingJavaScript:@"window.lock.released" inFrame:frameOfConcern()] boolValue]);
}

void ScreenWakeLockTests::setUpCrossSite()
{
    ASCIILiteral mainFrameHTML = "<iframe src='https://examplesubframe.com/subframe' allow='screen-wake-lock' id='subFrame'></iframe>";
    ASCIILiteral subFrameHTML = "<script> alert('frame loaded'); </script>";
    TestWebKitAPI::HTTPServer server({
        { "/mainframe"_s, { mainFrameHTML } },
        { "/subframe"_s, { subFrameHTML } } },
        TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    auto configuration = server.httpsProxyConfiguration();
    m_navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [m_navigationDelegate allowAnyTLSCertificate];
    m_webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration addToWindow:YES]);
    [m_webView setNavigationDelegate:m_navigationDelegate.get()];
    [m_webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://examplemainframe.com/mainframe"]]];
    EXPECT_WK_STREQ([m_webView _test_waitForAlert], "frame loaded");
}

void ScreenWakeLockTests::setUpSameSite()
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    m_webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    [m_webView synchronouslyLoadHTMLString:@"<body></body>"];
}

WKFrameInfo *ScreenWakeLockTests::frameOfConcern()
{
    return GetParam().crossSite == ScreenWakeLockTestsConfig::CrossSite::Yes ? [m_webView firstChildFrame] : nil;
}
