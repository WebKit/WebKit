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

#if PLATFORM(IOS)

#import "WebVideoFullscreenInterfaceAVKit.h"

#if HAVE(AVKIT)

#import "AVKitSPI.h"
#import "GeometryUtilities.h"
#import "Logging.h"
#import "RuntimeApplicationChecksIOS.h"
#import "TimeRanges.h"
#import "WebCoreSystemInterface.h"
#import "WebVideoFullscreenModel.h"
#import <AVFoundation/AVTime.h>
#import <UIKit/UIKit.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

// Soft-linking headers must be included last since they #define functions, constants, etc.
#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVPlayerLayer)
SOFT_LINK_CONSTANT(AVFoundation, AVLayerVideoGravityResize, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVLayerVideoGravityResizeAspect, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVLayerVideoGravityResizeAspectFill, NSString *)

SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVPictureInPictureController)
SOFT_LINK_CLASS(AVKit, AVPlayerController)
SOFT_LINK_CLASS(AVKit, AVPlayerViewController)
SOFT_LINK_CLASS(AVKit, AVValueTiming)
SOFT_LINK_CLASS(AVKit, __AVPlayerLayerView)

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

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebFullScreenVideoRootViewController.m>
#else
@interface WebFullScreenVideoRootViewController : UIViewController
- (instancetype)initWithSourceWindow:(UIWindow *)sourceWindow;
@end

static Class createFullScreenVideoRootViewControllerClass()
{
    Class newClass = objc_allocateClassPair(getUIViewControllerClass(), "WebFullScreenVideoRootViewController", 0);

    class_addMethod(newClass, @selector(initWithSourceWindow:), imp_implementationWithBlock(^(id self, UIWindow*){
        return [self init];
    }), "@@:@");

    objc_registerClassPair(newClass);
    return newClass;
}

static WebFullScreenVideoRootViewController *allocWebFullScreenVideoRootViewControllerInstance()
{
    static Class fullScreenVideoRootViewControllerClass = createFullScreenVideoRootViewControllerClass();
    return [fullScreenVideoRootViewControllerClass alloc];
}
#endif

static const double DefaultWatchdogTimerInterval = 1;

@class WebAVMediaSelectionOption;

@interface WebAVPlayerController : NSObject <AVPlayerViewControllerDelegate_WebKitOnly> {
    WebAVMediaSelectionOption *_currentAudioMediaSelectionOption;
    WebAVMediaSelectionOption *_currentLegibleMediaSelectionOption;
    BOOL _pictureInPictureInterrupted;
}

- (void)resetState;

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
@property BOOL allowsExternalPlayback;

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason;
@end

@implementation WebAVPlayerController

- (instancetype)init
{
    if (!(self = [super init]))
        return self;
    
    _pictureInPictureInterrupted = NO;
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

- (void)resetState
{
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

- (AVPlayer *) player
{
    return nil;
}

- (id)forwardingTargetForSelector:(SEL)selector
{
    UNUSED_PARAM(selector);
    return self.playerControllerProxy;
}

- (void)playerViewControllerWillStartPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->willStartPictureInPicture();
}

- (void)playerViewControllerDidStartPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->didStartPictureInPicture();
}

- (void)playerViewControllerFailedToStartPictureInPicture:(AVPlayerViewController *)playerViewController withError:(NSError *)error
{
    UNUSED_PARAM(playerViewController);
    UNUSED_PARAM(error);
    self.fullscreenInterface->failedToStartPictureInPicture();
}

- (void)playerViewControllerWillStopPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->willStopPictureInPicture();
}

- (void)playerViewControllerDidStopPictureInPicture:(AVPlayerViewController *)playerViewController
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->didStopPictureInPicture();
}

static WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason convertToExitFullScreenReason(AVPlayerViewControllerExitFullScreenReason reason)
{
    switch (reason) {
    case AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped:
        return WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason::DoneButtonTapped;
    case AVPlayerViewControllerExitFullScreenReasonFullScreenButtonTapped:
        return WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason::FullScreenButtonTapped;
    case AVPlayerViewControllerExitFullScreenReasonPictureInPictureStarted:
        return WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason::PictureInPictureStarted;
    case AVPlayerViewControllerExitFullScreenReasonPinchGestureHandled:
        return WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason::PinchGestureHandled;
    case AVPlayerViewControllerExitFullScreenReasonRemoteControlStopEventReceived:
        return WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason::RemoteControlStopEventReceived;
    }
}

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason
{
    UNUSED_PARAM(playerViewController);
    return self.fullscreenInterface->shouldExitFullscreenWithReason(convertToExitFullScreenReason(reason));
}

