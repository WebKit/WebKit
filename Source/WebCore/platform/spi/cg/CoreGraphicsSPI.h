/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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

#ifndef CoreGraphicsSPI_h
#define CoreGraphicsSPI_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#if USE(APPLE_INTERNAL_SDK)

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
#define CGFAbs(value) fabs((value))
#else
#define CGRound(value) roundf((value))
#define CGFloor(value) floorf((value))
#define CGFAbs(value) fabsf((value))
#endif

static inline CGFloat CGFloatMin(CGFloat a, CGFloat b) { return isnan(a) ? b : ((isnan(b) || a < b) ? a : b); }

typedef struct CGFontCache CGFontCache;

#endif // USE(APPLE_INTERNAL_SDK)

EXTERN_C CGColorRef CGColorTransformConvertColor(CGColorTransformRef, CGColorRef, CGColorRenderingIntent);
EXTERN_C CGColorTransformRef CGColorTransformCreate(CGColorSpaceRef, CFDictionaryRef attributes);

EXTERN_C CGAffineTransform CGContextGetBaseCTM(CGContextRef);
EXTERN_C CGCompositeOperation CGContextGetCompositeOperation(CGContextRef);
EXTERN_C CGColorRef CGContextGetFillColorAsColor(CGContextRef);
EXTERN_C CGFloat CGContextGetLineWidth(CGContextRef);
EXTERN_C bool CGContextGetShouldSmoothFonts(CGContextRef);
EXTERN_C void CGContextSetBaseCTM(CGContextRef, CGAffineTransform);
EXTERN_C void CGContextSetCTM(CGContextRef, CGAffineTransform);
EXTERN_C void CGContextSetCompositeOperation(CGContextRef, CGCompositeOperation);
EXTERN_C void CGContextSetShouldAntialiasFonts(CGContextRef, bool shouldAntialiasFonts);

EXTERN_C CFStringRef CGFontCopyFamilyName(CGFontRef);
EXTERN_C bool CGFontGetDescriptor(CGFontRef, CGFontDescriptor*);
EXTERN_C bool CGFontGetGlyphAdvancesForStyle(CGFontRef, const CGAffineTransform* , CGFontRenderingStyle, const CGGlyph[], size_t count, CGSize advances[]);
EXTERN_C void CGFontGetGlyphsForUnichars(CGFontRef, const UniChar[], CGGlyph[], size_t count);
EXTERN_C const CGFontHMetrics* CGFontGetHMetrics(CGFontRef);
EXTERN_C const char* CGFontGetPostScriptName(CGFontRef);
EXTERN_C bool CGFontIsFixedPitch(CGFontRef);
EXTERN_C void CGFontSetShouldUseMulticache(bool);

EXTERN_C void CGImageSetCachingFlags(CGImageRef, CGImageCachingFlags);
EXTERN_C CGImageCachingFlags CGImageGetCachingFlags(CGImageRef);

#if PLATFORM(COCOA)
EXTERN_C CGSRegionEnumeratorObj CGSRegionEnumerator(CGRegionRef);
EXTERN_C CGRect* CGSNextRect(const CGSRegionEnumeratorObj);
EXTERN_C CGError CGSReleaseRegionEnumerator(const CGSRegionEnumeratorObj);
#endif

#if PLATFORM(WIN)
EXTERN_C CGFontCache* CGFontCacheGetLocalCache();
EXTERN_C void CGFontCacheSetShouldAutoExpire(CGFontCache*, bool);
EXTERN_C void CGFontCacheSetMaxSize(CGFontCache*, size_t);
#endif

#endif // CoreGraphicsSPI_h
