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
#import "GeometryUtilities.h"
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
#import <WebCore/RuntimeApplicationChecksIOS.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/TimeRanges.h>
#import <WebCore/WebCoreThreadRun.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>

using namespace WebCore;

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVPlayerLayer)

SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVPlayerController)
SOFT_LINK_CLASS(AVKit, AVPlayerViewController)
SOFT_LINK_CLASS(AVKit, AVValueTiming)

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIApplication)
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
SOFT_LINK(CoreMedia, CMTimeMaximum, CMTime, (CMTime time1, CMTime time2), (time1, time2))
SOFT_LINK(CoreMedia, CMTimeMinimum, CMTime, (CMTime time1, CMTime time2), (time1, time2))
SOFT_LINK_CONSTANT(CoreMedia, kCMTimeIndefinite, CMTime)

#define kCMTimeIndefinite getkCMTimeIndefinite()

@class WebAVMediaSelectionOption;

@interface WebAVPlayerController : NSObject <AVPlayerViewControllerDelegate>
{
    WebAVMediaSelectionOption *_currentAudioMediaSelectionOption;
    WebAVMediaSelectionOption *_currentLegibleMediaSelectionOption;
}

@property(retain) AVPlayerController* playerControllerProxy;
@property(assign) WebVideoFullscreenModel* delegate;

@property (readonly) BOOL canScanForward;
@property BOOL canScanBackward;
@property (readonly) BOOL canSeekToBeginning;
@property (readonly) BOOL canSeekToEnd;

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

@property (readonly, getter=isPlayingOnExternalScreen) BOOL playingOnExternalScreen;
@property (getter=isExternalPlaybackActive) BOOL externalPlaybackActive;
@property AVPlayerControllerExternalPlaybackType externalPlaybackType;
@property (retain) NSString *externalPlaybackAirPlayDeviceLocalizedName;

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason;
@end

@implementation WebAVPlayerController

- (instancetype)init
{
    if (!(self = [super init]))
        return self;
    
    initAVPlayerController();
    self.playerControllerProxy = [[[getAVPlayerControllerClass() alloc] init] autorelease];
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

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason
{
    UNUSED_PARAM(playerViewController);
    UNUSED_PARAM(reason);
    ASSERT(self.delegate);
    if (reason == AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped || reason == AVPlayerViewControllerExitFullScreenReasonRemoteControlStopEventReceived)
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

- (void)beginScrubbing:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->beginScrubbing();
}

- (void)endScrubbing:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->endScrubbing();
}

- (void)seekToTime:(NSTimeInterval)time
{
    ASSERT(self.delegate);
    self.delegate->fastSeek(time);
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

- (BOOL)canScanForward
{
    return [self canPlay];
}

+ (NSSet *)keyPathsForValuesAffectingCanScanForward
{
    return [NSSet setWithObject:@"canPlay"];
}

- (void)beginScanningForward:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->beginScanningForward();
}

- (void)endScanningForward:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->endScanning();
}

- (void)beginScanningBackward:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->beginScanningBackward();
}

- (void)endScanningBackward:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->endScanning();
}

- (BOOL)canSeekToBeginning
{
    CMTime minimumTime = kCMTimeIndefinite;

    for (NSValue *value in [self seekableTimeRanges])
        minimumTime = CMTimeMinimum([value CMTimeRangeValue].start, minimumTime);

    return CMTIME_IS_NUMERIC(minimumTime);
}

+ (NSSet *)keyPathsForValuesAffectingCanSeekToBeginning
{
    return [NSSet setWithObject:@"seekableTimeRanges"];
}

- (void)seekToBeginning:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);

    self.delegate->seekToTime(-INFINITY);
}

- (void)seekChapterBackward:(id)sender
{
    [self seekToBeginning:sender];
}

- (BOOL)canSeekToEnd
{
    CMTime maximumTime = kCMTimeIndefinite;

    for (NSValue *value in [self seekableTimeRanges])
        maximumTime = CMTimeMaximum(CMTimeRangeGetEnd([value CMTimeRangeValue]), maximumTime);

    return CMTIME_IS_NUMERIC(maximumTime);
}

+ (NSSet *)keyPathsForValuesAffectingCanSeekToEnd
{
    return [NSSet setWithObject:@"seekableTimeRanges"];
}

- (void)seekToEnd:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);

    self.delegate->seekToTime(INFINITY);
}

