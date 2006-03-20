/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
#if SVG_SUPPORT
#include "ksvg.h"
#include "SVGPaint.h"

using namespace WebCore;

SVGPaint::SVGPaint() : SVGColor()
{
    m_paintType = SVG_PAINTTYPE_UNKNOWN;
}

SVGPaint::SVGPaint(StringImpl *uri) : SVGColor()
{
    m_paintType = SVG_PAINTTYPE_URI;
    setUri(uri);
}

SVGPaint::SVGPaint(unsigned short paintType) : SVGColor()
{
    m_paintType = paintType;
}

SVGPaint::SVGPaint(unsigned short paintType, StringImpl *uri, StringImpl *rgbPaint, StringImpl *) : SVGColor(rgbPaint)
{
    m_paintType = paintType;
    setUri(uri);
}

SVGPaint::~SVGPaint()
{
}

unsigned short SVGPaint::paintType() const
{
    return m_paintType;
}

StringImpl *SVGPaint::uri() const
{
    return m_uri.impl();
}

void SVGPaint::setUri(StringImpl *uri)
{
    m_uri = uri;
}

void SVGPaint::setPaint(unsigned short paintType, StringImpl *uri, StringImpl *rgbPaint, StringImpl *)
{
    m_paintType = paintType;

    if(m_paintType == SVG_PAINTTYPE_URI)
        setUri(uri);
    else if(m_paintType == SVG_PAINTTYPE_RGBCOLOR)
        setRGBColor(rgbPaint);
}

String SVGPaint::cssText() const
{
    if(m_paintType == SVG_PAINTTYPE_NONE)
        return "none";
    else if(m_paintType == SVG_PAINTTYPE_CURRENTCOLOR)
        return "currentColor";
    else if(m_paintType == SVG_PAINTTYPE_URI)
        return String(DeprecatedString::fromLatin1("url(") + m_uri.deprecatedString() + DeprecatedString::fromLatin1(")"));

    return SVGColor::cssText();
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

