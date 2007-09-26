/*
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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

#include "FontDescription.h"
#include <wtf/HashMap.h>

#if PLATFORM(QT)
#include <QtGui/qfont.h>
#include <QtGui/qfontmetrics.h>
#endif

namespace WebCore {

class FloatPoint;
class FloatRect;
class FontData;
class FontFallbackList;
class FontPlatformData;
class GlyphBuffer;
class GlyphPageTreeNode;
class GraphicsContext;
class IntPoint;
class TextStyle;

struct GlyphData;

class TextRun {
public:
    TextRun(const UChar* c, int len)
    :m_characters(c), m_len(len)
    {}

    TextRun(const String& s)
    :m_characters(s.characters()), m_len(s.length())
    {}

    const UChar operator[](int i) const { return m_characters[i]; }
    const UChar* data(int i) const { return &m_characters[i]; }

    const UChar* characters() const { return m_characters; }
    int length() const { return m_len; }
   
private:
    const UChar* m_characters;
    int m_len;
};

class Font {
public:
    Font();
    Font(const FontDescription&, short letterSpacing, short wordSpacing);
#if !PLATFORM(QT)
    Font(const FontPlatformData&, bool isPrinting); // This constructor is only used if the platform wants to start with a native font.
#endif
    ~Font();
    
    Font(const Font&);
    Font& operator=(const Font&);

    bool operator==(const Font& other) const
    {
        // Our FontData doesn't have to be checked, since checking the font description will be fine.
        // FIXME: This does not work if the font was made with the FontPlatformData constructor.
        return m_fontDescription == other.m_fontDescription
            && m_letterSpacing == other.m_letterSpacing
            && m_wordSpacing == other.m_wordSpacing;
    }

    bool operator!=(const Font& other) const {
        return !(*this == other);
    }

    const FontDescription& fontDescription() const { return m_fontDescription; }

    int pixelSize() const { return fontDescription().computedPixelSize(); }
    float size() const { return fontDescription().computedSize(); }
    
    void update() const;

    void drawText(GraphicsContext*, const TextRun&, const TextStyle&, const FloatPoint&, int from = 0, int to = -1) const;

    int width(const TextRun&, const TextStyle&) const;
    int width(const TextRun&) const;
    float floatWidth(const TextRun&, const TextStyle&) const;
    float floatWidth(const TextRun&) const;
    
    int offsetForPosition(const TextRun&, const TextStyle&, int position, bool includePartialGlyphs) const;
    FloatRect selectionRectForText(const TextRun&, const TextStyle&, const IntPoint&, int h, int from = 0, int to = -1) const;

    bool isSmallCaps() const { return m_fontDescription.smallCaps(); }

    short wordSpacing() const { return m_wordSpacing; }
    short letterSpacing() const { return m_letterSpacing; }
    void setWordSpacing(short s) { m_wordSpacing = s; }
    void setLetterSpacing(short s) { m_letterSpacing = s; }

    bool isFixedPitch() const;
    bool isPrinterFont() const { return m_fontDescription.usePrinterFont(); }

    FontFamily& firstFamily() { return m_fontDescription.firstFamily(); }
    const FontFamily& family() const { return m_fontDescription.family(); }

    bool italic() const { return m_fontDescription.italic(); }
    unsigned weight() const { return m_fontDescription.weight(); }
    bool bold() const { return m_fontDescription.bold(); }

#if !PLATFORM(QT)
    bool isPlatformFont() const { return m_isPlatformFont; }
#endif
    
#if PLATFORM(QT)
    inline const QFont &font() const { return m_font; }
    inline const QFont &scFont() const { return m_scFont; }
#endif
    
    // Metrics that we query the FontFallbackList for.
    int ascent() const;
    int descent() const;
    int height() const { return ascent() + descent(); }
    int lineSpacing() const;
    float xHeight() const;
    int spaceWidth() const;
    int tabWidth() const { return 8 * spaceWidth(); }

#if !PLATFORM(QT)
    const FontData* primaryFont() const;
    const FontData* fontDataAt(unsigned) const;
    const GlyphData& glyphDataForCharacter(UChar32, const UChar* cluster, unsigned clusterLength, bool mirror, bool attemptFontSubstitution) const;
    // Used for complex text, and does not utilize the glyph map cache.
    const FontData* fontDataForCharacters(const UChar*, int length) const;

private:
    bool canUseGlyphCache(const TextRun&) const;
    void drawSimpleText(GraphicsContext*, const TextRun&, const TextStyle&, const FloatPoint&, int from, int to) const;
    void drawGlyphs(GraphicsContext*, const FontData*, const GlyphBuffer&, int from, int to, const FloatPoint&) const;
    void drawGlyphBuffer(GraphicsContext*, const GlyphBuffer&, const TextRun&, const TextStyle&, const FloatPoint&) const;
    void drawComplexText(GraphicsContext*, const TextRun&, const TextStyle&, const FloatPoint&, int from, int to) const;
    float floatWidthForSimpleText(const TextRun&, const TextStyle&, GlyphBuffer*) const;
    float floatWidthForComplexText(const TextRun&, const TextStyle&) const;
    int offsetForPositionForSimpleText(const TextRun&, const TextStyle&, int position, bool includePartialGlyphs) const;
    int offsetForPositionForComplexText(const TextRun&, const TextStyle&, int position, bool includePartialGlyphs) const;
    FloatRect selectionRectForSimpleText(const TextRun&, const TextStyle&, const IntPoint&, int h, int from, int to) const;
    FloatRect selectionRectForComplexText(const TextRun&, const TextStyle&, const IntPoint&, int h, int from, int to) const;
#endif
    friend struct WidthIterator;
    
    // Useful for debugging the different font rendering code paths.
public:
#if !PLATFORM(QT)
    enum CodePath { Auto, Simple, Complex };
    static void setCodePath(CodePath);
    static CodePath codePath;
    
    static const uint8_t gRoundingHackCharacterTable[256];
    static bool isRoundingHackCharacter(UChar32 c)
    {
        return (((c & ~0xFF) == 0 && gRoundingHackCharacterTable[c])); 
    }
#endif
    static bool treatAsSpace(UChar c) { return c == ' ' || c == '\t' || c == '\n' || c == 0x00A0; }
    static bool treatAsZeroWidthSpace(UChar c) { return c < 0x20 || (c >= 0x7F && c < 0xA0) || c == 0x200e || c == 0x200f; }
private:
    FontDescription m_fontDescription;
#if !PLATFORM(QT)
    mutable RefPtr<FontFallbackList> m_fontList;
    mutable HashMap<int, GlyphPageTreeNode*> m_pages;
    mutable GlyphPageTreeNode* m_pageZero;
#endif
    short m_letterSpacing;
    short m_wordSpacing;
#if !PLATFORM(QT)
    bool m_isPlatformFont;
#else
    QFont m_font;
    QFont m_scFont;
    int m_spaceWidth;
#endif
};

}

#endif
