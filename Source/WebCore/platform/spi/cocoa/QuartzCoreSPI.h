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
#import <WebCore/IOSurfaceSPI.h>

#if USE(APPLE_INTERNAL_SDK)

#include <QuartzCore/CAColorMatrix.h>
#include <QuartzCore/CARenderServer.h>

#ifdef __OBJC__

#import <QuartzCore/CALayerHost.h>
#import <QuartzCore/CALayerPrivate.h>
#import <QuartzCore/QuartzCorePrivate.h>

#if PLATFORM(IOS)
#import <QuartzCore/CADisplay.h>
#endif

#endif // __OBJC__

#else

#ifdef __OBJC__
@interface CAContext : NSObject
@end

@interface CAContext ()
+ (NSArray *)allContexts;
+ (CAContext *)remoteContextWithOptions:(NSDictionary *)dict;
+ (id)objectForSlot:(uint32_t)name;
- (uint32_t)createImageSlot:(CGSize)size hasAlpha:(BOOL)flag;
- (void)deleteSlot:(uint32_t)name;
- (void)invalidate;
- (mach_port_t)createFencePort;
- (void)setFencePort:(mach_port_t)port;
- (void)setFencePort:(mach_port_t)port commitHandler:(void(^)(void))block;
#if PLATFORM(MAC)
@property BOOL colorMatchUntaggedContent;
#endif
@property (readonly) uint32_t contextId;
@property (strong) CALayer *layer;
@property CGColorSpaceRef colorSpace;
@end

@interface CALayer ()
- (CAContext *)context;
- (CGSize)size;
- (void *)regionBeingDrawn;
- (void)setContentsChanged;
@property BOOL acceleratesDrawing;
@property BOOL allowsGroupBlending;
@property BOOL canDrawConcurrently;
@property BOOL contentsOpaque;
@property BOOL hitTestsAsOpaque;
@property BOOL needsLayoutOnGeometryChange;
@property BOOL shadowPathIsBounds;
@end

@interface CATiledLayer ()
- (void)displayInRect:(CGRect)rect levelOfDetail:(int)levelOfDetail options:(NSDictionary *)dictionary;
- (void)setNeedsDisplayInRect:(CGRect)rect levelOfDetail:(int)levelOfDetail options:(NSDictionary *)dictionary;
@end

#if PLATFORM(IOS)
@interface CADisplay : NSObject
@end

@interface CADisplay ()
@property (nonatomic, readonly) NSString *name;
@end
#endif

#if ENABLE(FILTERS_LEVEL_2)
@interface CABackdropLayer : CALayer
@property BOOL windowServerAware;
@end
#endif

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

@interface CAFilter ()
+ (CAFilter *)filterWithType:(NSString *)type;
@property (copy) NSString *name;
@end

#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
typedef enum {
    kCATransactionPhasePreLayout,
    kCATransactionPhasePreCommit,
    kCATransactionPhasePostCommit,
} CATransactionPhase;

@interface CATransaction ()
+ (void)addCommitHandler:(void(^)(void))block forPhase:(CATransactionPhase)phase;
@end
#endif

@interface CALayerHost : CALayer
@property uint32_t contextId;
@property BOOL inheritsSecurity;
@end

#endif // __OBJC__

#endif

EXTERN_C void CARenderServerCaptureLayerWithTransform(mach_port_t serverPort, uint32_t clientId, uint64_t layerId,
                                                      uint32_t slotId, int32_t ox, int32_t oy, const CATransform3D *);

#if USE(IOSURFACE)
EXTERN_C void CARenderServerRenderLayerWithTransform(mach_port_t server_port, uint32_t client_id, uint64_t layer_id, IOSurfaceRef iosurface, int32_t ox, int32_t oy, const CATransform3D *matrix);
EXTERN_C void CARenderServerRenderDisplayLayerWithTransformAndTimeOffset(mach_port_t server_port, CFStringRef display_name, uint32_t client_id, uint64_t layer_id, IOSurfaceRef iosurface, int32_t ox, int32_t oy, const CATransform3D *matrix, CFTimeInterval offset);
#endif


// FIXME: Move this into the APPLE_INTERNAL_SDK block once it's in an SDK.
@interface CAContext (AdditionalDetails)
#if PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
- (void)invalidateFences;
#endif
@end

extern NSString * const kCATiledLayerRemoveImmediately;

extern NSString * const kCAFilterColorInvert;
extern NSString * const kCAFilterColorMatrix;
extern NSString * const kCAFilterColorMonochrome;
extern NSString * const kCAFilterColorHueRotate;
extern NSString * const kCAFilterColorSaturate;
extern NSString * const kCAFilterGaussianBlur;
extern NSString * const kCAFilterPlusD;
extern NSString * const kCAFilterPlusL;

extern NSString * const kCAFilterNormalBlendMode;
extern NSString * const kCAFilterMultiplyBlendMode;
extern NSString * const kCAFilterScreenBlendMode;
extern NSString * const kCAFilterOverlayBlendMode;
extern NSString * const kCAFilterDarkenBlendMode;
extern NSString * const kCAFilterLightenBlendMode;
extern NSString * const kCAFilterColorDodgeBlendMode;
extern NSString * const kCAFilterColorBurnBlendMode;
extern NSString * const kCAFilterSoftLightBlendMode;
extern NSString * const kCAFilterHardLightBlendMode;
extern NSString * const kCAFilterDifferenceBlendMode;
extern NSString * const kCAFilterExclusionBlendMode;

extern NSString * const kCAContextDisplayName;
extern NSString * const kCAContextDisplayId;
extern NSString * const kCAContextIgnoresHitTest;

#if (PLATFORM(APPLETV) && __TV_OS_VERSION_MIN_REQUIRED < 100000) \
    || (PLATFORM(WATCHOS) && __WATCH_OS_VERSION_MIN_REQUIRED < 30000) \
    || (PLATFORM(IOS) && TARGET_OS_IOS && __IPHONE_OS_VERSION_MIN_REQUIRED < 100000) \
    || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101200)
@protocol CALayerDelegate <NSObject>
@end

@protocol CAAnimationDelegate <NSObject>
@end
#endif
