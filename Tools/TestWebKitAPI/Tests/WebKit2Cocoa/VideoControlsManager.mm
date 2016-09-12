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

#if PLATFORM(MAC)
#import <Carbon/Carbon.h>
#endif

#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED && PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200

@interface WKWebView (WKWebViewAdditions)

- (void)_interactWithMediaControlsForTesting;

@end

@interface MessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation MessageHandler {
    dispatch_block_t _handler;
    NSString *_message;
}

- (instancetype)initWithMessage:(NSString *)message handler:(dispatch_block_t)handler
{
    if (!(self = [super init]))
        return nil;

    _handler = [handler copy];
    _message = message;

    return self;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([(NSString *)[message body] isEqualToString:_message] && _handler)
        _handler();
}

@end

@interface VideoControlsManagerTestWebView : WKWebView
@end

@implementation VideoControlsManagerTestWebView {
    bool _isDoneQueryingControlledElementID;
    NSString *_controlledElementID;
}

- (void)mouseDownAtPoint:(NSPoint)point {
    [self mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(point.x, point.y) modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:0 context:[NSGraphicsContext currentContext] eventNumber:0 clickCount:0 pressure:0]];
}

- (void)performAfterLoading:(dispatch_block_t)actions {
    MessageHandler *handler = [[MessageHandler alloc] initWithMessage:@"loaded" handler:actions];
    NSString *onloadScript = @"window.onload = function() { window.webkit.messageHandlers.onloadHandler.postMessage('loaded'); }";
    WKUserScript *script = [[WKUserScript alloc] initWithSource:onloadScript injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO];

    WKUserContentController* contentController = [[self configuration] userContentController];
    [contentController addUserScript:script];
    [contentController addScriptMessageHandler:handler name:@"onloadHandler"];
}

- (void)callJavascriptFunction:(NSString *)functionName
{
    NSString *command = [NSString stringWithFormat:@"%@()", functionName];
    [self evaluateJavaScript:command completionHandler:nil];
}

- (void)loadTestPageNamed:(NSString *)pageName
{
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:pageName withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [self loadRequest:request];
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

- (void)performAfterReceivingMessage:(NSString *)message action:(dispatch_block_t)action
{
    RetainPtr<MessageHandler> handler = adoptNS([[MessageHandler alloc] initWithMessage:message handler:action]);
    WKUserContentController* contentController = [[self configuration] userContentController];
    [contentController removeScriptMessageHandlerForName:@"playingHandler"];
    [contentController addScriptMessageHandler:handler.get() name:@"playingHandler"];
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
    RetainPtr<VideoControlsManagerTestWebView> webView = adoptNS([[VideoControlsManagerTestWebView alloc] initWithFrame:frame configuration:configuration.get()]);
    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    return webView;
}

TEST(VideoControlsManager, VideoControlsManagerSingleLargeVideo)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    // A large video with audio should have a controls manager even if it is played via script like this video.
    // So the expectation is YES.
    [webView loadTestPageNamed:@"large-video-with-audio"];
    [webView expectControlsManager:YES afterReceivingMessage:@"playing"];
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

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosWithAudioAndAutoplay)
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

    [webView callJavascriptFunction:@"pauseFirstVideoAndScrollToSecondVideo"];
    [webView expectControlsManager:NO afterReceivingMessage:@"paused"];
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosScrollPlayingVideoWithSoundOutOfView)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 500, 500));

    [webView loadTestPageNamed:@"large-videos-playing-video-keeps-controls"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];

    [webView callJavascriptFunction:@"scrollToSecondVideo"];
    [webView expectControlsManager:YES afterReceivingMessage:@"scrolled"];
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosScrollPlayingMutedVideoOutOfView)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 500, 500));

    [webView loadTestPageNamed:@"large-videos-playing-muted-video-hides-controls"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];

    [webView callJavascriptFunction:@"muteFirstVideoAndScrollToSecondVideo"];
    [webView expectControlsManager:NO afterReceivingMessage:@"playing"];
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosShowControlsForLastInteractedVideo)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 800, 600));
    NSPoint clickPoint = NSMakePoint(400, 300);

    [webView loadTestPageNamed:@"large-videos-autoplaying-click-to-pause"];
    [webView waitForPageToLoadWithAutoplayingVideos:2];

    [webView mouseDownAtPoint:clickPoint];

    __block bool firstVideoPaused = false;
    __block bool secondVideoPaused = false;
    [webView performAfterReceivingMessage:@"paused" action:^ {
        NSString *controlledElementID = [webView controlledElementID];
        if (firstVideoPaused) {
            EXPECT_TRUE([controlledElementID isEqualToString:@"second"]);
            secondVideoPaused = true;
        } else {
            EXPECT_TRUE([controlledElementID isEqualToString:@"first"]);
            [webView mouseDownAtPoint:clickPoint];
        }
        firstVideoPaused = true;
    }];

    TestWebKitAPI::Util::run(&secondVideoPaused);
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosSwitchControlledVideoWhenScrolling)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 800, 600));

    [webView loadTestPageNamed:@"large-videos-autoplaying-scroll-to-video"];
    [webView waitForPageToLoadWithAutoplayingVideos:2];

    [webView callJavascriptFunction:@"scrollToSecondView"];
    [webView expectControlsManager:YES afterReceivingMessage:@"scrolled"];

    EXPECT_TRUE([[webView controlledElementID] isEqualToString:@"second"]);
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosScrollOnlyLargeVideoOutOfView)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 500, 500));

    [webView loadTestPageNamed:@"large-video-playing-scroll-away"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];
    [webView callJavascriptFunction:@"scrollVideoOutOfView"];
    [webView expectControlsManager:YES afterReceivingMessage:@"scrolled"];
}

TEST(VideoControlsManager, VideoControlsManagerSingleSmallAutoplayingVideo)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    [webView loadTestPageNamed:@"autoplaying-video-with-audio"];
    [webView waitForPageToLoadWithAutoplayingVideos:1];

    [webView mouseDownAtPoint:NSMakePoint(50, 50)];
    [webView expectControlsManager:YES afterReceivingMessage:@"paused"];
}

TEST(VideoControlsManager, VideoControlsManagerLargeAutoplayingVideoSeeksAfterEnding)
{
    RetainPtr<VideoControlsManagerTestWebView*> webView = setUpWebViewForTestingVideoControlsManager(NSMakeRect(0, 0, 100, 100));

    // Since the video has ended, the expectation is NO even if the page programmatically seeks to the beginning.
    [webView loadTestPageNamed:@"large-video-seek-after-ending"];
    [webView expectControlsManager:NO afterReceivingMessage:@"ended"];
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
    [webView mouseDownAtPoint:NSMakePoint(50, 50)];

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

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