- (void)seekChapterForward:(id)sender
{
    [self seekToEnd:sender];
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

- (BOOL)isPlayingOnExternalScreen
{
    return [self isExternalPlaybackActive];
}

+ (NSSet *)keyPathsForValuesAffectingPlayingOnExternalScreen
{
    return [NSSet setWithObjects:@"externalPlaybackActive", nil];
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
- (void)setPlayerViewController:(AVPlayerViewController *)playerViewController;
- (void)setPlayerController:(AVPlayerController *)playerController;
@end

@implementation WebAVVideoLayer
{
    RetainPtr<WebAVPlayerController> _avPlayerController;
    RetainPtr<AVPlayerViewController> _avPlayerViewController;
    AVVideoLayerGravity _videoLayerGravity;
}

+(WebAVVideoLayer *)videoLayer
{
    return [[[WebAVVideoLayer alloc] init] autorelease];
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setMasksToBounds:YES];
        [self setVideoLayerGravity:AVVideoLayerGravityResizeAspect];
    }
    return self;
}

- (void)setPlayerController:(AVPlayerController *)playerController
{
    ASSERT(!playerController || [playerController isKindOfClass:[WebAVPlayerController class]]);
    _avPlayerController = (WebAVPlayerController *)playerController;
}

- (void)setPlayerViewController:(AVPlayerViewController *)playerViewController
{
    _avPlayerViewController = playerViewController;
}

- (void)setBounds:(CGRect)bounds
{
    [super setBounds:bounds];

    if (![_avPlayerController delegate] || !_avPlayerViewController)
        return;

    UIView* rootView = [[_avPlayerViewController view] window];
    if (!rootView)
        return;

    FloatRect rootBounds = [rootView bounds];
    [_avPlayerController delegate]->setVideoLayerFrame(rootBounds);

    FloatRect sourceBounds = largestRectWithAspectRatioInsideRect(CGRectGetWidth(bounds) / CGRectGetHeight(bounds), rootBounds);
    CATransform3D transform = CATransform3DMakeScale(bounds.size.width / sourceBounds.width(), bounds.size.height / sourceBounds.height(), 1);
    transform = CATransform3DTranslate(transform, bounds.origin.x - sourceBounds.x(), bounds.origin.y - sourceBounds.y(), 0);
    [self setSublayerTransform:transform];
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
    [m_playerController setDelegate:m_videoFullscreenModel];
}

void WebVideoFullscreenInterfaceAVKit::setWebVideoFullscreenChangeObserver(WebVideoFullscreenChangeObserver* observer)
{
    m_fullscreenChangeObserver = observer;
}

void WebVideoFullscreenInterfaceAVKit::setDuration(double duration)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [strongThis, duration] {
        WebAVPlayerController* playerController = strongThis->playerController();

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
    });
}

void WebVideoFullscreenInterfaceAVKit::setCurrentTime(double currentTime, double anchorTime)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [strongThis, currentTime, anchorTime] {
        NSTimeInterval anchorTimeStamp = ![strongThis->playerController() rate] ? NAN : anchorTime;
        AVValueTiming *timing = [getAVValueTimingClass() valueTimingWithAnchorValue:currentTime
            anchorTimeStamp:anchorTimeStamp rate:0];
        strongThis->playerController().timing = timing;
    });
}

void WebVideoFullscreenInterfaceAVKit::setRate(bool isPlaying, float playbackRate)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [strongThis, isPlaying, playbackRate] {
        strongThis->playerController().rate = isPlaying ? playbackRate : 0.;
    });
}

void WebVideoFullscreenInterfaceAVKit::setVideoDimensions(bool hasVideo, float width, float height)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [strongThis, hasVideo, width, height] {
        strongThis->playerController().hasEnabledVideo = hasVideo;
        strongThis->playerController().contentDimensions = CGSizeMake(width, height);
    });
}

void WebVideoFullscreenInterfaceAVKit::setSeekableRanges(const TimeRanges& timeRanges)
{
    RetainPtr<NSMutableArray> seekableRanges = adoptNS([[NSMutableArray alloc] init]);
    ExceptionCode exceptionCode;

    for (unsigned i = 0; i < timeRanges.length(); i++) {
        double start = timeRanges.start(i, exceptionCode);
        double end = timeRanges.end(i, exceptionCode);
        
        CMTimeRange range = CMTimeRangeMake(CMTimeMakeWithSeconds(start, 1000), CMTimeMakeWithSeconds(end-start, 1000));
        [seekableRanges addObject:[NSValue valueWithCMTimeRange:range]];
    }
    
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [strongThis, seekableRanges] {
        strongThis->playerController().seekableTimeRanges = seekableRanges.get();
    });
}

void WebVideoFullscreenInterfaceAVKit::setCanPlayFastReverse(bool canPlayFastReverse)
{
    playerController().canScanBackward = canPlayFastReverse;
}

static RetainPtr<NSMutableArray> mediaSelectionOptions(const Vector<String>& options)
{
    RetainPtr<NSMutableArray> webOptions = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& name : options) {
        RetainPtr<WebAVMediaSelectionOption> webOption = adoptNS([[WebAVMediaSelectionOption alloc] init]);
        [webOption setLocalizedDisplayName:name];
        [webOptions addObject:webOption.get()];
    }
    return webOptions;
}

