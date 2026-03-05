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

#if !__has_feature(modules)

#import <wtf/Compiler.h>
#import <wtf/Platform.h>

DECLARE_SYSTEM_HEADER

#import <objc/runtime.h>

#if PLATFORM(IOS_FAMILY)
#import <AVFoundation/AVPlayer.h>
#import <AVKit/AVKit.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#endif // PLATFORM(IOS_FAMILY)

#endif // !__has_feature(modules)

@class AVPlayer;

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV) && USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)
#import <AVKit/AVValueTiming.h>
#else
NS_ASSUME_NONNULL_BEGIN

@interface AVValueTiming : NSObject <NSSecureCoding, NSCopying, NSMutableCopying>

@property (NS_NONATOMIC_IOSONLY, readonly) double anchorValue;

@property (NS_NONATOMIC_IOSONLY, readonly) NSTimeInterval anchorTimeStamp;

@property (NS_NONATOMIC_IOSONLY, readonly) double rate;

@end

@interface AVValueTiming ()

+ (AVValueTiming *)valueTimingWithAnchorValue:(double)anchorValue anchorTimeStamp:(NSTimeInterval)timeStamp rate:(double)rate;

@property (NS_NONATOMIC_IOSONLY, readonly) double currentValue;

+ (NSTimeInterval)currentTimeStamp;
- (double)valueForTimeStamp:(NSTimeInterval)timeStamp;
@end

NS_ASSUME_NONNULL_END
#endif // !PLATFORM(WATCHOS) && !PLATFORM(APPLETV) && USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)

#if !USE(APPLE_INTERNAL_SDK) || __has_feature(modules) || PLATFORM(APPLETV)

typedef NSString * AVMediaType NS_EXTENSIBLE_STRING_ENUM;

@class AVPlayer;

typedef NS_ENUM(NSInteger, AVPlayerControllerExternalPlaybackType) {
    AVPlayerControllerExternalPlaybackTypeNone,
    AVPlayerControllerExternalPlaybackTypeAirPlay,
    AVPlayerControllerExternalPlaybackTypeTVOut
};

typedef NS_ENUM(NSInteger, AVPlayerControllerStatus) {
    AVPlayerControllerStatusUnknown,
    AVPlayerControllerStatusLoading,
    AVPlayerControllerStatusReadyToPlay,
    AVPlayerControllerStatusFailed
};

#endif // !USE(APPLE_INTERNAL_SDK) || __has_feature(modules) || PLATFORM(APPLETV)

#if !USE(APPLE_INTERNAL_SDK) || PLATFORM(APPLETV)

@interface AVPlayerController : NSObject

- (instancetype)initWithPlayer:(AVPlayer *)player;

@property (NS_NONATOMIC_IOSONLY, readonly) AVPlayer *player;

@property (NS_NONATOMIC_IOSONLY, readonly) AVPlayerControllerStatus status;

@property (NS_NONATOMIC_IOSONLY, readonly) BOOL isCurrentItemReadyForInspection;

@property (NS_NONATOMIC_IOSONLY, readonly) NSError *error;

@property (NS_NONATOMIC_IOSONLY, readonly, getter=isAtLiveEdge) BOOL atLiveEdge;

- (void)startUsingNetworkResourcesForLiveStreamingWhilePaused;

- (void)stopUsingNetworkResourcesForLiveStreamingWhilePaused;

@end

#endif // !USE(APPLE_INTERNAL_SDK) || PLATFORM(APPLETV)

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)

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
#endif // PLATFORM(APPLETV)

#if PLATFORM(MAC)
#import <AVKit/AVPlayerView_Private.h>
#endif

#else

#if PLATFORM(WATCHOS) && !__has_feature(modules)
#import <AVKit/AVPlayerViewController.h> // not part of AVKit's umbrella header
#endif

#if PLATFORM(IOS_FAMILY)
NS_ASSUME_NONNULL_BEGIN

