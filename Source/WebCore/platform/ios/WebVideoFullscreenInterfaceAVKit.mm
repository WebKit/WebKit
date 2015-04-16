/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED > 80200 && HAVE(AVKIT)

#import "WebVideoFullscreenInterfaceAVKit.h"

#import "AVKitSPI.h"
#import "GeometryUtilities.h"
#import "Logging.h"
#import "RuntimeApplicationChecksIOS.h"
#import "TimeRanges.h"
#import "WebCoreSystemInterface.h"
#import "WebCoreThreadRun.h"
#import "WebVideoFullscreenModel.h"
#import <AVFoundation/AVTime.h>
#import <UIKit/UIKit.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

// Soft-linking headers must be included last since they #define functions, constants, etc.
#import "CoreMediaSoftLink.h"

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

#if !LOG_DISABLED
static const char* boolString(bool val)
{
    return val ? "true" : "false";
}
#endif


@class WebAVMediaSelectionOption;

@interface WebAVPlayerController : NSObject <AVPlayerViewControllerDelegate>
{
    WebAVMediaSelectionOption *_currentAudioMediaSelectionOption;
    WebAVMediaSelectionOption *_currentLegibleMediaSelectionOption;
}

-(void)resetState;

@property (retain) AVPlayerController* playerControllerProxy;
@property (assign) WebVideoFullscreenModel* delegate;
@property (assign) WebVideoFullscreenInterfaceAVKit* fullscreenInterface;

@property (readonly) BOOL canScanForward;
@property BOOL canScanBackward;
@property (readonly) BOOL canSeekToBeginning;
@property (readonly) BOOL canSeekToEnd;

@property BOOL canPlay;
@property (getter=isPlaying) BOOL playing;
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
@property (retain) NSArray *loadedTimeRanges;
@property AVPlayerControllerStatus status;
@property (retain) AVValueTiming *timing;
@property (retain) NSArray *seekableTimeRanges;

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
    self.playerControllerProxy = [[allocAVPlayerControllerInstance() init] autorelease];
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

-(void)resetState {
    self.contentDuration = 0;
    self.maxTime = 0;
    self.contentDurationWithinEndTimes = 0;
    self.loadedTimeRanges = @[];
    
    self.canPlay = NO;
    self.canPause = NO;
    self.canTogglePlayback = NO;
    self.hasEnabledAudio = NO;
    self.canSeek = NO;
    self.minTime = 0;
    self.status = AVPlayerControllerStatusUnknown;
    
    self.timing = nil;
    self.rate = 0;
    
    self.hasEnabledVideo = NO;
    self.contentDimensions = CGSizeMake(0, 0);
    
    self.seekableTimeRanges = [NSMutableArray array];
    
    self.canScanBackward = NO;
    
    self.audioMediaSelectionOptions = nil;
    self.currentAudioMediaSelectionOption = nil;
    
    self.legibleMediaSelectionOptions = nil;
    self.currentLegibleMediaSelectionOption = nil;
}

- (AVPlayer*) player {
    return nil;
}

- (id)forwardingTargetForSelector:(SEL)selector
{
    UNUSED_PARAM(selector);
    return self.playerControllerProxy;
}

- (void)playerViewControllerWillStartOptimizedFullscreen:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->willStartOptimizedFullscreen();
}

- (void)playerViewControllerDidStartOptimizedFullscreen:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->didStartOptimizedFullscreen();
}

- (void)playerViewControllerWillStopOptimizedFullscreen:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->willStopOptimizedFullscreen();
}

- (void)playerViewControllerDidStopOptimizedFullscreen:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->didStopOptimizedFullscreen();
}

- (void)playerViewControllerWillCancelOptimizedFullscreen:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->willCancelOptimizedFullscreen();
}

