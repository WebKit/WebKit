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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef FONT_H
#define FONT_H

#include "Color.h"
#include "FontDescription.h"
#include "TextDirection.h"
#include "GlyphBuffer.h"

#if __APPLE__
// FIXME: Should not be necessary.
#include "FontPlatformData.h"
#endif

namespace WebCore {

class FontFallbackList;
class GraphicsContext;
class IntPoint;
class IntRect;
class FloatRect;
class FloatPoint;

enum Pitch { UnknownPitch, FixedPitch, VariablePitch };

class TextRun
{
public:
    TextRun(const UChar* c, int len)
    :m_characters(c), m_len(len), m_from(0), m_to(len)
    {}

    TextRun(const UChar* c, int len, int from, int to) // This constructor is only used in one place in Mac-specific code.
    :m_characters(c), m_len(len), m_from(from), m_to(to)
    {}

    TextRun(const StringImpl* s, int offset = 0, int from = -1, int to = -1)
    :m_characters(s->characters() + offset), m_len(s->length() - offset), m_from(adjustFrom(from)), m_to(adjustTo(to))
    {}

    const UChar operator[](int i) const { return m_characters[i]; }
    const UChar* data(int i) const { return &m_characters[i]; }

    int adjustFrom(int from) const { return from == -1 ? 0 : from; }
    int adjustTo(int to) const { return to == -1 ? m_len : to; }

    const UChar* characters() const { return m_characters; }
    int length() const { return m_len; }
    int from() const { return m_from; }
    int to() const { return m_to; }

private:
    const UChar* m_characters;
    int m_len;
    int m_from;
    int m_to;
};

class Font {
public:
    Font();
    Font(const FontDescription&, short letterSpacing, short wordSpacing);
    ~Font();
    
    Font(const Font&);
    Font& operator=(const Font&);

    bool operator==(const Font& other) const {
        // The renderer pointer doesn't have to be checked, since
        // checking the font description will be fine.
        return (m_fontDescription == other.m_fontDescription &&
                m_letterSpacing == other.m_letterSpacing &&
                m_wordSpacing == other.m_wordSpacing);
    }

    bool operator!=(const Font& other) const {
        return !(*this == other);
    }

    const FontDescription& fontDescription() const { return m_fontDescription; }

    int pixelSize() const { return fontDescription().computedPixelSize(); }
    float size() const { return fontDescription().computedSize(); }
    
    void update() const;

    void drawText(GraphicsContext*, const TextRun&, const IntPoint&, int tabWidth, int xpos,
                  int toAdd, TextDirection, bool visuallyOrdered) const;
    void drawLineForText(GraphicsContext*, const IntPoint&, int yOffset, int width) const;
    void drawLineForMisspelling(GraphicsContext*, const IntPoint&, int width) const;
    int misspellingLineThickness(GraphicsContext*) const;

    float floatWidth(const TextRun&, int tabWidth, int xpos, bool runRounding = true) const;
    
    int checkSelectionPoint(const TextRun&, int toAdd, int tabWidth, int xpos,
        int x, TextDirection, bool visuallyOrdered, bool includePartialGlyphs) const;
    FloatRect selectionRectForText(const TextRun&, const IntPoint&, int h, int tabWidth, int xpos, int width,
                                   bool rtl, bool visuallyOrdered = false) const;
    
    int width(const TextRun&, int tabWidth = 0, int xpos = 0) const;

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

#if __APPLE__
    // FIXME: Shouldn't need to access FontPlatformData... should just need NSFont.
    NSString* getNSFamily() const { return m_fontDescription.family().getNSFamily(); }    
    NSFont* getNSFont() const { return platformFont().font; }
    const FontPlatformData& platformFont() const;
#endif

    // Metrics that we query the FontFallbackList for.
    int ascent() const;
    int descent() const;
    int height() const { return ascent() + descent(); }
    int lineSpacing() const;
    float xHeight() const;

    const FontData* primaryFont() const;

private:
#if __APPLE__
    // FIXME: This will eventually be cross-platform, but we want to keep Windows compiling for now.
    bool canUseGlyphCache(const TextRun&) const;
    void drawSimpleText(GraphicsContext*, const TextRun&, const IntPoint&, 
                        int tabWidth, int xpos, int toAdd, 
                        TextDirection, bool visuallyOrdered) const;
    void drawGlyphs(GraphicsContext*, const FontData*, const GlyphBuffer&, int from, int to, const FloatPoint&) const;
    void drawComplexText(GraphicsContext*, const TextRun&, const IntPoint&, 
                         int tabWidth, int xpos, int toAdd, TextDirection, bool visuallyOrdered) const;
    float floatWidthForSimpleText(const TextRun&, 
                                  int tabWidth, int xpos, int toAdd, 
                                  TextDirection, bool visuallyOrdered, 
                                  bool applyWordRounding, bool applyRunRounding,
                                  const FontData* substituteFont,
                                  float* startX, GlyphBuffer*) const;
    float floatWidthForComplexText(const TextRun&, int tabWidth, int xpos, bool runRounding = true) const;

    friend struct WidthIterator;
    
    // Useful for debugging the complex font rendering code path.
public:
    static void setAlwaysUseComplexPath(bool);
    static bool gAlwaysUseComplexPath;
    
    static const uint8_t gRoundingHackCharacterTable[256];
    static bool treatAsSpace(UChar c) { return c == ' ' || c == '\t' || c == '\n' || c == 0x00A0; }
    static bool isRoundingHackCharacter(UChar32 c)
    {
        return (((c & ~0xFF) == 0 && gRoundingHackCharacterTable[c])); 
    }
#endif

private:
    FontDescription m_fontDescription;
    mutable RefPtr<FontFallbackList> m_fontList;
    short m_letterSpacing;
    short m_wordSpacing;
};

}

#endif
