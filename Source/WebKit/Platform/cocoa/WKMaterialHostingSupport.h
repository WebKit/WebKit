/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <wtf/PlatformHave.h>

#if HAVE(MATERIAL_HOSTING)

@class CALayer;

#if PLATFORM(IOS_FAMILY)
@class UIView;
#endif

typedef NS_ENUM(NSInteger, WKHostedMaterialEffectType) {
    WKHostedMaterialEffectTypeNone,
    WKHostedMaterialEffectTypeGlass,
    WKHostedMaterialEffectTypeClearGlass,
    WKHostedMaterialEffectTypeSubduedGlass,
    WKHostedMaterialEffectTypeMediaControlsGlass,
    WKHostedMaterialEffectTypeSubduedMediaControlsGlass,
};

typedef NS_ENUM(NSInteger, WKHostedMaterialColorScheme) {
    WKHostedMaterialColorSchemeLight,
    WKHostedMaterialColorSchemeDark,
};

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_UI_ACTOR
@interface WKMaterialHostingSupport : NSObject

+ (BOOL)isMaterialHostingAvailable;

+ (CALayer *)hostingLayer;
+ (void)updateHostingLayer:(CALayer *)hostingLayer materialEffectType:(WKHostedMaterialEffectType)materialEffectType colorScheme:(WKHostedMaterialColorScheme) colorScheme cornerRadius:(CGFloat)cornerRadius;
+ (nullable CALayer *)contentLayerForMaterialHostingLayer:(CALayer *)hostingLayer;

#if PLATFORM(IOS_FAMILY)
+ (UIView *)hostingView:(UIView *)contentView;
+ (void)updateHostingView:(UIView *)hostingView contentView:(UIView *)contentView materialEffectType:(WKHostedMaterialEffectType)materialEffectType colorScheme:(WKHostedMaterialColorScheme) colorScheme cornerRadius:(CGFloat)cornerRadius;
#endif

@end

NS_ASSUME_NONNULL_END

#endif // HAVE(MATERIAL_HOSTING)
