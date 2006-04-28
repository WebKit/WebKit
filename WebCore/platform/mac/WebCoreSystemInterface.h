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

#ifdef __cplusplus
extern "C" {
#endif

// In alphabetical order.

extern BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef cgContext);
extern void (*wkClearGlyphVector)(void* glyphs);
extern OSStatus (*wkConvertCharToGlyphs)(void *styleGroup, const UniChar *characters, unsigned numCharacters, void* glyphs);
extern void (*wkDrawBezeledTextFieldCell)(NSRect, BOOL enabled);
extern void (*wkDrawFocusRing)(CGContextRef, CGRect clipRect, CGColorRef, int radius);
extern BOOL (*wkFontSmoothingModeIsLCD)(int mode);
extern OSStatus (*wkGetATSStyleGroup)(ATSUStyle fontStyle, void **styleGroup);
extern CGFontRef (*wkGetCGFontFromNSFont)(NSFont *font);
extern ATSGlyphRef (*wkGetDefaultGlyphForChar)(NSFont *font, UniChar c);
extern NSFont* (*wkGetFontInLanguageForRange)(NSFont *font, NSString *string, NSRange range);
extern NSFont* (*wkGetFontInLanguageForCharacter)(NSFont *font, UniChar ch);
extern void (*wkGetFontMetrics)(NSFont *font, int *ascent, int *descent, int *lineGap, unsigned *unitsPerEm);
extern BOOL (*wkGetGlyphTransformedAdvances)(NSFont *font, CGAffineTransform *m, ATSGlyphRef *glyph, CGSize *advance);
extern ATSLayoutRecord* (*wkGetGlyphVectorFirstRecord)(void* glyphVector);
extern int (*wkGetGlyphVectorNumGlyphs)(void* glyphVector);
extern size_t (*wkGetGlyphVectorRecordSize)(void* glyphVector);
extern ATSUFontID (*wkGetNSFontATSUFontId)(NSFont *font);
extern OSStatus (*wkInitializeGlyphVector)(int count, void* glyphs);
extern void (*wkReleaseStyleGroup)(void *group);
extern void (*wkSetCGFontRenderingMode)(CGContextRef cgContext, NSFont *font);
extern void (*wkSetDragImage)(NSImage*, NSPoint offset);
extern void (*wkSetUpFontCache)(size_t s);

#ifdef __cplusplus
}
#endif

#endif
