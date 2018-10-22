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
#import "TestWKWebView.h"

#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED && PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101201

@interface WKWebView (WKWebViewAdditions)

- (void)_interactWithMediaControlsForTesting;

@end

@interface VideoControlsManagerTestWebView : TestWKWebView
@end

@implementation VideoControlsManagerTestWebView {
    bool _isDoneQueryingControlledElementID;
    NSString *_controlledElementID;
}

- (void)expectControlsManager:(BOOL)expectControlsManager afterReceivingMessage:(NSString *)message
{
    __block bool doneWaiting = false;
    [self performAfterReceivingMessage:message action:^ {
        BOOL hasVideoForControlsManager = [self _hasActiveVideoForControlsManager];
        if (expectControlsManager)
            EXPECT_TRUE(hasVideoForControlsManager);
        else
            EXPECT_FALSE(hasVideoForControlsManager);
        doneWaiting = true;
    }];

    TestWebKitAPI::Util::run(&doneWaiting);
}

- (void)waitForMediaControlsToShow
{
    while (![self _hasActiveVideoForControlsManager])
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
}

- (void)waitForMediaControlsToHide
{
    while ([self _hasActiveVideoForControlsManager])
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
}

- (void)waitForPageToLoadWithAutoplayingVideos:(int)numberOfAutoplayingVideos
{
    __block int remainingAutoplayedCount = numberOfAutoplayingVideos;
    __block bool autoplayingIsFinished = !numberOfAutoplayingVideos;
    __block bool pageHasLoaded = false;

    [self performAfterLoading:^()
    {
        pageHasLoaded = true;
    }];

    if (numberOfAutoplayingVideos) {
        [self performAfterReceivingMessage:@"autoplayed" action:^()
        {
            remainingAutoplayedCount--;
            if (remainingAutoplayedCount <= 0)
                autoplayingIsFinished = true;
        }];
    }

    TestWebKitAPI::Util::run(&pageHasLoaded);
    TestWebKitAPI::Util::run(&autoplayingIsFinished);
}

- (NSString *)controlledElementID
{
    _isDoneQueryingControlledElementID = false;
    [self _requestControlledElementID];
    TestWebKitAPI::Util::run(&_isDoneQueryingControlledElementID);
    return _controlledElementID;
}

- (void)_handleControlledElementIDResponse:(NSString *)identifier
{
    _controlledElementID = [identifier copy];
    _isDoneQueryingControlledElementID = true;
}

@end

namespace TestWebKitAPI {

RetainPtr<VideoControlsManagerTestWebView*> setUpWebViewForTestingVideoControlsManager(NSRect frame)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    return adoptNS([[VideoControlsManagerTestWebView alloc] initWithFrame:frame configuration:configuration.get()]);
}

TEST(VideoControlsManager, VideoControlsManagerSingleLargeVideo)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    // A large video with audio should have a controls manager even if it is played via script like this video.
    // So the expectation is YES.
    [webView loadTestPageNamed:@"large-video-with-audio"];
    [webView waitForMediaControlsToShow];
}

TEST(VideoControlsManager, VideoControlsManagerSingleSmallVideo)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    // A small video will not have a controls manager unless it started playing because of a user gesture. Since this
    // video is started with a script, the expectation is NO.
    [webView loadTestPageNamed:@"video-with-audio"];
    [webView expectControlsManager:NO afterReceivingMessage:@"playing"];
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosWithAudio)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    [webView loadTestPageNamed:@"large-videos-with-audio"];
    [webView waitForPageToLoadWithAutoplayingVideos:0];

    EXPECT_FALSE([webView _hasActiveVideoForControlsManager]);
}

