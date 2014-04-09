/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#import "config.h"

#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 80000

#import "WebVideoFullscreenInterfaceAVKit.h"

#import "Logging.h"
#import "WebVideoFullscreenModel.h"
#import <AVFoundation/AVTime.h>
#import <AVKit/AVKit.h>
#import <AVKit/AVPlayerController.h>
#import <AVKit/AVPlayerViewController_Private.h>
#import <AVKit/AVPlayerViewController_WebKitOnly.h>
#import <AVKit/AVValueTiming.h>
#import <AVKit/AVVideoLayer.h>
#import <CoreMedia/CMTime.h>
#import <UIKit/UIKit.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/TimeRanges.h>
#import <WebCore/WebCoreThreadRun.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVPlayerLayer)

SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVPlayerController)
SOFT_LINK_CLASS(AVKit, AVPlayerViewController)
SOFT_LINK_CLASS(AVKit, AVValueTiming)

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIScreen)
SOFT_LINK_CLASS(UIKit, UIWindow)
SOFT_LINK_CLASS(UIKit, UIView)
SOFT_LINK_CLASS(UIKit, UIViewController)
SOFT_LINK_CLASS(UIKit, UIColor)

SOFT_LINK_FRAMEWORK(CoreMedia)
SOFT_LINK(CoreMedia, CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale))
SOFT_LINK(CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time))
SOFT_LINK(CoreMedia, CMTimeMake, CMTime, (int64_t value, int32_t timescale), (value, timescale))
SOFT_LINK(CoreMedia, CMTimeRangeContainsTime, Boolean, (CMTimeRange range, CMTime time), (range, time))
SOFT_LINK(CoreMedia, CMTimeRangeGetEnd, CMTime, (CMTimeRange range), (range))
SOFT_LINK(CoreMedia, CMTimeRangeMake, CMTimeRange, (CMTime start, CMTime duration), (start, duration))
SOFT_LINK(CoreMedia, CMTimeSubtract, CMTime, (CMTime minuend, CMTime subtrahend), (minuend, subtrahend))

@class WebAVMediaSelectionOption;

@interface WebAVPlayerController : NSObject <AVPlayerViewControllerDelegate>
{
    WebAVMediaSelectionOption *_currentAudioMediaSelectionOption;
    WebAVMediaSelectionOption *_currentLegibleMediaSelectionOption;
}

@property(retain) AVPlayerController* playerControllerProxy;
@property(assign) WebVideoFullscreenModel* delegate;

@property BOOL canPlay;
@property(getter=isPlaying) BOOL playing;
@property BOOL canPause;
@property BOOL canTogglePlayback;
@property double rate;
@property BOOL canSeek;
@property NSTimeInterval contentDuration;
@property NSSize contentDimensions;
@property BOOL hasEnabledAudio;
@property BOOL hasEnabledVideo;
@property NSTimeInterval minTime;
@property NSTimeInterval maxTime;
@property NSTimeInterval contentDurationWithinEndTimes;
@property(retain) NSArray *loadedTimeRanges;
@property AVPlayerControllerStatus status;
@property(retain) AVValueTiming *timing;
@property(retain) NSArray *seekableTimeRanges;

@property (readonly) BOOL hasMediaSelectionOptions;
@property (readonly) BOOL hasAudioMediaSelectionOptions;
@property (retain) NSArray *audioMediaSelectionOptions;
@property (retain) WebAVMediaSelectionOption *currentAudioMediaSelectionOption;
@property (readonly) BOOL hasLegibleMediaSelectionOptions;
@property (retain) NSArray *legibleMediaSelectionOptions;
@property (retain) WebAVMediaSelectionOption *currentLegibleMediaSelectionOption;

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldDismissWithReason:(AVPlayerViewControllerExitFullScreenReason)reason;
@end

@implementation WebAVPlayerController

- (instancetype)init
{
    if (!(self = [super init]))
        return self;
    
    initAVPlayerController();
    self.playerControllerProxy = [[classAVPlayerController alloc] init];
    return self;
}

