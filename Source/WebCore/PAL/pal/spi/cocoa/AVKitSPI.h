/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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

#pragma once

DECLARE_SYSTEM_HEADER

#import <objc/runtime.h>
#import <wtf/SoftLinking.h>

#if PLATFORM(IOS_FAMILY)
#import <AVFoundation/AVPlayer.h>
#import <AVKit/AVKit.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#endif

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV) && USE(APPLE_INTERNAL_SDK)
#import <AVKit/AVValueTiming.h>
#else
NS_ASSUME_NONNULL_BEGIN

@interface AVValueTiming : NSObject <NSCoding, NSCopying, NSMutableCopying>
@end

@interface AVValueTiming ()
+ (AVValueTiming *)valueTimingWithAnchorValue:(double)anchorValue anchorTimeStamp:(NSTimeInterval)timeStamp rate:(double)rate;
@property (NS_NONATOMIC_IOSONLY, readonly) double currentValue;
@property (NS_NONATOMIC_IOSONLY, readonly) double rate;
@property (NS_NONATOMIC_IOSONLY, readonly) NSTimeInterval anchorTimeStamp;
@property (NS_NONATOMIC_IOSONLY, readonly) double anchorValue;

+ (NSTimeInterval)currentTimeStamp;
- (double)valueForTimeStamp:(NSTimeInterval)timeStamp;
@end

NS_ASSUME_NONNULL_END
#endif


#if USE(APPLE_INTERNAL_SDK)

#if PLATFORM(IOS_FAMILY)

#import <AVKit/AVPlayerViewController_Private.h>

#if HAVE(AVPLAYERCONTROLLER)
#import <AVKit/AVPlayerController.h>
#endif

#if HAVE(AVPLAYERLAYERVIEW)
IGNORE_WARNINGS_BEGIN("objc-property-no-attribute")
#import <AVKit/AVPlayerLayerView.h>
IGNORE_WARNINGS_END
#endif

#if !PLATFORM(APPLETV)
#import <AVKit/AVPlayerViewController_WebKitOnly.h>
#endif

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(IOS) || PLATFORM(MACCATALYST) || PLATFORM(VISION)
#import <AVKit/AVBackgroundView.h>
#endif

#if PLATFORM(WATCHOS)
#import <AVFoundation/AVPlayerLayer.h>
NS_ASSUME_NONNULL_BEGIN

@interface AVPictureInPicturePlayerLayerView : UIView
@property (nonatomic, readonly) AVPlayerLayer *playerLayer;
@end

@interface __AVPlayerLayerView  (Details)
@property (nonatomic, readonly) AVPlayerLayer *playerLayer;
- (AVPictureInPicturePlayerLayerView*) pictureInPicturePlayerLayerView;
- (void)startRoutingVideoToPictureInPicturePlayerLayerView;
- (void)stopRoutingVideoToPictureInPicturePlayerLayerView;
@end

@class AVPlayerLayerView;
@interface AVPlayerViewController (AVPlayerViewController_WebKitOnly_Internal)
- (void)enterFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError * __nullable error))completionHandler;
- (void)exitFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError * __nullable error))completionHandler;
- (void)startPictureInPicture;
- (void)stopPictureInPicture;

@property (nonatomic) BOOL showsExitFullScreenButton;
@property (nonatomic, readonly, getter=isPictureInPicturePossible) BOOL pictureInPicturePossible;
@property (nonatomic, readonly, getter=isPictureInPictureActive) BOOL pictureInPictureActive;
@property (nonatomic, readonly, getter=isPictureInPictureSuspended) BOOL pictureInPictureSuspended;
@property (nonatomic, readonly) BOOL pictureInPictureWasStartedWhenEnteringBackground;
- (void)setWebKitOverrideRouteSharingPolicy:(NSUInteger)routeSharingPolicy routingContextUID:(NSString *)routingContextUID;
@end

@protocol AVPlayerViewControllerDelegate_WebKitOnly <AVPlayerViewControllerDelegate>
@optional
typedef NS_ENUM(NSInteger, AVPlayerViewControllerExitFullScreenReason) {
    AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped,
    AVPlayerViewControllerExitFullScreenReasonFullScreenButtonTapped,
    AVPlayerViewControllerExitFullScreenReasonPinchGestureHandled,
    AVPlayerViewControllerExitFullScreenReasonRemoteControlStopEventReceived,
    AVPlayerViewControllerExitFullScreenReasonPictureInPictureStarted
};
- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason;
@end

