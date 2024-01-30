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

#import <Foundation/Foundation.h>

#if defined(TARGET_OS_VISION) && TARGET_OS_VISION

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, WKSLinearMediaContentMode) {
    WKSLinearMediaContentModeNone = 0,
    WKSLinearMediaContentModeScaleAspectFit,
    WKSLinearMediaContentModeScaleAspectFill,
    WKSLinearMediaContentModeScaleToFill
};

typedef NS_ENUM(NSInteger, WKSLinearMediaContentType) {
    WKSLinearMediaContentTypeNone = 0,
    WKSLinearMediaContentTypeImmersive,
    WKSLinearMediaContentTypeSpatial,
    WKSLinearMediaContentTypePlanar,
    WKSLinearMediaContentTypeAudioOnly
};

typedef NS_ENUM(NSInteger, WKSLinearMediaPresentationMode) {
    WKSLinearMediaPresentationModeNone = 0,
    WKSLinearMediaPresentationModeInline,
    WKSLinearMediaPresentationModeFullscreen,
    WKSLinearMediaPresentationModeFullscreenFromInline,
    WKSLinearMediaPresentationModePip
};

typedef NS_ENUM(NSInteger, WKSLinearMediaViewingMode) {
    WKSLinearMediaViewingModeNone = 0,
    WKSLinearMediaViewingModeMono,
    WKSLinearMediaViewingModeStereo,
    WKSLinearMediaViewingModeImmersive,
    WKSLinearMediaViewingModeSpatial
};

typedef NS_OPTIONS(NSInteger, WKSLinearMediaFullscreenBehaviors) {
    WKSLinearMediaFullscreenBehaviorsSceneResize = 1 << 0,
    WKSLinearMediaFullscreenBehaviorsSceneSizeRestrictions = 1 << 1,
    WKSLinearMediaFullscreenBehaviorsSceneChromeOptions = 1 << 2,
    WKSLinearMediaFullscreenBehaviorsHostContentInline = 1 << 3,
};

API_AVAILABLE(visionos(1.0))
@interface WKSLinearMediaTimeRange : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithLowerBound:(NSTimeInterval)lowerBound upperBound:(NSTimeInterval)upperBound NS_DESIGNATED_INITIALIZER;
@property (nonatomic, readonly) NSTimeInterval lowerBound;
@property (nonatomic, readonly) NSTimeInterval upperBound;
@end

API_AVAILABLE(visionos(1.0))
@interface WKSLinearMediaTrack : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithLocalizedDisplayName:(NSString *)localizedDisplayName NS_DESIGNATED_INITIALIZER;
@property (nonatomic, readonly, copy) NSString *localizedDisplayName;
@end

NS_ASSUME_NONNULL_END

#endif /* defined(TARGET_OS_VISION) && TARGET_OS_VISION */