void WebVideoFullscreenInterfaceAVKit::setAudioMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [webOptions, strongThis, selectedIndex] {
        strongThis->playerController().audioMediaSelectionOptions = webOptions.get();
        if (selectedIndex < [webOptions count])
            strongThis->playerController().currentAudioMediaSelectionOption = [webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)];
    });
}

void WebVideoFullscreenInterfaceAVKit::setLegibleMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);

    dispatch_async(dispatch_get_main_queue(), [webOptions, strongThis, selectedIndex] {
        strongThis->playerController().legibleMediaSelectionOptions = webOptions.get();
        if (selectedIndex < [webOptions count])
            strongThis->playerController().currentLegibleMediaSelectionOption = [webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)];
    });
}

void WebVideoFullscreenInterfaceAVKit::setExternalPlayback(bool enabled, ExternalPlaybackTargetType targetType, String localizedDeviceName)
{
    AVPlayerControllerExternalPlaybackType externalPlaybackType = AVPlayerControllerExternalPlaybackTypeNone;
    if (targetType == TargetTypeAirPlay)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeAirPlay;
    else if (targetType == TargetTypeTVOut)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeTVOut;

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);

    dispatch_async(dispatch_get_main_queue(), [strongThis, enabled, localizedDeviceName, externalPlaybackType] {
        WebAVPlayerController* playerController = strongThis->playerController();
        playerController.externalPlaybackAirPlayDeviceLocalizedName = localizedDeviceName;
        playerController.externalPlaybackType = externalPlaybackType;
        playerController.externalPlaybackActive = enabled;
        [strongThis->m_videoLayerContainer.get() setHidden:enabled];
    });
}

void WebVideoFullscreenInterfaceAVKit::setupFullscreen(PlatformLayer& videoLayer, WebCore::IntRect initialRect, UIView* parentView)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);

    m_videoLayer = &videoLayer;

    dispatch_async(dispatch_get_main_queue(), [strongThis, &videoLayer, initialRect, parentView] {
        strongThis->setupFullscreenInternal(videoLayer, initialRect, parentView);
    });
}

void WebVideoFullscreenInterfaceAVKit::setupFullscreenInternal(PlatformLayer& videoLayer, WebCore::IntRect initialRect, UIView* parentView)
{
    UNUSED_PARAM(videoLayer);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    m_parentView = parentView;

    if (!applicationIsAdSheet()) {
        m_window = adoptNS([[getUIWindowClass() alloc] initWithFrame:[[getUIScreenClass() mainScreen] bounds]]);
        [m_window setBackgroundColor:[getUIColorClass() clearColor]];
        m_viewController = adoptNS([[getUIViewControllerClass() alloc] init]);
        [[m_viewController view] setFrame:[m_window bounds]];
        [m_window setRootViewController:m_viewController.get()];
        [m_window makeKeyAndVisible];
    }

    [m_videoLayer removeFromSuperlayer];

    m_videoLayerContainer = [WebAVVideoLayer videoLayer];
    [m_videoLayerContainer setHidden:playerController().externalPlaybackActive];
    [m_videoLayerContainer addSublayer:m_videoLayer.get()];

    CGSize videoSize = playerController().contentDimensions;
    CGRect videoRect = CGRectMake(0, 0, videoSize.width, videoSize.height);
    [m_videoLayerContainer setVideoRect:videoRect];

    m_playerViewController = adoptNS([[getAVPlayerViewControllerClass() alloc] initWithVideoLayer:m_videoLayerContainer.get()]);
    [m_playerViewController setShowsPlaybackControls:NO];
    [m_playerViewController setPlayerController:(AVPlayerController *)playerController()];
    [m_playerViewController setDelegate:playerController()];
    [m_videoLayerContainer setPlayerViewController:m_playerViewController.get()];

    if (m_viewController) {
        [m_viewController addChildViewController:m_playerViewController.get()];
        [[m_viewController view] addSubview:[m_playerViewController view]];
        [m_playerViewController view].frame = [parentView convertRect:initialRect toView:nil];
    } else {
        [parentView addSubview:[m_playerViewController view]];
        [m_playerViewController view].frame = initialRect;
    }

    [[m_playerViewController view] setBackgroundColor:[getUIColorClass() clearColor]];
    [[m_playerViewController view] setNeedsLayout];
    [[m_playerViewController view] layoutIfNeeded];

    [CATransaction commit];

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    WebThreadRun([strongThis] {
        if (strongThis->m_fullscreenChangeObserver)
            strongThis->m_fullscreenChangeObserver->didSetupFullscreen();
    });
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreen()
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis] {
        [strongThis->m_videoLayerContainer setBackgroundColor:[[getUIColorClass() blackColor] CGColor]];
        strongThis->enterFullscreenStandard();
    });
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreenStandard()
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    [m_playerViewController enterFullScreenWithCompletionHandler:[this, strongThis] (BOOL, NSError*) {
        [m_playerViewController setShowsPlaybackControls:YES];

        WebThreadRun([this, strongThis] {
            if (m_fullscreenChangeObserver)
                m_fullscreenChangeObserver->didEnterFullscreen();
        });
    }];
}

