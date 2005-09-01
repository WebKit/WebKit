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

#include <kdom/core/AttrImpl.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGZoomAndPanImpl.h"

using namespace KSVG;

SVGZoomAndPanImpl::SVGZoomAndPanImpl()
{
    m_zoomAndPan = SVG_ZOOMANDPAN_MAGNIFY;
}

SVGZoomAndPanImpl::~SVGZoomAndPanImpl()
{
}

unsigned short SVGZoomAndPanImpl::zoomAndPan() const
{
    return m_zoomAndPan;
}

void SVGZoomAndPanImpl::setZoomAndPan(unsigned short zoomAndPan)
{
    m_zoomAndPan = zoomAndPan;
}

bool SVGZoomAndPanImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMString value(attr->value());
    switch(id)
    {
        case ATTR_ZOOMANDPAN:
        {
            if(value == "disable")
                setZoomAndPan(SVG_ZOOMANDPAN_DISABLE);
            else if(value == "magnify")
                setZoomAndPan(SVG_ZOOMANDPAN_MAGNIFY);
            return true;
        }
    }

    return false;
}

// vim:ts=4:noet
