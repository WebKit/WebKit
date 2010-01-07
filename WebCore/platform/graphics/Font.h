/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007 Apple Computer, Inc.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Font_h
#define Font_h

#include "CharacterNames.h"
#include "TextRun.h"
#include "FontDescription.h"
#include "SimpleFontData.h"
#include "TypesettingFeatures.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MathExtras.h>

#if PLATFORM(QT)
#include <QFont>
#endif

namespace WebCore {

class FloatPoint;
class FloatRect;
class FontData;
class FontFallbackList;
class FontPlatformData;
class FontSelector;
class GlyphBuffer;
class GlyphPageTreeNode;
class GraphicsContext;
class IntPoint;
class SVGFontElement;

struct GlyphData;

const unsigned defaultUnitsPerEm = 1000;

class Font {
public:
    Font();
    Font(const FontDescription&, short letterSpacing, short wordSpacing);
    // This constructor is only used if the platform wants to start with a native font.
    Font(const FontPlatformData&, bool isPrinting);
    ~Font();

    Font(const Font&);
    Font& operator=(const Font&);

    bool operator==(const Font& other) const;
    bool operator!=(const Font& other) const {
        return !(*this == other);
    }

    const FontDescription& fontDescription() const { return m_fontDescription; }

    int pixelSize() const { return fontDescription().computedPixelSize(); }
    float size() const { return fontDescription().computedSize(); }

    void update(PassRefPtr<FontSelector>) const;

    void drawText(GraphicsContext*, const TextRun&, const FloatPoint&, int from = 0, int to = -1) const;

    int width(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts = 0) const { return lroundf(floatWidth(run, fallbackFonts)); }
    float floatWidth(const TextRun&, HashSet<const SimpleFontData*>* fallbackFonts = 0) const;
    float floatWidth(const TextRun& run, int extraCharsAvailable, int& charsConsumed, String& glyphName) const;

    int offsetForPosition(const TextRun&, int position, bool includePartialGlyphs) const;
    FloatRect selectionRectForText(const TextRun&, const IntPoint&, int h, int from = 0, int to = -1) const;

    bool isSmallCaps() const { return m_fontDescription.smallCaps(); }

    short wordSpacing() const { return m_wordSpacing; }
    short letterSpacing() const { return m_letterSpacing; }
    void setWordSpacing(short s) { m_wordSpacing = s; }
    void setLetterSpacing(short s) { m_letterSpacing = s; }
    bool isFixedPitch() const;
    bool isPrinterFont() const { return m_fontDescription.usePrinterFont(); }
    
    FontRenderingMode renderingMode() const { return m_fontDescription.renderingMode(); }

    TypesettingFeatures typesettingFeatures() const
    {
        TextRenderingMode textRenderingMode = m_fontDescription.textRenderingMode();
        return textRenderingMode == OptimizeLegibility || textRenderingMode == GeometricPrecision ? Kerning | Ligatures : 0;
    }

    FontFamily& firstFamily() { return m_fontDescription.firstFamily(); }
    const FontFamily& family() const { return m_fontDescription.family(); }

    bool italic() const { return m_fontDescription.italic(); }
    FontWeight weight() const { return m_fontDescription.weight(); }

    bool isPlatformFont() const { return m_isPlatformFont; }

    // Metrics that we query the FontFallbackList for.
    int ascent() const { return primaryFont()->ascent(); }
    int descent() const { return primaryFont()->descent(); }
    int height() const { return ascent() + descent(); }
    int lineSpacing() const { return primaryFont()->lineSpacing(); }
    int lineGap() const { return primaryFont()->lineGap(); }
    float xHeight() const { return primaryFont()->xHeight(); }
    unsigned unitsPerEm() const { return primaryFont()->unitsPerEm(); }
    int spaceWidth() const { return (int)ceilf(primaryFont()->adjustedSpaceWidth() + m_letterSpacing); }
    int tabWidth() const { return 8 * spaceWidth(); }