- (void)dealloc
{
    [_playerControllerProxy release];
    [_loadedTimeRanges release];
    [_seekableTimeRanges release];
    [_timing release];
    [_audioMediaSelectionOptions release];
    [_legibleMediaSelectionOptions release];
    [_currentAudioMediaSelectionOption release];
    [_currentLegibleMediaSelectionOption release];
    [super dealloc];
}

- (id)forwardingTargetForSelector:(SEL)selector
{
    UNUSED_PARAM(selector);
    return self.playerControllerProxy;
}

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldDismissWithReason:(AVPlayerViewControllerExitFullScreenReason)reason
{
    UNUSED_PARAM(playerViewController);
    UNUSED_PARAM(reason);
    ASSERT(self.delegate);
    self.delegate->pause();
    self.delegate->requestExitFullscreen();
    return NO;
}

- (void)play:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->play();
}

- (void)pause:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->pause();
}

- (void)togglePlayback:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->togglePlayState();
}

- (BOOL)isPlaying
{
    return [self rate] != 0;
}

- (void)setPlaying:(BOOL)playing
{
    ASSERT(self.delegate);
    if (playing)
        self.delegate->play();
    else
        self.delegate->pause();
    }

+ (NSSet *)keyPathsForValuesAffectingPlaying
{
    return [NSSet setWithObject:@"rate"];
}

- (void)seekToTime:(NSTimeInterval)time
{
    ASSERT(self.delegate);
    self.delegate->seekToTime(time);
}

- (BOOL)hasLiveStreamingContent
{
    if ([self status] == AVPlayerControllerStatusReadyToPlay)
        return [self contentDuration] == std::numeric_limits<float>::infinity();
    return NO;
}

+ (NSSet *)keyPathsForValuesAffectingHasLiveStreamingContent
{
    return [NSSet setWithObjects:@"contentDuration", @"status", nil];
}

- (void)skipBackwardThirtySeconds:(id)sender
{
    UNUSED_PARAM(sender);
    BOOL isTimeWithinSeekableTimeRanges = NO;
    CMTime currentTime = CMTimeMakeWithSeconds([[self timing] currentValue], 1000);
    CMTime thirtySecondsBeforeCurrentTime = CMTimeSubtract(currentTime, CMTimeMake(30, 1));
    
    for (NSValue *seekableTimeRangeValue in [self seekableTimeRanges]) {
        if (CMTimeRangeContainsTime([seekableTimeRangeValue CMTimeRangeValue], thirtySecondsBeforeCurrentTime)) {
            isTimeWithinSeekableTimeRanges = YES;
            break;
        }
    }
    
    if (isTimeWithinSeekableTimeRanges)
        [self seekToTime:CMTimeGetSeconds(thirtySecondsBeforeCurrentTime)];
}

- (void)gotoEndOfSeekableRanges:(id)sender
{
    UNUSED_PARAM(sender);
    NSTimeInterval timeAtEndOfSeekableTimeRanges = NAN;
    
    for (NSValue *seekableTimeRangeValue in [self seekableTimeRanges]) {
        CMTimeRange seekableTimeRange = [seekableTimeRangeValue CMTimeRangeValue];
        NSTimeInterval endOfSeekableTimeRange = CMTimeGetSeconds(CMTimeRangeGetEnd(seekableTimeRange));
        if (isnan(timeAtEndOfSeekableTimeRanges) || endOfSeekableTimeRange > timeAtEndOfSeekableTimeRanges)
            timeAtEndOfSeekableTimeRanges = endOfSeekableTimeRange;
    }
    
    if (!isnan(timeAtEndOfSeekableTimeRanges))
        [self seekToTime:timeAtEndOfSeekableTimeRanges];
}

- (BOOL)hasMediaSelectionOptions
{
    return [self hasAudioMediaSelectionOptions] || [self hasLegibleMediaSelectionOptions];
}

+ (NSSet *)keyPathsForValuesAffectingHasMediaSelectionOptions
{
    return [NSSet setWithObjects:@"hasAudioMediaSelectionOptions", @"hasLegibleMediaSelectionOptions", nil];
}

