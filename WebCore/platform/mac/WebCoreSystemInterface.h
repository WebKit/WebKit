/*
 * Copyright 2006 Apple Computer, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebCoreSystemInterface_h
#define WebCoreSystemInterface_h

typedef signed char BOOL;

#ifndef CGGEOMETRY_H_
typedef struct CGRect CGRect;
#endif

#if NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGRect NSRect;
#else
typedef struct _NSRect NSRect;
#endif

#ifndef CGGEOMETRY_H_
typedef struct CGPoint CGPoint;
#endif

#if NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
#else
typedef struct _NSPoint NSPoint;
#endif

typedef struct _NSRange NSRange;

#ifndef __OBJC__
class NSImage;
class NSString;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GLYPH_VECTOR_SIZE (50 * 32)

// In alphabetical order.

extern BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef);
extern void (*wkClearGlyphVector)(void* glyphs);
extern OSStatus (*wkConvertCharToGlyphs)(void* styleGroup, const UniChar*, unsigned numCharacters, void* glyphs);
extern void (*wkDrawBezeledTextFieldCell)(NSRect, BOOL enabled);
extern void (*wkDrawFocusRing)(CGContextRef, CGRect clipRect, CGColorRef, int radius);
extern BOOL (*wkFontSmoothingModeIsLCD)(int mode);
extern OSStatus (*wkGetATSStyleGroup)(ATSUStyle, void** styleGroup);
extern CGFontRef (*wkGetCGFontFromNSFont)(NSFont*);
extern ATSGlyphRef (*wkGetDefaultGlyphForChar)(NSFont*, UniChar);
extern NSFont* (*wkGetFontInLanguageForRange)(NSFont*, NSString*, NSRange);
extern NSFont* (*wkGetFontInLanguageForCharacter)(NSFont*, UniChar);
extern void (*wkGetFontMetrics)(NSFont*, int* ascent, int* descent, int* lineGap, unsigned* unitsPerEm);
extern BOOL (*wkGetGlyphTransformedAdvances)(NSFont*, CGAffineTransform*, ATSGlyphRef*, CGSize* advance);
extern ATSLayoutRecord* (*wkGetGlyphVectorFirstRecord)(void* glyphVector);
extern int (*wkGetGlyphVectorNumGlyphs)(void* glyphVector);
extern size_t (*wkGetGlyphVectorRecordSize)(void* glyphVector);
extern NSString* (*wkGetMIMETypeForExtension)(NSString*);
extern ATSUFontID (*wkGetNSFontATSUFontId)(NSFont*);
extern OSStatus (*wkInitializeGlyphVector)(int count, void* glyphs);
extern void (*wkReleaseStyleGroup)(void* group);
extern void (*wkSetCGFontRenderingMode)(CGContextRef, NSFont*);
extern void (*wkSetDragImage)(NSImage*, NSPoint offset);
extern void (*wkSetPatternPhaseInUserSpace)(CGContextRef, CGPoint point);
extern void (*wkSetUpFontCache)(size_t);

#ifdef __cplusplus
}
#endif

#endif
