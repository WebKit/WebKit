/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#if HAVE(IOSURFACE)
#include <pal/spi/cocoa/IOSurfaceSPI.h>
#endif

#if PLATFORM(MAC)
#include <ColorSync/ColorSync.h>
#endif

#if USE(APPLE_INTERNAL_SDK)

#if PLATFORM(MAC)
#include <ColorSync/ColorSyncPriv.h>
#endif
#include <CoreGraphics/CGFontCache.h>
#include <CoreGraphics/CoreGraphicsPrivate.h>

#else

struct CGFontHMetrics {
    int ascent;
    int descent;
    int lineGap;
    int maxAdvanceWidth;
    int minLeftSideBearing;
    int minRightSideBearing;
};

struct CGFontDescriptor {
    CGRect bbox;
    CGFloat ascent;
    CGFloat descent;
    CGFloat capHeight;
    CGFloat italicAngle;
    CGFloat stemV;
    CGFloat stemH;
    CGFloat avgWidth;
    CGFloat maxWidth;
    CGFloat missingWidth;
    CGFloat leading;
    CGFloat xHeight;
};

typedef const struct CGColorTransform* CGColorTransformRef;

typedef enum {
    kCGContextTypeUnknown,
    kCGContextTypePDF,
    kCGContextTypePostScript,
    kCGContextTypeWindow,
    kCGContextTypeBitmap,
    kCGContextTypeGL,
    kCGContextTypeDisplayList,
    kCGContextTypeKSeparation,
    kCGContextTypeIOSurface,
    kCGContextTypeCount
} CGContextType;

typedef enum {
    kCGCompositeCopy = 1,
    kCGCompositeSover = 2,
} CGCompositeOperation;

enum {
    kCGFontRenderingStyleAntialiasing = 1 << 0,
    kCGFontRenderingStyleSmoothing = 1 << 1,
    kCGFontRenderingStyleSubpixelPositioning = 1 << 2,
    kCGFontRenderingStyleSubpixelQuantization = 1 << 3,
    kCGFontRenderingStylePlatformNative = 1 << 9,
    kCGFontRenderingStyleMask = 0x20F,
};
typedef uint32_t CGFontRenderingStyle;

enum {
    kCGFontAntialiasingStyleUnfiltered = 0 << 7,
    kCGFontAntialiasingStyleFilterLight = 1 << 7,
#if PLATFORM(MAC)
    kCGFontAntialiasingStyleUnfilteredCustomDilation = (8 << 7),
#endif
};
typedef uint32_t CGFontAntialiasingStyle;

enum {
    kCGImageCachingTransient = 1,
    kCGImageCachingTemporary = 3,
};
typedef uint32_t CGImageCachingFlags;

#if PLATFORM(COCOA)
typedef struct CGSRegionEnumeratorObject* CGSRegionEnumeratorObj;
typedef struct CGSRegionObject* CGSRegionObj;
typedef struct CGSRegionObject* CGRegionRef;
#endif

#ifdef CGFLOAT_IS_DOUBLE
#define CGRound(value) round((value))
#define CGFloor(value) floor((value))
#define CGCeiling(value) ceil((value))
#define CGFAbs(value) fabs((value))
#else
#define CGRound(value) roundf((value))
#define CGFloor(value) floorf((value))
#define CGCeiling(value) ceilf((value))
#define CGFAbs(value) fabsf((value))
#endif

static inline CGFloat CGFloatMin(CGFloat a, CGFloat b) { return isnan(a) ? b : ((isnan(b) || a < b) ? a : b); }

typedef struct CGFontCache CGFontCache;

#if PLATFORM(COCOA)

enum {
    kCGSWindowCaptureNominalResolution = 0x0200,
    kCGSCaptureIgnoreGlobalClipShape = 0x0800,
};
typedef uint32_t CGSWindowCaptureOptions;

typedef CF_ENUM (int32_t, CGStyleDrawOrdering) {
    kCGStyleDrawOrderingStyleOnly = 0,
    kCGStyleDrawOrderingBelow = 1,
    kCGStyleDrawOrderingAbove = 2,
};

typedef CF_ENUM (int32_t, CGFocusRingOrdering) {
    kCGFocusRingOrderingNone = kCGStyleDrawOrderingStyleOnly,
    kCGFocusRingOrderingBelow = kCGStyleDrawOrderingBelow,
    kCGFocusRingOrderingAbove = kCGStyleDrawOrderingAbove,
};

typedef CF_ENUM (int32_t, CGFocusRingTint) {
    kCGFocusRingTintBlue = 0,
    kCGFocusRingTintGraphite = 1,
};