NS_ASSUME_NONNULL_END
#endif // PLATFORM(WATCHOS)

#if PLATFORM(APPLETV)
NS_ASSUME_NONNULL_BEGIN
@interface AVPlayerViewController (AVPlayerViewController_WebKitOnly_OverrideRouteSharingPolicy)
- (void)setWebKitOverrideRouteSharingPolicy:(NSUInteger)routeSharingPolicy routingContextUID:(NSString *)routingContextUID;
@end
NS_ASSUME_NONNULL_END
#endif

#if PLATFORM(MAC)
#import <AVKit/AVPlayerView_Private.h>
#endif

#else

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIResponder.h>
@interface AVPlayerController : UIResponder
@end
#else
#import <AppKit/NSResponder.h>
@protocol NSUserInterfaceValidations;
@interface AVPlayerController : NSResponder <NSUserInterfaceValidations>
@end
#endif

#if PLATFORM(WATCHOS)
#import <AVKit/AVPlayerViewController.h> // not part of AVKit's umbrella header
#endif

@interface AVPlayerController ()
typedef NS_ENUM(NSInteger, AVPlayerControllerStatus) {
    AVPlayerControllerStatusUnknown = 0,
    AVPlayerControllerStatusReadyToPlay = 2,
};

typedef NS_ENUM(NSInteger, AVPlayerControllerExternalPlaybackType) {
    AVPlayerControllerExternalPlaybackTypeNone = 0,
    AVPlayerControllerExternalPlaybackTypeAirPlay = 1,
    AVPlayerControllerExternalPlaybackTypeTVOut = 2,
};

@property (NS_NONATOMIC_IOSONLY, readonly) AVPlayerControllerStatus status;
@end

#if PLATFORM(IOS_FAMILY)
NS_ASSUME_NONNULL_BEGIN

@interface AVBackgroundView : UIView
@property (nonatomic) BOOL automaticallyDrawsRoundedCorners;
typedef NS_ENUM(NSInteger, AVBackgroundViewMaterialStyle) {
    AVBackgroundViewMaterialStylePrimary,
    AVBackgroundViewMaterialStyleSecondary
};
typedef NS_ENUM(NSInteger, AVBackgroundViewTintEffectStyle) {
    AVBackgroundViewTintEffectStylePrimary,
    AVBackgroundViewTintEffectStyleSecondary
};
- (void)addSubview:(UIView *)subview applyingMaterialStyle:(AVBackgroundViewMaterialStyle)materialStyle tintEffectStyle:(AVBackgroundViewTintEffectStyle)tintEffectStyle;
@end

@class AVPlayerLayer;

@interface AVPictureInPicturePlayerLayerView : UIView
@property (nonatomic, readonly) AVPlayerLayer *playerLayer;
@end

@interface __AVPlayerLayerView : UIView
@property (nonatomic, readonly) AVPlayerLayer *playerLayer;
@property (nonatomic, readonly) AVPictureInPicturePlayerLayerView *pictureInPicturePlayerLayerView;
- (void)startRoutingVideoToPictureInPicturePlayerLayerView;
- (void)stopRoutingVideoToPictureInPicturePlayerLayerView;
@end

@protocol AVPlayerViewControllerDelegate_WebKitOnly <AVPlayerViewControllerDelegate>
@optional
typedef NS_ENUM(NSInteger, AVPlayerViewControllerExitFullScreenReason) {
    AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped,
    AVPlayerViewControllerExitFullScreenReasonFullScreenButtonTapped,
    AVPlayerViewControllerExitFullScreenReasonPinchGestureHandled,
    AVPlayerViewControllerExitFullScreenReasonRemoteControlStopEventReceived,
    AVPlayerViewControllerExitFullScreenReasonPictureInPictureStarted
};
- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason;
@end

@interface AVPlayerViewController ()
- (instancetype)initWithPlayerLayerView:(__AVPlayerLayerView *)playerLayerView;
- (void)enterFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError *))completionHandler;
- (void)exitFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError *))completionHandler;

- (BOOL)isPictureInPicturePossible;
- (void)startPictureInPicture;
- (void)stopPictureInPicture;

- (void)flashPlaybackControlsWithDuration:(NSTimeInterval)duration;

@property (nonatomic, strong, nullable) AVPlayerController *playerController;
@property (nonatomic, readonly, getter=isPictureInPictureActive) BOOL pictureInPictureActive;
@property (nonatomic, readonly) BOOL pictureInPictureWasStartedWhenEnteringBackground;
- (void)setWebKitOverrideRouteSharingPolicy:(NSUInteger)routeSharingPolicy routingContextUID:(NSString *)routingContextUID;
@end

