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

#import <QuartzCore/QuartzCore.h>

#if USE(APPLE_INTERNAL_SDK)

#include <QuartzCore/CAColorMatrix.h>

#ifdef __OBJC__
#import <QuartzCore/CALayerPrivate.h>

// FIXME: As a workaround for <rdar://problem/18985152>, we conditionally enclose the following
// headers in an extern "C" linkage block to make it suitable for Objective-C++ use. Once this
// bug has been fixed we can simply include header <QuartzCore/QuartzCorePrivate.h> instead of
// including specific QuartzCore headers.
#ifdef __cplusplus
extern "C" {
#endif

#import <QuartzCore/CAContext.h>
#import <QuartzCore/CAFilter.h>
#import <QuartzCore/CATiledLayerPrivate.h>

#ifdef __cplusplus
}
#endif
#endif // __OBJC__

#else

#ifdef __OBJC__
@interface CAContext : NSObject
@end

@interface CAContext (Details)
+ (CAContext *)remoteContextWithOptions:(NSDictionary *)dict;
+ (id)objectForSlot:(uint32_t)name;
- (uint32_t)createImageSlot:(CGSize)size hasAlpha:(BOOL)flag;
- (void)deleteSlot:(uint32_t)name;
@end

@interface CALayer (Details)
- (CAContext *)context;
- (CGSize)size;
- (void *)regionBeingDrawn;
- (void)setContentsChanged;
@property BOOL acceleratesDrawing;
@property BOOL allowsGroupBlending;
@property BOOL canDrawConcurrently;
@property BOOL contentsOpaque;
@property BOOL needsLayoutOnGeometryChange;
@property BOOL shadowPathIsBounds;
@end

@interface CATiledLayer (Details)
- (void)displayInRect:(CGRect)rect levelOfDetail:(int)levelOfDetail options:(NSDictionary *)dictionary;
- (void)setNeedsDisplayInRect:(CGRect)rect levelOfDetail:(int)levelOfDetail options:(NSDictionary *)dictionary;
@end

struct CAColorMatrix {
    float m11, m12, m13, m14, m15;
    float m21, m22, m23, m24, m25;
    float m31, m32, m33, m34, m35;
    float m41, m42, m43, m44, m45;
};
typedef struct CAColorMatrix CAColorMatrix;

@interface NSValue (CADetails)
+ (NSValue *)valueWithCAColorMatrix:(CAColorMatrix)t;
@end

@interface CAFilter : NSObject <NSCopying, NSMutableCopying, NSCoding>
@end

@interface CAFilter (Details)
+ (CAFilter *)filterWithType:(NSString *)type;
@property (copy) NSString *name;
@end
#endif // __OBJC__

#endif

EXTERN_C NSString * const kCATiledLayerRemoveImmediately;

EXTERN_C NSString * const kCAFilterColorInvert;
EXTERN_C NSString * const kCAFilterColorMatrix;
EXTERN_C NSString * const kCAFilterColorMonochrome;
EXTERN_C NSString * const kCAFilterColorHueRotate;
EXTERN_C NSString * const kCAFilterColorSaturate;
EXTERN_C NSString * const kCAFilterGaussianBlur;


EXTERN_C NSString * const kCAFilterNormalBlendMode;
EXTERN_C NSString * const kCAFilterMultiplyBlendMode;
EXTERN_C NSString * const kCAFilterScreenBlendMode;
EXTERN_C NSString * const kCAFilterOverlayBlendMode;
EXTERN_C NSString * const kCAFilterDarkenBlendMode;
EXTERN_C NSString * const kCAFilterLightenBlendMode;
EXTERN_C NSString * const kCAFilterColorDodgeBlendMode;
EXTERN_C NSString * const kCAFilterColorBurnBlendMode;
EXTERN_C NSString * const kCAFilterSoftLightBlendMode;
EXTERN_C NSString * const kCAFilterHardLightBlendMode;
EXTERN_C NSString * const kCAFilterDifferenceBlendMode;
EXTERN_C NSString * const kCAFilterExclusionBlendMode;

EXTERN_C NSString * const kCAContextDisplayName;