#if !__has_feature(modules)

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

#endif // !__has_feature(modules)

@class AVPlayerLayer;

#if !__has_feature(modules)

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

#endif // !__has_feature(modules)

NS_ASSUME_NONNULL_END
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

#import <AppKit/AppKit.h>

#if !__has_feature(modules)
#import <AVKit/AVPlayerView.h>

@class AVPlayerController;

NS_ASSUME_NONNULL_BEGIN

@interface AVPlayerView (WebKitFullscreenSPI)
@property AVPlayerController *playerController;
@property (readonly) BOOL isFullScreen;
- (void)enterFullScreen:(id)sender;
- (void)exitFullScreen:(id)sender;
@end

NS_ASSUME_NONNULL_END

#else
@class AVPlayerView;
#endif // !__has_feature(modules)

#endif // PLATFORM(MAC)

#endif // USE(APPLE_INTERNAL_SDK)

#if PLATFORM(IOS_FAMILY) && HAVE(AVPLAYERCONTROLLER) && !__has_feature(modules)
@interface AVPlayerController ()
@property (NS_NONATOMIC_IOSONLY) double defaultPlaybackRate;
@end
#endif

#if HAVE(AVOBSERVATIONCONTROLLER)
// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)
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
// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)
#import <AVKit/AVOutputDeviceMenuController.h>
#else

@class AVOutputContext;

@interface AVOutputDeviceMenuController : NSObject

- (instancetype)initWithOutputContext:(AVOutputContext *)outputContext NS_DESIGNATED_INITIALIZER;

@property (readonly) AVOutputContext *outputContext;

@property (readonly, getter=isExternalOutputDeviceAvailable) BOOL externalOutputDeviceAvailable;

@property (readonly, getter=isExternalOutputDevicePicked) BOOL externalOutputDevicePicked;

- (BOOL)showMenuForRect:(NSRect)screenRect appearanceName:(NSString *)appearanceName allowReselectionOfSelectedOutputDevice:(BOOL)allowReselectionOfSelectedOutputDevice;

@end

#endif // USE(APPLE_INTERNAL_SDK)
#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)


#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER) && PLATFORM(MAC)

OBJC_CLASS AVTouchBarMediaSelectionOption;
OBJC_CLASS AVTouchBarPlaybackControlsProvider;
OBJC_CLASS AVTouchBarScrubber;

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)
#import <AVKit/AVTouchBarPlaybackControlsProvider.h>
#import <AVKit/AVTouchBarScrubber.h>
#else
NS_ASSUME_NONNULL_BEGIN

@class AVThumbnail;

@protocol AVTouchBarPlaybackControlsControlling <NSObject>

@property (readonly) NSTimeInterval contentDuration;
@property (readonly) AVValueTiming *timing;
@property (readonly, getter=isSeeking) BOOL seeking;
@property (readonly) NSTimeInterval seekToTime;
- (void)seekToTime:(NSTimeInterval)time toleranceBefore:(NSTimeInterval)toleranceBefore toleranceAfter:(NSTimeInterval)toleranceAfter;
@property (readonly) BOOL hasEnabledAudio;
@property (readonly) BOOL hasEnabledVideo;

@optional

@property (getter=isPlaying) BOOL playing;

@property (readonly) BOOL canTogglePlayback;

- (void)togglePlayback;

@property double defaultPlaybackRate;

@property (readonly) BOOL allowsPictureInPicturePlayback;

@property (getter=isPictureInPictureActive) BOOL pictureInPictureActive;

@property (readonly) BOOL canTogglePictureInPicture;

- (void)togglePictureInPicture;

@property (nonatomic, readonly) BOOL canSeek;

@property (readonly) NSArray *seekableTimeRanges;

@property (readonly) NSArray<AVTouchBarMediaSelectionOption *> *audioTouchBarMediaSelectionOptions;

@property (strong) AVTouchBarMediaSelectionOption *currentAudioTouchBarMediaSelectionOption;

