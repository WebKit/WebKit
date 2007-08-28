/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef WebKitSystemInterface_h
#define WebKitSystemInterface_h

struct CGAffineTransform;
struct CGSize;

typedef struct CGColor* CGColorRef;
typedef struct CGContext* CGContextRef;
typedef unsigned short CGFontIndex;
typedef struct CGFont* CGFontRef;
typedef CGFontIndex CGGlyph;
typedef wchar_t UChar;
typedef struct _CFURLResponse* CFURLResponseRef;

void wkSetFontSmoothingLevel(int type);
int wkGetFontSmoothingLevel();
uint32_t wkSetFontSmoothingStyle(CGContextRef cg);
void wkRestoreFontSmoothingStyle(CGContextRef cg, uint32_t oldStyle);
void wkGetGlyphAdvances(CGFontRef, const CGAffineTransform&, bool isSystemFont, bool isPrinterFont, CGGlyph, CGSize& advance);
void wkGetGlyphs(CGFontRef, const UChar[], CGGlyph[], size_t count);
void wkSetUpFontCache(size_t s);

void wkDrawFocusRing(CGContextRef, CGColorRef, float radius);

CFDictionaryRef wkGetSSLCertificateInfo(CFURLResponseRef);
void* wkGetSSLPeerCertificateData(CFDictionaryRef);

#endif