    const SimpleFontData* primaryFont() const;
    const FontData* fontDataAt(unsigned) const;
    GlyphData glyphDataForCharacter(UChar32, bool mirror, bool forceSmallCaps = false) const;
    // Used for complex text, and does not utilize the glyph map cache.
    const FontData* fontDataForCharacters(const UChar*, int length) const;

#if PLATFORM(QT)
    QFont font() const;
#endif

    static void setShouldUseSmoothing(bool);
    static bool shouldUseSmoothing();

private:
#if ENABLE(SVG_FONTS)
    void drawTextUsingSVGFont(GraphicsContext*, const TextRun&, const FloatPoint&, int from, int to) const;
    float floatWidthUsingSVGFont(const TextRun&) const;
    float floatWidthUsingSVGFont(const TextRun&, int extraCharsAvailable, int& charsConsumed, String& glyphName) const;
    FloatRect selectionRectForTextUsingSVGFont(const TextRun&, const IntPoint&, int h, int from, int to) const;
    int offsetForPositionForTextUsingSVGFont(const TextRun&, int position, bool includePartialGlyphs) const;
#endif

#if USE(FONT_FAST_PATH)
    bool canUseGlyphCache(const TextRun&) const;
    void drawSimpleText(GraphicsContext*, const TextRun&, const FloatPoint&, int from, int to) const;
    void drawGlyphs(GraphicsContext*, const SimpleFontData*, const GlyphBuffer&, int from, int to, const FloatPoint&) const;
    void drawGlyphBuffer(GraphicsContext*, const GlyphBuffer&, const TextRun&, const FloatPoint&) const;
    float floatWidthForSimpleText(const TextRun&, GlyphBuffer*, HashSet<const SimpleFontData*>* fallbackFonts = 0) const;
    int offsetForPositionForSimpleText(const TextRun&, int position, bool includePartialGlyphs) const;
    FloatRect selectionRectForSimpleText(const TextRun&, const IntPoint&, int h, int from, int to) const;

    static bool canReturnFallbackFontsForComplexText();
#endif

    void drawComplexText(GraphicsContext*, const TextRun&, const FloatPoint&, int from, int to) const;
    float floatWidthForComplexText(const TextRun&, HashSet<const SimpleFontData*>* fallbackFonts = 0) const;
    int offsetForPositionForComplexText(const TextRun&, int position, bool includePartialGlyphs) const;
    FloatRect selectionRectForComplexText(const TextRun&, const IntPoint&, int h, int from, int to) const;

    friend struct WidthIterator;

public:
    // Useful for debugging the different font rendering code paths.
#if USE(FONT_FAST_PATH)
    enum CodePath { Auto, Simple, Complex };
    static void setCodePath(CodePath);
    static CodePath codePath();
    static CodePath s_codePath;

    static const uint8_t gRoundingHackCharacterTable[256];
    static bool isRoundingHackCharacter(UChar32 c)
    {
        return (((c & ~0xFF) == 0 && gRoundingHackCharacterTable[c]));
    }
#endif

    FontSelector* fontSelector() const;
    static bool treatAsSpace(UChar c) { return c == ' ' || c == '\t' || c == '\n' || c == 0x00A0; }
    static bool treatAsZeroWidthSpace(UChar c) { return c < 0x20 || (c >= 0x7F && c < 0xA0) || c == 0x200e || c == 0x200f || (c >= 0x202a && c <= 0x202e) || c == 0xFFFC; }

    static inline UChar normalizeSpaces(UChar character)
    {
        if (treatAsSpace(character))
            return space;

        if (treatAsZeroWidthSpace(character))
            return zeroWidthSpace;

        return character;
    }

    static String normalizeSpaces(const String&);

#if ENABLE(SVG_FONTS)
    bool isSVGFont() const;
    SVGFontElement* svgFont() const;
#endif

private:
    FontDescription m_fontDescription;
    mutable RefPtr<FontFallbackList> m_fontList;
    short m_letterSpacing;
    short m_wordSpacing;
    bool m_isPlatformFont;
};

}

#endif
