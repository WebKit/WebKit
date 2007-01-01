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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
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
    ec = SVG_INVALID_VALUE_ERR;
    if (rgbColor.isNull())
        return;

    Color color;
    String parse = rgbColor.stripWhiteSpace();
    if (parse.startsWith("rgb(")) {
        double r = -1, g = -1, b = -1;
        const UChar* ptr = parse.characters() + 4;
        const UChar* end = parse.characters() + parse.length();
        skipOptionalSpaces(ptr, end);
        if (!parseNumber(ptr, end, r))
            return;
        if (*ptr == '%') {
            r = int((255.0 * r) / 100.0);
            ptr++;
        }
        if (!parseNumber(ptr, end, g))
            return;
        if (*ptr == '%') {
            g = int((255.0 * g) / 100.0);
            ptr++;
        }
        if (!parseNumber(ptr, end, b))
            return;
        if (*ptr == '%') {
            b = int((255.0 * b) / 100.0);
            ptr++;
        }

        if (*ptr != ')')
            return;
        ptr++;
        if (ptr != end)
            return;

        color = Color(int(r), int(g), int(b));
    } else {
        String colorName = parse.lower();
        color = Color(colorName);
    }
    if (color.isValid()) {
        ec = 0;
        m_color = color;
    }
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
#endif // SVG_SUPPORT