- (void)playerViewControllerDidCancelOptimizedFullscreen:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->didCancelOptimizedFullscreen();
}

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason
{
    UNUSED_PARAM(playerViewController);
    UNUSED_PARAM(reason);
    if (!self.delegate)
        return YES;
    
    if (reason == AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped || reason == AVPlayerViewControllerExitFullScreenReasonRemoteControlStopEventReceived)
        self.delegate->pause();
    
    self.delegate->requestExitFullscreen();
    return NO;
}

- (void)playerViewController:(AVPlayerViewController *)playerViewController restoreUserInterfaceForOptimizedFullscreenStopWithCompletionHandler:(void (^)(BOOL restored))completionHandler
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->prepareForOptimizedFullscreenStopWithCompletionHandler(completionHandler);
}

- (void)play:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->play();
}

- (void)pause:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->pause();
}

- (void)togglePlayback:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->togglePlayState();
}

- (void)togglePlaybackEvenWhenInBackground:(id)sender
{
    [self togglePlayback:sender];
}

- (BOOL)isPlaying
{
    return [self rate] != 0;
}

- (void)setPlaying:(BOOL)playing
{
    if (!self.delegate)
        return;
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
    if (!self.delegate)
        return;
    self.delegate->beginScrubbing();
}

- (void)endScrubbing:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->endScrubbing();
}

- (void)seekToTime:(NSTimeInterval)time
{
    if (!self.delegate)
        return;
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
    if (!self.delegate)
        return;
    self.delegate->beginScanningForward();
}

- (void)endScanningForward:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->endScanning();
}

- (void)beginScanningBackward:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->beginScanningBackward();
}

- (void)endScanningBackward:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
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
    if (!self.delegate)
        return;
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
    if (!self.delegate)
        return;
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
    return [[self audioMediaSelectionOptions] count] > 1;
}

+ (NSSet *)keyPathsForValuesAffectingHasAudioMediaSelectionOptions
{
    return [NSSet setWithObject:@"audioMediaSelectionOptions"];
}

- (BOOL)hasLegibleMediaSelectionOptions
{
    const NSUInteger numDefaultLegibleOptions = 2;
    return [[self legibleMediaSelectionOptions] count] > numDefaultLegibleOptions;
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
    
    if (!self.delegate)
        return;
    
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
    
    if (!self.delegate)
        return;
    
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
@property (nonatomic, retain) CALayer* videoSublayer;
@end

@implementation WebAVVideoLayer
{
    RetainPtr<WebAVPlayerController> _avPlayerController;
    RetainPtr<AVPlayerViewController> _avPlayerViewController;
    RetainPtr<CALayer> _videoSublayer;
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

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
    [super dealloc];
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

- (void)setVideoSublayer:(CALayer *)videoSublayer
{
    _videoSublayer = videoSublayer;
    [self addSublayer:videoSublayer];
}

- (CALayer*)videoSublayer
{
    return _videoSublayer.get();
}

- (void)setBounds:(CGRect)bounds
{
    [super setBounds:bounds];

    [_videoSublayer setPosition:CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds))];

    if (![_avPlayerController delegate] || !_avPlayerViewController)
        return;

    FloatRect videoFrame = [_avPlayerController delegate]->videoLayerFrame();
    FloatRect targetFrame;
    switch ([_avPlayerController delegate]->videoLayerGravity()) {
    case WebCore::WebVideoFullscreenModel::VideoGravityResize:
        targetFrame = bounds;
        break;
    case WebCore::WebVideoFullscreenModel::VideoGravityResizeAspect:
        targetFrame = largestRectWithAspectRatioInsideRect(videoFrame.size().aspectRatio(), bounds);
        break;
    case WebCore::WebVideoFullscreenModel::VideoGravityResizeAspectFill:
        targetFrame = smallestRectWithAspectRatioAroundRect(videoFrame.size().aspectRatio(), bounds);
        break;
    }
    CATransform3D transform = CATransform3DMakeScale(targetFrame.width() / videoFrame.width(), targetFrame.height() / videoFrame.height(), 1);
    [_videoSublayer setSublayerTransform:transform];

    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
    [self performSelector:@selector(resolveBounds) withObject:nil afterDelay:[CATransaction animationDuration] + 0.1];
}