@property (readonly) NSArray<AVTouchBarMediaSelectionOption *> *legibleTouchBarMediaSelectionOptions;

@property (strong) AVTouchBarMediaSelectionOption *currentLegibleTouchBarMediaSelectionOption;

@property (readonly) BOOL canBeginTouchBarScrubbing;

- (void)beginTouchBarScrubbing;

- (void)endTouchBarScrubbing;

- (void)generateTouchBarThumbnailsForTimes:(NSArray<NSNumber *> *)thumbnailTimes tolerance:(NSTimeInterval)tolerance size:(NSSize)size thumbnailHandler:(void (^)(NSArray<AVThumbnail *> *thumbnails, BOOL thumbnailGenerationFailed))thumbnailHandler;

- (void)cancelThumbnailGeneration;

@property (readonly, nullable) NSURL *assetURL;

- (void)controlsViewWillAppear;

- (void)controlsViewDidDisappear;

typedef NS_ENUM(NSInteger, AVTouchBarMediaSelectionOptionType) {
    AVTouchBarMediaSelectionOptionTypeRegular,
    AVTouchBarMediaSelectionOptionTypeLegibleOff,
    AVTouchBarMediaSelectionOptionTypeLegibleAuto,
};

@end

@class AVPlaybackSpeedCollection;

@protocol NSTouchBarProvider;

@interface AVTouchBarPlaybackControlsProvider : NSResponder <NSTouchBarProvider>

@property (nullable, readonly, strong) NSTouchBar *touchBar;

@property (nullable, strong) id<AVTouchBarPlaybackControlsControlling> playbackControlsController;

@property (nonatomic, strong) AVPlaybackSpeedCollection *playbackSpeedCollection;

- (void)reloadThumbailsOrAudioWaveform;

@end

@protocol AVTouchBarScrubberDelegate;

@interface AVTouchBarScrubber : NSView

@property (nullable, strong) id<AVTouchBarPlaybackControlsControlling> playbackControlsController;

@property (nullable, weak) id <AVTouchBarScrubberDelegate> delegate;

@property BOOL drawsBackground;

@property BOOL showsInlinePlayButton;

@property BOOL canShowMediaSelectionButton;

@property BOOL canCollapse;

@property BOOL collapsesIntoPlayButton;

@property (nullable, strong) NSColor *audioWaveformColor;

@end

@interface AVTouchBarMediaSelectionOption : NSObject

- (instancetype)initWithTitle:(NSString *)title type:(AVTouchBarMediaSelectionOptionType)type NS_DESIGNATED_INITIALIZER;

@property (nonatomic, readonly) NSString *title;

@property (nonatomic, readonly) AVTouchBarMediaSelectionOptionType type;

@property (nonatomic, nullable, strong) id representedObject;

@end

NS_ASSUME_NONNULL_END
#endif // USE(APPLE_INTERNAL_SDK)
#endif // ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER) && PLATFORM(MAC)


#if ENABLE(WIRELESS_PLAYBACK_TARGET) && HAVE(AVROUTEPICKERVIEW)
#if USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)
#import <AVKit/AVRoutePickerView_Private.h>
#import <AVKit/AVRoutePickerView_WebKitOnly.h>
#else

#if !__has_feature(modules)
#import <AVKit/AVRoutePickerView.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVRoutePickerView (SPI)

- (void)showRoutePickingControlsForOutputContext:(AVOutputContext *)outputContext relativeToRect:(CGRect)positioningRect ofView:(NSView *)positioningView;

@property (nonatomic) BOOL routeListAlwaysHasDarkAppearance;

@end

NS_ASSUME_NONNULL_END
#endif // !__has_feature(modules)
#endif // USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)
#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && HAVE(AVROUTEPICKERVIEW)

// AVPictureInPicture SPI
#if HAVE(PIP_CONTROLLER)
// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)

