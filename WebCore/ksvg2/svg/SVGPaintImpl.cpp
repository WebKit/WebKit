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
#include "SVGPaintImpl.h"

using namespace KSVG;

SVGPaintImpl::SVGPaintImpl() : SVGColorImpl()
{
    m_paintType = SVG_PAINTTYPE_UNKNOWN;
}

SVGPaintImpl::SVGPaintImpl(KDOM::DOMStringImpl *uri) : SVGColorImpl()
{
    m_paintType = SVG_PAINTTYPE_URI;
    setUri(uri);
}

SVGPaintImpl::SVGPaintImpl(unsigned short paintType) : SVGColorImpl()
{
    m_paintType = paintType;
}

SVGPaintImpl::SVGPaintImpl(unsigned short paintType, KDOM::DOMStringImpl *uri, KDOM::DOMStringImpl *rgbPaint, KDOM::DOMStringImpl *) : SVGColorImpl(rgbPaint)
{
    m_paintType = paintType;
    setUri(uri);
}

SVGPaintImpl::~SVGPaintImpl()
{
}

unsigned short SVGPaintImpl::paintType() const
{
    return m_paintType;
}

KDOM::DOMStringImpl *SVGPaintImpl::uri() const
{
    return m_uri.impl();
}

void SVGPaintImpl::setUri(KDOM::DOMStringImpl *uri)
{
    m_uri = uri;
}

void SVGPaintImpl::setPaint(unsigned short paintType, KDOM::DOMStringImpl *uri, KDOM::DOMStringImpl *rgbPaint, KDOM::DOMStringImpl *)
{
    m_paintType = paintType;

    if(m_paintType == SVG_PAINTTYPE_URI)
        setUri(uri);
    else if(m_paintType == SVG_PAINTTYPE_RGBCOLOR)
        setRGBColor(rgbPaint);
}

KDOM::DOMString SVGPaintImpl::cssText() const
{
    if(m_paintType == SVG_PAINTTYPE_NONE)
        return "none";
    else if(m_paintType == SVG_PAINTTYPE_CURRENTCOLOR)
        return "currentColor";
    else if(m_paintType == SVG_PAINTTYPE_URI)
        return KDOM::DOMString(QString::fromLatin1("url(") + m_uri.qstring() + QString::fromLatin1(")"));

    return SVGColorImpl::cssText();
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