- (BOOL)hasAudioMediaSelectionOptions
{
    return [[self audioMediaSelectionOptions] count] > 0;
}

+ (NSSet *)keyPathsForValuesAffectingHasAudioMediaSelectionOptions
{
    return [NSSet setWithObject:@"audioMediaSelectionOptions"];
}

- (BOOL)hasLegibleMediaSelectionOptions
{
    return [[self legibleMediaSelectionOptions] count] > 0;
}

+ (NSSet *)keyPathsForValuesAffectingHasLegibleMediaSelectionOptions
{
    return [NSSet setWithObject:@"legibleMediaSelectionOptions"];
}

- (WebAVMediaSelectionOption *)currentAudioMediaSelectionOption
{
    return _currentAudioMediaSelectionOption;
}

- (void)setCurrentAudioMediaSelectionOption:(WebAVMediaSelectionOption *)option
{
    if (option == _currentAudioMediaSelectionOption)
        return;
    
    [_currentAudioMediaSelectionOption release];
    _currentAudioMediaSelectionOption = [option retain];
    
    ASSERT(self.delegate);
    
    NSInteger index = NSNotFound;
    
    if (option && self.audioMediaSelectionOptions)
        index = [self.audioMediaSelectionOptions indexOfObject:option];
    
    self.delegate->selectAudioMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

- (WebAVMediaSelectionOption *)currentLegibleMediaSelectionOption
{
    return _currentLegibleMediaSelectionOption;
}

- (void)setCurrentLegibleMediaSelectionOption:(WebAVMediaSelectionOption *)option
{
    if (option == _currentLegibleMediaSelectionOption)
        return;
    
    [_currentLegibleMediaSelectionOption release];
    _currentLegibleMediaSelectionOption = [option retain];
    
    ASSERT(self.delegate);
    
    NSInteger index = NSNotFound;
    
    if (option && self.legibleMediaSelectionOptions)
        index = [self.legibleMediaSelectionOptions indexOfObject:option];
    
    self.delegate->selectLegibleMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

@end

@interface WebAVMediaSelectionOption : NSObject
@property (retain) NSString *localizedDisplayName;
@end

@implementation WebAVMediaSelectionOption
@end

@interface WebAVVideoLayer : CALayer <AVVideoLayer>
+(WebAVVideoLayer *)videoLayer;
@property (nonatomic) AVVideoLayerGravity videoLayerGravity;
@property (nonatomic, getter = isReadyForDisplay) BOOL readyForDisplay;
@property (nonatomic) CGRect videoRect;
- (void)setPlayerController:(AVPlayerController *)playerController;
@end

@implementation WebAVVideoLayer
{
    RetainPtr<WebAVPlayerController> _avPlayerController;
    AVVideoLayerGravity _videoLayerGravity;
}

+(WebAVVideoLayer *)videoLayer
{
    return [[[WebAVVideoLayer alloc] init] autorelease];
}

- (void)setPlayerController:(AVPlayerController *)playerController
{
    ASSERT(!playerController || [playerController isKindOfClass:[WebAVPlayerController class]]);
    _avPlayerController = (WebAVPlayerController *)playerController;
}

- (void)setBounds:(CGRect)bounds
{
    [super setBounds:bounds];
    if ([_avPlayerController delegate])
        [_avPlayerController delegate]->setVideoLayerFrame(FloatRect(0, 0, bounds.size.width, bounds.size.height));
}

- (void)setVideoLayerGravity:(AVVideoLayerGravity)videoLayerGravity
{
    _videoLayerGravity = videoLayerGravity;
    
    if (![_avPlayerController delegate])
        return;

    WebCore::WebVideoFullscreenModel::VideoGravity gravity = WebCore::WebVideoFullscreenModel::VideoGravityResizeAspect;
    if (videoLayerGravity == AVVideoLayerGravityResize)
        gravity = WebCore::WebVideoFullscreenModel::VideoGravityResize;
    if (videoLayerGravity == AVVideoLayerGravityResizeAspect)
        gravity = WebCore::WebVideoFullscreenModel::VideoGravityResizeAspect;
    else if (videoLayerGravity == AVVideoLayerGravityResizeAspectFill)
        gravity = WebCore::WebVideoFullscreenModel::VideoGravityResizeAspectFill;
    else
        ASSERT_NOT_REACHED();
    
    [_avPlayerController delegate]->setVideoLayerGravity(gravity);
}

- (AVVideoLayerGravity)videoLayerGravity
{
    return _videoLayerGravity;
}

@end

WebVideoFullscreenInterfaceAVKit::WebVideoFullscreenInterfaceAVKit()
    : m_videoFullscreenModel(nullptr)
{
    initUIScreen();
    initUIView();
    initUIColor();
    initUIWindow();
    initAVPlayerController();
    initAVPlayerViewController();
    initAVValueTiming();
    initUIViewController();
    initAVPlayerLayer();
}

WebAVPlayerController *WebVideoFullscreenInterfaceAVKit::playerController()
{
    if (!m_playerController)
    {
        m_playerController = adoptNS([[WebAVPlayerController alloc] init]);
        if (m_videoFullscreenModel)
            [m_playerController setDelegate:m_videoFullscreenModel];
    }
    return m_playerController.get();
}


void WebVideoFullscreenInterfaceAVKit::setWebVideoFullscreenModel(WebVideoFullscreenModel* model)
{
    m_videoFullscreenModel = model;
    playerController().delegate = m_videoFullscreenModel;
}

void WebVideoFullscreenInterfaceAVKit::setWebVideoFullscreenChangeObserver(WebVideoFullscreenChangeObserver* observer)
{
    m_fullscreenChangeObserver = observer;
}

void WebVideoFullscreenInterfaceAVKit::setDuration(double duration)
{
    WebAVPlayerController* playerController = this->playerController();
    
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=127017 use correct values instead of duration for all these
    playerController.contentDuration = duration;
    playerController.maxTime = duration;
    playerController.contentDurationWithinEndTimes = duration;
    playerController.loadedTimeRanges = @[@0, @(duration)];
    
    // FIXME: we take this as an indication that playback is ready.
    playerController.canPlay = YES;
    playerController.canPause = YES;
    playerController.canTogglePlayback = YES;
    playerController.hasEnabledAudio = YES;
    playerController.canSeek = YES;
    playerController.minTime = 0;
    playerController.status = AVPlayerControllerStatusReadyToPlay;
}

void WebVideoFullscreenInterfaceAVKit::setCurrentTime(double currentTime, double anchorTime)
{
    NSTimeInterval anchorTimeStamp = ![playerController() rate] ? NAN : anchorTime;
    AVValueTiming *timing = [classAVValueTiming valueTimingWithAnchorValue:currentTime
        anchorTimeStamp:anchorTimeStamp rate:0];
    playerController().timing = timing;
}

void WebVideoFullscreenInterfaceAVKit::setRate(bool isPlaying, float playbackRate)
{
    playerController().rate = isPlaying ? playbackRate : 0.;
}

void WebVideoFullscreenInterfaceAVKit::setVideoDimensions(bool hasVideo, float width, float height)
{
    playerController().hasEnabledVideo = hasVideo;
    playerController().contentDimensions = CGSizeMake(width, height);
}

void WebVideoFullscreenInterfaceAVKit::setSeekableRanges(const TimeRanges& timeRanges)
{
    NSMutableArray* seekableRanges = [NSMutableArray array];
    ExceptionCode exceptionCode;

    for (unsigned i = 0; i < timeRanges.length(); i++) {
        double start = timeRanges.start(i, exceptionCode);
        double end = timeRanges.end(i, exceptionCode);
        
        CMTimeRange range = CMTimeRangeMake(CMTimeMakeWithSeconds(start, 1000), CMTimeMakeWithSeconds(end-start, 1000));
        [seekableRanges addObject:[NSValue valueWithCMTimeRange:range]];
    }
    
    playerController().seekableTimeRanges = seekableRanges;
}

static NSMutableArray *mediaSelectionOptions(const Vector<String>& options)
{
    NSMutableArray *webOptions = [NSMutableArray arrayWithCapacity:options.size()];
    for (auto& name : options) {
        RetainPtr<WebAVMediaSelectionOption> webOption = adoptNS([[WebAVMediaSelectionOption alloc] init]);
        RetainPtr<NSString> nameString = adoptNS([[NSString alloc] initWithUTF8String:name.utf8().data()]);
        [webOption setLocalizedDisplayName:nameString.get()];
        [webOptions addObject:webOption.get()];
    }
    return webOptions;
}

void WebVideoFullscreenInterfaceAVKit::setAudioMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    NSMutableArray *webOptions = mediaSelectionOptions(options);
    playerController().audioMediaSelectionOptions = webOptions;
    if (selectedIndex < webOptions.count)
        playerController().currentAudioMediaSelectionOption = webOptions[(size_t)selectedIndex];
}

void WebVideoFullscreenInterfaceAVKit::setLegibleMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    NSMutableArray *webOptions = mediaSelectionOptions(options);
    playerController().legibleMediaSelectionOptions = webOptions;
    if (selectedIndex < webOptions.count)
        playerController().currentLegibleMediaSelectionOption = webOptions[(size_t)selectedIndex];
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreen(PlatformLayer& videoLayer)
{
    __block RefPtr<WebVideoFullscreenInterfaceAVKit> protect(this);
    
    m_videoLayer = &videoLayer;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [m_videoLayer removeFromSuperlayer];
        
        m_videoLayerContainer = [WebAVVideoLayer videoLayer];
        [m_videoLayerContainer addSublayer:m_videoLayer.get()];
        
        CGSize videoSize = playerController().contentDimensions;
        CGRect videoRect = CGRectMake(0, 0, videoSize.width, videoSize.height);
        [m_videoLayerContainer setVideoRect:videoRect];

        m_playerViewController = adoptNS([[classAVPlayerViewController alloc] initWithVideoLayer:m_videoLayerContainer.get()]);
        [m_playerViewController setPlayerController:(AVPlayerController *)playerController()];
        [m_playerViewController setDelegate:playerController()];
        
        m_viewController = adoptNS([[classUIViewController alloc] init]);
        
        m_window = adoptNS([[classUIWindow alloc] initWithFrame:[[classUIScreen mainScreen] bounds]]);
        [m_window setBackgroundColor:[classUIColor clearColor]];
        [m_window setRootViewController:m_viewController.get()];
        [m_window makeKeyAndVisible];
        
        __block RefPtr<WebVideoFullscreenInterfaceAVKit> protect2(this);

        dispatch_async(dispatch_get_main_queue(), ^{
            [m_viewController presentViewController:m_playerViewController.get() animated:YES completion:^{
                if (m_fullscreenChangeObserver)
                    m_fullscreenChangeObserver->didEnterFullscreen();
                protect2.clear();
            }];
        });
        ￼
        protect.clear();
    });
}

void WebVideoFullscreenInterfaceAVKit::exitFullscreen()
{
    __block RefPtr<WebVideoFullscreenInterfaceAVKit> protect(this);

    m_playerController.clear();
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [m_viewController dismissViewControllerAnimated:YES completion:^{
            [m_window setHidden:YES];
            [m_window setRootViewController:nil];
            [m_playerViewController setDelegate:nil];
            [m_playerViewController setPlayerController:nil];
            m_playerViewController = nil;
            m_viewController = nil;
            m_window = nil;
            [m_videoLayer removeFromSuperlayer];
            m_videoLayer = nil;
            [m_videoLayerContainer removeFromSuperlayer];
            m_videoLayerContainer = nil;

            if (m_fullscreenChangeObserver)
                m_fullscreenChangeObserver->didExitFullscreen();
            protect.clear();
        }];
    });
}

#endif