- (void)resolveBounds
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
    if (!_avPlayerController || ![_avPlayerController delegate])
        return;

    [CATransaction begin];
    [CATransaction setAnimationDuration:0];

    [_videoSublayer setSublayerTransform:CATransform3DIdentity];
    [_avPlayerController delegate]->setVideoLayerFrame([self bounds]);

    [CATransaction commit];
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
    : m_playerController(adoptNS([[WebAVPlayerController alloc] init]))
    , m_videoFullscreenModel(nullptr)
    , m_fullscreenChangeObserver(nullptr)
    , m_mode(HTMLMediaElement::VideoFullscreenModeNone)
    , m_exitRequested(false)
    , m_exitCompleted(false)
    , m_enterRequested(false)
{
    [m_playerController setFullscreenInterface:this];
}

void WebVideoFullscreenInterfaceAVKit::resetMediaState()
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [strongThis] {
        if (!strongThis->m_playerController) {
            strongThis->m_playerController = adoptNS([[WebAVPlayerController alloc] init]);
            [strongThis->m_playerController setDelegate:strongThis->m_videoFullscreenModel];
            [strongThis->m_playerController setFullscreenInterface:strongThis.get()];
            
        } else
            [strongThis->m_playerController resetState];
    });
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
        WebAVPlayerController* playerController = strongThis->m_playerController.get();

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=127017 use correct values instead of duration for all these
        playerController.contentDuration = duration;
        playerController.maxTime = duration;
        playerController.contentDurationWithinEndTimes = duration;

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
        NSTimeInterval anchorTimeStamp = ![strongThis->m_playerController rate] ? NAN : anchorTime;
        AVValueTiming *timing = [getAVValueTimingClass() valueTimingWithAnchorValue:currentTime
            anchorTimeStamp:anchorTimeStamp rate:0];
        [strongThis->m_playerController setTiming:timing];
    });
}

void WebVideoFullscreenInterfaceAVKit::setBufferedTime(double bufferedTime)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);

    dispatch_async(dispatch_get_main_queue(), [strongThis, bufferedTime] {
        WebAVPlayerController* playerController = strongThis->m_playerController.get();
        double duration = playerController.contentDuration;
        double normalizedBufferedTime;
        if (!duration)
            normalizedBufferedTime = 0;
        else
            normalizedBufferedTime = bufferedTime / duration;
        playerController.loadedTimeRanges = @[@0, @(normalizedBufferedTime)];
    });
}

void WebVideoFullscreenInterfaceAVKit::setRate(bool isPlaying, float playbackRate)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [strongThis, isPlaying, playbackRate] {
        [strongThis->m_playerController setRate:isPlaying ? playbackRate : 0.];
    });
}

void WebVideoFullscreenInterfaceAVKit::setVideoDimensions(bool hasVideo, float width, float height)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [strongThis, hasVideo, width, height] {
        [strongThis->m_playerController setHasEnabledVideo:hasVideo];
        [strongThis->m_playerController setContentDimensions:CGSizeMake(width, height)];
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
        [strongThis->m_playerController setSeekableTimeRanges:seekableRanges.get()];
    });
}

void WebVideoFullscreenInterfaceAVKit::setCanPlayFastReverse(bool canPlayFastReverse)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    
    dispatch_async(dispatch_get_main_queue(), [strongThis, canPlayFastReverse] {
        [strongThis->m_playerController setCanScanBackward:canPlayFastReverse];
    });
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
        [strongThis->m_playerController setAudioMediaSelectionOptions:webOptions.get()];
        if (selectedIndex < [webOptions count])
            [strongThis->m_playerController setCurrentAudioMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
    });
}

