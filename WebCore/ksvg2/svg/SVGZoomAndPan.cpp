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
#include "SVGZoomAndPan.h"

#include "MappedAttribute.h"
#include "SVGNames.h"

namespace WebCore {

SVGZoomAndPan::SVGZoomAndPan()
    : m_zoomAndPan(SVG_ZOOMANDPAN_MAGNIFY)
{
}

SVGZoomAndPan::~SVGZoomAndPan()
{
}

unsigned short SVGZoomAndPan::zoomAndPan() const
{
    return m_zoomAndPan;
}

void SVGZoomAndPan::setZoomAndPan(unsigned short zoomAndPan)
{
    m_zoomAndPan = zoomAndPan;
}

bool SVGZoomAndPan::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::zoomAndPanAttr) {
        if (attr->value() == "disable")
            setZoomAndPan(SVG_ZOOMANDPAN_DISABLE);
        else if (attr->value() == "magnify")
            setZoomAndPan(SVG_ZOOMANDPAN_MAGNIFY);
        return true;
    }

    return false;
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

