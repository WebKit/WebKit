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
#include "SVGAnimatedStringImpl.h"

using namespace KSVG;

SVGAnimatedStringImpl::SVGAnimatedStringImpl(const SVGStyledElementImpl *context) : SVGAnimatedTemplate<KDOM::DOMStringImpl>(context)
{
}

SVGAnimatedStringImpl::~SVGAnimatedStringImpl()
{
}

KDOM::DOMStringImpl *SVGAnimatedStringImpl::create() const
{
    // TODO: Do we need notifications on string manipulations? ie. appendData()?
    return new KDOM::DOMStringImpl("");
}

void SVGAnimatedStringImpl::assign(KDOM::DOMStringImpl *src, KDOM::DOMStringImpl *dst) const
{
    if(!src || !dst)
        return;
    
    dst->truncate(0);
    dst->insert(src->copy(), 0);
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