- (void)playerViewController:(AVPlayerViewController *)playerViewController restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:(void (^)(BOOL restored))completionHandler
{
    UNUSED_PARAM(playerViewController);
    self.fullscreenInterface->prepareForPictureInPictureStopWithCompletionHandler(completionHandler);
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

- (BOOL)isPictureInPicturePossible
{
    return self.fullscreenInterface->allowsPictureInPicturePlayback();
}

- (BOOL)isPictureInPictureInterrupted
{
    return _pictureInPictureInterrupted;
}

- (void)setPictureInPictureInterrupted:(BOOL)pictureInPictureInterrupted
{
    if (_pictureInPictureInterrupted != pictureInPictureInterrupted) {
        _pictureInPictureInterrupted = pictureInPictureInterrupted;
        if (pictureInPictureInterrupted)
            [self setPlaying:NO];
    }
}

@end

@interface WebAVMediaSelectionOption : NSObject
@property (retain) NSString *localizedDisplayName;
@end

@implementation WebAVMediaSelectionOption
@end

@interface WebAVPlayerLayer : CALayer
@property (nonatomic, retain) NSString *videoGravity;
@property (nonatomic, getter=isReadyForDisplay) BOOL readyForDisplay;
@property (nonatomic, retain) AVPlayerController *playerController;
@property (nonatomic, retain) CALayer *videoSublayer;
@property (nonatomic, copy, nullable) NSDictionary *pixelBufferAttributes;
@property CGSize videoDimensions;
@property CGRect modelVideoLayerFrame;
@end

@implementation WebAVPlayerLayer {
    RetainPtr<WebAVPlayerController> _avPlayerController;
    RetainPtr<CALayer> _videoSublayer;
    RetainPtr<NSString> _videoGravity;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setMasksToBounds:YES];
        _videoGravity = getAVLayerVideoGravityResizeAspect();
    }
    return self;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
    [super dealloc];
}

- (AVPlayerController *)playerController
{
    return (AVPlayerController *)_avPlayerController.get();
}

- (void)setPlayerController:(AVPlayerController *)playerController
{
    ASSERT(!playerController || [playerController isKindOfClass:[WebAVPlayerController class]]);
    _avPlayerController = (WebAVPlayerController *)playerController;
}

- (void)setVideoSublayer:(CALayer *)videoSublayer
{
    _videoSublayer = videoSublayer;
}

- (CALayer*)videoSublayer
{
    return _videoSublayer.get();
}

- (void)layoutSublayers
{
    if ([_videoSublayer superlayer] != self)
        return;

    if (![_avPlayerController delegate])
        return;

    [_videoSublayer setPosition:CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds))];

    FloatRect sourceVideoFrame;
    FloatRect targetVideoFrame;
    float videoAspectRatio = self.videoDimensions.width / self.videoDimensions.height;
    
    if ([getAVLayerVideoGravityResize() isEqualToString:self.videoGravity]) {
        sourceVideoFrame = self.modelVideoLayerFrame;
        targetVideoFrame = self.bounds;
    } else if ([getAVLayerVideoGravityResizeAspect() isEqualToString:self.videoGravity]) {
        sourceVideoFrame = largestRectWithAspectRatioInsideRect(videoAspectRatio, self.modelVideoLayerFrame);
        targetVideoFrame = largestRectWithAspectRatioInsideRect(videoAspectRatio, self.bounds);
    } else if ([getAVLayerVideoGravityResizeAspectFill() isEqualToString:self.videoGravity]) {
        sourceVideoFrame = smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.modelVideoLayerFrame);
        self.modelVideoLayerFrame = CGRectMake(0, 0, sourceVideoFrame.width(), sourceVideoFrame.height());
        [_avPlayerController delegate]->setVideoLayerFrame(self.modelVideoLayerFrame);
        targetVideoFrame = smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.bounds);
    } else
        ASSERT_NOT_REACHED();

    UIView *view = [_videoSublayer delegate];
    CGAffineTransform transform = CGAffineTransformMakeScale(targetVideoFrame.width() / sourceVideoFrame.width(), targetVideoFrame.height() / sourceVideoFrame.height());
    [view setTransform:transform];
    
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
    
    if (!CGAffineTransformEqualToTransform(CGAffineTransformIdentity, transform))
        [self performSelector:@selector(resolveBounds) withObject:nil afterDelay:[CATransaction animationDuration] + 0.1];
}

- (void)resolveBounds
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
    if (![_avPlayerController delegate])
        return;
    
    if ([_videoSublayer superlayer] != self)
        return;
    
    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];
    
    self.modelVideoLayerFrame = [self bounds];
    [_avPlayerController delegate]->setVideoLayerFrame(self.modelVideoLayerFrame);
    [(UIView *)[_videoSublayer delegate] setTransform:CGAffineTransformIdentity];
    
    [CATransaction commit];
}

