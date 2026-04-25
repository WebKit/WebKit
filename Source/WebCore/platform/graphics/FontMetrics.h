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

#include <WebCore/FontBaseline.h>
#include <wtf/Markable.h>
#include <wtf/MathExtras.h>

namespace WebCore {

class FontMetrics {
public:
    static constexpr unsigned defaultUnitsPerEm = 1000;

    unsigned unitsPerEm() const { return m_unitsPerEm; }
    void setUnitsPerEm(unsigned unitsPerEm) { m_unitsPerEm = unitsPerEm; }

    float height(FontBaseline baselineType = FontBaseline::Alphabetic) const
    {
        return ascent(baselineType) + descent(baselineType);
    }
    int intHeight(FontBaseline baselineType = FontBaseline::Alphabetic) const
    {
        return intAscent(baselineType) + intDescent(baselineType);
    }

    float ascent(FontBaseline baselineType = FontBaseline::Alphabetic) const
    {
        if (baselineType == FontBaseline::Alphabetic)
            return m_ascent.value_or(0.f);
        return height() / 2;
    }
    int intAscent(FontBaseline baselineType = FontBaseline::Alphabetic) const
    {
        if (baselineType == FontBaseline::Alphabetic)
            return m_intAscent;
        return intHeight() - intHeight() / 2;
    }
    void setAscent(float ascent)
    {
        m_ascent = ascent;
        m_intAscent = std::max(static_cast<int>(lroundf(ascent)), 0);
    }

    float descent(FontBaseline baselineType = FontBaseline::Alphabetic) const
    {
        if (baselineType == FontBaseline::Alphabetic)
            return m_descent.value_or(0.f);
        return height() / 2;
    }
    int intDescent(FontBaseline baselineType = FontBaseline::Alphabetic) const
    {
        if (baselineType == FontBaseline::Alphabetic)
            return m_intDescent;
        return intHeight() / 2;
    }
    void setDescent(float descent)
    {
        m_descent = descent;
        m_intDescent = lroundf(descent);
    }

    float lineGap() const { return m_lineGap.value_or(0.f); }
    int intLineGap() const { return m_intLineGap; }
    void setLineGap(float lineGap)
    {
        m_lineGap = lineGap;
        m_intLineGap = lroundf(lineGap);
    }

    float lineSpacing() const { return m_lineSpacing.value_or(0.f); }
    int intLineSpacing() const { return m_intLineSpacing; }
    void setLineSpacing(float lineSpacing)
    {
        m_lineSpacing = lineSpacing;
        m_intLineSpacing = lroundf(lineSpacing);
    }

    Markable<float> xHeight() const { return m_xHeight; }
    void setXHeight(float xHeight) { m_xHeight = xHeight; }

    Markable<float> capHeight() const { return m_capHeight; }
    int intCapHeight() const { return m_intCapHeight; }
    void setCapHeight(float capHeight)
    {
        m_capHeight = capHeight;
        m_intCapHeight = lroundf(capHeight);
    }

    Markable<float> zeroWidth() const { return m_zeroWidth; }
    void setZeroWidth(float zeroWidth) { m_zeroWidth = zeroWidth; }

    Markable<float> ideogramWidth() const { return m_ideogramWidth; }
    void setIdeogramWidth(float ideogramWidth) { m_ideogramWidth = ideogramWidth; }

    Markable<float> underlinePosition() const { return m_underlinePosition; }
    void setUnderlinePosition(float underlinePosition) { m_underlinePosition = underlinePosition; }

    Markable<float> underlineThickness() const { return m_underlineThickness; }
    void setUnderlineThickness(float underlineThickness) { m_underlineThickness = underlineThickness; }

    bool hasIdenticalAscentDescentAndLineGap(const FontMetrics& other) const
    {
        return intAscent() == other.intAscent()
            && intDescent() == other.intDescent()
            && intLineGap() == other.intLineGap();
    }

private:
    friend class Font;

    void reset()
    {
        m_unitsPerEm = defaultUnitsPerEm;

        m_ascent = { };
        m_capHeight = { };
        m_descent = { };
        m_ideogramWidth = { };
        m_lineGap = { };
        m_lineSpacing = { };
        m_underlinePosition = { };
        m_underlineThickness = { };
        m_xHeight = { };
        m_zeroWidth = { };

        m_intAscent = 0;
        m_intDescent = 0;
        m_intLineGap = 0;
        m_intLineSpacing = 0;
        m_intCapHeight = 0;
    }

    unsigned m_unitsPerEm { defaultUnitsPerEm };

    Markable<float> m_ascent;
    Markable<float> m_capHeight;
    Markable<float> m_descent;
    Markable<float> m_ideogramWidth;
    Markable<float> m_lineGap;
    Markable<float> m_lineSpacing;
    Markable<float> m_underlinePosition;
    Markable<float> m_underlineThickness;
    Markable<float> m_xHeight;
    Markable<float> m_zeroWidth;

    // Integer variants of certain metrics are cached for HTML rendering performance.
    int m_intAscent { 0 };
    int m_intDescent { 0 };
    int m_intLineGap { 0 };
    int m_intLineSpacing { 0 };
    int m_intCapHeight { 0 };
};

static inline float scaleEmToUnits(float x, unsigned unitsPerEm)
{
    return unitsPerEm ? x / unitsPerEm : x;
}

} // namespace WebCore

#endif // FontMetrics_h
