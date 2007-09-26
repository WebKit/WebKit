/*
 * This file is part of the internal font implementation.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef FontData_h
#define FontData_h

#include "FontPlatformData.h"
#include "GlyphPageTreeNode.h"
#include "GlyphWidthMap.h"
#include <wtf/Noncopyable.h>

#include <wtf/unicode/Unicode.h>

#if PLATFORM(MAC)
typedef struct OpaqueATSUStyle* ATSUStyle;
#endif

#if PLATFORM(WIN)
#include <usp10.h>
#endif

namespace WebCore {

class FontDescription;
class FontPlatformData;
class WidthMap;

enum Pitch { UnknownPitch, FixedPitch, VariablePitch };

class FontData : Noncopyable {
public:
    FontData(const FontPlatformData&);
    ~FontData();

public:
    const FontPlatformData& platformData() const { return m_font; }
    FontData* smallCapsFontData(const FontDescription& fontDescription) const;

    // vertical metrics
    int ascent() const { return m_ascent; }
    int descent() const { return m_descent; }
    int lineSpacing() const { return m_lineSpacing; }
    int lineGap() const { return m_lineGap; }
    float xHeight() const { return m_xHeight; }

    float widthForGlyph(Glyph) const;
    float platformWidthForGlyph(Glyph) const;

    bool containsCharacters(const UChar* characters, int length) const;

    void determinePitch();
    Pitch pitch() const { return m_treatAsFixedPitch ? FixedPitch : VariablePitch; }

    const GlyphData& missingGlyphData() const { return m_missingGlyphData; }

#if PLATFORM(MAC)
    NSFont* getNSFont() const { return m_font.font(); }
    void checkShapesArabic() const;
    bool shapesArabic() const
    {
        if (!m_checkedShapesArabic)
            checkShapesArabic();
        return m_shapesArabic;
    }
#endif

#if PLATFORM(WIN)
    bool isSystemFont() const { return m_isSystemFont; }
    SCRIPT_FONTPROPERTIES* scriptFontProperties() const;
    SCRIPT_CACHE* scriptCache() const { return &m_scriptCache; }
#endif

#if PLATFORM(GTK)
    void setFont(cairo_t*) const;
#endif

private:
    void platformInit();
    void platformDestroy();

public:
    int m_ascent;
    int m_descent;
    int m_lineSpacing;
    int m_lineGap;
    float m_xHeight;
    
    FontPlatformData m_font;
    mutable GlyphWidthMap m_glyphToWidthMap;

    bool m_treatAsFixedPitch;
    Glyph m_spaceGlyph;
    float m_spaceWidth;
    float m_adjustedSpaceWidth;

    GlyphData m_missingGlyphData;

    mutable FontData* m_smallCapsFontData;

#if PLATFORM(CG)
    float m_syntheticBoldOffset;
#endif

#if PLATFORM(MAC)
    void* m_styleGroup;
    mutable ATSUStyle m_ATSUStyle;
    mutable bool m_ATSUStyleInitialized;
    mutable bool m_ATSUMirrors;
    mutable bool m_checkedShapesArabic;
    mutable bool m_shapesArabic;
#endif

#if PLATFORM(WIN)
    bool m_isSystemFont;
    mutable SCRIPT_CACHE m_scriptCache;
    mutable SCRIPT_FONTPROPERTIES* m_scriptFontProperties;
#endif
};

} // namespace WebCore

#endif // FontData_h
