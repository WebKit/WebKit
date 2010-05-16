/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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

#ifndef SimpleFontData_h
#define SimpleFontData_h

#include "FontData.h"
#include "FontPlatformData.h"
#include "FloatRect.h"
#include "GlyphMetricsMap.h"
#include "GlyphPageTreeNode.h"
#include "TypesettingFeatures.h"
#include <wtf/OwnPtr.h>

#if USE(ATSUI)
typedef struct OpaqueATSUStyle* ATSUStyle;
#endif

#if USE(CORE_TEXT)
#include <ApplicationServices/ApplicationServices.h>
#include <wtf/RetainPtr.h>
#endif

#if (PLATFORM(WIN) && !OS(WINCE)) \
    || (OS(WINDOWS) && PLATFORM(WX))
#include <usp10.h>
#endif

#if PLATFORM(CAIRO)
#include <cairo.h>
#endif

#if PLATFORM(QT)
#include <QFont>
#endif

#if PLATFORM(HAIKU)
#include <Font.h>
#endif

namespace WebCore {

class FontDescription;
class FontPlatformData;
class SharedBuffer;
class SVGFontData;

enum Pitch { UnknownPitch, FixedPitch, VariablePitch };

class SimpleFontData : public FontData {
public:
    SimpleFontData(const FontPlatformData&, bool customFont = false, bool loading = false, SVGFontData* data = 0);
    virtual ~SimpleFontData();

public:
    const FontPlatformData& platformData() const { return m_platformData; }
    SimpleFontData* smallCapsFontData(const FontDescription& fontDescription) const;

    // vertical metrics
    int ascent() const { return m_ascent; }
    int descent() const { return m_descent; }
    int lineSpacing() const { return m_lineSpacing; }
    int lineGap() const { return m_lineGap; }
    float maxCharWidth() const { return m_maxCharWidth; }
    float avgCharWidth() const { return m_avgCharWidth; }
    float xHeight() const { return m_xHeight; }
    unsigned unitsPerEm() const { return m_unitsPerEm; }

    FloatRect boundsForGlyph(Glyph) const;
    float widthForGlyph(Glyph glyph) const;
    FloatRect platformBoundsForGlyph(Glyph) const;
    float platformWidthForGlyph(Glyph) const;

    float spaceWidth() const { return m_spaceWidth; }
    float adjustedSpaceWidth() const { return m_adjustedSpaceWidth; }

#if PLATFORM(CG) || PLATFORM(CAIRO) || PLATFORM(WX)
    float syntheticBoldOffset() const { return m_syntheticBoldOffset; }
#endif

    Glyph spaceGlyph() const { return m_spaceGlyph; }

    virtual const SimpleFontData* fontDataForCharacter(UChar32) const;
    virtual bool containsCharacters(const UChar*, int length) const;

    void determinePitch();
    Pitch pitch() const { return m_treatAsFixedPitch ? FixedPitch : VariablePitch; }

#if ENABLE(SVG_FONTS)
    SVGFontData* svgFontData() const { return m_svgFontData.get(); }
    bool isSVGFont() const { return m_svgFontData; }
#else
    bool isSVGFont() const { return false; }
#endif

    virtual bool isCustomFont() const { return m_isCustomFont; }
    virtual bool isLoading() const { return m_isLoading; }
    virtual bool isSegmented() const;

    const GlyphData& missingGlyphData() const { return m_missingGlyphData; }

#ifndef NDEBUG
    virtual String description() const;
#endif

#if PLATFORM(MAC) || (PLATFORM(CHROMIUM) && OS(DARWIN))
    NSFont* getNSFont() const { return m_platformData.font(); }
#elif (PLATFORM(WX) && OS(DARWIN)) 
    NSFont* getNSFont() const { return m_platformData.nsFont(); }
#endif

#if USE(CORE_TEXT)
    CTFontRef getCTFont() const;
    CFDictionaryRef getCFStringAttributes(TypesettingFeatures) const;
#endif

#if USE(ATSUI)
    void checkShapesArabic() const;
    bool shapesArabic() const
    {
        if (!m_checkedShapesArabic)
            checkShapesArabic();
        return m_shapesArabic;
    }
#endif

#if PLATFORM(QT)
    QFont getQtFont() const { return m_platformData.font(); }
#endif

#if PLATFORM(WIN) || (OS(WINDOWS) && PLATFORM(WX))
    bool isSystemFont() const { return m_isSystemFont; }
#if !OS(WINCE) // disable unused members to save space
    SCRIPT_FONTPROPERTIES* scriptFontProperties() const;
    SCRIPT_CACHE* scriptCache() const { return &m_scriptCache; }
#endif
    static void setShouldApplyMacAscentHack(bool);
    static bool shouldApplyMacAscentHack();
#endif

#if PLATFORM(WX)
    wxFont* getWxFont() const { return m_platformData.font(); }
#endif

private:
    void platformInit();
    void platformGlyphInit();
    void platformCharWidthInit();
    void platformDestroy();
    
