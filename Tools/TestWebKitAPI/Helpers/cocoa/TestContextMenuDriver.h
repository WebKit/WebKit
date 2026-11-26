/*
 * Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

#import <wtf/Platform.h>

#if USE(UICONTEXTMENU)

#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <UIKit/UIEvent_Private.h>
#import <UIKit/_UIClickInteractionDriving.h>

#else

typedef NS_ENUM(NSUInteger, _UIInputPrecision) {
    _UIInputPrecisionUnknown,
    _UIInputPrecisionLow,
    _UIInputPrecisionHigh,
};

typedef NS_ENUM(NSUInteger, _UIClickInteractionEvent) {
    _UIClickInteractionEventBegan = 0,
    _UIClickInteractionEventClickedDown,
    _UIClickInteractionEventClickedUp,
    _UIClickInteractionEventEnded,
};

typedef NS_ENUM(NSUInteger, _UIClickInteractionShouldBeginResult) {
    _UIClickInteractionShouldBeginResultBegin
};

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@protocol _UIClickInteractionDriverDelegate;

@protocol _UIClickInteractionDriving <NSObject>
@property (nonatomic, weak, nullable) id<_UIClickInteractionDriverDelegate> delegate;
@end

@protocol _UIClickInteractionDriverDelegate <NSObject>
- (void)clickDriver:(id<_UIClickInteractionDriving>)driver shouldBegin:(void(^)(_UIClickInteractionShouldBeginResult))completion;
- (void)clickDriver:(id<_UIClickInteractionDriving>)driver didPerformEvent:(_UIClickInteractionEvent)event;
@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // USE(APPLE_INTERNAL_SDK)

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

NS_SWIFT_UI_ACTOR
@interface TestContextMenuDriver : NSObject <_UIClickInteractionDriving>

@property (nonatomic, weak, nullable) id<_UIClickInteractionDriverDelegate> delegate;
@property (nonatomic, weak, nullable) UIView *view;
@property (nonatomic) CGFloat allowableMovement;
@property (nonatomic, strong, nullable) UIGestureRecognizer *primaryGestureRecognizer;
@property (nonatomic) NSTimeInterval touchDuration;
@property (nonatomic) BOOL cancelsTouchesInView;

@property (nonatomic, class, readonly) BOOL requiresForceCapability;
@property (nonatomic, class, readonly) BOOL prefersCancelsTouchesInView;
@property (nonatomic, readonly) BOOL hasExceededAllowableMovement;
@property (nonatomic, readonly) BOOL isCurrentlyAcceleratedByForce;
@property (nonatomic, readonly) BOOL clicksUpAutomaticallyAfterTimeout;
@property (nonatomic, readonly) CGFloat maximumEffectProgress;
@property (nonatomic, readonly) _UIInputPrecision inputPrecision;

- (CGPoint)locationInCoordinateSpace:(nullable id<UICoordinateSpace>)coordinateSpace;
- (void)cancelInteraction;

- (void)begin:(NS_SWIFT_UI_ACTOR void (^)(BOOL))completionHandler;
- (void)clickDown;
- (void)clickUp;
- (void)end;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // USE(UICONTEXTMENU)