#if PLATFORM(IOS_FAMILY)
#import <AVKit/AVPictureInPictureController_GenericSupport.h>
#endif

#else

#if PLATFORM(IOS_FAMILY) && !__has_feature(modules)
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
#endif // PLATFORM(IOS_FAMILY) && !__has_feature(modules)

#endif // USE(APPLE_INTERNAL_SDK)

#if !__has_feature(modules)

@interface AVPictureInPictureController (IPI)
@property (nonatomic) BOOL pictureInPictureWasStartedWhenEnteringBackground;
@end

#endif // !__has_feature(modules)

#endif // HAVE(PIP_CONTROLLER)

#if PLATFORM(VISION) && !__has_feature(modules)

// FIXME: rdar://111125392 – import SPI using a header, following rdar://111123290.

typedef NS_OPTIONS(NSUInteger, AVPlayerViewControllerFullScreenBehaviors) {
    AVPlayerViewControllerFullScreenBehaviorHostContentInline = 1 << 3,
};

@interface AVPlayerViewController ()
@property (nonatomic) BOOL prefersRoomDimming;
@property (nonatomic) AVPlayerViewControllerFullScreenBehaviors fullScreenBehaviors;
@end

#endif // PLATFORM(VISION) && !__has_feature(modules)

#if PLATFORM(APPLETV)

// FIXME (116592344): Remove these temporary declarations once AVPlayerController API is available on tvOS.

NS_ASSUME_NONNULL_BEGIN

#if USE(APPLE_INTERNAL_SDK)

#if !__has_feature(modules)

@interface __AVPlayerLayerView : UIView
@end

#endif // !__has_feature(modules)

#endif // USE(APPLE_INTERNAL_SDK)

typedef NS_ENUM(NSInteger, AVPlayerControllerTimeControlStatus) {
    AVPlayerControllerTimeControlStatusPaused,
    AVPlayerControllerTimeControlStatusWaitingToPlayAtSpecifiedRate,
    AVPlayerControllerTimeControlStatusPlaying
};

@interface AVTimeRange : NSObject
@end

#if !__has_feature(modules)

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

#endif // !__has_feature(modules)

NS_ASSUME_NONNULL_END

#endif // PLATFORM(APPLETV)

// CLEANUP(rdar://164667890)
#if HAVE(AVLEGIBLEMEDIAOPTIONSMENUCONTROLLER) && USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)
#if __has_include(<AVKit/AVLegibleMediaOptionsMenuController_Private.h>)
#import <AVKit/AVLegibleMediaOptionsMenuController_Private.h>
#else
// SDK does not have -menuWithContents:. Redeclare:
#import <AVKit/AVLegibleMediaOptionsMenuController.h>
typedef NS_OPTIONS(NSInteger, AVLegibleMediaOptionsMenuContents) {
    AVLegibleMediaOptionsMenuContentsLegible = 1 << 0,
    AVLegibleMediaOptionsMenuContentsCaptionAppearance = 1 << 1,
    AVLegibleMediaOptionsMenuContentsAll = (AVLegibleMediaOptionsMenuContentsLegible | AVLegibleMediaOptionsMenuContentsCaptionAppearance)
} API_AVAILABLE(ios(26.4), macos(26.4), visionos(26.4)) API_UNAVAILABLE(tvos, watchos);

@interface AVLegibleMediaOptionsMenuController (MenuWithContents)
#if TARGET_OS_OSX && !TARGET_OS_MACCATALYST
- (nullable NSMenu *)menuWithContents:(AVLegibleMediaOptionsMenuContents)contents;
#else
- (nullable UIMenu *)menuWithContents:(AVLegibleMediaOptionsMenuContents)contents;
#endif
@end

#endif // __has_include(<AVKit/AVLegibleMediaOptionsMenuController_Private.h>)
#endif // HAVE(AVLEGIBLEMEDIAOPTIONSMENUCONTROLLER) && USE(APPLE_INTERNAL_SDK)
