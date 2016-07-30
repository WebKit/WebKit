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

static bool testedControlsManagerAfterPlaying;
static bool receivedScriptMessage;

@interface MediaPlaybackMessageHandler : NSObject <WKScriptMessageHandler> {
    RetainPtr<WKWebView> _webView;
}

@property (nonatomic) BOOL expectedToHaveControlsManager;
@property (nonatomic, retain) NSString *finalMessageString;

- (instancetype)initWithWKWebView:(WKWebView*)webView finalMessageString:(NSString *)finalMessageString;
@end

@implementation MediaPlaybackMessageHandler

- (instancetype)initWithWKWebView:(WKWebView*)webView finalMessageString:(NSString *)finalMessageString
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    _finalMessageString = finalMessageString;

    return self;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;

    NSString *bodyString = (NSString *)[message body];
    if ([bodyString isEqualToString:self.finalMessageString]) {
        BOOL hasControlsManager = [_webView _hasActiveVideoForControlsManager];
        if (self.expectedToHaveControlsManager)
            EXPECT_TRUE(hasControlsManager);
        else
            EXPECT_FALSE(hasControlsManager);
        testedControlsManagerAfterPlaying = true;
    }
}
@end

@interface OnLoadMessageHandler : NSObject <WKScriptMessageHandler> {
    RetainPtr<WKWebView> _webView;
}

@property (nonatomic, strong) dispatch_block_t onloadHandler;

- (instancetype)initWithWKWebView:(WKWebView*)webView handler:(dispatch_block_t)handler;
@end

@implementation OnLoadMessageHandler

- (instancetype)initWithWKWebView:(WKWebView*)webView handler:(dispatch_block_t)handler
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    _onloadHandler = handler;

    return self;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if (![(NSString *)[message body] isEqualToString:@"loaded"])
        return;

    if (_onloadHandler)
        _onloadHandler();

    _onloadHandler = nil;
}
@end

@interface WKWebView (WKWebViewAdditions)

- (void)_interactWithMediaControlsForTesting;

@end

@interface WKWebView (TestingAdditions)

- (void)mouseDownAtPoint:(NSPoint)point;
- (void)performAfterLoading:(dispatch_block_t)actions;

@end

@implementation WKWebView (TestingAdditions)

- (void)mouseDownAtPoint:(NSPoint)point {
    [self mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(point.x, point.y) modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:0 context:[NSGraphicsContext currentContext] eventNumber:0 clickCount:0 pressure:0]];
}

- (void)performAfterLoading:(dispatch_block_t)actions {
    OnLoadMessageHandler *handler = [[OnLoadMessageHandler alloc] initWithWKWebView:self handler:actions];
    NSString *onloadScript = @"window.onload = function() { window.webkit.messageHandlers.onloadHandler.postMessage('loaded'); }";
    WKUserScript *script = [[WKUserScript alloc] initWithSource:onloadScript injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO];
    [[[self configuration] userContentController] addUserScript:script];
    [[[self configuration] userContentController] addScriptMessageHandler:handler name:@"onloadHandler"];
}

@end

namespace TestWebKitAPI {

TEST(VideoControlsManager, VideoControlsManagerSingleLargeVideo)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<MediaPlaybackMessageHandler> handler = adoptNS([[MediaPlaybackMessageHandler alloc] initWithWKWebView:webView.get() finalMessageString:@"playing"]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    // A large video with audio should have a controls manager even if it is played via script like this video.
    // So the expectation is YES.
    [handler setExpectedToHaveControlsManager:YES];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"large-video-with-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerSingleSmallVideo)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<MediaPlaybackMessageHandler> handler = adoptNS([[MediaPlaybackMessageHandler alloc] initWithWKWebView:webView.get() finalMessageString:@"playing"]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    // A small video will not have a controls manager unless it started playing because of a user gesture. Since this
    // video is started with a script, the expectation is NO.
    [handler setExpectedToHaveControlsManager:NO];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"video-with-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosWithAudio)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);

    __block BOOL didShowMediaControls;
    __block bool isDoneLoading = false;

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    RetainPtr<OnLoadMessageHandler> onloadHandler = adoptNS([[OnLoadMessageHandler alloc] initWithWKWebView:webView.get() handler:^() {
        didShowMediaControls = [webView _hasActiveVideoForControlsManager];
        isDoneLoading = true;
    }]);
    [[configuration userContentController] addScriptMessageHandler:onloadHandler.get() name:@"onloadHandler"];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"large-videos-with-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&isDoneLoading);
    EXPECT_FALSE(didShowMediaControls);
}

TEST(VideoControlsManager, VideoControlsManagerMultipleVideosWithAudioAndAutoplay)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);

    __block BOOL didShowMediaControls;
    __block bool isDoneLoading = false;

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    RetainPtr<OnLoadMessageHandler> onloadHandler = adoptNS([[OnLoadMessageHandler alloc] initWithWKWebView:webView.get() handler:^() {
        didShowMediaControls = [webView _hasActiveVideoForControlsManager];
        isDoneLoading = true;
    }]);
    [[configuration userContentController] addScriptMessageHandler:onloadHandler.get() name:@"onloadHandler"];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"large-videos-with-audio-autoplay" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&isDoneLoading);
    EXPECT_TRUE(didShowMediaControls);
}

