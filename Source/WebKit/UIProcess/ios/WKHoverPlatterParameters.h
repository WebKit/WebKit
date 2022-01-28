/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)

#import "PrototypeToolsSPI.h"

@interface WKHoverPlatterParameters : PTSettings

@property (nonatomic) BOOL platterEnabledForMouse;
@property (nonatomic) BOOL platterEnabledForHover;

@property (nonatomic) CGFloat platterCornerRadius;
@property (nonatomic) CGFloat platterPadding;
@property (nonatomic) CGFloat platterShadowOpacity;
@property (nonatomic) CGFloat platterShadowRadius;
@property (nonatomic) unsigned platterInflationSize;

@property (nonatomic) BOOL animateBetweenPlatters;
@property (nonatomic) CGFloat springMass;
@property (nonatomic) CGFloat springStiffness;
@property (nonatomic) CGFloat springDamping;
@property (nonatomic) NSTimeInterval duration;
@property (nonatomic) CGFloat useSpring;

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WKHoverPlatterParametersAdditions.h>
#endif

@end

@interface WKHoverPlatterDomain : PTDomain
+ (WKHoverPlatterParameters *)rootSettings;
@end

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT)