struct CGFocusRingStyle {
    unsigned int version;
    CGFocusRingTint tint;
    CGFocusRingOrdering ordering;
    CGFloat alpha;
    CGFloat radius;
    CGFloat threshold;
    CGRect bounds;
    int accumulate;
};
typedef struct CGFocusRingStyle CGFocusRingStyle;

#endif // PLATFORM(COCOA)

#if PLATFORM(MAC)

typedef CF_ENUM(uint32_t, CGSNotificationType) {
    kCGSFirstConnectionNotification = 900,
    kCGSFirstSessionNotification = 1500,
};

static const CGSNotificationType kCGSConnectionWindowModificationsStarted = (CGSNotificationType)(kCGSFirstConnectionNotification + 6);
static const CGSNotificationType kCGSConnectionWindowModificationsStopped = (CGSNotificationType)(kCGSFirstConnectionNotification + 7);
static const CGSNotificationType kCGSessionConsoleConnect = kCGSFirstSessionNotification;
static const CGSNotificationType kCGSessionConsoleDisconnect = (CGSNotificationType)(kCGSessionConsoleConnect + 1);

#endif // PLATFORM(MAC)

#endif // USE(APPLE_INTERNAL_SDK)

#if PLATFORM(COCOA)
typedef uint32_t CGSByteCount;
typedef uint32_t CGSConnectionID;
typedef uint32_t CGSWindowCount;
typedef uint32_t CGSWindowID;

typedef CGSWindowID* CGSWindowIDList;
typedef struct CF_BRIDGED_TYPE(id) CGSRegionObject* CGSRegionObj;
typedef struct CF_BRIDGED_TYPE(id) CGStyle* CGStyleRef;

typedef void* CGSNotificationArg;
typedef void* CGSNotificationData;
#endif

#if PLATFORM(MAC)
typedef void (*CGSNotifyConnectionProcPtr)(CGSNotificationType, void* data, uint32_t data_length, void* arg, CGSConnectionID);
typedef void (*CGSNotifyProcPtr)(CGSNotificationType, void* data, uint32_t data_length, void* arg);
#endif

WTF_EXTERN_C_BEGIN

CGColorRef CGColorTransformConvertColor(CGColorTransformRef, CGColorRef, CGColorRenderingIntent);
CGColorTransformRef CGColorTransformCreate(CGColorSpaceRef, CFDictionaryRef attributes);

CGAffineTransform CGContextGetBaseCTM(CGContextRef);
CGCompositeOperation CGContextGetCompositeOperation(CGContextRef);
CGColorRef CGContextGetFillColorAsColor(CGContextRef);
CGFloat CGContextGetLineWidth(CGContextRef);
bool CGContextGetShouldSmoothFonts(CGContextRef);
bool CGContextGetShouldAntialias(CGContextRef);
void CGContextSetBaseCTM(CGContextRef, CGAffineTransform);
void CGContextSetCTM(CGContextRef, CGAffineTransform);
void CGContextSetCompositeOperation(CGContextRef, CGCompositeOperation);
void CGContextSetShouldAntialiasFonts(CGContextRef, bool shouldAntialiasFonts);
void CGContextResetClip(CGContextRef);
CGContextType CGContextGetType(CGContextRef);

CFStringRef CGFontCopyFamilyName(CGFontRef);
bool CGFontGetDescriptor(CGFontRef, CGFontDescriptor*);
bool CGFontGetGlyphAdvancesForStyle(CGFontRef, const CGAffineTransform* , CGFontRenderingStyle, const CGGlyph[], size_t count, CGSize advances[]);
void CGFontGetGlyphsForUnichars(CGFontRef, const UniChar[], CGGlyph[], size_t count);
const CGFontHMetrics* CGFontGetHMetrics(CGFontRef);
const char* CGFontGetPostScriptName(CGFontRef);
bool CGFontIsFixedPitch(CGFontRef);
void CGFontSetShouldUseMulticache(bool);

void CGImageSetCachingFlags(CGImageRef, CGImageCachingFlags);
CGImageCachingFlags CGImageGetCachingFlags(CGImageRef);

CGDataProviderRef CGPDFDocumentGetDataProvider(CGPDFDocumentRef);

CGFontAntialiasingStyle CGContextGetFontAntialiasingStyle(CGContextRef);
void CGContextSetFontAntialiasingStyle(CGContextRef, CGFontAntialiasingStyle);
bool CGContextGetAllowsFontSubpixelPositioning(CGContextRef);
bool CGContextDrawsWithCorrectShadowOffsets(CGContextRef);
CGPatternRef CGPatternCreateWithImage2(CGImageRef, CGAffineTransform, CGPatternTiling);

