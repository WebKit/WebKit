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

#import <objc/runtime.h>
#import <wtf/SoftLinking.h>

#if PLATFORM(IOS_FAMILY)
#import <AVKit/AVKit.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#if !PLATFORM(WATCHOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000
#import <AVKit/AVBackgroundView.h>
#endif

#import <AVKit/AVPlayerController.h>
IGNORE_CLANG_WARNINGS_BEGIN("objc-property-no-attribute")
#import <AVKit/AVPlayerLayerView.h>
IGNORE_CLANG_WARNINGS_END
#import <AVKit/AVPlayerViewController_Private.h>
#import <AVKit/AVPlayerViewController_WebKitOnly.h>

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

#elif __IPHONE_OS_VERSION_MAX_ALLOWED < 120200
NS_ASSUME_NONNULL_BEGIN
@interface AVPlayerViewController (AVPlayerViewController_WebKitOnly_OverrideRouteSharingPolicy)
- (void)setWebKitOverrideRouteSharingPolicy:(NSUInteger)routeSharingPolicy routingContextUID:(NSString *)routingContextUID;
@end
NS_ASSUME_NONNULL_END
#endif

#else

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

@interface AVPlayerController : UIResponder
@end

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

@property (nonatomic, strong, nullable) AVPlayerController *playerController;
@property (nonatomic, readonly, getter=isPictureInPictureActive) BOOL pictureInPictureActive;
@property (nonatomic, readonly) BOOL pictureInPictureWasStartedWhenEnteringBackground;
- (void)setWebKitOverrideRouteSharingPolicy:(NSUInteger)routeSharingPolicy routingContextUID:(NSString *)routingContextUID;
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)
#endif // PLATFORM(IOS_FAMILY)

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

#if PLATFORM(MAC) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

OBJC_CLASS AVFunctionBarPlaybackControlsProvider;
OBJC_CLASS AVFunctionBarScrubber;
OBJC_CLASS AVFunctionBarMediaSelectionOption;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
OBJC_CLASS AVTouchBarPlaybackControlsProvider;
OBJC_CLASS AVTouchBarScrubber;
OBJC_CLASS AVTouchBarMediaSelectionOption;
#else
typedef AVFunctionBarMediaSelectionOption AVTouchBarMediaSelectionOption;
#endif

#if USE(APPLE_INTERNAL_SDK)
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
#import <AVKit/AVTouchBarPlaybackControlsProvider.h>
#import <AVKit/AVTouchBarScrubber.h>
#else
#import <AVKit/AVFunctionBarPlaybackControlsProvider.h>
#import <AVKit/AVFunctionBarScrubber.h>
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300

#else

NS_ASSUME_NONNULL_BEGIN

__attribute__((availability(macosx, obsoleted = 10.13))) @protocol AVFunctionBarPlaybackControlsControlling <NSObject>;
@property (readonly) NSTimeInterval contentDuration;
@property (readonly, nullable) AVValueTiming *timing;
@property (readonly, getter=isSeeking) BOOL seeking;
@property (readonly) NSTimeInterval seekToTime;
- (void)seekToTime:(NSTimeInterval)time toleranceBefore:(NSTimeInterval)toleranceBefore toleranceAfter:(NSTimeInterval)toleranceAfter;
@property (readonly) BOOL hasEnabledAudio;
@property (readonly) BOOL hasEnabledVideo;
@end

__attribute__((availability(macosx, obsoleted = 10.13))) @interface AVFunctionBarPlaybackControlsProvider : NSResponder
@property (strong, readonly, nullable) NSTouchBar *touchBar;
@property (assign, nullable) id<AVFunctionBarPlaybackControlsControlling> playbackControlsController;
@end

@class AVThumbnail;

__attribute__((availability(macosx, obsoleted = 10.13))) @interface AVFunctionBarScrubber : NSView
@property (assign, nullable) id<AVFunctionBarPlaybackControlsControlling> playbackControlsController;
@end

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300

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

@class AVThumbnail;

#endif

NS_ASSUME_NONNULL_END

#endif

#endif // PLATFORM(MAC) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