- (void)setVideoGravity:(NSString *)videoGravity
{
    _videoGravity = videoGravity;
    
    if (![_avPlayerController delegate])
        return;

    WebCore::WebVideoFullscreenModel::VideoGravity gravity = WebCore::WebVideoFullscreenModel::VideoGravityResizeAspect;
    if (videoGravity == getAVLayerVideoGravityResize())
        gravity = WebCore::WebVideoFullscreenModel::VideoGravityResize;
    if (videoGravity == getAVLayerVideoGravityResizeAspect())
        gravity = WebCore::WebVideoFullscreenModel::VideoGravityResizeAspect;
    else if (videoGravity == getAVLayerVideoGravityResizeAspectFill())
        gravity = WebCore::WebVideoFullscreenModel::VideoGravityResizeAspectFill;
    else
        ASSERT_NOT_REACHED();
    
    [_avPlayerController delegate]->setVideoLayerGravity(gravity);
}

- (NSString *)videoGravity
{
    return _videoGravity.get();
}

- (CGRect)videoRect
{
    float videoAspectRatio = self.videoDimensions.width / self.videoDimensions.height;
    
    if ([getAVLayerVideoGravityResizeAspect() isEqualToString:self.videoGravity])
        return largestRectWithAspectRatioInsideRect(videoAspectRatio, self.bounds);
    if ([getAVLayerVideoGravityResizeAspectFill() isEqualToString:self.videoGravity])
        return smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.bounds);

    return self.bounds;
}

+ (NSSet *)keyPathsForValuesAffectingVideoRect
{
    return [NSSet setWithObjects:@"videoDimensions", @"videoGravity", nil];
}

@end

@interface WebAVPictureInPicturePlayerLayerView : UIView
@end

static CALayer* WebAVPictureInPicturePlayerLayerView_layerClass(id, SEL)
{
    return [WebAVPlayerLayer class];
}

static Class getWebAVPictureInPicturePlayerLayerViewClass()
{
    static Class theClass = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        theClass = objc_allocateClassPair(getUIViewClass(), "WebAVPictureInPicturePlayerLayerView", 0);
        objc_registerClassPair(theClass);
        Class metaClass = objc_getMetaClass("WebAVPictureInPicturePlayerLayerView");
        class_addMethod(metaClass, @selector(layerClass), (IMP)WebAVPictureInPicturePlayerLayerView_layerClass, "@@:");
    });
    
    return theClass;
}

@interface WebAVPlayerLayerView : __AVPlayerLayerView
@property (retain) UIView* videoView;
@end

static CALayer *WebAVPlayerLayerView_layerClass(id, SEL)
{
    return [WebAVPlayerLayer class];
}

static AVPlayerController *WebAVPlayerLayerView_playerController(id aSelf, SEL)
{
    __AVPlayerLayerView *playerLayer = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayer playerLayer];
    return [webAVPlayerLayer playerController];
}

static void WebAVPlayerLayerView_setPlayerController(id aSelf, SEL, AVPlayerController *playerController)
{
    __AVPlayerLayerView *playerLayerView = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    [webAVPlayerLayer setPlayerController: playerController];
}

static UIView *WebAVPlayerLayerView_videoView(id aSelf, SEL)
{
    __AVPlayerLayerView *playerLayer = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayer playerLayer];
    CALayer* videoLayer = [webAVPlayerLayer videoSublayer];
    if (!videoLayer)
        return nil;
    ASSERT([[videoLayer delegate] isKindOfClass:getUIViewClass()]);
    return (UIView *)[videoLayer delegate];
}

static void WebAVPlayerLayerView_setVideoView(id aSelf, SEL, UIView *videoView)
{
    __AVPlayerLayerView *playerLayerView = aSelf;
    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    [webAVPlayerLayer setVideoSublayer:[videoView layer]];
}

static void WebAVPlayerLayerView_startRoutingVideoToPictureInPicturePlayerLayerView(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    WebAVPictureInPicturePlayerLayerView *pipView = (WebAVPictureInPicturePlayerLayerView *)[playerLayerView pictureInPicturePlayerLayerView];

    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    WebAVPlayerLayer *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [playerLayer setVideoGravity:getAVLayerVideoGravityResizeAspect()];
    [pipPlayerLayer setVideoSublayer:playerLayer.videoSublayer];
    [pipPlayerLayer setVideoDimensions:playerLayer.videoDimensions];
    [pipPlayerLayer setVideoGravity:playerLayer.videoGravity];
    [pipPlayerLayer setModelVideoLayerFrame:playerLayer.modelVideoLayerFrame];
    [pipPlayerLayer setPlayerController:playerLayer.playerController];
    [pipView addSubview:playerLayerView.videoView];
}

