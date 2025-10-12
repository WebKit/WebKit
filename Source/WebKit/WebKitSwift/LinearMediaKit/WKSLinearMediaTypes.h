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

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

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

typedef NS_ENUM(NSInteger, WKSLinearMediaPresentationState) {
    WKSLinearMediaPresentationStateInline = 0,
    WKSLinearMediaPresentationStateEnteringFullscreen,
    WKSLinearMediaPresentationStateFullscreen,
    WKSLinearMediaPresentationStateExitingFullscreen,
    WKSLinearMediaPresentationStateEnteringExternal,
    WKSLinearMediaPresentationStateExternal
};

typedef NS_ENUM(NSInteger, WKSLinearMediaViewingMode) {
    WKSLinearMediaViewingModeNone = 0,
    WKSLinearMediaViewingModeMono,
    WKSLinearMediaViewingModeStereo,
    WKSLinearMediaViewingModeImmersive,
    WKSLinearMediaViewingModeSpatial
};

@interface WKSLinearMediaContentMetadata : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithTitle:(nullable NSString *)title subtitle:(nullable NSString *)subtitle NS_DESIGNATED_INITIALIZER;
@property (nonatomic, readonly, copy, nullable) NSString *title;
@property (nonatomic, readonly, copy, nullable) NSString *subtitle;
@end

@interface WKSLinearMediaTimeRange : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithLowerBound:(NSTimeInterval)lowerBound upperBound:(NSTimeInterval)upperBound NS_DESIGNATED_INITIALIZER;
@property (nonatomic, readonly) NSTimeInterval lowerBound;
@property (nonatomic, readonly) NSTimeInterval upperBound;
@end

@interface WKSLinearMediaTrack : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithLocalizedDisplayName:(NSString *)localizedDisplayName NS_DESIGNATED_INITIALIZER;
@property (nonatomic, readonly, copy) NSString *localizedDisplayName;
@end

@interface WKSLinearMediaSpatialVideoMetadata : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithWidth:(SInt32)width height:(SInt32)height horizontalFOVDegrees:(float)horizontalFOVDegrees baseline:(float)baseline disparityAdjustment:(float)disparityAdjustment NS_DESIGNATED_INITIALIZER;
@property (nonatomic, readonly) SInt32 width;
@property (nonatomic, readonly) SInt32 height;
@property (nonatomic, readonly) float horizontalFOVDegrees;
@property (nonatomic, readonly) float baseline;
@property (nonatomic, readonly) float disparityAdjustment;
@end

NS_SWIFT_UI_ACTOR
@interface WKSPlayableViewControllerHost : NSObject

@property (nonatomic, readonly) UIViewController *viewController;

@property (nonatomic, readonly, nullable) UIViewController *environmentPickerButtonViewController;

@property (nonatomic) BOOL dismissFullScreenOnExitingDocking;

@property (nonatomic) BOOL automaticallyDockOnFullScreenPresentation;

@property (nonatomic) BOOL prefersAutoDimming;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif /* defined(TARGET_OS_VISION) && TARGET_OS_VISION */
