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

#import <wtf/Platform.h>

#if PLATFORM(IOS_FAMILY)

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <MediaPlayer/MPAVRoutingController.h>

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000 && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
#import <MediaPlayer/MPMediaControlsViewController.h>
#else
#import <MediaPlayer/MPAVRoutingSheet.h>
#import <MediaPlayer/MPAudioVideoRoutingPopoverController.h>
#endif

#else

NS_ASSUME_NONNULL_BEGIN

enum {
    MPRouteDiscoveryModeDisabled = 0,
    MPRouteDiscoveryModePresence = 1,
    MPRouteDiscoveryModeDetailed = 2,
};
typedef NSInteger MPRouteDiscoveryMode;

@interface MPAVRoutingController : NSObject
@end

@interface MPAVRoutingController ()
- (instancetype)initWithName:(NSString *)name;
@property (nonatomic, assign) MPRouteDiscoveryMode discoveryMode;
@end

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000 && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

@interface MPMediaControlsViewController : UIViewController
@property (nonatomic, copy, nullable) void (^didDismissHandler)(void);
@end

#else

enum {
    MPAVItemTypeUnknown = 0,
    MPAVItemTypeAudio = 1,
    MPAVItemTypeVideo = 2,
};
typedef NSUInteger MPAVItemType;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
@interface MPAudioVideoRoutingPopoverController : UIPopoverController
@end
ALLOW_DEPRECATED_DECLARATIONS_END

@interface MPAudioVideoRoutingPopoverController ()
- (id)initWithType:(MPAVItemType)avItemType;
@end

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
@interface MPAVRoutingSheet : UIView
@end
ALLOW_DEPRECATED_DECLARATIONS_END

@interface MPAVRoutingSheet ()
@property (nonatomic, assign, setter=setAVItemType:) MPAVItemType avItemType;
@property (nonatomic, assign) BOOL mirroringOnly;
- (id)initWithAVItemType:(MPAVItemType)avItemType;
- (void)showInView:(UIView *)view withCompletionHandler:(void (^)(void))completionHandler;
- (void)dismiss;
@end

#endif

NS_ASSUME_NONNULL_END

#endif

#endif // PLATFORM(IOS_FAMILY)
