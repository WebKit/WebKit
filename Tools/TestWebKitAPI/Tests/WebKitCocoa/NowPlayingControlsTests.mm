/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "config.h"

#import "PlatformUtilities.h"
#import "TestWKWebView.h"

#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>

#if WK_API_ENABLED && (PLATFORM(IOS_FAMILY) || __MAC_OS_X_VERSION_MAX_ALLOWED >= 101201)

@interface NowPlayingTestWebView : TestWKWebView
@property (nonatomic, readonly) BOOL hasActiveNowPlayingSession;
@property (nonatomic, readonly) BOOL registeredAsNowPlayingApplication;
@property (readonly) NSString *lastUpdatedTitle;
@property (readonly) double lastUpdatedDuration;
@property (readonly) double lastUpdatedElapsedTime;
@property (readonly) NSInteger lastUniqueIdentifier;
@end

@implementation NowPlayingTestWebView {
    bool _receivedNowPlayingInfoResponse;
    BOOL _hasActiveNowPlayingSession;
    BOOL _registeredAsNowPlayingApplication;
}
- (BOOL)hasActiveNowPlayingSession
{
    _receivedNowPlayingInfoResponse = false;

    auto completionHandler = [retainedSelf = retainPtr(self), self](BOOL active, BOOL registeredAsNowPlayingApplication, NSString *title, double duration, double elapsedTime, NSInteger uniqueIdentifier) {
        _hasActiveNowPlayingSession = active;
        _registeredAsNowPlayingApplication = registeredAsNowPlayingApplication;
        _lastUpdatedTitle = [title copy];
        _lastUpdatedDuration = duration;
        _lastUpdatedElapsedTime = elapsedTime;
        _lastUniqueIdentifier = uniqueIdentifier;

        _receivedNowPlayingInfoResponse = true;
    };

    [self _requestActiveNowPlayingSessionInfo:completionHandler];

    TestWebKitAPI::Util::run(&_receivedNowPlayingInfoResponse);

    return _hasActiveNowPlayingSession;
}

- (void)expectHasActiveNowPlayingSession:(BOOL)hasActiveNowPlayingSession
{
    bool finishedWaiting = false;
    while (!finishedWaiting) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        finishedWaiting = self.hasActiveNowPlayingSession == hasActiveNowPlayingSession;
    }
}

- (void)setWindowVisible:(BOOL)isVisible
{
#if PLATFORM(MAC)
    [self.window setIsVisible:isVisible];
#else
    self.window.hidden = !isVisible;
#endif
}
@end

namespace TestWebKitAPI {

#if PLATFORM(MAC)
TEST(NowPlayingControlsTests, NowPlayingControlsDoNotShowForForegroundPage)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView setWindowVisible:NO];
    [webView.get().window resignKeyWindow];
    [webView expectHasActiveNowPlayingSession:YES];

    [webView setWindowVisible:YES];
    [webView.get().window makeKeyWindow];
    [webView expectHasActiveNowPlayingSession:NO];

    ASSERT_STREQ("foo", webView.get().lastUpdatedTitle.UTF8String);
    ASSERT_EQ(10, webView.get().lastUpdatedDuration);
    ASSERT_GE(webView.get().lastUpdatedElapsedTime, 0);
}

TEST(NowPlayingControlsTests, NowPlayingControlsShowForBackgroundPage)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView setWindowVisible:NO];
    [webView.get().window resignKeyWindow];
    [webView expectHasActiveNowPlayingSession:YES];

    ASSERT_STREQ("foo", webView.get().lastUpdatedTitle.UTF8String);
    ASSERT_EQ(10, webView.get().lastUpdatedDuration);
    ASSERT_GE(webView.get().lastUpdatedElapsedTime, 0);
}

TEST(NowPlayingControlsTests, NowPlayingControlsHideAfterShowingKeepsSessionActive)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView setWindowVisible:NO];
    [webView.get().window resignKeyWindow];

    [webView expectHasActiveNowPlayingSession:YES];

    [webView setWindowVisible:YES];
    [webView.get().window makeKeyWindow];

    [webView expectHasActiveNowPlayingSession:NO];

    ASSERT_STREQ("foo", webView.get().lastUpdatedTitle.UTF8String);
    ASSERT_EQ(10, webView.get().lastUpdatedDuration);
    ASSERT_GE(webView.get().lastUpdatedElapsedTime, 0);
}

TEST(NowPlayingControlsTests, NowPlayingControlsClearInfoAfterSessionIsNoLongerValid)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration]);
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    BOOL initialHasActiveNowPlayingSession = webView.get().hasActiveNowPlayingSession;
    NSString *initialTitle = webView.get().lastUpdatedTitle;
    double initialDuration = webView.get().lastUpdatedDuration;
    double initialElapsedTime = webView.get().lastUpdatedElapsedTime;
    NSInteger initialUniqueIdentifier = webView.get().lastUniqueIdentifier;

    [webView stringByEvaluatingJavaScript:@"document.querySelector('video').muted = true"];
    [webView setWindowVisible:NO];
    [webView.get().window resignKeyWindow];

    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        if (initialHasActiveNowPlayingSession != webView.get().hasActiveNowPlayingSession)
            break;

        if (initialUniqueIdentifier != webView.get().lastUniqueIdentifier)
            break;

        if (initialDuration != webView.get().lastUpdatedDuration)
            break;

        if (initialElapsedTime != webView.get().lastUpdatedElapsedTime)
            break;

        if (![initialTitle isEqualToString:webView.get().lastUpdatedTitle])
            break;
    }

    ASSERT_STREQ("", webView.get().lastUpdatedTitle.UTF8String);
    ASSERT_TRUE(isnan(webView.get().lastUpdatedDuration));
    ASSERT_TRUE(isnan(webView.get().lastUpdatedElapsedTime));
    ASSERT_TRUE(!webView.get().lastUniqueIdentifier);
}

TEST(NowPlayingControlsTests, NowPlayingControlsCheckRegistered)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];
    [webView setWindowVisible:NO];
    [webView.get().window resignKeyWindow];

    [webView expectHasActiveNowPlayingSession:YES];
    ASSERT_TRUE(webView.get().registeredAsNowPlayingApplication);

    [webView stringByEvaluatingJavaScript:@"pause()"];
    [webView waitForMessage:@"paused"];
    [webView expectHasActiveNowPlayingSession:YES];
    ASSERT_TRUE(webView.get().registeredAsNowPlayingApplication);

    auto result = [webView stringByEvaluatingJavaScript:@"removeVideoElement()"];
    ASSERT_STREQ("<null>", result.UTF8String);
    [webView expectHasActiveNowPlayingSession:NO];
    ASSERT_FALSE(webView.get().registeredAsNowPlayingApplication);
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
// FIXME: Re-enable this test once <webkit.org/b/175204> is resolved.
TEST(NowPlayingControlsTests, DISABLED_NowPlayingControlsIOS)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration]);
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView expectHasActiveNowPlayingSession:YES];
    ASSERT_STREQ("foo", webView.get().lastUpdatedTitle.UTF8String);
    ASSERT_EQ(10, webView.get().lastUpdatedDuration);
    ASSERT_GE(webView.get().lastUpdatedElapsedTime, 0);
}
#endif

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && (PLATFORM(IOS_FAMILY) || __MAC_OS_X_VERSION_MAX_ALLOWED >= 101201)
