/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#import <CoreVideo/CoreVideo.h>
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/IOSurfaceSPI.h>

#if USE(APPLE_INTERNAL_SDK)

#import <QuartzCore/CABackingStore.h>
#import <QuartzCore/CAColorMatrix.h>
#import <QuartzCore/CARenderServer.h>

#ifdef __OBJC__

#import <QuartzCore/CAContext.h>
#import <QuartzCore/CALayerHost.h>
#import <QuartzCore/CALayerPrivate.h>
#import <QuartzCore/CAMediaTimingFunctionPrivate.h>
#import <QuartzCore/QuartzCorePrivate.h>

#if PLATFORM(MAC)
#import <QuartzCore/CARenderCG.h>
#endif

#endif // __OBJC__

#else

#ifdef __OBJC__
typedef struct _CARenderContext CARenderContext;

@interface CAContext : NSObject
@end

@interface CAContext ()
+ (NSArray *)allContexts;
+ (CAContext *)localContext;
+ (CAContext *)remoteContextWithOptions:(NSDictionary *)dict;
#if PLATFORM(MAC)
+ (CAContext *)contextWithCGSConnection:(CGSConnectionID)cid options:(NSDictionary *)dict;
#endif
+ (id)objectForSlot:(uint32_t)name;
- (uint32_t)createImageSlot:(CGSize)size hasAlpha:(BOOL)flag;
- (void)deleteSlot:(uint32_t)name;
- (void)invalidate;
- (void)invalidateFences;
- (mach_port_t)createFencePort;
- (void)setFencePort:(mach_port_t)port;
- (void)setFencePort:(mach_port_t)port commitHandler:(void(^)(void))block;

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
+ (void)setAllowsCGSConnections:(BOOL)flag;
#endif

#if PLATFORM(MAC)
@property uint32_t commitPriority;
@property BOOL colorMatchUntaggedContent;
#endif
@property (readonly) uint32_t contextId;
@property (strong) CALayer *layer;
@property CGColorSpaceRef colorSpace;
@property (readonly) CARenderContext* renderContext;
@end

@interface CALayer ()
- (CAContext *)context;
- (void)setContextId:(uint32_t)contextID;
- (CGSize)size;
- (void *)regionBeingDrawn;
- (void)reloadValueForKeyPath:(NSString *)keyPath;
- (void)setCornerRadius:(CGFloat)cornerRadius;
@property BOOL allowsGroupBlending;
@property BOOL allowsHitTesting;
@property BOOL canDrawConcurrently;
@property BOOL contentsOpaque;
@property BOOL hitTestsAsOpaque;
@property BOOL needsLayoutOnGeometryChange;
@property BOOL shadowPathIsBounds;
@property BOOL continuousCorners;
@end

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

typedef enum {
    kCATransactionPhasePreLayout,
    kCATransactionPhasePreCommit,
    kCATransactionPhasePostCommit,
    kCATransactionPhaseNull = ~0u
} CATransactionPhase;

@interface CATransaction ()
+ (void)addCommitHandler:(void(^)(void))block forPhase:(CATransactionPhase)phase;

#if PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
+ (CATransactionPhase)currentPhase;
#endif

@end

@interface CALayerHost : CALayer
@property uint32_t contextId;
@property BOOL inheritsSecurity;
@end

@interface CASpringAnimation (Private)
@property CGFloat velocity;
@end

@interface CAMediaTimingFunction ()
- (float)_solveForInput:(float)t;
@end

#endif // __OBJC__

#endif

WTF_EXTERN_C_BEGIN

// FIXME: Declare these functions even when USE(APPLE_INTERNAL_SDK) is true once we can fix <rdar://problem/26584828> in a better way.
#if !USE(APPLE_INTERNAL_SDK)
void CARenderServerCaptureLayerWithTransform(mach_port_t, uint32_t clientId, uint64_t layerId, uint32_t slotId, int32_t ox, int32_t oy, const CATransform3D*);

#if HAVE(IOSURFACE)
void CARenderServerRenderLayerWithTransform(mach_port_t server_port, uint32_t client_id, uint64_t layer_id, IOSurfaceRef, int32_t ox, int32_t oy, const CATransform3D*);
void CARenderServerRenderDisplayLayerWithTransformAndTimeOffset(mach_port_t, CFStringRef display_name, uint32_t client_id, uint64_t layer_id, IOSurfaceRef, int32_t ox, int32_t oy, const CATransform3D*, CFTimeInterval);
#else
typedef struct CARenderServerBuffer* CARenderServerBufferRef;
CARenderServerBufferRef CARenderServerCreateBuffer(size_t, size_t);
void CARenderServerDestroyBuffer(CARenderServerBufferRef);
size_t CARenderServerGetBufferWidth(CARenderServerBufferRef);
size_t CARenderServerGetBufferHeight(CARenderServerBufferRef);
size_t CARenderServerGetBufferRowBytes(CARenderServerBufferRef);
uint8_t* CARenderServerGetBufferData(CARenderServerBufferRef);
size_t CARenderServerGetBufferDataSize(CARenderServerBufferRef);

bool CARenderServerRenderLayerWithTransform(mach_port_t, uint32_t client_id, uint64_t layer_id, CARenderServerBufferRef, int32_t ox, int32_t oy, const CATransform3D*);
#endif
#endif // USE(APPLE_INTERNAL_SDK)

typedef struct _CAMachPort *CAMachPortRef;
CAMachPortRef CAMachPortCreate(mach_port_t);
mach_port_t CAMachPortGetPort(CAMachPortRef);
CFTypeID CAMachPortGetTypeID(void);

void CABackingStoreCollectBlocking(void);

typedef struct _CARenderCGContext CARenderCGContext;
typedef struct _CARenderContext CARenderContext;
typedef struct _CARenderUpdate CARenderUpdate;
CARenderCGContext *CARenderCGNew(uint32_t feature_flags);
CARenderUpdate* CARenderUpdateBegin(void* buffer, size_t, CFTimeInterval, const CVTimeStamp*, uint32_t finished_seed, const CGRect* bounds);
bool CARenderServerStart();
mach_port_t CARenderServerGetPort();
void CARenderCGDestroy(CARenderCGContext*);
void CARenderCGRender(CARenderCGContext*, CARenderUpdate*, CGContextRef);
void CARenderUpdateAddContext(CARenderUpdate*, CARenderContext*);
void CARenderUpdateAddRect(CARenderUpdate*, const CGRect*);
void CARenderUpdateFinish(CARenderUpdate*);

WTF_EXTERN_C_END

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

extern NSString * const kCAContextCIFilterBehavior;
extern NSString * const kCAContextDisplayName;
extern NSString * const kCAContextDisplayId;
extern NSString * const kCAContextIgnoresHitTest;
extern NSString * const kCAContextPortNumber;

#if PLATFORM(IOS_FAMILY)
extern NSString * const kCAContextSecure;
extern NSString * const kCAContentsFormatRGBA10XR;
#endif

#if PLATFORM(MAC)
extern NSString * const kCAContentsFormatRGBA8ColorRGBA8LinearGlyphMask;
#endif