NS_ASSUME_NONNULL_END
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
#import <AVKit/AVPlayerView.h>
NS_ASSUME_NONNULL_BEGIN

@interface AVPlayerView (WebKitFullscreenSPI)
@property AVPlayerController *playerController;
@property (readonly) BOOL isFullScreen;
- (void)enterFullScreen:(id)sender;
- (void)exitFullScreen:(id)sender;
@end

NS_ASSUME_NONNULL_END
#endif

#endif // USE(APPLE_INTERNAL_SDK)

#if PLATFORM(IOS_FAMILY) && HAVE(AVPLAYERCONTROLLER)
@interface AVPlayerController ()
@property (NS_NONATOMIC_IOSONLY) double defaultPlaybackRate;
@end
#endif

#if HAVE(AVOBSERVATIONCONTROLLER)
#if USE(APPLE_INTERNAL_SDK)
#import <AVKit/AVObservationController.h>
#else
@class AVKeyValueChange;
NS_ASSUME_NONNULL_BEGIN

@interface AVObservationController<Owner> : NSObject
- (instancetype)initWithOwner:(Owner)owner NS_DESIGNATED_INITIALIZER;
- (id)startObserving:(id)object keyPath:(NSString *)keyPath includeInitialValue:(BOOL)shouldIncludeInitialValue observationHandler:(void (^)(Owner owner, id observed, AVKeyValueChange *change))observationHandler;
- (void)stopAllObservation;
@end

NS_ASSUME_NONNULL_END
#endif // USE(APPLE_INTERNAL_SDK)
#endif // HAVE(AVOBSERVATIONCONTROLLER)


#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
#if USE(APPLE_INTERNAL_SDK)
#import <AVKit/AVOutputDeviceMenuController.h>
#else
NS_ASSUME_NONNULL_BEGIN

@class AVOutputContext;

NS_CLASS_AVAILABLE_MAC(10_11)
@interface AVOutputDeviceMenuController : NSObject

- (instancetype)initWithOutputContext:(AVOutputContext *)outputContext NS_DESIGNATED_INITIALIZER;

@property (readonly) AVOutputContext *outputContext;
@property (readonly, getter=isExternalOutputDeviceAvailable) BOOL externalOutputDeviceAvailable;
@property (readonly, getter=isExternalOutputDevicePicked) BOOL externalOutputDevicePicked;

- (void)showMenuForRect:(NSRect)screenRect appearanceName:(NSString *)appearanceName;
- (BOOL)showMenuForRect:(NSRect)screenRect appearanceName:(NSString *)appearanceName allowReselectionOfSelectedOutputDevice:(BOOL)allowReselectionOfSelectedOutputDevice;

@end

NS_ASSUME_NONNULL_END
#endif // USE(APPLE_INTERNAL_SDK)
#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)


#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER) && PLATFORM(MAC)

OBJC_CLASS AVTouchBarMediaSelectionOption;
OBJC_CLASS AVTouchBarPlaybackControlsProvider;
OBJC_CLASS AVTouchBarScrubber;

#if USE(APPLE_INTERNAL_SDK)
#import <AVKit/AVTouchBarPlaybackControlsProvider.h>
#import <AVKit/AVTouchBarScrubber.h>
#else
NS_ASSUME_NONNULL_BEGIN

@class AVThumbnail;

@protocol AVTouchBarPlaybackControlsControlling <NSObject>
@property (readonly) NSTimeInterval contentDuration;
@property (readonly, nullable) AVValueTiming *timing;
@property (readonly, getter=isSeeking) BOOL seeking;
@property (readonly) NSTimeInterval seekToTime;
- (void)seekToTime:(NSTimeInterval)time toleranceBefore:(NSTimeInterval)toleranceBefore toleranceAfter:(NSTimeInterval)toleranceAfter;
@property (readonly) BOOL hasEnabledAudio;
@property (readonly) BOOL hasEnabledVideo;
@property (readonly) BOOL allowsPictureInPicturePlayback;
@property (readonly, getter=isPictureInPictureActive) BOOL pictureInPictureActive;
@property (readonly) BOOL canTogglePictureInPicture;
- (void)togglePictureInPicture;
@property (nonatomic, readonly) BOOL canSeek;