// FIXME: Re-enable this test once <webkit.org/b/175143> is resolved.
TEST(VideoControlsManager, DISABLED_VideoControlsManagerMultipleVideosWithAudioAndAutoplay)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    [webView loadTestPageNamed:@"large-videos-with-audio-autoplay"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];

    EXPECT_TRUE([webView _hasActiveVideoForControlsManager]);
    EXPECT_TRUE([[webView controlledElementID] isEqualToString:@"bar"]);
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosScrollPausedVideoOutOfView)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 500, 500));

    [webView loadTestPageNamed:@"large-videos-paused-video-hides-controls"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];

    [webView stringByEvaluatingJavaScript:@"pauseFirstVideoAndScrollToSecondVideo()"];
    [webView expectControlsManager:NO afterReceivingMessage:@"paused"];
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosScrollPlayingVideoWithSoundOutOfView)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 500, 500));

    [webView loadTestPageNamed:@"large-videos-playing-video-keeps-controls"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];

    [webView stringByEvaluatingJavaScript:@"scrollToSecondVideo()"];
    [webView expectControlsManager:YES afterReceivingMessage:@"scrolled"];
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosScrollPlayingMutedVideoOutOfView)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 500, 500));

    [webView loadTestPageNamed:@"large-videos-playing-muted-video-hides-controls"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];

    [webView stringByEvaluatingJavaScript:@"muteFirstVideoAndScrollToSecondVideo()"];
    [webView expectControlsManager:NO afterReceivingMessage:@"playing"];
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosShowControlsForLastInteractedVideo)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 800, 600));
    NSPoint clickPoint = NSMakePoint(400, 300);

    [webView loadTestPageNamed:@"large-videos-autoplaying-click-to-pause"];
    [webView waitForPageToLoadWithAutoplayingVideos:2];

    [webView mouseDownAtPoint:clickPoint simulatePressure:YES];

    __block bool firstVideoPaused = false;
    __block bool secondVideoPaused = false;
    [webView performAfterReceivingMessage:@"paused" action:^ {
        NSString *controlledElementID = [webView controlledElementID];
        if (firstVideoPaused) {
            EXPECT_TRUE([controlledElementID isEqualToString:@"second"]);
            secondVideoPaused = true;
        } else {
            EXPECT_TRUE([controlledElementID isEqualToString:@"first"]);
            [webView mouseDownAtPoint:clickPoint simulatePressure:YES];
        }
        firstVideoPaused = true;
    }];

    TestWebKitAPI::Util::run(&secondVideoPaused);
}
// FIXME: Re-enable this test once <webkit.org/b/175909> is resolved.
TEST(VideoControlsManager, DISABLED_VideoControlsManagerMultipleVideosSwitchControlledVideoWhenScrolling)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 800, 600));

    [webView loadTestPageNamed:@"large-videos-autoplaying-scroll-to-video"];
    [webView waitForPageToLoadWithAutoplayingVideos:2];

    [webView stringByEvaluatingJavaScript:@"scrollToSecondView()"];
    [webView expectControlsManager:YES afterReceivingMessage:@"scrolled"];

    EXPECT_TRUE([[webView controlledElementID] isEqualToString:@"second"]);
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosScrollOnlyLargeVideoOutOfView)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 500, 500));

    [webView loadTestPageNamed:@"large-video-playing-scroll-away"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];
    [webView stringByEvaluatingJavaScript:@"scrollVideoOutOfView()"];
    [webView expectControlsManager:YES afterReceivingMessage:@"scrolled"];
}

TEST(VideoControlsManager, VideoControlsManagerSingleSmallAutoplayingVideo)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    [webView loadTestPageNamed:@"autoplaying-video-with-audio"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];

    [webView mouseDownAtPoint:NSMakePoint(50, 50) simulatePressure:YES];
    [webView expectControlsManager:YES afterReceivingMessage:@"paused"];
}

TEST(VideoControlsManager, VideoControlsManagerLargeAutoplayingVideoSeeksAfterEnding)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    [webView loadTestPageNamed:@"large-video-seek-after-ending"];

    // Immediately after ending, the controls should still be present.
    [webView expectControlsManager:YES afterReceivingMessage:@"ended"];

    // At some point in the future, they should automatically hide.
    [webView waitForMediaControlsToHide];
}

TEST(VideoControlsManager, VideoControlsManagerLargeAutoplayingVideoSeeksAndPlaysAfterEnding)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    // Since the video is still playing, the expectation is YES even if the video has ended once.
    [webView loadTestPageNamed:@"large-video-seek-to-beginning-and-play-after-ending"];
    [webView expectControlsManager:YES afterReceivingMessage:@"replaying"];
}

TEST(VideoControlsManager, VideoControlsManagerLargeAutoplayingVideoAfterSeekingToEnd)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    [webView loadTestPageNamed:@"large-video-hides-controls-after-seek-to-end"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];
    [webView mouseDownAtPoint:NSMakePoint(50, 50) simulatePressure:YES];

    // We expect there to be media controls, since this is a user gestured seek to the end.
    // This is akin to seeking to the end by scrubbing in the controls.
    [webView expectControlsManager:YES afterReceivingMessage:@"ended"];
}

TEST(VideoControlsManager, VideoControlsManagerSingleLargeVideoWithoutAudio)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    // A large video with no audio will not have a controls manager unless it started playing because of a user gesture. Since this
    // video is started with a script, the expectation is NO.
    [webView loadTestPageNamed:@"large-video-without-audio"];
    [webView expectControlsManager:NO afterReceivingMessage:@"playing"];
}

TEST(VideoControlsManager, VideoControlsManagerAudioElementStartedWithScript)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    // An audio element MUST be started with a user gesture in order to have a controls manager, so the expectation is NO.
    [webView loadTestPageNamed:@"audio-only"];
    [webView expectControlsManager:NO afterReceivingMessage:@"playing"];
}

TEST(VideoControlsManager, VideoControlsManagerAudioElementStartedByInteraction)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 400, 400));

    [webView loadTestPageNamed:@"play-audio-on-click"];
    [webView waitForPageToLoadWithAutoplayingVideos:0];
    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:YES];

    // An audio element MUST be started with a user gesture in order to have a controls manager, so the expectation is YES.
    [webView expectControlsManager:YES afterReceivingMessage:@"playing-first"];
    EXPECT_TRUE([[webView controlledElementID] isEqualToString:@"first"]);
}

