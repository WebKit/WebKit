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

#if !PLATFORM(IOS_FAMILY_SIMULATOR) && ENABLE(FULLSCREEN_API)

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>

static bool didEnterFullscreen;
static bool didExitFullscreen;

@interface ExitFullscreenOnEnterPiPUIDelegate : NSObject <WKUIDelegate, _WKFullscreenDelegate>
@end

@implementation ExitFullscreenOnEnterPiPUIDelegate

- (void)_webView:(WKWebView *)webView hasVideoInPictureInPictureDidChange:(BOOL)value
{
    if (value)
        didEnterPiP = true;
    else
        didExitPiP = true;
}

- (void)_webViewDidEnterFullscreen:(WKWebView *)webView
{
    didEnterFullscreen = true;
}

- (void)_webViewDidExitFullscreen:(WKWebView *)webView
{
    didExitFullscreen = true;
}
@end

namespace TestWebKitAPI {

TEST(ExitFullscreenOnEnterPiP, VideoFullscreen)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    [configuration preferences]._allowsPictureInPictureMediaPlayback = YES;
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    RetainPtr<ExitFullscreenOnEnterPiPUIDelegate> handler = adoptNS([[ExitFullscreenOnEnterPiPUIDelegate alloc] init]);
    [webView setUIDelegate:handler.get()];
    [webView _setFullscreenDelegate:handler.get()];

    [webView synchronouslyLoadTestPageNamed:@"ExitFullscreenOnEnterPiP"];

    didEnterFullscreen = false;
    [webView evaluateJavaScript:@"document.getElementById('enter-video-fullscreen').click()" completionHandler: nil];
    TestWebKitAPI::Util::run(&didEnterFullscreen);
    ASSERT_TRUE(didEnterFullscreen);

    didEnterPiP = false;
    didExitFullscreen = false;
    [webView evaluateJavaScript:@"document.getElementById('enter-pip').click()" completionHandler: nil];
    TestWebKitAPI::Util::run(&didEnterPiP);
    TestWebKitAPI::Util::run(&didExitFullscreen);

    sleep(1_s); // Wait for PIPAgent to launch, or it won't call -pipDidClose: callback.

    [webView evaluateJavaScript:@"document.getElementById('exit-pip').click()" completionHandler: nil];
    TestWebKitAPI::Util::run(&didExitPiP);
}

// FIXME: Re-enable this test for Big Sur once webkit.org/b/245241 is resolved
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 120000)
TEST(ExitFullscreenOnEnterPiP, DISABLED_ElementFullscreen)
#else
TEST(ExitFullscreenOnEnterPiP, ElementFullscreen)
#endif
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        @"WebCoreLogging": @"Fullscreen=debug",
        @"WebKit2Logging": @"Fullscreen=debug",
    }];
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    [configuration preferences]._allowsPictureInPictureMediaPlayback = YES;
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    RetainPtr<ExitFullscreenOnEnterPiPUIDelegate> handler = adoptNS([[ExitFullscreenOnEnterPiPUIDelegate alloc] init]);
    [webView setUIDelegate:handler.get()];
    [webView _setFullscreenDelegate:handler.get()];

    [webView synchronouslyLoadTestPageNamed:@"ExitFullscreenOnEnterPiP"];

    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        @"WebCoreLogging": @"",
        @"WebKit2Logging": @"",
    }];

    didEnterFullscreen = false;
    [webView evaluateJavaScript:@"document.getElementById('enter-element-fullscreen').click()" completionHandler: nil];
    ASSERT_TRUE(TestWebKitAPI::Util::runFor(&didEnterFullscreen, 10_s));
    ASSERT_TRUE(didEnterFullscreen);

    // Make the video the "main content" by playing with a user gesture.
    __block bool didBeginPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ didBeginPlaying = true; }];
    [webView evaluateJavaScript:@"document.getElementById('play').click()" completionHandler:nil];
    ASSERT_TRUE(TestWebKitAPI::Util::runFor(&didBeginPlaying, 10_s));

    didEnterPiP = false;
    didExitFullscreen = false;
    [webView evaluateJavaScript:@"document.getElementById('enter-pip').click()" completionHandler: nil];
    ASSERT_TRUE(TestWebKitAPI::Util::runFor(&didEnterPiP, 10_s));
    ASSERT_TRUE(TestWebKitAPI::Util::runFor(&didExitFullscreen, 10_s));

    sleep(1_s); // Wait for PIPAgent to launch, or it won't call -pipDidClose: callback.

    [webView evaluateJavaScript:@"document.getElementById('exit-pip').click()" completionHandler: nil];
    ASSERT_TRUE(TestWebKitAPI::Util::runFor(&didExitPiP, 10_s));

}

} // namespace TestWebKitAPI

#endif