typedef NS_ENUM(NSInteger, AVTouchBarMediaSelectionOptionType) {
    AVTouchBarMediaSelectionOptionTypeRegular,
    AVTouchBarMediaSelectionOptionTypeLegibleOff,
    AVTouchBarMediaSelectionOptionTypeLegibleAuto,
};

@end

@interface AVTouchBarPlaybackControlsProvider : NSResponder
@property (strong, readonly, nullable) NSTouchBar *touchBar;
@property (assign, nullable) id<AVTouchBarPlaybackControlsControlling> playbackControlsController;
@end

@interface AVTouchBarScrubber : NSView
@property (assign, nullable) id<AVTouchBarPlaybackControlsControlling> playbackControlsController;
@property BOOL canShowMediaSelectionButton;
@end

@interface AVTouchBarMediaSelectionOption : NSObject
- (instancetype)initWithTitle:(nonnull NSString *)title type:(AVTouchBarMediaSelectionOptionType)type;
@end

NS_ASSUME_NONNULL_END
#endif // USE(APPLE_INTERNAL_SDK)
#endif // ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER) && PLATFORM(MAC)


#if ENABLE(WIRELESS_PLAYBACK_TARGET) && HAVE(AVROUTEPICKERVIEW)
#if USE(APPLE_INTERNAL_SDK)
#import <AVKit/AVRoutePickerView_Private.h>
#import <AVKit/AVRoutePickerView_WebKitOnly.h>
#else

#import <AVKit/AVRoutePickerView.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVRoutePickerView (SPI)

- (void)showRoutePickingControlsForOutputContext:(AVOutputContext *)outputContext relativeToRect:(CGRect)positioningRect ofView:(NSView *)positioningView;

@property (nonatomic) BOOL routeListAlwaysHasDarkAppearance;

@end

NS_ASSUME_NONNULL_END
#endif // USE(APPLE_INTERNAL_SDK)
#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && HAVE(AVROUTEPICKERVIEW)

// AVPictureInPicture SPI
#if HAVE(PIP_CONTROLLER)
#if USE(APPLE_INTERNAL_SDK)

#if PLATFORM(IOS_FAMILY)
#import <AVKit/AVPictureInPictureController_GenericSupport.h>
#endif

#else

#if PLATFORM(IOS_FAMILY)
NS_ASSUME_NONNULL_BEGIN

@interface AVPictureInPictureContentViewController : UIViewController
@property (nonatomic, strong, readonly, nullable) AVPlayerController *playerController;
@end

@interface AVPictureInPictureControllerContentSource (GenericSupport)
- (instancetype)initWithSourceView:(UIView *)sourceView contentViewController:(AVPictureInPictureContentViewController *)contentViewController playerController:(__kindof AVPlayerController *)playerController API_AVAILABLE(ios(16.0),tvos(16.0)) API_UNAVAILABLE(macos, watchos);
@property (nonatomic, weak, readonly) UIView *activeSourceView API_AVAILABLE(ios(16.0),tvos(16.0)) API_UNAVAILABLE(macos, watchos);
@property (nonatomic, readonly) __kindof AVPictureInPictureContentViewController *activeContentViewController API_AVAILABLE(ios(16.0),tvos(16.0)) API_UNAVAILABLE(macos, watchos);
@end


NS_ASSUME_NONNULL_END
#endif // PLATFORM(IOS_FAMILY)

#endif // USE(APPLE_INTERNAL_SDK)

@interface AVPictureInPictureController (IPI)
@property (nonatomic) BOOL pictureInPictureWasStartedWhenEnteringBackground;
@end

#endif // HAVE(PIP_CONTROLLER)

#if PLATFORM(VISION)

// FIXME: rdar://111125392 – import SPI using a header, following rdar://111123290.

typedef NS_OPTIONS(NSUInteger, AVPlayerViewControllerFullScreenBehaviors) {
    AVPlayerViewControllerFullScreenBehaviorHostContentInline = 1 << 3,
};

@interface AVPlayerViewController ()
@property (nonatomic) BOOL prefersRoomDimming;
@property (nonatomic) AVPlayerViewControllerFullScreenBehaviors fullScreenBehaviors;
@end

#endif // PLATFORM(VISION)

#if PLATFORM(APPLETV)

// FIXME (116592344): Remove these temporary declarations once AVPlayerController API is available on tvOS.

NS_ASSUME_NONNULL_BEGIN

#if USE(APPLE_INTERNAL_SDK)

