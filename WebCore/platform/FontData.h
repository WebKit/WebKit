/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef FONTDATA_H
#define FONTDATA_H

#if __APPLE__
// FIXME: This is going to be cross-platform eventually, but for now we just compile on OS X.

#include "FontPlatformData.h"
#include "GlyphMap.h"
#include <wtf/Noncopyable.h>

// FIXME: Temporary.  Only needed to support API that's going to move.
#include <unicode/umachine.h>

enum Pitch { UnknownPitch, FixedPitch, VariablePitch };

namespace WebCore
{

class FontDescription;

class WidthMap;

class FontData : Noncopyable
{
public:
    FontData(const FontPlatformData& f);
    ~FontData();

public:
    const FontPlatformData& platformData() const { return m_font; }
    FontData* smallCapsFontData(const FontDescription& fontDescription) const;

    // vertical metrics
    int ascent() const { return m_ascent; }
    int descent() const { return m_descent; }
    int lineSpacing() const { return m_lineSpacing; }
    int lineGap() const { return m_lineGap; }

    float xHeight() const;

    float widthForGlyph(Glyph glyph) const;
    bool containsCharacters(const UChar* characters, int length) const;

    void determinePitch();
    Pitch pitch() const { return m_treatAsFixedPitch ? FixedPitch : VariablePitch; }
    
    // FIXME: Should go away when we pull the glyph map out of the FontData.
    GlyphData glyphDataForCharacter(UChar32 c) const { return m_characterToGlyphMap.glyphDataForCharacter(c, this); }
    void setGlyphDataForCharacter(UChar32 c, Glyph glyph, const FontData* fontData) const { m_characterToGlyphMap.setGlyphDataForCharacter(c, glyph, fontData); }

#if __APPLE__
    NSFont* getNSFont() const { return m_font.font; }
#endif

private:
    bool platformInit();

public:
    int m_ascent;
    int m_descent;
    int m_lineSpacing;
    int m_lineGap;
    
    void* m_styleGroup;
    
    FontPlatformData m_font;
    mutable GlyphMap m_characterToGlyphMap;
    mutable WidthMap* m_glyphToWidthMap;

    bool m_treatAsFixedPitch;
    ATSGlyphRef m_spaceGlyph;
    float m_spaceWidth;
    float m_adjustedSpaceWidth;
    float m_syntheticBoldOffset;
    
    mutable FontData* m_smallCapsFontData;
    mutable ATSUStyle m_ATSUStyle;
    mutable bool m_ATSUStyleInitialized;
    mutable bool m_ATSUMirrors;
};

}

#else

#include "FontPlatformData.h"

namespace WebCore {

class FontDescription;

enum Pitch { UnknownPitch, FixedPitch, VariablePitch };

class FontData
{
public:
#if WIN32
    FontData(HFONT font, const FontDescription& fontDescription)
        :m_platformData(font, fontDescription) {}
#endif

    const FontPlatformData& platformData() const { return m_platformData; }

    void setMetrics(int ascent, int descent, int xHeight, int lineSpacing)
    {
        m_ascent = ascent;
        m_descent = descent;
        m_xHeight = xHeight;
        m_lineSpacing = lineSpacing;
    }

    int ascent() const { return m_ascent; }
    int descent() const { return m_descent; }
    int xHeight() const { return m_xHeight; }
    int lineSpacing() const { return m_lineSpacing; }

private:
    FontPlatformData m_platformData;
    int m_ascent;
    int m_descent;
    int m_xHeight;
    int m_lineSpacing;
};

}
#endif

#endif
