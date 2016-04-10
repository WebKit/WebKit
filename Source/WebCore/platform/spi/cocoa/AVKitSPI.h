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

#import "SoftLinking.h"
#import <objc/runtime.h>

#if PLATFORM(IOS)
#import <AVKit/AVKit.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <AVKit/AVPlayerController.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-property-no-attribute"
#import <AVKit/AVPlayerLayerView.h>
#pragma clang diagnostic pop
#import <AVKit/AVPlayerViewController_Private.h>
#import <AVKit/AVPlayerViewController_WebKitOnly.h>

#else

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

@property (nonatomic, strong) AVPlayerController *playerController;
@property (nonatomic, readonly, getter=isPictureInPictureActive) BOOL pictureInPictureActive;
@property (nonatomic, readonly) BOOL pictureInPictureWasStartedWhenEnteringBackground;
@end

#endif // USE(APPLE_INTERNAL_SDK)
#endif // PLATFORM(IOS)

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)

#if USE(APPLE_INTERNAL_SDK)

#import <AVKit/AVOutputDeviceMenuController.h>

#else

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


#endif // USE(APPLE_INTERNAL_SDK)

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)

@interface AVValueTiming : NSObject <NSCoding, NSCopying, NSMutableCopying> 
@end

@interface AVValueTiming ()
+ (AVValueTiming *)valueTimingWithAnchorValue:(double)anchorValue anchorTimeStamp:(NSTimeInterval)timeStamp rate:(double)rate;
@property (NS_NONATOMIC_IOSONLY, readonly) double currentValue;
@end
