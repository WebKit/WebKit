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

#import <AVKit/AVPlayerViewController.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>

#if USE(APPLE_INTERNAL_SDK)
#import <AVKit/AVPlayerViewController_Private.h>
#import <AVKit/AVPlayerViewController_WebKitOnly.h>
#else
@protocol AVPlayerViewControllerDelegate;
@protocol AVVideoLayer;

@interface AVPlayerViewController (Details)
- (instancetype)initWithVideoLayer:(CALayer <AVVideoLayer> *)videoLayer;
- (void)enterFullScreenWithCompletionHandler:(void (^)(BOOL success, NSError *))completionHandler;
- (void)exitFullScreenAnimated:(BOOL)animated completionHandler:(void (^)(BOOL success, NSError *))completionHandler;
- (void)exitFullScreenWithCompletionHandler:(void (^)(BOOL success, NSError *))completionHandler;
@property (nonatomic, strong) AVPlayerController *playerController;
@property (nonatomic, weak) id <AVPlayerViewControllerDelegate> delegate;
@end

@protocol AVPlayerViewControllerDelegate <NSObject>
@optional
typedef NS_ENUM(NSInteger, AVPlayerViewControllerExitFullScreenReason) {
    AVPlayerViewControllerExitFullScreenReasonDoneButtonTapped = 0,
    AVPlayerViewControllerExitFullScreenReasonRemoteControlStopEventReceived = 3,
};
- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldExitFullScreenWithReason:(AVPlayerViewControllerExitFullScreenReason)reason;
@end
#endif