typedef NS_ENUM(NSInteger, AVPlayerControllerExternalPlaybackType) {
    AVPlayerControllerExternalPlaybackTypeNone = 0,
    AVPlayerControllerExternalPlaybackTypeAirPlay = 1,
    AVPlayerControllerExternalPlaybackTypeTVOut = 2,
};

typedef NS_ENUM(NSInteger, AVPlayerControllerStatus) {
    AVPlayerControllerStatusUnknown = 0,
    AVPlayerControllerStatusReadyToPlay = 2,
};

@interface AVPlayerController : NSObject
@end

@interface __AVPlayerLayerView : UIView
@end

#endif // USE(APPLE_INTERNAL_SDK)

typedef NS_ENUM(NSInteger, AVPlayerControllerTimeControlStatus) {
    AVPlayerControllerTimeControlStatusPaused,
    AVPlayerControllerTimeControlStatusWaitingToPlayAtSpecifiedRate,
    AVPlayerControllerTimeControlStatusPlaying
};

@interface AVTimeRange : NSObject
@end

@interface __AVPlayerLayerView (IPI)
@property (nonatomic, strong, nullable) AVPlayerController *playerController;
@property (nonatomic, readonly) AVPlayerLayer *playerLayer;
@property (nonatomic, copy, nullable) NSDictionary<NSString *, id> *pixelBufferAttributes;
@end

@interface AVPlayerViewController (IPI)
@property (nonatomic, strong) __AVPlayerLayerView *playerLayerView;
@end

@interface AVTimeRange (IPI)
- (instancetype)initWithCMTimeRange:(CMTimeRange)timeRange;
- (instancetype)initWithStartTime:(NSTimeInterval)startTime endTime:(NSTimeInterval)duration;
@end

NS_ASSUME_NONNULL_END

#endif // PLATFORM(APPLETV)

#if HAVE(AVKIT_CONTENT_SOURCE)

#if USE(APPLE_INTERNAL_SDK)

#import <AVKit/AVMediaSource.h>

#else

@class CALayer;
@class AVInterstitialTimeRange;

typedef struct REEntity *REEntityRef;

NS_ASSUME_NONNULL_BEGIN

@protocol AVMediaPlaybackSource <NSObject>

@property (nonatomic, readonly) double rate;
@property (nonatomic, readonly) BOOL canTogglePlayback;
@property (nonatomic, readonly) BOOL isLoading;
@property (nonatomic, readonly) BOOL canSeek;
@property (nonatomic, readonly) BOOL isSeeking;
@property (nonatomic, readonly) BOOL canScanForward;
@property (nonatomic, readonly) BOOL canScanBackward;
@property (nonatomic, readonly) BOOL requiresLinearPlayback;
@property (nonatomic, readonly) BOOL hasLiveStreamContent;
@property (nonatomic, readonly, nullable) NSError *playbackError;
- (void)play;
- (void)pause;
- (void)seekTo:(double)time;
- (void)beginScanningForward;
- (void)endScanningForward;
- (void)beginScanningBackward;
- (void)endScanningBackward;

@end

@protocol AVMediaTimelineSource <NSObject>

@property (nonatomic, readonly) float minValue;
@property (nonatomic, readonly) float maxValue;
@property (nonatomic, readonly) float currentValue;

@optional

@property (nonatomic, readonly, nullable) NSArray<NSValue *> *seekableTimeRanges;
- (void)beginScrubbing;
- (void)endScrubbing;

@end

@protocol AVListable <NSObject>

@property (nonatomic, readonly) NSString *localizedTitle;

@end

@protocol AVMediaAudioAndCaptionSource <NSObject>

@property (nonatomic, readonly, nullable) id<AVListable> currentAudioOption;
@property (nonatomic, readonly, nullable) NSArray<AVListable> *audioOptions;
- (void)updateCurrentAudioOption:(id<AVListable>)currentAudioOption;
@property (nonatomic, readonly, nullable) id<AVListable> currentCaptionOption;
@property (nonatomic, readonly, nullable) NSArray<AVListable> *captionOptions;
- (void)updateCurrentCaptionOption:(id<AVListable>)currentCaptionOption;
@property (nonatomic, readonly, nullable) CALayer *captionLayer;
- (void)setCaptionContentInsets:(UIEdgeInsets)insets;

@end

@protocol AVMediaVolumeSource <NSObject>

@property (nonatomic, readonly) BOOL hasAudio;
@property (nonatomic, readonly) BOOL muted;
@property (nonatomic, readonly) double volume;
- (void)updateVolume:(double)volume;
- (void)updateMuted:(BOOL)muted;