void WebVideoFullscreenInterfaceAVKit::setLegibleMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);

    dispatch_async(dispatch_get_main_queue(), [webOptions, strongThis, selectedIndex] {
        [strongThis->m_playerController setLegibleMediaSelectionOptions:webOptions.get()];
        if (selectedIndex < [webOptions count])
            [strongThis->m_playerController setCurrentLegibleMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
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
        WebAVPlayerController* playerController = strongThis->m_playerController.get();
        playerController.externalPlaybackAirPlayDeviceLocalizedName = localizedDeviceName;
        playerController.externalPlaybackType = externalPlaybackType;
        playerController.externalPlaybackActive = enabled;
        [strongThis->m_videoLayerContainer.get() setHidden:enabled];
    });
}

void WebVideoFullscreenInterfaceAVKit::setupFullscreen(PlatformLayer& videoLayer, const WebCore::IntRect& initialRect, UIView* parentView, HTMLMediaElement::VideoFullscreenMode mode, bool allowOptimizedFullscreen)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);

    ASSERT(mode != HTMLMediaElement::VideoFullscreenModeNone);
    m_videoLayer = &videoLayer;

    m_mode = mode;

    dispatch_async(dispatch_get_main_queue(), [strongThis, &videoLayer, initialRect, parentView, mode, allowOptimizedFullscreen] {
        strongThis->setupFullscreenInternal(videoLayer, initialRect, parentView, mode, allowOptimizedFullscreen);
    });
}

void WebVideoFullscreenInterfaceAVKit::setupFullscreenInternal(PlatformLayer& videoLayer, const WebCore::IntRect& initialRect, UIView* parentView, HTMLMediaElement::VideoFullscreenMode mode, bool allowOptimizedFullscreen)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::setupFullscreenInternal(%p)", this);
    UNUSED_PARAM(videoLayer);
    UNUSED_PARAM(mode);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    m_parentView = parentView;
    m_parentWindow = parentView.window;

    if (!applicationIsAdSheet()) {
        m_window = adoptNS([allocUIWindowInstance() initWithFrame:[[getUIScreenClass() mainScreen] bounds]]);
        [m_window setBackgroundColor:[getUIColorClass() clearColor]];
        m_viewController = adoptNS([allocUIViewControllerInstance() init]);
        [[m_viewController view] setFrame:[m_window bounds]];
        [m_window setRootViewController:m_viewController.get()];
        [m_window makeKeyAndVisible];
    }

    [m_videoLayer removeFromSuperlayer];

    m_videoLayerContainer = [WebAVVideoLayer videoLayer];
    [m_videoLayerContainer setHidden:[m_playerController isExternalPlaybackActive]];
    [m_videoLayerContainer setVideoSublayer:m_videoLayer.get()];

    CGSize videoSize = [m_playerController contentDimensions];
    CGRect videoRect = CGRectMake(0, 0, videoSize.width, videoSize.height);
    [m_videoLayerContainer setVideoRect:videoRect];
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->setVideoLayerFrame(videoRect);

    m_playerViewController = adoptNS([allocAVPlayerViewControllerInstance() initWithVideoLayer:m_videoLayerContainer.get()]);
    [m_playerViewController setShowsPlaybackControls:NO];
    [m_playerViewController setPlayerController:(AVPlayerController *)m_playerController.get()];
    [m_playerViewController setDelegate:m_playerController.get()];
    [m_playerViewController setAllowsOptimizedFullscreen:allowOptimizedFullscreen];

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
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterFullscreen(%p)", this);

    m_exitCompleted = false;
    m_exitRequested = false;
    m_enterRequested = true;

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis] {
        [strongThis->m_videoLayerContainer setBackgroundColor:[[getUIColorClass() blackColor] CGColor]];
        if (strongThis->mode() == HTMLMediaElement::VideoFullscreenModeOptimized)
            strongThis->enterFullscreenOptimized();
        else if (strongThis->mode() == HTMLMediaElement::VideoFullscreenModeStandard)
            strongThis->enterFullscreenStandard();
        else
            ASSERT_NOT_REACHED();
    });
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreenOptimized()
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);

    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterFullscreenOptimized(%p)", this);
    [m_playerViewController startOptimizedFullscreen];
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreenStandard()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterFullscreenStandard(%p)", this);
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    [m_playerViewController enterFullScreenWithCompletionHandler:[this, strongThis] (BOOL succeeded, NSError*) {
        UNUSED_PARAM(succeeded);
        LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterFullscreenStandard - lambda(%p) - succeeded(%s)", this, boolString(succeeded));
        [m_playerViewController setShowsPlaybackControls:YES];

        WebThreadRun([this, strongThis] {
            if (m_fullscreenChangeObserver)
                m_fullscreenChangeObserver->didEnterFullscreen();
        });
    }];
}

