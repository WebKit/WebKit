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

#import <AVKit/AVKit.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <AVKit/AVPlayerController.h>
#import <AVKit/AVPlayerViewController_Private.h>
#import <AVKit/AVPlayerViewController_WebKitOnly.h>
#import <AVKit/AVVideoLayer.h>

#else

@interface AVPlayerController : UIResponder
@end

@interface AVPlayerController (Details)
typedef NS_ENUM(NSInteger, AVPlayerControllerStatus) {
    AVPlayerControllerStatusReadyToPlay = 2,
};

typedef NS_ENUM(NSInteger, AVPlayerControllerExternalPlaybackType) {
    AVPlayerControllerExternalPlaybackTypeNone = 0,
    AVPlayerControllerExternalPlaybackTypeAirPlay = 1,
    AVPlayerControllerExternalPlaybackTypeTVOut = 2,
};

@property (NS_NONATOMIC_IOSONLY, readonly) AVPlayerControllerStatus status;
@end

@protocol AVVideoLayer
typedef NS_ENUM(NSInteger, AVVideoLayerGravity) {
    AVVideoLayerGravityInvalid = 0,
    AVVideoLayerGravityResizeAspect = 1,
    AVVideoLayerGravityResizeAspectFill = 2,
    AVVideoLayerGravityResize = 3,
};
- (void)setPlayerController:(AVPlayerController *)playerController;
@property (nonatomic) AVVideoLayerGravity videoLayerGravity;
@property (nonatomic) CGRect videoRect;
@property (nonatomic, readonly, getter=isReadyForDisplay) BOOL readyForDisplay;
@end

@protocol AVPlayerViewControllerDelegate <NSObject>
@optional
typedef NS_ENUM(NSInteger, AVPlayerViewControllerExitFullScreenReason) {
    AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped = 0,
    AVPlayerViewControllerExitFullScreenReasonRemoteControlStopEventReceived = 3,
};
- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason;
@end

typedef NSInteger AVPlayerViewControllerOptimizedFullscreenStopReason;

@interface AVPlayerViewController (Details)
- (instancetype)initWithVideoLayer:(CALayer <AVVideoLayer> *)videoLayer;
- (void)enterFullScreenWithCompletionHandler:(void (^)(BOOL success, NSError *))completionHandler;
- (void)exitFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError *))completionHandler;
- (void)exitFullScreenWithCompletionHandler:(void (^)(BOOL success, NSError *))completionHandler;

- (void)startOptimizedFullscreenWithStartCompletionHandler:(void (^)(BOOL success, NSError*))startCompletionHandler stopCompletionHandler:(void (^)(AVPlayerViewControllerOptimizedFullscreenStopReason))stopCompletionHandler;
- (void)stopOptimizedFullscreen;
- (void)cancelOptimizedFullscreen;
- (void)setAllowsOptimizedFullscreen:(BOOL)allowsOptimizedFullscreen;

@property (nonatomic, strong) AVPlayerController *playerController;
@property (nonatomic, weak) id <AVPlayerViewControllerDelegate> delegate;
@end

#endif

#if USE(APPLE_INTERNAL_SDK) && __IPHONE_OS_VERSION_MIN_REQUIRED < 90000

#import <AVKit/AVValueTiming.h>

#else

@interface AVValueTiming : NSObject <NSCoding, NSCopying, NSMutableCopying>
@end

@interface AVValueTiming (Details)
+ (AVValueTiming *)valueTimingWithAnchorValue:(double)anchorValue anchorTimeStamp:(NSTimeInterval)timeStamp rate:(double)rate;
@property (NS_NONATOMIC_IOSONLY, readonly) double currentValue;
@end

#endif
