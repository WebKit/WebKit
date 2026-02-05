/*
 * Copyright (C) 2016-2026 Apple Inc. All rights reserved.
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
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

@interface NowPlayingTestWebView : TestWKWebView
@property (nonatomic, readonly) BOOL hasActiveNowPlayingSession;
@property (nonatomic, readonly) BOOL registeredAsNowPlayingApplication;
@property (readonly) NSString *lastUpdatedTitle;
@property (readonly) double lastUpdatedDuration;
@property (readonly) double lastUpdatedElapsedTime;
@property (readonly) NSInteger lastUniqueIdentifier;
@property (readonly) NSUInteger lastUpdateTime;
@end

@implementation NowPlayingTestWebView {
    bool _receivedNowPlayingInfoResponse;
    BOOL _hasActiveNowPlayingSession;
    BOOL _registeredAsNowPlayingApplication;
}
- (void)requestActiveNowPlayingSessionInfo
{
    _receivedNowPlayingInfoResponse = false;

    auto completionHandler = [retainedSelf = retainPtr(self), self](BOOL active, BOOL registeredAsNowPlayingApplication, NSString *title, double duration, double elapsedTime, NSInteger uniqueIdentifier, NSUInteger updateTime) {
        _hasActiveNowPlayingSession = active;
        _registeredAsNowPlayingApplication = registeredAsNowPlayingApplication;
        _lastUpdatedTitle = [title copy];
        _lastUpdatedDuration = duration;
        _lastUpdatedElapsedTime = elapsedTime;
        _lastUniqueIdentifier = uniqueIdentifier;
        _lastUpdateTime = updateTime;
        _receivedNowPlayingInfoResponse = true;
    };

    [self _requestActiveNowPlayingSessionInfo:completionHandler];

    TestWebKitAPI::Util::run(&_receivedNowPlayingInfoResponse);
}

- (void)expectHasActiveNowPlayingSession:(BOOL)hasActiveNowPlayingSession
{
    [self requestActiveNowPlayingSessionInfo];

    bool finishedWaiting = false;
    while (!finishedWaiting) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        finishedWaiting = self.hasActiveNowPlayingSession == hasActiveNowPlayingSession;
    }
}

- (void)expectRegisteredAsNowPlayingApplication:(BOOL)registeredAsNowPlayingApplication
{
    [self requestActiveNowPlayingSessionInfo];

    bool finishedWaiting = false;
    while (!finishedWaiting) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        finishedWaiting = self.registeredAsNowPlayingApplication == registeredAsNowPlayingApplication;
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

#if PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
TEST(NowPlayingControlsTests, NowPlayingControlsDoNotShowForForegroundPage)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
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
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView setWindowVisible:NO];
    [webView.get().window resignKeyWindow];
    [webView expectHasActiveNowPlayingSession:YES];

    ASSERT_STREQ("foo", webView.get().lastUpdatedTitle.UTF8String);
    ASSERT_EQ(10, webView.get().lastUpdatedDuration);
    ASSERT_GE(webView.get().lastUpdatedElapsedTime, 0);
}

#if ENABLE(REQUIRES_PAGE_VISIBILITY_FOR_NOW_PLAYING)
TEST(NowPlayingControlsTests, NowPlayingApplicationNotRegisteredForBackgroundPage)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    [configuration preferences]._requiresPageVisibilityForVideoToBeNowPlayingForTesting = YES;
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView setWindowVisible:NO];
    [webView.get().window resignKeyWindow];

    [webView expectRegisteredAsNowPlayingApplication:NO];

    [webView setWindowVisible:YES];
    [webView.get().window makeKeyWindow];

    [webView expectRegisteredAsNowPlayingApplication:YES];

    ASSERT_STREQ("foo", webView.get().lastUpdatedTitle.UTF8String);
    ASSERT_EQ(10, webView.get().lastUpdatedDuration);
    ASSERT_GE(webView.get().lastUpdatedElapsedTime, 0);
}
#endif // ENABLE(REQUIRES_PAGE_VISIBILITY_FOR_NOW_PLAYING)

TEST(NowPlayingControlsTests, NowPlayingControlsHideAfterShowingKeepsSessionActive)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
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
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration.get()]);
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
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
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

// FIXME: Re-enable this test once <webkit.org/b/175204> is resolved.
TEST(NowPlayingControlsTests, DISABLED_NowPlayingControlsIOS)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration.get()]);
    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    [webView expectHasActiveNowPlayingSession:YES];
    ASSERT_STREQ("foo", webView.get().lastUpdatedTitle.UTF8String);
    ASSERT_EQ(10, webView.get().lastUpdatedDuration);
    ASSERT_GE(webView.get().lastUpdatedElapsedTime, 0);
}
#endif

TEST(NowPlayingControlsTests, LazyRegisterAsNowPlayingApplication)
{
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES]);
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<body>Hello world</body>"];

    auto haveMediaSessionManager = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.hasMediaSessionManager"].boolValue;
    };

    [webView expectRegisteredAsNowPlayingApplication:NO];
    ASSERT_FALSE(haveMediaSessionManager());

    [webView setWindowVisible:NO];
    [webView expectRegisteredAsNowPlayingApplication:NO];
    ASSERT_FALSE(haveMediaSessionManager());

    [webView setWindowVisible:YES];
    [webView expectRegisteredAsNowPlayingApplication:NO];
    ASSERT_FALSE(haveMediaSessionManager());
}

TEST(NowPlayingControlsTests, DISABLED_NowPlayingUpdatesThrottled)
{
    struct NowPlayingState {
        NowPlayingState(NowPlayingTestWebView *webView)
        {
            [webView requestActiveNowPlayingSessionInfo];
            hasActiveNowPlayingSession = [webView hasActiveNowPlayingSession];
            registeredAsNowPlayingApplication = [webView registeredAsNowPlayingApplication];
            title = [webView lastUpdatedTitle];
            duration = [webView lastUpdatedDuration];
            elapsedTime = [webView lastUpdatedElapsedTime];
            uniqueIdentifier = [webView lastUniqueIdentifier];
            updateTime = [webView lastUpdateTime];
        }
        NowPlayingState() = default;

        bool operator==(const NowPlayingState&) const = default;

        bool hasActiveNowPlayingSession { false };
        bool registeredAsNowPlayingApplication { false };
        String title;
        double duration { 0 };
        double elapsedTime { 0 };
        long uniqueIdentifier { 0 };
        unsigned long updateTime { 0 };
    };

    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    auto webView = adoptNS([[NowPlayingTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration.get()]);

    constexpr double internalTestTimout = 20;
    auto waitForMessageOrTimeout = [&] (const char* eventName) -> bool {
        __block bool receivedEvent = false;
        [webView performAfterReceivingMessage:[NSString stringWithUTF8String:eventName] action:^{ receivedEvent = true; }];

        NSDate *startTime = [NSDate date];
        while (!receivedEvent && [[NSDate date] timeIntervalSinceDate:startTime] < internalTestTimout)
            TestWebKitAPI::Util::runFor(0.05_s);

        return receivedEvent;
    };

    [webView loadTestPageNamed:@"small-video-test-now-playing"];
    ASSERT_TRUE(waitForMessageOrTimeout("ready"));

    auto videoError = [webView stringByEvaluatingJavaScript:@"videoError()"];
    ASSERT_STREQ("null", videoError.UTF8String);

    constexpr double initialTime = 3;
    NSError *error;
    id result = [webView objectByCallingAsyncFunction:[NSString stringWithFormat:@"play(%f)", initialTime] withArguments:nil error:&error];
    EXPECT_NULL(error);
    EXPECT_EQ(result, nil);
    ASSERT_TRUE(waitForMessageOrTimeout("playing"));

    videoError = [webView stringByEvaluatingJavaScript:@"videoError()"];
    ASSERT_STREQ("null", videoError.UTF8String);

    constexpr double impossibleUpdateInterval = 10.;
    [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"window.internals.nowPlayingUpdateInterval = %f", impossibleUpdateInterval]];
    auto actualInterval = [[webView stringByEvaluatingJavaScript:@"window.internals.nowPlayingUpdateInterval"] doubleValue];
    EXPECT_LT(actualInterval, impossibleUpdateInterval);

    constexpr double updateInterval = 0.5;
    [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"window.internals.nowPlayingUpdateInterval = %f", updateInterval]];
    actualInterval = [[webView stringByEvaluatingJavaScript:@"window.internals.nowPlayingUpdateInterval"] doubleValue];
    EXPECT_EQ(updateInterval, actualInterval);

    NowPlayingState initialState;
    NowPlayingState previousState;
    NSDate *startTime = [NSDate date];
    bool videoLooped = false;
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {

        if ([[NSDate date] timeIntervalSinceDate:startTime] > internalTestTimout)
            break;

        NowPlayingState currentState(webView.get());
        if (!initialState.elapsedTime) {
            initialState = currentState;
            previousState = initialState;
            continue;
        }

        if (currentState.elapsedTime && currentState.elapsedTime < initialTime) {
            videoLooped = true;
            break;
        }

        if (previousState.updateTime == currentState.updateTime) {
            ASSERT_TRUE(previousState == currentState);
            continue;
        }

        auto stateOtherThanUpdateTimeHasChanged = [&] {
            if (initialState.hasActiveNowPlayingSession != currentState.hasActiveNowPlayingSession)
                return true;
            if (initialState.registeredAsNowPlayingApplication != currentState.registeredAsNowPlayingApplication)
                return true;
            if (initialState.title != currentState.title)
                return true;
            if (initialState.duration != currentState.duration)
                return true;
            if (initialState.elapsedTime != currentState.elapsedTime)
                return true;
            if (initialState.uniqueIdentifier != currentState.uniqueIdentifier)
                return true;

            return false;
        };
        ASSERT_TRUE(stateOtherThanUpdateTimeHasChanged());
        previousState = currentState;
    }

    ASSERT_TRUE(videoLooped);

    [webView stringByEvaluatingJavaScript:@"pause()"];
}

} // namespace TestWebKitAPI