static void WebAVPlayerLayerView_stopRoutingVideoToPictureInPicturePlayerLayerView(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    [playerLayerView addSubview:playerLayerView.videoView];
    WebAVPictureInPicturePlayerLayerView *pipView = (WebAVPictureInPicturePlayerLayerView *)[playerLayerView pictureInPicturePlayerLayerView];
    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[playerLayerView playerLayer];
    WebAVPlayerLayer *pipPlayerLayer = (WebAVPlayerLayer *)[pipView layer];
    [playerLayer setModelVideoLayerFrame:pipPlayerLayer.modelVideoLayerFrame];
}

static WebAVPictureInPicturePlayerLayerView *WebAVPlayerLayerView_pictureInPicturePlayerLayerView(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    WebAVPictureInPicturePlayerLayerView *pipView = [playerLayerView valueForKey:@"_pictureInPicturePlayerLayerView"];
    if (!pipView) {
        pipView = [[getWebAVPictureInPicturePlayerLayerViewClass() alloc] initWithFrame:CGRectZero];
        [playerLayerView setValue:pipView forKey:@"_pictureInPicturePlayerLayerView"];
    }
    return pipView;
}

static void WebAVPlayerLayerView_dealloc(id aSelf, SEL)
{
    WebAVPlayerLayerView *playerLayerView = aSelf;
    RetainPtr<WebAVPictureInPicturePlayerLayerView> pipView = adoptNS([playerLayerView valueForKey:@"_pictureInPicturePlayerLayerView"]);
    [playerLayerView setValue:nil forKey:@"_pictureInPicturePlayerLayerView"];
    objc_super superClass { playerLayerView, get__AVPlayerLayerViewClass() };
    auto super_dealloc = reinterpret_cast<void(*)(objc_super*, SEL)>(objc_msgSendSuper);
    super_dealloc(&superClass, @selector(dealloc));
}

#pragma mark - Methods

static Class getWebAVPlayerLayerViewClass()
{
    static Class theClass = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        theClass = objc_allocateClassPair(get__AVPlayerLayerViewClass(), "WebAVPlayerLayerView", 0);
        class_addMethod(theClass, @selector(dealloc), (IMP)WebAVPlayerLayerView_dealloc, "v@:");
        class_addMethod(theClass, @selector(setPlayerController:), (IMP)WebAVPlayerLayerView_setPlayerController, "v@:@");
        class_addMethod(theClass, @selector(playerController), (IMP)WebAVPlayerLayerView_playerController, "@@:");
        class_addMethod(theClass, @selector(setVideoView:), (IMP)WebAVPlayerLayerView_setVideoView, "v@:@");
        class_addMethod(theClass, @selector(videoView), (IMP)WebAVPlayerLayerView_videoView, "@@:");
        class_addMethod(theClass, @selector(startRoutingVideoToPictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_startRoutingVideoToPictureInPicturePlayerLayerView, "v@:");
        class_addMethod(theClass, @selector(stopRoutingVideoToPictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_stopRoutingVideoToPictureInPicturePlayerLayerView, "v@:");
        class_addMethod(theClass, @selector(pictureInPicturePlayerLayerView), (IMP)WebAVPlayerLayerView_pictureInPicturePlayerLayerView, "@@:");
        
        class_addIvar(theClass, "_pictureInPicturePlayerLayerView", sizeof(WebAVPictureInPicturePlayerLayerView *), log2(sizeof(WebAVPictureInPicturePlayerLayerView *)), "@");
        
        objc_registerClassPair(theClass);
        Class metaClass = objc_getMetaClass("WebAVPlayerLayerView");
        class_addMethod(metaClass, @selector(layerClass), (IMP)WebAVPlayerLayerView_layerClass, "@@:");
    });
    return theClass;
}

WebVideoFullscreenInterfaceAVKit::WebVideoFullscreenInterfaceAVKit()
    : m_playerController(adoptNS([[WebAVPlayerController alloc] init]))
    , m_watchdogTimer(*this, &WebVideoFullscreenInterfaceAVKit::watchdogTimerFired)
{
    [m_playerController setFullscreenInterface:this];
}

WebVideoFullscreenInterfaceAVKit::~WebVideoFullscreenInterfaceAVKit()
{
    WebAVPlayerController* playerController = m_playerController.get();
    if (playerController && playerController.externalPlaybackActive)
        setExternalPlayback(false, TargetTypeNone, "");
}

void WebVideoFullscreenInterfaceAVKit::resetMediaState()
{
    if (!m_playerController) {
        m_playerController = adoptNS([[WebAVPlayerController alloc] init]);
        [m_playerController setDelegate:m_videoFullscreenModel];
        [m_playerController setFullscreenInterface:this];
        
    } else
        [m_playerController resetState];
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
    WebAVPlayerController* playerController = m_playerController.get();

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
}

