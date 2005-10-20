/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import <Cocoa/Cocoa.h>

struct WebCoreTextStyle
{
    NSColor *textColor;
    NSColor *backgroundColor;
    int letterSpacing;
    int wordSpacing;
    int padding;
    int tabWidth;
    int xpos;
    NSString **families;
    bool smallCaps;
    bool rtl;
    bool directionalOverride;
    bool applyRunRounding;
    bool applyWordRounding;
    bool attemptFontSubstitution;
};

struct WebCoreTextRun
{
    const UniChar *characters;
    unsigned int length;
    int from;
    int to;
};

struct WebCoreTextGeometry
{
    NSPoint point;
    float selectionY;
    float selectionHeight;
    bool useFontMetricsForSelectionYAndHeight;
};

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WebCoreTextRun WebCoreTextRun;
typedef struct WebCoreTextStyle WebCoreTextStyle;
typedef struct WebCoreTextGeometry WebCoreTextGeometry;

extern void WebCoreInitializeTextRun(WebCoreTextRun *run, const UniChar *characters, unsigned int length, int from, int to);
extern void WebCoreInitializeEmptyTextStyle(WebCoreTextStyle *style);
extern void WebCoreInitializeEmptyTextGeometry(WebCoreTextGeometry *geometry);

#ifdef __cplusplus
}
#endif

@protocol WebCoreTextRenderer <NSObject>

// WebCoreTextRenderer must guarantee that no calls to any of these
// methods will raise any ObjC exceptions. It's too expensive to do
// blocking for all of them at the WebCore level, and some
// implementations may be able to guarantee no exceptions without the
// use of NS_DURING.

// vertical metrics
- (int)ascent;
- (int)descent;
- (int)lineSpacing;
- (float)xHeight;

// horizontal metrics
- (float)floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style;

// drawing
- (void)drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry;
- (void)drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry;
- (void)drawLineForCharacters:(NSPoint)point yOffset:(float)yOffset width: (int)width color:(NSColor *)color thickness:(float)thickness;
- (void)drawLineForMisspelling:(NSPoint)point withWidth:(int)width;
- (int)misspellingLineThickness;

// selection point check
- (int)pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs;

@end
