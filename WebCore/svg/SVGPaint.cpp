/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#if ENABLE(SVG)
#include "SVGPaint.h"
#include "SVGURIReference.h"

namespace WebCore {

SVGPaint::SVGPaint()
    : m_paintType(SVG_PAINTTYPE_UNKNOWN)
{
}

SVGPaint::SVGPaint(const String& uri)
    : m_paintType(SVG_PAINTTYPE_URI_RGBCOLOR)
{
    setUri(uri);
}

SVGPaint::SVGPaint(SVGPaintType paintType)
    : m_paintType(paintType)
{
}

SVGPaint::SVGPaint(SVGPaintType paintType, const String& uri, const String& rgbPaint, const String&)
    : SVGColor(rgbPaint)
    , m_paintType(paintType)
{
    setUri(uri);
}

SVGPaint::SVGPaint(const Color& c)
    : SVGColor(c)
    , m_paintType(SVG_PAINTTYPE_RGBCOLOR)
{
}

SVGPaint::SVGPaint(const String& uri, const Color& c)
    : SVGColor(c)
    , m_paintType(SVG_PAINTTYPE_URI_RGBCOLOR)
{
    setUri(uri);
}

SVGPaint* SVGPaint::defaultFill()
{
    static SVGPaint* staticDefaultFill = create(Color::black).releaseRef();
    return staticDefaultFill;
}

SVGPaint* SVGPaint::defaultStroke()
{
    static SVGPaint* staticDefaultStroke = create(SVG_PAINTTYPE_NONE).releaseRef();
    return staticDefaultStroke;
}

String SVGPaint::uri() const
{
    return m_uri;
}

void SVGPaint::setUri(const String& uri)
{
    m_uri = uri;
}

void SVGPaint::setPaint(SVGPaintType paintType, const String& uri, const String& rgbPaint, const String&, ExceptionCode&)
{
    m_paintType = paintType;

    if (m_paintType == SVG_PAINTTYPE_URI)
        setUri(uri);
    else if (m_paintType == SVG_PAINTTYPE_RGBCOLOR)
        setRGBColor(rgbPaint);
}

String SVGPaint::cssText() const
{
    if (m_paintType == SVG_PAINTTYPE_NONE)
        return "none";
    else if (m_paintType == SVG_PAINTTYPE_CURRENTCOLOR)
        return "currentColor";
    else if (m_paintType == SVG_PAINTTYPE_URI)
        return "url(" + m_uri + ")";

    return SVGColor::cssText();
}

bool SVGPaint::matchesTargetURI(const String& referenceId)
{
    if (m_paintType != SVG_PAINTTYPE_URI && m_paintType != SVG_PAINTTYPE_URI_RGBCOLOR)
        return false;

    return referenceId == SVGURIReference::getTarget(m_uri);
}

}

#endif // ENABLE(SVG)