@optional

- (void)beginChangingVolume;
- (void)endChangingVolume;

@end

@protocol AVMediaContainerSource <NSObject>

@property (nonatomic, readonly, nullable) CALayer *videoLayer;
#if PLATFORM(VISION)
@property (nonatomic, readonly, nullable) REEntityRef entityRef;
#endif
@property (nonatomic, readonly) CGSize videoSize;

@end

@protocol AVMediaThumbnailSource <NSObject>
@end

@protocol AVMediaInterstitialSource <NSObject>

@property (nonatomic, readonly, nullable) NSArray<AVInterstitialTimeRange *> *interstitialTimeRanges;
@property (nonatomic, readonly) BOOL isInterstitialActive;
- (void)skipActiveInterstitial;

@end

@protocol AVMediaMetadataSource <NSObject>

@property (nonatomic, readonly, nullable) NSString *title;
@property (nonatomic, readonly, nullable) NSString *subtitle;

@optional

@property (nonatomic, readonly, nullable) NSDate *approximateStartDate;
@property (nonatomic, readonly, nullable) NSDate *approximateEndDate;
@property (nonatomic, readonly, nullable) NSDate *exactStartDate;
@property (nonatomic, readonly, nullable) NSDate *exactEndDate;

@end

@protocol AVMediaSource <
    AVMediaTimelineSource,
    AVMediaPlaybackSource,
    AVMediaAudioAndCaptionSource,
    AVMediaVolumeSource,
    AVMediaContainerSource,
    AVMediaThumbnailSource,
    AVMediaMetadataSource
>
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)

#endif // HAVE(AVKIT_CONTENT_SOURCE)

#if USE(APPLE_INTERNAL_SDK)

#if !__has_include(<AVKit/AVLegibleMediaOptionsMenuController.h>)

typedef NS_ENUM(NSInteger, AVLegibleMediaOptionsMenuType) {
    AVLegibleMediaOptionsMenuTypeDefault,
    AVLegibleMediaOptionsMenuTypeCaptionAppearance
} API_AVAILABLE(ios(26.4), macos(26.4), macCatalyst(26.4), visionos(26.4)) API_UNAVAILABLE(tvos, watchos);

typedef NS_ENUM(NSInteger, AVLegibleMediaOptionEnablementState) {
    AVLegibleMediaOptionEnablementStateOff,
    AVLegibleMediaOptionEnablementStateOn
} API_AVAILABLE(ios(26.4), macos(26.4), macCatalyst(26.4), visionos(26.4)) API_UNAVAILABLE(tvos, watchos);

NS_ASSUME_NONNULL_BEGIN

@protocol AVLegibleMediaOptionsMenuControllerDelegate;

API_AVAILABLE(ios(26.4), macos(26.4), macCatalyst(26.4), visionos(26.4)) API_UNAVAILABLE(tvos, watchos)
@interface AVLegibleMediaOptionsMenuController : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPlayer:(nullable AVPlayer *)player NS_DESIGNATED_INITIALIZER;

#if TARGET_OS_OSX && !TARGET_OS_MACCATALYST
- (nullable NSMenu *)buildMenuOfType:(AVLegibleMediaOptionsMenuType)type;
#else
- (nullable UIMenu *)buildMenuOfType:(AVLegibleMediaOptionsMenuType)type;
#endif

- (void)updatePlayer:(nullable AVPlayer *)player;

@property (nonatomic, weak) id<AVLegibleMediaOptionsMenuControllerDelegate> delegate;
@property (nonatomic, readonly) AVLegibleMediaOptionEnablementState currentEnablementState;

@end

API_AVAILABLE(ios(26.4), macos(26.4), macCatalyst(26.4), visionos(26.4)) API_UNAVAILABLE(tvos, watchos)
@protocol AVLegibleMediaOptionsMenuControllerDelegate <NSObject>
@optional

- (void)legibleMenuController:(AVLegibleMediaOptionsMenuController *)menuController didUpdateEnablementState:(AVLegibleMediaOptionEnablementState)enablementState;
- (void)legibleMenuController:(AVLegibleMediaOptionsMenuController *)menuController didRequestCaptionPreviewForProfileID:(NSString *)profileID;
- (void)legibleMenuControllerDidRequestStoppingSubtitleCaptionPreview:(AVLegibleMediaOptionsMenuController *)menuController;

@end

NS_ASSUME_NONNULL_END

#endif

#endif // USE(APPLE_INTERNAL_SDK)