TEST(VideoControlsManager, DISABLED_VideoControlsManagerAudioElementFollowingUserInteraction)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 400, 400));

    [webView loadTestPageNamed:@"play-audio-on-click"];
    [webView waitForPageToLoadWithAutoplayingVideos:0];

    [webView performAfterReceivingMessage:@"playing-first" action:^ {
        [webView evaluateJavaScript:@"seekToEnd()" completionHandler:nil];
    }];

    __block bool secondAudioPlaying = false;
    [webView performAfterReceivingMessage:@"playing-second" action:^ {
        secondAudioPlaying = true;
    }];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:YES];

    TestWebKitAPI::Util::run(&secondAudioPlaying);
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        if ([webView _hasActiveVideoForControlsManager] && [[webView controlledElementID] isEqualToString:@"second"])
            break;
    }
}

TEST(VideoControlsManager, VideoControlsManagerTearsDownMediaControlsOnDealloc)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    NSURL *urlOfVideo = [[NSBundle mainBundle] URLForResource:@"video-with-audio" withExtension:@"mp4" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadFileURL:urlOfVideo allowingReadAccessToURL:[urlOfVideo URLByDeletingLastPathComponent]];

    __block bool finishedTest = false;
    [webView performAfterLoading:^ {
        // Verify that we tear down the media controls properly, such that we don't crash when the web view is released.
        if ([webView respondsToSelector:@selector(_interactWithMediaControlsForTesting)])
            [webView _interactWithMediaControlsForTesting];

        [webView release];
        finishedTest = true;
    }];

    TestWebKitAPI::Util::run(&finishedTest);
}

TEST(VideoControlsManager, VideoControlsManagerDoesNotShowMediaControlsForOffscreenVideo)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 1024, 768));

    [webView loadTestPageNamed:@"large-video-offscreen"];
    [webView waitForMediaControlsToHide];
}

TEST(VideoControlsManager, VideoControlsManagerKeepsControlsStableDuringSrcChangeOnClick)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 800, 600));

    [webView loadTestPageNamed:@"change-video-source-on-click"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];
    [webView mouseDownAtPoint:NSMakePoint(400, 300) simulatePressure:YES];

    [webView expectControlsManager:YES afterReceivingMessage:@"changed"];
}

TEST(VideoControlsManager, VideoControlsManagerKeepsControlsStableDuringSrcChangeOnEnd)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 800, 600));

    [webView loadTestPageNamed:@"change-video-source-on-end"];
    [webView expectControlsManager:YES afterReceivingMessage:@"changed"];
}

TEST(VideoControlsManager, VideoControlsManagerSmallVideoInMediaDocument)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    __block bool finishedLoad = false;
    [webView performAfterLoading:^ {
        finishedLoad = true;
    }];
    
    NSURL *urlOfVideo = [[NSBundle mainBundle] URLForResource:@"video-with-audio" withExtension:@"mp4" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadFileURL:urlOfVideo allowingReadAccessToURL:[urlOfVideo URLByDeletingLastPathComponent]];
    
    TestWebKitAPI::Util::run(&finishedLoad);
    
    // We expect the media controller to be present because this is a media document.
    EXPECT_TRUE([webView _hasActiveVideoForControlsManager]);
}
    
TEST(VideoControlsManager, VideoControlsManagerOffscreenIframeMediaDocument)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 800, 600));
    [webView loadTestPageNamed:@"offscreen-iframe-of-media-document"];
    
    // We do not expect a controls manager becuase the media document is in an iframe.
    [webView expectControlsManager:NO afterReceivingMessage:@"loaded"];
}

TEST(VideoControlsManager, VideoControlsManagerLongSkinnyVideoInWideMainFrame)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 1600, 800));

    [webView loadTestPageNamed:@"skinny-autoplaying-video-with-audio"];
    [webView expectControlsManager:NO afterReceivingMessage:@"playing"];
}

TEST(VideoControlsManager, VideoControlsManagerWideMediumSizedVideoInWideMainFrame)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 1600, 800));

    [webView loadTestPageNamed:@"wide-autoplaying-video-with-audio"];
    [webView expectControlsManager:YES afterReceivingMessage:@"playing"];
}

TEST(VideoControlsManager, VideoControlsManagerFullSizeVideoInWideMainFrame)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 1600, 800));

    [webView loadTestPageNamed:@"full-size-autoplaying-video-with-audio"];
    [webView expectControlsManager:YES afterReceivingMessage:@"playing"];
}

TEST(VideoControlsManager, VideoControlsManagerVideoMutesOnPlaying)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 500, 500));

    [webView loadTestPageNamed:@"large-video-mutes-onplaying"];
    [webView expectControlsManager:NO afterReceivingMessage:@"playing"];
}

TEST(VideoControlsManager, VideoControlsManagerPageWithEnormousVideo)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 500, 500));

    [webView loadTestPageNamed:@"enormous-video-with-sound"];
    [webView expectControlsManager:NO afterReceivingMessage:@"playing"];
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101201
