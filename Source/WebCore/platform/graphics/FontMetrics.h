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
    static constexpr unsigned defaultUnitsPerEm = 1000;

    unsigned unitsPerEm() const { return m_unitsPerEm; }
    void setUnitsPerEm(unsigned unitsPerEm) { m_unitsPerEm = unitsPerEm; }

    float height(FontBaseline baselineType = AlphabeticBaseline) const
    {
        return ascent(baselineType).value_or(0) + descent(baselineType).value_or(0);
    }
    int intHeight(FontBaseline baselineType = AlphabeticBaseline) const
    {
        return intAscent(baselineType) + intDescent(baselineType);
    }

    std::optional<float> ascent(FontBaseline baselineType = AlphabeticBaseline) const
    {
        if (baselineType == AlphabeticBaseline)
            return m_ascent;
        return height() / 2;
    }
    int intAscent(FontBaseline baselineType = AlphabeticBaseline) const
    {
        if (baselineType == AlphabeticBaseline)
            return m_intAscent;
        return intHeight() - intHeight() / 2;
    }
    void setAscent(float ascent)
    {
        m_ascent = ascent;
        m_intAscent = std::max(static_cast<int>(lroundf(ascent)), 0);
    }

    std::optional<float> descent(FontBaseline baselineType = AlphabeticBaseline) const
    {
        if (baselineType == AlphabeticBaseline)
            return m_descent;
        return height() / 2;
    }
    int intDescent(FontBaseline baselineType = AlphabeticBaseline) const
    {
        if (baselineType == AlphabeticBaseline)
            return m_intDescent;
        return intHeight() / 2;
    }
    void setDescent(float descent)
    {
        m_descent = descent;
        m_intDescent = lroundf(descent);
    }

    std::optional<float> lineGap() const { return m_lineGap; }
    int intLineGap() const { return m_intLineGap; }
    void setLineGap(float lineGap)
    {
        m_lineGap = lineGap;
        m_intLineGap = lroundf(lineGap);
    }

    std::optional<float> lineSpacing() const { return m_lineSpacing; }
    int intLineSpacing() const { return m_intLineSpacing; }
    void setLineSpacing(float lineSpacing)
    {
        m_lineSpacing = lineSpacing;
        m_intLineSpacing = lroundf(lineSpacing);
    }

    std::optional<float> xHeight() const { return m_xHeight; }
    void setXHeight(float xHeight) { m_xHeight = xHeight; }

    std::optional<float> capHeight() const { return m_capHeight; }
    int intCapHeight() const { return m_intCapHeight; }
    void setCapHeight(float capHeight)
    {
        m_capHeight = capHeight;
        m_intCapHeight = lroundf(capHeight);
    }

    std::optional<float> zeroWidth() const { return m_zeroWidth; }
    void setZeroWidth(float zeroWidth) { m_zeroWidth = zeroWidth; }

    std::optional<float> ideogramWidth() const { return m_ideogramWidth; }
    void setIdeogramWidth(float ideogramWidth) { m_ideogramWidth = ideogramWidth; }

    std::optional<float> underlinePosition() const { return m_underlinePosition; }
    void setUnderlinePosition(float underlinePosition) { m_underlinePosition = underlinePosition; }

    std::optional<float> underlineThickness() const { return m_underlineThickness; }
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

        m_ascent = std::nullopt;
        m_capHeight = std::nullopt;
        m_descent = std::nullopt;
        m_ideogramWidth = std::nullopt;
        m_lineGap = std::nullopt;
        m_lineSpacing = std::nullopt;
        m_underlinePosition = std::nullopt;
        m_underlineThickness = std::nullopt;
        m_xHeight = std::nullopt;
        m_zeroWidth = std::nullopt;

        m_intAscent = 0;
        m_intDescent = 0;
        m_intLineGap = 0;
        m_intLineSpacing = 0;
        m_intCapHeight = 0;
    }

    unsigned m_unitsPerEm { defaultUnitsPerEm };

    std::optional<float> m_ascent;
    std::optional<float> m_capHeight;
    std::optional<float> m_descent;
    std::optional<float> m_ideogramWidth;
    std::optional<float> m_lineGap;
    std::optional<float> m_lineSpacing;
    std::optional<float> m_underlinePosition;
    std::optional<float> m_underlineThickness;
    std::optional<float> m_xHeight;
    std::optional<float> m_zeroWidth;

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