    void initCharWidths();

    void commonInit();

#if (PLATFORM(WIN) && !OS(WINCE)) \
    || (OS(WINDOWS) && PLATFORM(WX))
    void initGDIFont();
    void platformCommonDestroy();
    FloatRect boundsForGDIGlyph(Glyph glyph) const;
    float widthForGDIGlyph(Glyph glyph) const;
#endif

    int m_ascent;
    int m_descent;
    int m_lineSpacing;
    int m_lineGap;
    float m_maxCharWidth;
    float m_avgCharWidth;
    float m_xHeight;
    unsigned m_unitsPerEm;

    FontPlatformData m_platformData;

    mutable OwnPtr<GlyphMetricsMap<FloatRect> > m_glyphToBoundsMap;
    mutable GlyphMetricsMap<float> m_glyphToWidthMap;

    bool m_treatAsFixedPitch;

#if ENABLE(SVG_FONTS)
    OwnPtr<SVGFontData> m_svgFontData;
#endif

    bool m_isCustomFont;  // Whether or not we are custom font loaded via @font-face
    bool m_isLoading; // Whether or not this custom font is still in the act of loading.

    Glyph m_spaceGlyph;
    float m_spaceWidth;
    float m_adjustedSpaceWidth;

    Glyph m_zeroWidthSpaceGlyph;

    GlyphData m_missingGlyphData;

    mutable SimpleFontData* m_smallCapsFontData;

#if PLATFORM(CG) || PLATFORM(CAIRO) || PLATFORM(WX)
    float m_syntheticBoldOffset;
#endif

#ifdef BUILDING_ON_TIGER
public:
    void* m_styleGroup;

private:
#endif

#if USE(ATSUI)
public:
    mutable HashMap<unsigned, ATSUStyle> m_ATSUStyleMap;
    mutable bool m_ATSUMirrors;
    mutable bool m_checkedShapesArabic;
    mutable bool m_shapesArabic;

private:
#endif

#if USE(CORE_TEXT)
    mutable RetainPtr<CTFontRef> m_CTFont;
    mutable HashMap<unsigned, RetainPtr<CFDictionaryRef> > m_CFStringAttributes;
#endif

#if PLATFORM(WIN) || (OS(WINDOWS) && PLATFORM(WX))
    bool m_isSystemFont;
#if !OS(WINCE) // disable unused members to save space
    mutable SCRIPT_CACHE m_scriptCache;
    mutable SCRIPT_FONTPROPERTIES* m_scriptFontProperties;
#endif
#endif
};
    
    
#if !PLATFORM(QT)
ALWAYS_INLINE FloatRect SimpleFontData::boundsForGlyph(Glyph glyph) const
{
    if (glyph == m_zeroWidthSpaceGlyph && glyph)
        return FloatRect();

    FloatRect bounds;
    if (m_glyphToBoundsMap) {
        bounds = m_glyphToBoundsMap->metricsForGlyph(glyph);
        if (bounds.width() != cGlyphSizeUnknown)
            return bounds;
    }

    bounds = platformBoundsForGlyph(glyph);
    if (!m_glyphToBoundsMap)
        m_glyphToBoundsMap.set(new GlyphMetricsMap<FloatRect>());
    m_glyphToBoundsMap->setMetricsForGlyph(glyph, bounds);
    return bounds;
}

ALWAYS_INLINE float SimpleFontData::widthForGlyph(Glyph glyph) const
{
    if (glyph == m_zeroWidthSpaceGlyph && glyph)
        return 0;

    float width = m_glyphToWidthMap.metricsForGlyph(glyph);
    if (width != cGlyphSizeUnknown)
        return width;

    width = platformWidthForGlyph(glyph);
    m_glyphToWidthMap.setMetricsForGlyph(glyph, width);
    return width;
}
#endif

} // namespace WebCore

#endif // SimpleFontData_h
