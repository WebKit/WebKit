/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if USE(APPLE_INTERNAL_SDK)
#import <RealitySystemSupport/RealitySystemSupport.h>
#else

#import <QuartzCore/CALayer.h>

@class CARemoteEffectGroup;

typedef NS_OPTIONS(NSUInteger, RCPRemoteEffectInputTypes) {
    RCPRemoteEffectInputTypeGaze = 1 << 0,
    RCPRemoteEffectInputTypeDirectTouch = 1 << 1,
    RCPRemoteEffectInputTypePointer = 1 << 2,
    RCPRemoteEffectInputTypeNearbyTouchLeftHand = 1 << 3,
    RCPRemoteEffectInputTypeNearbyTouchRightHand = 1 << 4,
    RCPRemoteEffectInputTypeNearbyTouchAnyHand = (RCPRemoteEffectInputTypeNearbyTouchLeftHand | RCPRemoteEffectInputTypeNearbyTouchRightHand),
    RCPRemoteEffectInputTypeAnyTouch = (RCPRemoteEffectInputTypeDirectTouch | RCPRemoteEffectInputTypeNearbyTouchAnyHand),
    RCPRemoteEffectInputTypeIndirect = RCPRemoteEffectInputTypeGaze,
    RCPRemoteEffectInputTypesNone = 0,
    RCPRemoteEffectInputTypesAll = ~0ul
};

typedef NS_OPTIONS(NSUInteger, RCPGlowEffectContentRenderingHints) {
    RCPGlowEffectContentRenderingHintPhoto = 1 << 0,
    RCPGlowEffectContentRenderingHintHasMotion = 1 << 1,
};

@interface RCPGlowEffectLayer : CALayer
@property (nonatomic, copy) void (^effectGroupConfigurator)(CARemoteEffectGroup *group);
@property (nonatomic) RCPGlowEffectContentRenderingHints contentRenderingHints;

- (void)setBrightnessMultiplier:(CGFloat)multiplier forInputTypes:(RCPRemoteEffectInputTypes)inputTypes;
@end

#endif // USE(APPLE_INTERNAL_SDK)