void WebVideoFullscreenInterfaceAVKit::setCurrentTime(double currentTime, double anchorTime)
{
    NSTimeInterval anchorTimeStamp = ![m_playerController rate] ? NAN : anchorTime;
    AVValueTiming *timing = [getAVValueTimingClass() valueTimingWithAnchorValue:currentTime
        anchorTimeStamp:anchorTimeStamp rate:0];
    [m_playerController setTiming:timing];
}

void WebVideoFullscreenInterfaceAVKit::setBufferedTime(double bufferedTime)
{
    WebAVPlayerController* playerController = m_playerController.get();
    double duration = playerController.contentDuration;
    double normalizedBufferedTime;
    if (!duration)
        normalizedBufferedTime = 0;
    else
        normalizedBufferedTime = bufferedTime / duration;
    playerController.loadedTimeRanges = @[@0, @(normalizedBufferedTime)];
}

void WebVideoFullscreenInterfaceAVKit::setRate(bool isPlaying, float playbackRate)
{
    [m_playerController setRate:isPlaying ? playbackRate : 0.];
}

void WebVideoFullscreenInterfaceAVKit::setVideoDimensions(bool hasVideo, float width, float height)
{
    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];

    [playerLayer setVideoDimensions:CGSizeMake(width, height)];
    [m_playerController setHasEnabledVideo:hasVideo];
    [m_playerController setContentDimensions:CGSizeMake(width, height)];
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
    
    [m_playerController setSeekableTimeRanges:seekableRanges.get()];
}