#if HAVE(IOSURFACE)
CGContextRef CGIOSurfaceContextCreate(IOSurfaceRef, size_t, size_t, size_t, size_t, CGColorSpaceRef, CGBitmapInfo);
CGImageRef CGIOSurfaceContextCreateImage(CGContextRef);
CGImageRef CGIOSurfaceContextCreateImageReference(CGContextRef);
CGColorSpaceRef CGIOSurfaceContextGetColorSpace(CGContextRef);
void CGIOSurfaceContextSetDisplayMask(CGContextRef, uint32_t mask);
#endif

#if PLATFORM(COCOA)
bool CGColorSpaceUsesExtendedRange(CGColorSpaceRef);

typedef struct CGPDFAnnotation *CGPDFAnnotationRef;
typedef bool (^CGPDFAnnotationDrawCallbackType)(CGContextRef context, CGPDFPageRef page, CGPDFAnnotationRef annotation);
void CGContextDrawPDFPageWithAnnotations(CGContextRef, CGPDFPageRef, CGPDFAnnotationDrawCallbackType);
void CGContextDrawPathDirect(CGContextRef, CGPathDrawingMode, CGPathRef, const CGRect* boundingBox);

CGColorSpaceRef CGContextCopyDeviceColorSpace(CGContextRef);
CFPropertyListRef CGColorSpaceCopyPropertyList(CGColorSpaceRef);
CGError CGSNewRegionWithRect(const CGRect*, CGRegionRef*);
CGError CGSPackagesEnableConnectionOcclusionNotifications(CGSConnectionID, bool flag, bool* outCurrentVisibilityState);
CGError CGSPackagesEnableConnectionWindowModificationNotifications(CGSConnectionID, bool flag, bool* outConnectionIsCurrentlyIdle);
CGError CGSReleaseRegion(const CGRegionRef CF_RELEASES_ARGUMENT);
CGError CGSReleaseRegionEnumerator(const CGSRegionEnumeratorObj);
CGError CGSSetWindowAlpha(CGSConnectionID, CGSWindowID, float alpha);
CGError CGSSetWindowClipShape(CGSConnectionID, CGSWindowID, CGRegionRef shape);
CGError CGSSetWindowWarp(CGSConnectionID, CGSWindowID, int w, int h, const float* mesh);
CGRect* CGSNextRect(const CGSRegionEnumeratorObj);
CGSRegionEnumeratorObj CGSRegionEnumerator(CGRegionRef);
CGStyleRef CGStyleCreateFocusRingWithColor(const CGFocusRingStyle*, CGColorRef);
void CGContextSetStyle(CGContextRef, CGStyleRef);

void CGContextDrawConicGradient(CGContextRef, CGGradientRef, CGPoint center, CGFloat angle);

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)
void CGPathAddUnevenCornersRoundedRect(CGMutablePathRef, const CGAffineTransform *, CGRect, const CGSize corners[4]);
#endif
#endif

#if PLATFORM(WIN)
CGFontCache* CGFontCacheGetLocalCache();
void CGFontCacheSetShouldAutoExpire(CGFontCache*, bool);
void CGFontCacheSetMaxSize(CGFontCache*, size_t);
#endif

#if PLATFORM(MAC)
void CGSShutdownServerConnections(void);

CGSConnectionID CGSMainConnectionID(void);
CFArrayRef CGSHWCaptureWindowList(CGSConnectionID, CGSWindowIDList windowList, CGSWindowCount, CGSWindowCaptureOptions);
CGError CGSSetConnectionProperty(CGSConnectionID, CGSConnectionID ownerCid, CFStringRef key, CFTypeRef value);
CGError CGSCopyConnectionProperty(CGSConnectionID, CGSConnectionID ownerCid, CFStringRef key, CFTypeRef *value);
CGError CGSGetScreenRectForWindow(CGSConnectionID, CGSWindowID, CGRect *);
CGError CGSRegisterConnectionNotifyProc(CGSConnectionID, CGSNotifyConnectionProcPtr, CGSNotificationType, void* arg);
CGError CGSRegisterNotifyProc(CGSNotifyProcPtr, CGSNotificationType, void* arg);
bool ColorSyncProfileIsWideGamut(ColorSyncProfileRef);

size_t CGDisplayModeGetPixelsWide(CGDisplayModeRef);
size_t CGDisplayModeGetPixelsHigh(CGDisplayModeRef);

#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
CGError CGSSetDenyWindowServerConnections(bool);

typedef int32_t CGSDisplayID;
CGSDisplayID CGSMainDisplayID(void);

#endif // ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)

#endif

WTF_EXTERN_C_END
