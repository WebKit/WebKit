/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
#if ENABLE(SVG)
#include "SVGColor.h"

#include "SVGException.h"
#include "SVGParserUtilities.h"

namespace WebCore {

SVGColor::SVGColor()
    : CSSValue()
    , m_colorType(SVG_COLORTYPE_UNKNOWN)
{
}

SVGColor::SVGColor(const String& rgbColor)
    : CSSValue()
    , m_colorType(SVG_COLORTYPE_RGBCOLOR)
{
    setRGBColor(rgbColor);
}

SVGColor::SVGColor(unsigned short colorType)
    : CSSValue()
    , m_colorType(colorType)
{
}

SVGColor::SVGColor(const Color& c)
    : CSSValue()
    , m_color(c)
    , m_colorType(SVG_COLORTYPE_RGBCOLOR)
{
}


SVGColor::~SVGColor()
{
}

unsigned short SVGColor::colorType() const
{
    return m_colorType;
}

unsigned SVGColor::rgbColor() const
{
    return m_color.rgb();
}

void SVGColor::setRGBColor(const String& rgbColor, ExceptionCode& ec)
{
    Color color = SVGColor::colorFromRGBColorString(rgbColor);
    if (color.isValid())
        m_color = color;
    else
        ec = SVG_INVALID_VALUE_ERR;
}

static inline bool parseNumberOrPercent(const UChar*& ptr, const UChar* end, double& value)
{
    if (!parseNumber(ptr, end, value))
        return false;
    if (ptr < end && *ptr == '%') {
        value = int((255.0 * value) / 100.0);
        ptr++;
    }
    return true;
}

Color SVGColor::colorFromRGBColorString(const String& colorString)
{
    if (colorString.isNull())
        return Color();

    String parse = colorString.stripWhiteSpace();
    if (parse.startsWith("rgb(")) {
        double r = -1, g = -1, b = -1;
        const UChar* ptr = parse.characters() + 4;
        const UChar* end = parse.characters() + parse.length();
        skipOptionalSpaces(ptr, end);
        if (!parseNumberOrPercent(ptr, end, r)
         || !parseNumberOrPercent(ptr, end, g)
         || !parseNumberOrPercent(ptr, end, b))
            return Color();

        if (ptr >= end || *ptr != ')')
            return Color();

         return Color(int(r), int(g), int(b));
    }
    return Color(parse.lower());
}

void SVGColor::setRGBColorICCColor(const String& /* rgbColor */, const String& /* iccColor */, ExceptionCode& ec)
{
    // TODO: implement me!
}

void SVGColor::setColor(unsigned short colorType, const String& /* rgbColor */ , const String& /* iccColor */, ExceptionCode& ec)
{
    // TODO: implement me!
    m_colorType = colorType;
}

String SVGColor::cssText() const
{
    if (m_colorType == SVG_COLORTYPE_RGBCOLOR)
        return m_color.name();

    return String();
}

const Color& SVGColor::color() const
{
    return m_color;
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