void WebVideoFullscreenInterfaceAVKit::setCanPlayFastReverse(bool canPlayFastReverse)
{
    [m_playerController setCanScanBackward:canPlayFastReverse];
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
    [m_playerController setAudioMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [m_playerController setCurrentAudioMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void WebVideoFullscreenInterfaceAVKit::setLegibleMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [m_playerController setLegibleMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [m_playerController setCurrentLegibleMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void WebVideoFullscreenInterfaceAVKit::setExternalPlayback(bool enabled, ExternalPlaybackTargetType targetType, String localizedDeviceName)
{
    AVPlayerControllerExternalPlaybackType externalPlaybackType = AVPlayerControllerExternalPlaybackTypeNone;
    if (targetType == TargetTypeAirPlay)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeAirPlay;
    else if (targetType == TargetTypeTVOut)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeTVOut;

    WebAVPlayerController* playerController = m_playerController.get();
    playerController.externalPlaybackAirPlayDeviceLocalizedName = localizedDeviceName;
    playerController.externalPlaybackType = externalPlaybackType;
    playerController.externalPlaybackActive = enabled;
    [m_playerLayerView setHidden:enabled];
}

void WebVideoFullscreenInterfaceAVKit::setWirelessVideoPlaybackDisabled(bool disabled)
{
    [m_playerController setAllowsExternalPlayback:!disabled];
}

bool WebVideoFullscreenInterfaceAVKit::wirelessVideoPlaybackDisabled() const
{
    return [m_playerController allowsExternalPlayback];
}

void WebVideoFullscreenInterfaceAVKit::applicationDidBecomeActive()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::applicationDidBecomeActive(%p)", this);
    if (m_shouldReturnToFullscreenAfterEnteringForeground && m_videoFullscreenModel && m_videoFullscreenModel->isVisible())
        [m_playerViewController stopPictureInPicture];
}

@interface UIWindow ()
- (BOOL)_isHostedInAnotherProcess;
@end

@interface UIViewController ()
@property (nonatomic, assign, setter=_setIgnoreAppSupportedOrientations:) BOOL _ignoreAppSupportedOrientations;
@end

void WebVideoFullscreenInterfaceAVKit::setupFullscreen(UIView& videoView, const WebCore::IntRect& initialRect, UIView* parentView, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback)
{
    ASSERT(mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::setupFullscreen(%p)", this);

    m_allowsPictureInPicturePlayback = allowsPictureInPicturePlayback;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    bool isInPictureInPictureMode = hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
    m_mode = mode;
    m_parentView = parentView;
    m_parentWindow = parentView.window;

    if (![[parentView window] _isHostedInAnotherProcess]) {
        if (!m_window)
            m_window = adoptNS([allocUIWindowInstance() initWithFrame:[[getUIScreenClass() mainScreen] bounds]]);
        [m_window setBackgroundColor:[getUIColorClass() clearColor]];
        if (!m_viewController)
            m_viewController = adoptNS([allocWebFullScreenVideoRootViewControllerInstance() initWithSourceWindow:[parentView window]]);
        [[m_viewController view] setFrame:[m_window bounds]];
        [m_viewController _setIgnoreAppSupportedOrientations:YES];
        [m_window setRootViewController:m_viewController.get()];
        [m_window makeKeyAndVisible];
    }

    if (!m_playerLayerView)
        m_playerLayerView = adoptNS([[getWebAVPlayerLayerViewClass() alloc] init]);
    [m_playerLayerView setHidden:[m_playerController isExternalPlaybackActive]];
    [m_playerLayerView setBackgroundColor:[getUIColorClass() clearColor]];

    if (!isInPictureInPictureMode) {
        [m_playerLayerView setVideoView:&videoView];
        [m_playerLayerView addSubview:&videoView];
    }

    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];

    [playerLayer setModelVideoLayerFrame:CGRectMake(0, 0, initialRect.width(), initialRect.height())];
    [playerLayer setVideoDimensions:[m_playerController contentDimensions]];

    if (!m_playerViewController)
        m_playerViewController = adoptNS([allocAVPlayerViewControllerInstance() initWithPlayerLayerView:m_playerLayerView.get()]);

    [m_playerViewController setShowsPlaybackControls:NO];
    [m_playerViewController setPlayerController:(AVPlayerController *)m_playerController.get()];
    [m_playerViewController setDelegate:m_playerController.get()];
    [m_playerViewController setAllowsPictureInPicturePlayback:m_allowsPictureInPicturePlayback];

    if (m_viewController) {
        [m_viewController addChildViewController:m_playerViewController.get()];
        [[m_viewController view] addSubview:[m_playerViewController view]];
    } else
        [parentView addSubview:[m_playerViewController view]];

    [m_playerViewController view].frame = [parentView convertRect:initialRect toView:[m_playerViewController view].superview];

    [[m_playerViewController view] setBackgroundColor:[getUIColorClass() clearColor]];
    [[m_playerViewController view] setAutoresizingMask:(UIViewAutoresizingFlexibleBottomMargin | UIViewAutoresizingFlexibleRightMargin)];

    [[m_playerViewController view] setNeedsLayout];
    [[m_playerViewController view] layoutIfNeeded];

    [CATransaction commit];

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    dispatch_async(dispatch_get_main_queue(), [strongThis, this] {
        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didSetupFullscreen();
    });
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterFullscreen(%p)", this);

    m_exitCompleted = false;
    m_exitRequested = false;
    m_enterRequested = true;

    [m_playerLayerView setBackgroundColor:[getUIColorClass() blackColor]];
    if (mode() == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
        enterPictureInPicture();
    else if (mode() == HTMLMediaElementEnums::VideoFullscreenModeStandard)
        enterFullscreenStandard();
    else
        ASSERT_NOT_REACHED();
}

void WebVideoFullscreenInterfaceAVKit::enterPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterPictureInPicture(%p)", this);
    
    if ([m_playerViewController isPictureInPicturePossible])
        [m_playerViewController startPictureInPicture];
    else
        failedToStartPictureInPicture();
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreenStandard()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterFullscreenStandard(%p)", this);
    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);

    if ([m_playerViewController isPictureInPictureActive]) {
        // NOTE: The fullscreen mode will be restored in prepareForPictureInPictureStopWithCompletionHandler().
        m_shouldReturnToFullscreenWhenStoppingPiP = true;
        [m_playerViewController stopPictureInPicture];
        return;
    }

    [m_playerLayerView setBackgroundColor:[getUIColorClass() blackColor]];
    [m_playerViewController enterFullScreenAnimated:YES completionHandler:[this, strongThis] (BOOL succeeded, NSError*) {
        UNUSED_PARAM(succeeded);
        LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::enterFullscreenStandard - lambda(%p) - succeeded(%s)", this, boolString(succeeded));
        [m_playerViewController setShowsPlaybackControls:YES];

        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didEnterFullscreen();
    }];
}

void WebVideoFullscreenInterfaceAVKit::exitFullscreen(const WebCore::IntRect& finalRect)
{
    m_watchdogTimer.stop();

    m_exitRequested = true;
    if (m_exitCompleted) {
        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didExitFullscreen();
        return;
    }
    
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::exitFullscreen(%p)", this);
    [m_playerViewController setShowsPlaybackControls:NO];
    
    [m_playerViewController view].frame = [m_parentView convertRect:finalRect toView:[m_playerViewController view].superview];

    WebAVPlayerLayer *playerLayer = (WebAVPlayerLayer *)[m_playerLayerView playerLayer];
    if ([playerLayer videoGravity] != getAVLayerVideoGravityResizeAspect())
        [playerLayer setVideoGravity:getAVLayerVideoGravityResizeAspect()];
    [[m_playerViewController view] layoutIfNeeded];

    if (isMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)) {
        [m_window setHidden:NO];
        [m_playerViewController stopPictureInPicture];
    } else if (isMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture | HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
        RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
        [m_playerViewController exitFullScreenAnimated:NO completionHandler:[strongThis, this] (BOOL, NSError*) {
            [m_window setHidden:NO];
            [m_playerViewController stopPictureInPicture];
        }];
    } else if (isMode(HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
        RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
        [m_playerViewController exitFullScreenAnimated:YES completionHandler:[strongThis, this] (BOOL, NSError*) {
            m_exitCompleted = true;

            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            [m_playerLayerView setBackgroundColor:[getUIColorClass() clearColor]];
            [[m_playerViewController view] setBackgroundColor:[getUIColorClass() clearColor]];
            [CATransaction commit];

            dispatch_async(dispatch_get_main_queue(), ^{
                if (m_fullscreenChangeObserver)
                    m_fullscreenChangeObserver->didExitFullscreen();
            });
        }];
    };
}

@interface UIApplication ()
- (void)_setStatusBarOrientation:(UIInterfaceOrientation)o;
@end

@interface UIWindow ()
- (UIInterfaceOrientation)interfaceOrientation;
@end

void WebVideoFullscreenInterfaceAVKit::cleanupFullscreen()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::cleanupFullscreen(%p)", this);
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
    
    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture))
        [m_playerViewController stopPictureInPicture];
    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard))
        [m_playerViewController exitFullScreenAnimated:NO completionHandler:[] (BOOL, NSError *) { }];
    
    [[m_playerViewController view] removeFromSuperview];
    if (m_viewController)
        [m_playerViewController removeFromParentViewController];
    
    [m_playerLayerView removeFromSuperview];
    [[m_viewController view] removeFromSuperview];

    m_playerLayerView = nil;
    m_playerViewController = nil;
    m_playerController = nil;
    m_viewController = nil;
    m_window = nil;
    m_parentView = nil;
    m_parentWindow = nil;
    
    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didCleanupFullscreen();

    m_enterRequested = false;
}