void WebVideoFullscreenInterfaceAVKit::exitFullscreen(const WebCore::IntRect& finalRect)
{
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);

    m_exitRequested = true;
    if (m_exitCompleted) {
        WebThreadRun([strongThis] {
            if (strongThis->m_fullscreenChangeObserver)
                strongThis->m_fullscreenChangeObserver->didExitFullscreen();
        });
        return;
    }

    dispatch_async(dispatch_get_main_queue(), [strongThis, finalRect] {
        strongThis->exitFullscreenInternal(finalRect);
    });
}

void WebVideoFullscreenInterfaceAVKit::exitFullscreenInternal(const WebCore::IntRect& finalRect)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::exitFullscreenInternal(%p)", this);
    [m_playerViewController setShowsPlaybackControls:NO];
    if (m_viewController)
        [m_playerViewController view].frame = [m_parentView convertRect:finalRect toView:nil];
    else
        [m_playerViewController view].frame = finalRect;

    if ([m_videoLayerContainer videoLayerGravity] != AVVideoLayerGravityResizeAspect)
        [m_videoLayerContainer setVideoLayerGravity:AVVideoLayerGravityResizeAspect];
    [[m_playerViewController view] layoutIfNeeded];


    if (isMode(HTMLMediaElement::VideoFullscreenModeOptimized)) {
        [m_window setHidden:NO];
        [m_playerViewController stopOptimizedFullscreen];
    } else if (isMode(HTMLMediaElement::VideoFullscreenModeOptimized | HTMLMediaElement::VideoFullscreenModeStandard)) {
        RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
        [m_playerViewController exitFullScreenAnimated:NO completionHandler:[strongThis] (BOOL, NSError*) {
            [strongThis->m_window setHidden:NO];
            [strongThis->m_playerViewController stopOptimizedFullscreen];
        }];
    } else if (isMode(HTMLMediaElement::VideoFullscreenModeStandard)) {
        RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
        [m_playerViewController exitFullScreenWithCompletionHandler:[strongThis] (BOOL, NSError*) {
            strongThis->m_exitCompleted = true;

            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            [strongThis->m_videoLayerContainer setBackgroundColor:[[getUIColorClass() clearColor] CGColor]];
            [[strongThis->m_playerViewController view] setBackgroundColor:[getUIColorClass() clearColor]];
            [CATransaction commit];

            WebThreadRun([strongThis] {
                if (strongThis->m_fullscreenChangeObserver)
                    strongThis->m_fullscreenChangeObserver->didExitFullscreen();
            });
        }];
    };
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
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::cleanupFullscreenInternal(%p)", this);
    if (m_window) {
        [m_window setHidden:YES];
        [m_window setRootViewController:nil];
        if (m_parentWindow)
            [[getUIApplicationClass() sharedApplication] _setStatusBarOrientation:[m_parentWindow interfaceOrientation]];
    }
    
    [m_playerController setDelegate:nil];
    [m_playerController setFullscreenInterface:nil];
    
    [m_playerViewController setDelegate:nil];
    [m_playerViewController setPlayerController:nil];
    
    if (hasMode(HTMLMediaElement::VideoFullscreenModeOptimized))
        [m_playerViewController cancelOptimizedFullscreen];
    if (hasMode(HTMLMediaElement::VideoFullscreenModeStandard))
        [m_playerViewController exitFullScreenAnimated:NO completionHandler:nil];
    
    [[m_playerViewController view] removeFromSuperview];
    if (m_viewController)
        [m_playerViewController removeFromParentViewController];
    
    [m_videoLayer removeFromSuperlayer];
    [m_videoLayerContainer removeFromSuperlayer];
    [m_videoLayerContainer setPlayerViewController:nil];
    [[m_viewController view] removeFromSuperview];
    
    m_videoLayer = nil;
    m_videoLayerContainer = nil;
    m_playerViewController = nil;
    m_playerController = nil;
    m_viewController = nil;
    m_window = nil;
    m_parentView = nil;
    m_parentWindow = nil;
    
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    WebThreadRun([strongThis] {
        if (strongThis->m_fullscreenChangeObserver)
            strongThis->m_fullscreenChangeObserver->didCleanupFullscreen();
        strongThis->m_enterRequested = false;
    });
}

