/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
#ifndef KWQMETRICS_H_
#define KWQMETRICS_H_

#import <Cocoa/Cocoa.h>

@class KWQTextStorage;


#ifdef DIRECT_TO_CG

#define LOCAL_GLYPH_BUFFER_SIZE 1024
#define GLYPH_CACHE_MAX 1024

#define FIRST_ASCII_CHAR ((int)' ')
#define LAST_ASCII_CHAR ((int)'}')

#define Boolean MacBoolean
#define Fixed MacFixed
#define Rect MacRect

#import <ApplicationServices/ApplicationServices.h>
#import <ATSUnicodePriv.h>

#undef Fixed
#undef Rect
#undef Boolean

@interface NSFont (IFPrivate)
- (ATSUFontID)_atsFontID;
- (CGFontRef) _backingCGSFont;
@end

extern "C" {

CG_EXTERN int CGFontGetGlyphScaledAdvances(CGFontRef font, const CGGlyph glyph[], size_t count, float advance[], float scale);
CG_EXTERN size_t CGFontGetNumberOfGlyphs(CGFontRef font);

}

#endif

@interface KWQLayoutInfo : NSObject
{
    NSMutableDictionary *attributes;
    NSLayoutManager *layoutManager;
    KWQTextStorage *textStorage;
    NSFont *font;
    int lineHeight;
#ifdef DIRECT_TO_CG
    ATSStyleGroupPtr _styleGroup;
    ATSUStyle _style;
    ATSGlyphVector _glyphVector;
    ATSStyleGroupPtr _asciiStyleGroup;
    ATSUStyle _asciiStyle;
    ATSGlyphVector _asciiCacheGlyphVector;
    int widthCacheSize;
    float *widthCache;
#endif
}

+ (void)drawString: (NSString *)string atPoint: (NSPoint)p withFont: (NSFont *)font color: (NSColor *)color;
+ (void)drawUnderlineForString: (NSString *)string atPoint: (NSPoint)p withFont: (NSFont *)font color: (NSColor *)color;
+ (KWQLayoutInfo *)getMetricsForFont: (NSFont *)aFont;
+ (void)setMetric: (KWQLayoutInfo *)info forFont: (NSFont *)aFont;
- initWithFont: (NSFont *)aFont;
- (NSRect)rectForString:(NSString *)string;
- (NSLayoutManager *)layoutManager;
- (KWQTextStorage *)textStorage;
- (void)setColor: (NSColor *)color;
- (NSDictionary *)attributes;
- (int)lineHeight;
- (NSFont *)font;
@end

@protocol KWQLayoutFragment
- (void)setGlyphRange: (NSRange)r;
- (NSRange)glyphRange;
- (void)setBoundingRect: (NSRect)r;
- (NSRect)boundingRect;

#ifdef _DEBUG_LAYOUT_FRAGMENT
- (int)accessCount;
#endif

@end

@interface KWQSmallLayoutFragment : NSObject <KWQLayoutFragment>
{
    // Assumes 0,0 boundingRect origin and < UINT16_MAX width and height,
    // and loss of precision for float to short is irrelevant.
    unsigned short width;
    unsigned short height;

    // Assumes 0 location and < UINT16_MAX length.
    unsigned short glyphRangeLength;
#ifdef _DEBUG_LAYOUT_FRAGMENT
    int accessCount;
#endif
}
@end

@interface KWQLargeLayoutFragment : NSObject <KWQLayoutFragment>
{
    NSRect boundingRect;
    NSRange glyphRange;
#ifdef _DEBUG_LAYOUT_FRAGMENT
    int accessCount;
#endif
}
@end

#endif
