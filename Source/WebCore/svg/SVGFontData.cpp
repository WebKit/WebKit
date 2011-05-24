/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"

#if ENABLE(SVG_FONTS)
#include "SVGFontData.h"

#include "SVGFontElement.h"
#include "SVGGlyph.h"

namespace WebCore {

SVGFontData::SVGFontData(SVGFontFaceElement* fontFaceElement)
    : m_svgFontFaceElement(fontFaceElement)
    , m_horizontalOriginX(fontFaceElement->horizontalOriginX())
    , m_horizontalOriginY(fontFaceElement->horizontalOriginY())
    , m_horizontalAdvanceX(fontFaceElement->horizontalAdvanceX())
    , m_verticalOriginX(fontFaceElement->verticalOriginX())
    , m_verticalOriginY(fontFaceElement->verticalOriginY())
    , m_verticalAdvanceY(fontFaceElement->verticalAdvanceY())
{
    ASSERT_ARG(fontFaceElement, fontFaceElement);
}

void SVGFontData::initializeFontData(SimpleFontData* fontData, int size)
{
    ASSERT(fontData);

    SVGFontFaceElement* svgFontFaceElement = this->svgFontFaceElement();
    unsigned unitsPerEm = svgFontFaceElement->unitsPerEm();

    float scale = size;
    if (unitsPerEm)
        scale /= unitsPerEm;

    float xHeight = svgFontFaceElement->xHeight() * scale;
    float ascent = svgFontFaceElement->ascent() * scale;
    float descent = svgFontFaceElement->descent() * scale;
    float lineGap = 0.1f * size;

    SVGFontElement* associatedFontElement = svgFontFaceElement->associatedFontElement();
    if (!xHeight) {
        // Fallback if x_heightAttr is not specified for the font element.
        Vector<SVGGlyph> letterXGlyphs;
        associatedFontElement->getGlyphIdentifiersForString(String("x", 1), letterXGlyphs);
        xHeight = letterXGlyphs.isEmpty() ? 2 * ascent / 3 : letterXGlyphs.first().horizontalAdvanceX * scale;
    }

    FontMetrics& fontMetrics = fontData->fontMetrics();
    fontMetrics.setUnitsPerEm(unitsPerEm);
    fontMetrics.setAscent(ascent);
    fontMetrics.setDescent(descent);
    fontMetrics.setLineGap(lineGap);
    fontMetrics.setLineSpacing(roundf(ascent) + roundf(descent) + roundf(lineGap));
    fontMetrics.setXHeight(xHeight);

    Vector<SVGGlyph> spaceGlyphs;
    associatedFontElement->getGlyphIdentifiersForString(String(" ", 1), spaceGlyphs);
    fontData->setSpaceWidth(spaceGlyphs.isEmpty() ? xHeight : spaceGlyphs.first().horizontalAdvanceX * scale);

    Vector<SVGGlyph> numeralZeroGlyphs;
    associatedFontElement->getGlyphIdentifiersForString(String("0", 1), numeralZeroGlyphs);
    fontData->setAvgCharWidth(numeralZeroGlyphs.isEmpty() ? fontData->spaceWidth() : numeralZeroGlyphs.first().horizontalAdvanceX * scale);

    Vector<SVGGlyph> letterWGlyphs;
    associatedFontElement->getGlyphIdentifiersForString(String("W", 1), letterWGlyphs);
    fontData->setMaxCharWidth(letterWGlyphs.isEmpty() ? ascent : letterWGlyphs.first().horizontalAdvanceX * scale);

    // FIXME: is there a way we can get the space glyph from the SVGGlyph above?
    fontData->setSpaceGlyph(0);
    fontData->setZeroWidthSpaceGlyph(0);
    fontData->determinePitch();

    GlyphData missingGlyphData;
    missingGlyphData.fontData = fontData;
    missingGlyphData.glyph = 0;
    fontData->setMissingGlyphData(missingGlyphData);
}

} // namespace WebCore

#endif
