/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(VISION)

#import "RealitySimulationServicesSPI.h"

#if USE(APPLE_INTERNAL_SDK)

#import <MRUIKit/MRUIStage.h>
#import <MRUIKit/UIApplication+MRUIKit.h>
#import <MRUIKit/UIWindowScene+MRUIKit.h>

#else

typedef NS_ENUM(NSUInteger, MRUIDarknessPreference) {
    MRUIDarknessPreferenceUnspecified = 0,
    MRUIDarknessPreferenceDim,
    MRUIDarknessPreferenceDark,
    MRUIDarknessPreferenceVeryDark,
};

@interface MRUIStage : NSObject
@property (nonatomic, readwrite) MRUIDarknessPreference preferredDarkness;
@end

typedef NS_ENUM(NSUInteger, MRUISceneResizingBehavior) {
    MRUISceneResizingBehaviorUnspecified = 0,
    MRUISceneResizingBehaviorNone,
    MRUISceneResizingBehaviorUniform,
    MRUISceneResizingBehaviorFreeform,
};

@interface MRUIWindowScenePlacement : NSObject
@property (nonatomic, assign) MRUISceneResizingBehavior preferredResizingBehavior;
@property (nonatomic, assign) RSSSceneChromeOptions preferredChromeOptions;
@end

typedef NS_ENUM(NSInteger, MRUICloseWindowSceneReason) {
    MRUICloseReasonCloseButton,
    MRUICloseReasonCommandKey,
};

@protocol MRUIWindowSceneDelegate <UIWindowSceneDelegate>
@optional
- (BOOL)windowScene:(UIWindowScene *)windowScene shouldCloseForReason:(MRUICloseWindowSceneReason)reason;
@end

@interface UIApplication (MRUIKit)
@property (nonatomic, readonly) MRUIStage *mrui_activeStage;
@end

@class MRUIWindowSceneResizeRequestOptions;

typedef void (^MRUIWindowSceneResizeRequestCompletion)(CGSize grantedSize, NSError *error);

@interface UIWindowScene (MRUIKit)

- (void)mrui_requestResizeToSize:(CGSize)size options:(MRUIWindowSceneResizeRequestOptions *)options completion:(MRUIWindowSceneResizeRequestCompletion)completion;

@property (nonatomic, readonly) MRUIWindowScenePlacement *mrui_placement;

@end

#endif // USE(APPLE_INTERNAL_SDK)

// FIXME: <rdar://111655142> Import ornaments SPI using framework headers.

@interface MRUIPlatterOrnament : NSObject
@property (nonatomic, assign, getter=_depthDisplacement, setter=_setDepthDisplacement:) CGFloat depthDisplacement;
@property (nonatomic, readwrite, strong) UIViewController *viewController;
@end

@interface MRUIPlatterOrnamentManager : NSObject
@property (nonatomic, readonly) NSArray<MRUIPlatterOrnament *> *ornaments;
@end

@interface UIWindowScene (MRUIPlatterOrnaments)
@property (nonatomic, readonly) MRUIPlatterOrnamentManager *_mrui_platterOrnamentManager;
@end

#endif // PLATFORM(VISION)
