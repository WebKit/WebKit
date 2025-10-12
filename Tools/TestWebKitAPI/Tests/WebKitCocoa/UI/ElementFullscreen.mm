/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKFullscreenDelegate.h>

#if PLATFORM(IOS_FAMILY)
@interface UIScrollView ()
@property (nonatomic, getter=isZoomEnabled) BOOL zoomEnabled;
@end
#endif

@interface TestElementFullscreenDelegate : NSObject <_WKFullscreenDelegate>
- (void)waitForDidEnterElementFullscreen;
- (void)waitForWillEnterElementFullscreen;
@end

@implementation TestElementFullscreenDelegate {
    bool _didEnterElementFullscreen;
    bool _willEnterElementFullscreen;
}

- (void)waitForDidEnterElementFullscreen
{
    _didEnterElementFullscreen = false;
    TestWebKitAPI::Util::run(&_didEnterElementFullscreen);
}

- (void)waitForWillEnterElementFullscreen
{
    _willEnterElementFullscreen = false;
    TestWebKitAPI::Util::run(&_willEnterElementFullscreen);
}

#pragma mark WKUIDelegate

#if PLATFORM(IOS)
- (void)_webViewWillEnterElementFullscreen:(WKWebView *)webView
#else
- (void)_webViewWillEnterFullscreen:(NSView *)webView
#endif
{
    _willEnterElementFullscreen = true;
}

#if PLATFORM(IOS_FAMILY)
- (void)_webViewDidEnterElementFullscreen:(WKWebView *)webView
#else
- (void)_webViewDidEnterFullscreen:(NSView *)webView
#endif
{
    _didEnterElementFullscreen = true;
}

@end

#if PLATFORM(IOS_FAMILY)
TEST(ElementFullscreen, ScrollViewSetToInitialScale)
{
    // This test must run in TestWebKitAPI.app
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setAllowsInlineMediaPlayback:YES];
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr fullscreenDelegate = adoptNS([[TestElementFullscreenDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setFullscreenDelegate:fullscreenDelegate.get()];

    RetainPtr preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences setPreferredContentMode:WKContentModeDesktop];
    [webView synchronouslyLoadHTMLString:@"<meta name=viewport content='width=1000'>" preferences:preferences.get()];

    [webView evaluateJavaScript:@"document.querySelector('body').requestFullscreen()" completionHandler:nil];
    [fullscreenDelegate waitForDidEnterElementFullscreen];
    [webView waitForNextPresentationUpdate];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: <rdar://155548417> ([ Build-Failure ] [ iOS26+ ] error: 'mainScreen' is deprecated: first deprecated in iOS 26.0)
    CGFloat expectedScale = std::min<CGFloat>(UIScreen.mainScreen.bounds.size.width / 1000, 1);
ALLOW_DEPRECATED_DECLARATIONS_END

    EXPECT_EQ([webView scrollView].zoomScale, expectedScale);
    EXPECT_EQ([webView scrollView].minimumZoomScale, expectedScale);
    EXPECT_EQ([webView scrollView].maximumZoomScale, expectedScale);
    EXPECT_EQ([webView scrollView].isZoomEnabled, NO);
}
#endif

TEST(ElementFullscreen, EnterVideoFullscreenWhileAnimating)
{
    // This test must run in TestWebKitAPI.app
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
#if PLATFORM(IOS_FAMILY)
    [configuration setAllowsInlineMediaPlayback:YES];
#endif
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr fullscreenDelegate = adoptNS([[TestElementFullscreenDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setFullscreenDelegate:fullscreenDelegate.get()];

    RetainPtr preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences setPreferredContentMode:WKContentModeDesktop];

    NSString *htmlContent = @"<video src='video-with-audio.mp4' controls></video>";

    [webView synchronouslyLoadHTMLString:htmlContent preferences:preferences.get()];

    // Start element fullscreen animation
    [webView evaluateJavaScript:@"document.querySelector('body').requestFullscreen()" completionHandler:nil];

    // Wait for the fullscreen animation to begin
    [fullscreenDelegate waitForWillEnterElementFullscreen];

    // While the element fullscreen animation is in progress, try to enter video fullscreen
    [webView objectByEvaluatingJavaScript:@"document.getElementById('video').webkitEnterFullscreen()"];

    // Wait for the element fullscreen animation to complete
    [fullscreenDelegate waitForDidEnterElementFullscreen];

    sleep(0.1_s);

    // Verify that the video element is NOT in fullscreen mode
    NSNumber *isVideoFullscreen = [webView objectByEvaluatingJavaScript:@"document.getElementById('video').webkitDisplayingFullscreen"];
    EXPECT_FALSE([isVideoFullscreen boolValue]);
}