void WebVideoFullscreenInterfaceAVKit::exitFullscreen(WebCore::IntRect finalRect)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);

    m_playerController = nil;

    dispatch_async(dispatch_get_main_queue(), [strongThis, finalRect] {
        strongThis->exitFullscreenInternal(finalRect);
    });
}

void WebVideoFullscreenInterfaceAVKit::exitFullscreenInternal(WebCore::IntRect finalRect)
{
    [m_playerViewController setShowsPlaybackControls:NO];
    if (m_viewController)
        [m_playerViewController view].frame = [m_parentView convertRect:finalRect toView:nil];
    else
        [m_playerViewController view].frame = finalRect;

    if ([m_videoLayerContainer videoLayerGravity] != AVVideoLayerGravityResizeAspect)
        [m_videoLayerContainer setVideoLayerGravity:AVVideoLayerGravityResizeAspect];
    [[m_playerViewController view] layoutIfNeeded];

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    [m_playerViewController exitFullScreenWithCompletionHandler:[strongThis] (BOOL, NSError*) {
        [strongThis->m_videoLayerContainer setBackgroundColor:[[getUIColorClass() clearColor] CGColor]];
        [[strongThis->m_playerViewController view] setBackgroundColor:[getUIColorClass() clearColor]];
        WebThreadRun([strongThis] {
            if (strongThis->m_fullscreenChangeObserver)
                strongThis->m_fullscreenChangeObserver->didExitFullscreen();
        });
    }];
}

@interface UIApplication ()
-(void)_setStatusBarOrientation:(UIInterfaceOrientation)o;
@end

@interface UIWindow ()
-(UIInterfaceOrientation)interfaceOrientation;
@end

void WebVideoFullscreenInterfaceAVKit::cleanupFullscreen()
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [strongThis] {
        strongThis->cleanupFullscreenInternal();
    });
}

void WebVideoFullscreenInterfaceAVKit::cleanupFullscreenInternal()
{
    if (m_window) {
        [m_window setHidden:YES];
        [m_window setRootViewController:nil];
        [[getUIApplicationClass() sharedApplication] _setStatusBarOrientation:[[m_parentView window] interfaceOrientation]];
    }
    [m_playerViewController setDelegate:nil];
    [[m_playerViewController view] removeFromSuperview];
    if (m_viewController)
        [m_playerViewController removeFromParentViewController];
    [m_playerViewController setPlayerController:nil];
    m_playerViewController = nil;
    [m_videoLayer removeFromSuperlayer];
    m_videoLayer = nil;
    [m_videoLayerContainer removeFromSuperlayer];
    [m_videoLayerContainer setPlayerViewController:nil];
    m_videoLayerContainer = nil;
    [[m_viewController view] removeFromSuperview];
    m_viewController = nil;
    m_window = nil;
    m_parentView = nil;
    
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    WebThreadRun([strongThis] {
        if (strongThis->m_fullscreenChangeObserver)
            strongThis->m_fullscreenChangeObserver->didCleanupFullscreen();
    });
}

void WebVideoFullscreenInterfaceAVKit::invalidate()
{
    [m_window setHidden:YES];
    [m_window setRootViewController:nil];
    [m_playerViewController exitFullScreenAnimated:NO completionHandler:nil];
    m_playerController = nil;
    [m_playerViewController setDelegate:nil];
    [[m_playerViewController view] removeFromSuperview];
    if (m_viewController)
        [m_playerViewController removeFromParentViewController];
    [m_playerViewController setPlayerController:nil];
    m_playerViewController = nil;
    [m_videoLayer removeFromSuperlayer];
    m_videoLayer = nil;
    [m_videoLayerContainer removeFromSuperlayer];
    [m_videoLayerContainer setPlayerViewController:nil];
    m_videoLayerContainer = nil;
    [[m_viewController view] removeFromSuperview];
    m_viewController = nil;
    m_window = nil;
    m_parentView = nil;
}

void WebVideoFullscreenInterfaceAVKit::requestHideAndExitFullscreen()
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis] {
        [strongThis->m_window setHidden:YES];
        [strongThis->m_playerViewController exitFullScreenAnimated:NO completionHandler:[strongThis] (BOOL, NSError*) {
        }];
    });

    if (m_videoFullscreenModel)
        m_videoFullscreenModel->requestExitFullscreen();
}


#endif
