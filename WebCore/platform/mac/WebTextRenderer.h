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
#import "Font.h"

namespace WebCore
{

class FloatRect;
class FloatPoint;

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

void WebCoreInitializeTextRun(WebCoreTextRun *run, const UniChar *characters, unsigned int length, int from, int to);
void WebCoreInitializeEmptyTextStyle(WebCoreTextStyle *style);
void WebCoreInitializeEmptyTextGeometry(WebCoreTextGeometry *geometry);
void WebCoreInitializeFont(WebCoreFont *font);

typedef struct WidthMap WidthMap;
typedef struct GlyphMap GlyphMap;

class WebTextRenderer
{
public:
    WebTextRenderer(const WebCoreFont& f);
    ~WebTextRenderer();

public:
    static void setAlwaysUseATSU(bool);
    static bool gAlwaysUseATSU;

    // vertical metrics
    int ascent() const { return m_ascent; }
    int descent() const { return m_descent; }
    int lineSpacing() const { return m_lineSpacing; }
    int lineGap() const { return m_lineGap; }

    float xHeight() const;

    // horizontal metrics
    float floatWidthForRun(const WebCoreTextRun* run, const WebCoreTextStyle* style);

    // drawing
    void drawRun(const WebCoreTextRun* run, const WebCoreTextStyle* style, const WebCoreTextGeometry* geometry);
    FloatRect selectionRectForRun(const WebCoreTextRun* run, const WebCoreTextStyle* style, const WebCoreTextGeometry* geometry);
    void drawHighlightForRun(const WebCoreTextRun* run, const WebCoreTextStyle* style, const WebCoreTextGeometry* geometry);
    void drawLineForCharacters(const FloatPoint& point, float yOffset, int width, const Color& color, float thickness);
    void drawLineForMisspelling(const FloatPoint& point, int width);
    int misspellingLineThickness() const { return 3; }

    // selection point check 
    int pointToOffset(const WebCoreTextRun* run, const WebCoreTextStyle* style, int x, bool includePartialGlyphs);

private:
    int misspellingLinePatternWidth() const { return 4; }
    int misspellingLinePatternGapWidth() const { return 1; } // the number of transparent pixels after the dot

public:
    int m_ascent;
    int m_descent;
    int m_lineSpacing;
    int m_lineGap;
    
    void* m_styleGroup;
    
    WebCoreFont m_font;
    GlyphMap* m_characterToGlyphMap;
    WidthMap* m_glyphToWidthMap;

    bool m_treatAsFixedPitch;
    ATSGlyphRef m_spaceGlyph;
    float m_spaceWidth;
    float m_adjustedSpaceWidth;
    float m_syntheticBoldOffset;
    
    WebTextRenderer* m_smallCapsRenderer;
    ATSUStyle m_ATSUStyle;
    bool m_ATSUStyleInitialized;
    bool m_ATSUMirrors;
};

}