void WebVideoFullscreenInterfaceAVKit::invalidate()
{
    m_videoFullscreenModel = nil;
    m_fullscreenChangeObserver = nil;
    
    cleanupFullscreenInternal();
}

void WebVideoFullscreenInterfaceAVKit::requestHideAndExitFullscreen()
{
    if (!m_enterRequested)
        return;
    
    if (hasMode(HTMLMediaElement::VideoFullscreenModeOptimized))
        return;
    
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::requestHideAndExitFullscreen(%p)", this);

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis] {
        [strongThis->m_window setHidden:YES];
    });

    if (m_videoFullscreenModel && !m_exitRequested) {
        m_videoFullscreenModel->pause();
        m_videoFullscreenModel->requestExitFullscreen();
    }
}

void WebVideoFullscreenInterfaceAVKit::preparedToReturnToInline(bool visible, const IntRect& inlineRect)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::preparedToReturnToInline(%p) - visible(%s)", this, boolString(visible));
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, visible, inlineRect] {
        if (strongThis->m_prepareToInlineCallback) {
            
            if (strongThis->m_viewController)
                [strongThis->m_playerViewController view].frame = [strongThis->m_parentView convertRect:inlineRect toView:nil];
            else
                [strongThis->m_playerViewController view].frame = inlineRect;

            std::function<void(bool)> callback = WTF::move(strongThis->m_prepareToInlineCallback);
            callback(visible);
        }
    });
}

bool WebVideoFullscreenInterfaceAVKit::mayAutomaticallyShowVideoOptimized() const
{
    return [m_playerController isPlaying] && m_mode == HTMLMediaElement::VideoFullscreenModeStandard && wkIsOptimizedFullscreenSupported();
}

void WebVideoFullscreenInterfaceAVKit::fullscreenMayReturnToInline(std::function<void(bool)> callback)
{
    m_prepareToInlineCallback = callback;
    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->fullscreenMayReturnToInline();
}

void WebVideoFullscreenInterfaceAVKit::willStartOptimizedFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::willStartOptimizedFullscreen(%p)", this);
    setMode(HTMLMediaElement::VideoFullscreenModeOptimized);

    if (!hasMode(HTMLMediaElement::VideoFullscreenModeStandard))
        return;

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    fullscreenMayReturnToInline([strongThis](bool visible) {
        LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::willStartOptimizedFullscreen - lambda(%p) - visible(%s)", strongThis.get(), boolString(visible));

        if (!visible) {
            [strongThis->m_window setHidden:YES];
            return;
        }

        [[strongThis->m_playerViewController view] layoutIfNeeded];

        [strongThis->m_playerViewController exitFullScreenAnimated:YES completionHandler:[strongThis] (BOOL completed, NSError*) {
            if (!completed)
                return;
            strongThis->clearMode(HTMLMediaElement::VideoFullscreenModeStandard);
            [strongThis->m_window setHidden:YES];
        }];
    });
}

