/*
   Copyright (C) 2007 Eric Seidel <eric@webkit.org>
   Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG_FONTS)
#include "SVGGlyphElement.h"

#include "FontData.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "XMLNames.h"

namespace WebCore {

using namespace SVGNames;

SVGGlyphElement::SVGGlyphElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
{
}

static inline SVGGlyphIdentifier::ArabicForm parseArabicForm(const AtomicString& value)
{
    if (value == "medial")
        return SVGGlyphIdentifier::Medial;
    else if (value == "terminal")
        return SVGGlyphIdentifier::Terminal;
    else if (value == "isolated")
        return SVGGlyphIdentifier::Isolated;

    return SVGGlyphIdentifier::Initial;
}

static inline SVGGlyphIdentifier::Orientation parseOrientation(const AtomicString& value)
{
    if (value == "h")
        return SVGGlyphIdentifier::Horizontal;
    else if (value == "v")
        return SVGGlyphIdentifier::Vertical;

    return SVGGlyphIdentifier::Both;
}

static inline Path parsePathData(const AtomicString& value)
{
    Path result;
    pathFromSVGData(result, value);

    return result;
}

SVGGlyphIdentifier SVGGlyphElement::buildGlyphIdentifier(SVGFontData* svgFontData) const
{
    ASSERT(svgFontData);

    SVGGlyphIdentifier identifier;
    identifier.glyphName = getAttribute(glyph_nameAttr);
    identifier.orientation = parseOrientation(getAttribute(orientationAttr));
    identifier.arabicForm = parseArabicForm(getAttribute(arabic_formAttr));
    identifier.pathData = parsePathData(getAttribute(dAttr));
    identifier.languages = parseDelimitedString(getAttribute(XMLNames::langAttr), ',');

    // Spec: The horizontal advance after rendering the glyph in horizontal orientation.
    // If the attribute is not specified, the effect is as if the attribute were set to the
    // value of the font's horiz-adv-x attribute. Glyph widths are required to be non-negative,
    // even if the glyph is typically rendered right-to-left, as in Hebrew and Arabic scripts.
    if (hasAttribute(horiz_adv_xAttr))
        identifier.horizontalAdvanceX = getAttribute(horiz_adv_xAttr).toFloat();
    else
        identifier.horizontalAdvanceX = svgFontData->horizontalAdvanceX;

    // Spec: The X-coordinate in the font coordinate system of the origin of the glyph to be
    // used when drawing vertically oriented text. If the attribute is not specified, the effect
    // is as if the attribute were set to the value of the font's vert-origin-x attribute.
    if (hasAttribute(vert_origin_xAttr))
        identifier.verticalOriginX = getAttribute(vert_origin_xAttr).toFloat();
    else
        identifier.verticalOriginX = svgFontData->verticalOriginX;

    // Spec: The Y-coordinate in the font coordinate system of the origin of a glyph to be
    // used when drawing vertically oriented text. If the attribute is not specified, the effect
    // is as if the attribute were set to the value of the font's vert-origin-y attribute.
    if (hasAttribute(vert_origin_yAttr))
        identifier.verticalOriginY = getAttribute(vert_origin_yAttr).toFloat();
    else
        identifier.verticalOriginY = svgFontData->verticalOriginY;

    // Spec: The vertical advance after rendering a glyph in vertical orientation.
    // If the attribute is not specified, the effect is as if the attribute were set to the
    // value of the font's vert-adv-y attribute.
    if (hasAttribute(vert_adv_yAttr))
        identifier.verticalAdvanceY = getAttribute(vert_adv_yAttr).toFloat();
    else
        identifier.verticalAdvanceY = svgFontData->verticalAdvanceY;

    return identifier;
}

}

#endif // ENABLE(SVG_FONTS)