void WebVideoFullscreenInterfaceAVKit::invalidate()
{
    m_videoFullscreenModel = nil;
    m_fullscreenChangeObserver = nil;
    
    cleanupFullscreen();
}

void WebVideoFullscreenInterfaceAVKit::requestHideAndExitFullscreen()
{
    if (!m_enterRequested)
        return;
    
    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture))
        return;
    
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::requestHideAndExitFullscreen(%p)", this);

    [m_window setHidden:YES];
    [[m_playerViewController view] setHidden:YES];

    if (m_videoFullscreenModel && !m_exitRequested) {
        m_videoFullscreenModel->pause();
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
    }
}

void WebVideoFullscreenInterfaceAVKit::preparedToReturnToInline(bool visible, const IntRect& inlineRect)
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::preparedToReturnToInline(%p) - visible(%s)", this, boolString(visible));
    if (m_prepareToInlineCallback) {
        
        [m_playerViewController view].frame = [m_parentView convertRect:inlineRect toView:[m_playerViewController view].superview];

        std::function<void(bool)> callback = WTF::move(m_prepareToInlineCallback);
        callback(visible);
    }
}

bool WebVideoFullscreenInterfaceAVKit::mayAutomaticallyShowVideoPictureInPicture() const
{
    return [m_playerController isPlaying] && m_mode == HTMLMediaElementEnums::VideoFullscreenModeStandard && supportsPictureInPicture();
}

void WebVideoFullscreenInterfaceAVKit::fullscreenMayReturnToInline(std::function<void(bool)> callback)
{
    m_prepareToInlineCallback = callback;
    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->fullscreenMayReturnToInline();
}

void WebVideoFullscreenInterfaceAVKit::willStartPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::willStartPictureInPicture(%p)", this);
    setMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
}

void WebVideoFullscreenInterfaceAVKit::didStartPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::didStartPictureInPicture(%p)", this);
    m_shouldReturnToFullscreenAfterEnteringForeground = [m_playerViewController pictureInPictureWasStartedWhenEnteringBackground];
    [m_playerViewController setShowsPlaybackControls:YES];

    if (m_mode & HTMLMediaElementEnums::VideoFullscreenModeStandard) {
        RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
        [m_playerViewController exitFullScreenAnimated:YES completionHandler:[strongThis, this] (BOOL, NSError*) {
            [m_window setHidden:YES];
            [[m_playerViewController view] setHidden:YES];
        }];
    } else {
        [m_window setHidden:YES];
        [[m_playerViewController view] setHidden:YES];
    }

    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didEnterFullscreen();
}