void WebVideoFullscreenInterfaceAVKit::didStartOptimizedFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::didStartOptimizedFullscreen(%p)", this);
    [m_playerViewController setShowsPlaybackControls:YES];

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    WebThreadRun([strongThis] {
        [strongThis->m_window setHidden:YES];
        if (strongThis->m_fullscreenChangeObserver)
            strongThis->m_fullscreenChangeObserver->didEnterFullscreen();
    });
}

void WebVideoFullscreenInterfaceAVKit::willStopOptimizedFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::willStopOptimizedFullscreen(%p)", this);
    [m_window setHidden:NO];

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    WebThreadRun([strongThis] {
        if (strongThis->m_videoFullscreenModel)
            strongThis->m_videoFullscreenModel->requestExitFullscreen();
    });
}

void WebVideoFullscreenInterfaceAVKit::didStopOptimizedFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::didStopOptimizedFullscreen(%p)", this);
    if (hasMode(HTMLMediaElement::VideoFullscreenModeStandard))
        return;

    m_exitCompleted = true;

    [m_videoLayerContainer setBackgroundColor:[[getUIColorClass() clearColor] CGColor]];
    [[m_playerViewController view] setBackgroundColor:[getUIColorClass() clearColor]];

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    WebThreadRun([strongThis] {
        strongThis->clearMode(HTMLMediaElement::VideoFullscreenModeOptimized);
        [strongThis->m_window setHidden:YES];
        if (strongThis->m_fullscreenChangeObserver)
            strongThis->m_fullscreenChangeObserver->didExitFullscreen();
    });
}

void WebVideoFullscreenInterfaceAVKit::willCancelOptimizedFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::willCancelOptimizedFullscreen(%p)", this);
    [m_window setHidden:NO];
    [m_videoLayerContainer setBackgroundColor:[[getUIColorClass() clearColor] CGColor]];
    [[m_playerViewController view] setBackgroundColor:[getUIColorClass() clearColor]];

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    WebThreadRun([strongThis] {
        if (strongThis->m_videoFullscreenModel)
            strongThis->m_videoFullscreenModel->requestExitFullscreen();
    });
}

void WebVideoFullscreenInterfaceAVKit::didCancelOptimizedFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::didCancelOptimizedFullscreen(%p)", this);
    if (hasMode(HTMLMediaElement::VideoFullscreenModeStandard))
        return;

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    WebThreadRun([strongThis] {
        strongThis->clearMode(HTMLMediaElement::VideoFullscreenModeOptimized);
        [strongThis->m_window setHidden:YES];
        if (strongThis->m_fullscreenChangeObserver)
            strongThis->m_fullscreenChangeObserver->didExitFullscreen();
    });
}

void WebVideoFullscreenInterfaceAVKit::prepareForOptimizedFullscreenStopWithCompletionHandler(void (^completionHandler)(BOOL restored))
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::prepareForOptimizedFullscreenStopWithCompletionHandler(%p)", this);
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    RetainPtr<id> strongCompletionHandler = adoptNS([completionHandler copy]);
    fullscreenMayReturnToInline([strongThis, strongCompletionHandler](bool restored)  {
        LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::prepareForOptimizedFullscreenStopWithCompletionHandler lambda(%p) - restored(%s)", strongThis.get(), boolString(restored));
        void (^completionHandler)(BOOL restored) = strongCompletionHandler.get();
        completionHandler(restored);
    });
}

void WebVideoFullscreenInterfaceAVKit::setMode(HTMLMediaElement::VideoFullscreenMode mode)
{
    HTMLMediaElement::VideoFullscreenMode newMode = m_mode | mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

void WebVideoFullscreenInterfaceAVKit::clearMode(HTMLMediaElement::VideoFullscreenMode mode)
{
    HTMLMediaElement::VideoFullscreenMode newMode = m_mode & ~mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}


#endif
