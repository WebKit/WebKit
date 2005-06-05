/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebCore/WebCoreTextRenderer.h>
#import <QD/ATSUnicodePriv.h>

typedef struct WidthMap WidthMap;
typedef struct WidthEntry WidthEntry;
typedef struct GlyphMap GlyphMap;
typedef struct GlyphEntry GlyphEntry;
typedef struct UnicodeGlyphMap UnicodeGlyphMap;
typedef struct SubstituteFontWidthMap SubstituteFontWidthMap;
typedef struct CharacterWidthIterator CharacterWidthIterator;

/// Should be more than enough for normal usage.
#define NUM_SUBSTITUTE_FONT_MAPS	10

@interface WebTextRenderer : NSObject <WebCoreTextRenderer>
{
    int ascent;
    int descent;
    int lineSpacing;
    int lineGap;
    
    ATSStyleGroupPtr styleGroup;
    
@public
    NSFont *font;
    GlyphMap *characterToGlyphMap;			// Used for 16bit clean unicode characters.
    UnicodeGlyphMap *unicodeCharacterToGlyphMap; 	// Used for surrogates.
    WidthMap *glyphToWidthMap;

    BOOL treatAsFixedPitch;
    ATSGlyphRef spaceGlyph;
    float spaceWidth;
    float adjustedSpaceWidth;

    int numSubstituteFontWidthMaps;
    int maxSubstituteFontWidthMaps;
    SubstituteFontWidthMap *substituteFontWidthMaps;
    BOOL usingPrinterFont;
    BOOL isSmallCapsRenderer;
    
@private
    WebTextRenderer *smallCapsRenderer;
    NSFont *smallCapsFont;
    ATSUStyle _ATSUSstyle;
    BOOL ATSUStyleInitialized;
}

+ (BOOL)shouldBufferTextDrawing;

- (id)initWithFont:(NSFont *)font usingPrinterFont:(BOOL)usingPrinterFont;

@end


@interface WebTextRenderer (WebPrivate)

+ (void)_setAlwaysUseATSU:(BOOL)f;

@end