void WebVideoFullscreenInterfaceAVKit::failedToStartPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::failedToStartPictureInPicture(%p)", this);
    [m_playerViewController setShowsPlaybackControls:YES];

    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard))
        return;

    m_exitCompleted = true;

    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didEnterFullscreen();

    if (m_videoFullscreenModel)
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
}

void WebVideoFullscreenInterfaceAVKit::willStopPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::willStopPictureInPicture(%p)", this);
    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard))
        return;

    [m_window setHidden:NO];
    [[m_playerViewController view] setHidden:NO];

    if (m_videoFullscreenModel)
        m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);
}

void WebVideoFullscreenInterfaceAVKit::didStopPictureInPicture()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::didStopPictureInPicture(%p)", this);
    if (hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
        // ASSUMPTION: we are exiting pip because we are entering fullscreen
        clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
        [m_playerViewController setShowsPlaybackControls:YES];

        if (m_fullscreenChangeObserver)
            m_fullscreenChangeObserver->didEnterFullscreen();
        return;
    }

    m_exitCompleted = true;

    [m_playerLayerView setBackgroundColor:[getUIColorClass() clearColor]];
    [[m_playerViewController view] setBackgroundColor:[getUIColorClass() clearColor]];

    clearMode(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
    [m_window setHidden:YES];
    [[m_playerViewController view] setHidden:YES];
    
    if (m_fullscreenChangeObserver)
        m_fullscreenChangeObserver->didExitFullscreen();
}

void WebVideoFullscreenInterfaceAVKit::prepareForPictureInPictureStopWithCompletionHandler(void (^completionHandler)(BOOL restored))
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::prepareForPictureInPictureStopWithCompletionHandler(%p)", this);
    if (m_shouldReturnToFullscreenWhenStoppingPiP || m_shouldReturnToFullscreenAfterEnteringForeground) {
        m_shouldReturnToFullscreenWhenStoppingPiP = false;
        m_shouldReturnToFullscreenAfterEnteringForeground = false;

        // ASSUMPTION: we are exiting pip because we are entering fullscreen
        [m_window setHidden:NO];
        [[m_playerViewController view] setHidden:NO];

        [m_playerViewController enterFullScreenAnimated:YES completionHandler:^(BOOL success, NSError*) {
            setMode(HTMLMediaElementEnums::VideoFullscreenModeStandard);
            completionHandler(success);
        }];
        return;
    }

    RefPtr<WebVideoFullscreenInterfaceAVKit> strongThis(this);
    RetainPtr<id> strongCompletionHandler = adoptNS([completionHandler copy]);
    fullscreenMayReturnToInline([strongThis, strongCompletionHandler](bool restored)  {
        LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::prepareForPictureInPictureStopWithCompletionHandler lambda(%p) - restored(%s)", strongThis.get(), boolString(restored));
        void (^completionHandler)(BOOL restored) = strongCompletionHandler.get();
        completionHandler(restored);
    });
}

bool WebVideoFullscreenInterfaceAVKit::shouldExitFullscreenWithReason(WebVideoFullscreenInterfaceAVKit::ExitFullScreenReason reason)
{
    if (!m_videoFullscreenModel)
        return true;

    if (reason == ExitFullScreenReason::PictureInPictureStarted) {
        m_shouldReturnToFullscreenWhenStoppingPiP = hasMode(HTMLMediaElementEnums::VideoFullscreenModeStandard);
        clearMode(HTMLMediaElementEnums::VideoFullscreenModeStandard);
        return true;
    }

    if (reason == ExitFullScreenReason::DoneButtonTapped || reason == ExitFullScreenReason::RemoteControlStopEventReceived)
        m_videoFullscreenModel->pause();
    

    m_videoFullscreenModel->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone);

    if (!m_watchdogTimer.isActive())
        m_watchdogTimer.startOneShot(DefaultWatchdogTimerInterval);

    return false;
}

NO_RETURN_DUE_TO_ASSERT void WebVideoFullscreenInterfaceAVKit::watchdogTimerFired()
{
    LOG(Fullscreen, "WebVideoFullscreenInterfaceAVKit::watchdogTimerFired(%p) - no exit fullscreen response in %gs; forcing exit", this);
    ASSERT_NOT_REACHED();
    exitFullscreen(IntRect());
}

void WebVideoFullscreenInterfaceAVKit::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode | mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

void WebVideoFullscreenInterfaceAVKit::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode & ~mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

#endif // HAVE(AVKIT)

bool WebCore::supportsPictureInPicture()
{
#if PLATFORM(IOS) && HAVE(AVKIT)
    return [getAVPictureInPictureControllerClass() isPictureInPictureSupported];
#else
    return false;
#endif
}

#endif // PLATFORM(IOS)
