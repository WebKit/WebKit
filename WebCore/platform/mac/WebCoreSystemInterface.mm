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

#import "config.h"
#import "WebCoreSystemInterface.h"

BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef);
void (*wkClearGlyphVector)(void* glyphs);
OSStatus (*wkConvertCharToGlyphs)(void* styleGroup, const UniChar*, unsigned numCharacters, void* glyphs);
void (*wkDrawBezeledTextFieldCell)(NSRect, BOOL enabled);
void (*wkDrawFocusRing)(CGContextRef, CGRect clipRect, CGColorRef, int radius);
BOOL (*wkFontSmoothingModeIsLCD)(int mode);
OSStatus (*wkGetATSStyleGroup)(ATSUStyle, void** styleGroup);
CGFontRef (*wkGetCGFontFromNSFont)(NSFont*);
ATSGlyphRef (*wkGetDefaultGlyphForChar)(NSFont*, UniChar);
NSFont* (*wkGetFontInLanguageForRange)(NSFont*, NSString*, NSRange);
NSFont* (*wkGetFontInLanguageForCharacter)(NSFont*, UniChar);
void (*wkGetFontMetrics)(NSFont*, int* ascent, int* descent, int* lineGap, unsigned* unitsPerEm);
BOOL (*wkGetGlyphTransformedAdvances)(NSFont*, CGAffineTransform*, ATSGlyphRef*, CGSize* advance);
ATSLayoutRecord* (*wkGetGlyphVectorFirstRecord)(void* glyphVector);
int (*wkGetGlyphVectorNumGlyphs)(void* glyphVector);
size_t (*wkGetGlyphVectorRecordSize)(void* glyphVector);
NSString* (*wkGetMIMETypeForExtension)(NSString*);
ATSUFontID (*wkGetNSFontATSUFontId)(NSFont*);
OSStatus (*wkInitializeGlyphVector)(int count, void* glyphs);
void (*wkReleaseStyleGroup)(void* group);
void (*wkSetCGFontRenderingMode)(CGContextRef, NSFont*);
void (*wkSetDragImage)(NSImage*, NSPoint offset);
void (*wkSetPatternPhaseInUserSpace)(CGContextRef, CGPoint point);
void (*wkSetUpFontCache)(size_t);
