/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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

#include "CSSParser.h"
#include "RGBColor.h"
#include "SVGException.h"

namespace WebCore {

SVGColor::SVGColor()
    : m_colorType(SVG_COLORTYPE_UNKNOWN)
{
}

SVGColor::SVGColor(const String& rgbColor)
    : m_colorType(SVG_COLORTYPE_RGBCOLOR)
{
    setRGBColor(rgbColor);
}

SVGColor::SVGColor(SVGColorType colorType)
    : m_colorType(colorType)
{
}

SVGColor::SVGColor(const Color& c)
    : m_color(c)
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

PassRefPtr<RGBColor> SVGColor::rgbColor() const
{
    return RGBColor::create(m_color.rgb());
}

void SVGColor::setRGBColor(const String& rgbColor, ExceptionCode& ec)
{
    Color color = SVGColor::colorFromRGBColorString(rgbColor);
    if (color.isValid())
        m_color = color;
    else
        ec = SVGException::SVG_INVALID_VALUE_ERR;
}

Color SVGColor::colorFromRGBColorString(const String& colorString)
{
    String s = colorString.stripWhiteSpace();
    // hsl, hsla and rgba are not in the SVG spec.
    // FIXME: rework css parser so it is more svg aware
    if (s.startsWith("hsl") || s.startsWith("rgba"))
        return Color();
    RGBA32 color;
    if (CSSParser::parseColor(color, s))
        return color;
    return Color();
}

void SVGColor::setRGBColorICCColor(const String& /* rgbColor */, const String& /* iccColor */, ExceptionCode&)
{
    // TODO: implement me!
}

void SVGColor::setColor(unsigned short colorType, const String& /* rgbColor */ , const String& /* iccColor */, ExceptionCode&)
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