TEST(VideoControlsManager, VideoControlsManagerSingleSmallAutoplayingVideo)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<MediaPlaybackMessageHandler> playbackHandler = adoptNS([[MediaPlaybackMessageHandler alloc] initWithWKWebView:webView.get() finalMessageString:@"paused"]);
    [[configuration userContentController] addScriptMessageHandler:playbackHandler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    RetainPtr<OnLoadMessageHandler> onloadHandler = adoptNS([[OnLoadMessageHandler alloc] initWithWKWebView:webView.get() handler:^() {
        [webView mouseDownAtPoint:NSMakePoint(50, 50)];
    }]);
    [[configuration userContentController] addScriptMessageHandler:onloadHandler.get() name:@"onloadHandler"];

    // A small video should have a controls manager after the first user gesture, which includes pausing the video. The expectation is YES.
    [playbackHandler setExpectedToHaveControlsManager:YES];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"autoplaying-video-with-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerLargeAutoplayingVideoSeeksAfterEnding)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<MediaPlaybackMessageHandler> handler = adoptNS([[MediaPlaybackMessageHandler alloc] initWithWKWebView:webView.get() finalMessageString:@"ended"]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    // Since the video has ended, the expectation is NO even if the page programmatically seeks to the beginning.
    [handler setExpectedToHaveControlsManager:NO];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"large-video-seek-after-ending" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerLargeAutoplayingVideoSeeksAndPlaysAfterEnding)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<MediaPlaybackMessageHandler> handler = adoptNS([[MediaPlaybackMessageHandler alloc] initWithWKWebView:webView.get() finalMessageString:@"replaying"]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    // Since the video is still playing, the expectation is YES even if the video has ended once.
    [handler setExpectedToHaveControlsManager:YES];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"large-video-seek-to-beginning-and-play-after-ending" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerLargeAutoplayingVideoHidesControlsAfterSeekingToEnd)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<MediaPlaybackMessageHandler> handler = adoptNS([[MediaPlaybackMessageHandler alloc] initWithWKWebView:webView.get() finalMessageString:@"ended"]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    RetainPtr<OnLoadMessageHandler> onloadHandler = adoptNS([[OnLoadMessageHandler alloc] initWithWKWebView:webView.get() handler:^() {
        [webView mouseDownAtPoint:NSMakePoint(50, 50)];
    }]);
    [[configuration userContentController] addScriptMessageHandler:onloadHandler.get() name:@"onloadHandler"];

    // Since the video has ended, the expectation is NO.
    [handler setExpectedToHaveControlsManager:NO];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"large-video-hides-controls-after-seek-to-end" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerSingleLargeVideoWithoutAudio)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<MediaPlaybackMessageHandler> handler = adoptNS([[MediaPlaybackMessageHandler alloc] initWithWKWebView:webView.get() finalMessageString:@"playing"]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    // A large video with no audio will not have a controls manager unless it started playing because of a user gesture. Since this
    // video is started with a script, the expectation is NO.
    [handler setExpectedToHaveControlsManager:NO];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"large-video-without-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerAudioElementStartedWithScript)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<MediaPlaybackMessageHandler> handler = adoptNS([[MediaPlaybackMessageHandler alloc] initWithWKWebView:webView.get() finalMessageString:@"playing"]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    // An audio element MUST be started with a user gesture in order to have a controls manager, so the expectation is NO.
    [handler setExpectedToHaveControlsManager:NO];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"audio-only" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerTearsDownMediaControlsOnDealloc)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    NSURL *urlOfVideo = [[NSBundle mainBundle] URLForResource:@"video-with-audio" withExtension:@"mp4" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadFileURL:urlOfVideo allowingReadAccessToURL:[urlOfVideo URLByDeletingLastPathComponent]];

    __block bool finishedTest = false;
    [webView performAfterLoading:^()
    {
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
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    
    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    
    __block bool finishedLoad = false;
    [webView performAfterLoading:^()
    {
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
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1600, 800) configuration:configuration.get()]);
    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    RetainPtr<MediaPlaybackMessageHandler> handler = adoptNS([[MediaPlaybackMessageHandler alloc] initWithWKWebView:webView.get() finalMessageString:@"playing"]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];
    [handler setExpectedToHaveControlsManager:NO];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"skinny-autoplaying-video-with-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerFullSizeVideoInWideMainFrame)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1600, 800) configuration:configuration.get()]);
    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    RetainPtr<MediaPlaybackMessageHandler> handler = adoptNS([[MediaPlaybackMessageHandler alloc] initWithWKWebView:webView.get() finalMessageString:@"playing"]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];
    [handler setExpectedToHaveControlsManager:YES];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"full-size-autoplaying-video-with-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
