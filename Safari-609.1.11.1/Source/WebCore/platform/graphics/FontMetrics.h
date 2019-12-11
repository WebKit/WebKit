/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
 */

#ifndef FontMetrics_h
#define FontMetrics_h

#include "FontBaseline.h"
#include <wtf/MathExtras.h>

namespace WebCore {

class FontMetrics {
public:
    static const unsigned defaultUnitsPerEm = 1000;

    unsigned unitsPerEm() const { return m_unitsPerEm; }
    void setUnitsPerEm(unsigned unitsPerEm) { m_unitsPerEm = unitsPerEm; }

    float floatAscent(FontBaseline baselineType = AlphabeticBaseline) const
    {
        if (baselineType == AlphabeticBaseline)
            return m_ascent;
        return floatHeight() / 2;
    }

    void setAscent(float ascent) { m_ascent = ascent; }

    float floatDescent(FontBaseline baselineType = AlphabeticBaseline) const
    {
        if (baselineType == AlphabeticBaseline)
            return m_descent;
        return floatHeight() / 2;
    }

    void setDescent(float descent) { m_descent = descent; }

    float floatHeight(FontBaseline baselineType = AlphabeticBaseline) const
    {
        return floatAscent(baselineType) + floatDescent(baselineType);
    }

    float floatLineGap() const { return m_lineGap; }
    void setLineGap(float lineGap) { m_lineGap = lineGap; }

    float floatLineSpacing() const { return m_lineSpacing; }
    void setLineSpacing(float lineSpacing) { m_lineSpacing = lineSpacing; }

    float xHeight() const { return m_xHeight; }
    void setXHeight(float xHeight) { m_xHeight = xHeight; }
    bool hasXHeight() const { return m_xHeight > 0; }
    
    bool hasCapHeight() const { return m_capHeight > 0; }
    float floatCapHeight() const { return m_capHeight; }
    void setCapHeight(float capHeight) { m_capHeight = capHeight; }
    
    // Integer variants of certain metrics, used for HTML rendering.
    int ascent(FontBaseline baselineType = AlphabeticBaseline) const
    {
        if (baselineType == AlphabeticBaseline)
            return lroundf(m_ascent);
        return height() - height() / 2;
    }
    
    int descent(FontBaseline baselineType = AlphabeticBaseline) const
    {
        if (baselineType == AlphabeticBaseline)
            return lroundf(m_descent);
        return height() / 2;
    }

    int height(FontBaseline baselineType = AlphabeticBaseline) const
    {
        return ascent(baselineType) + descent(baselineType);
    }

    int lineGap() const { return lroundf(m_lineGap); }
    int lineSpacing() const { return lroundf(m_lineSpacing); }
    
    int capHeight() const { return lroundf(m_capHeight); }
    
    bool hasIdenticalAscentDescentAndLineGap(const FontMetrics& other) const
    {
        return ascent() == other.ascent() && descent() == other.descent() && lineGap() == other.lineGap();
    }

    float zeroWidth() const { return m_zeroWidth; }
    void setZeroWidth(float zeroWidth) { m_zeroWidth = zeroWidth; }

    float underlinePosition() const { return m_underlinePosition; }
    void setUnderlinePosition(float underlinePosition) { m_underlinePosition = underlinePosition; }

    float underlineThickness() const { return m_underlineThickness; }
    void setUnderlineThickness(float underlineThickness) { m_underlineThickness = underlineThickness; }

private:
    friend class Font;

    void reset()
    {
        m_unitsPerEm = defaultUnitsPerEm;
        m_ascent = 0;
        m_descent = 0;
        m_lineGap = 0;
        m_lineSpacing = 0;
        m_xHeight = 0;
        m_capHeight = 0;
        m_zeroWidth = 0;
    }

    unsigned m_unitsPerEm { defaultUnitsPerEm };
    float m_ascent { 0 };
    float m_descent { 0 };
    float m_lineGap { 0 };
    float m_lineSpacing { 0 };
    float m_zeroWidth { 0 };
    float m_xHeight { 0 };
    float m_capHeight { 0 };
    float m_underlinePosition { 0 };
    float m_underlineThickness { 0 };
};

static inline float scaleEmToUnits(float x, unsigned unitsPerEm)
{
    return unitsPerEm ? x / unitsPerEm : x;
}

} // namespace WebCore

#endif // FontMetrics_h
