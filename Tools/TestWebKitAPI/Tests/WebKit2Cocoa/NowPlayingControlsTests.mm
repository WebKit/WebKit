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

#include "config.h"

#import "PlatformUtilities.h"
#import "TestWKWebViewMac.h"

#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>

#if WK_API_ENABLED && PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101201

@interface NowPlayingTestWebView : TestWKWebView
@property (nonatomic, readonly) BOOL hasActiveNowPlayingSession;
@property (readonly) NSString *lastUpdatedTitle;
@property (readonly) double lastUpdatedDuration;
@property (readonly) double lastUpdatedElapsedTime;
@end

@implementation NowPlayingTestWebView {
    bool _receivedNowPlayingInfoResponse;
    BOOL _hasActiveNowPlayingSession;
}
- (BOOL)hasActiveNowPlayingSession
{
    _receivedNowPlayingInfoResponse = false;
    [self _requestActiveNowPlayingSessionInfo];
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

- (void)_handleActiveNowPlayingSessionInfoResponse:(BOOL)hasActiveSession title:(NSString *)title duration:(double)duration elapsedTime:(double)elapsedTime
{
    _hasActiveNowPlayingSession = hasActiveSession;
    _lastUpdatedTitle = [title copy];
    _lastUpdatedDuration = duration;
    _lastUpdatedElapsedTime = elapsedTime;

    _receivedNowPlayingInfoResponse = true;
}
@end

namespace TestWebKitAPI {

TEST(NowPlayingControlsTests, NowPlayingControlsDoNotShowForForegroundPage)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    NowPlayingTestWebView *webView = [[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration];
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView.window setIsVisible:YES];
    [webView.window makeKeyWindow];
    [webView expectHasActiveNowPlayingSession:NO];

    ASSERT_STREQ("", webView.lastUpdatedTitle.UTF8String);
    ASSERT_TRUE(isnan(webView.lastUpdatedDuration));
    ASSERT_TRUE(isnan(webView.lastUpdatedElapsedTime));
}

TEST(NowPlayingControlsTests, NowPlayingControlsShowForBackgroundPage)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    NowPlayingTestWebView *webView = [[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration];
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView.window setIsVisible:NO];
    [webView.window resignKeyWindow];
    [webView expectHasActiveNowPlayingSession:YES];

    ASSERT_STREQ("foo", webView.lastUpdatedTitle.UTF8String);
    ASSERT_EQ(10, webView.lastUpdatedDuration);
    ASSERT_GE(webView.lastUpdatedElapsedTime, 0);
}

TEST(NowPlayingControlsTests, NowPlayingControlsHideAfterShowingClearsInfo)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    NowPlayingTestWebView *webView = [[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration];
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView.window setIsVisible:NO];
    [webView.window resignKeyWindow];

    [webView expectHasActiveNowPlayingSession:YES];

    [webView.window setIsVisible:YES];
    [webView.window makeKeyWindow];

    [webView expectHasActiveNowPlayingSession:NO];

    ASSERT_STREQ("", webView.lastUpdatedTitle.UTF8String);
    ASSERT_TRUE(isnan(webView.lastUpdatedDuration));
    ASSERT_TRUE(isnan(webView.lastUpdatedElapsedTime));
}

TEST(NowPlayingControlsTests, NowPlayingControlsClearInfoAfterSessionIsNoLongerValid)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    NowPlayingTestWebView *webView = [[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration];
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView mouseDownAtPoint:NSMakePoint(240, 160) simulatePressure:YES];
    [webView.window setIsVisible:NO];
    [webView.window resignKeyWindow];

    [webView expectHasActiveNowPlayingSession:NO];

    ASSERT_STREQ("", webView.lastUpdatedTitle.UTF8String);
    ASSERT_TRUE(isnan(webView.lastUpdatedDuration));
    ASSERT_TRUE(isnan(webView.lastUpdatedElapsedTime));
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101201
