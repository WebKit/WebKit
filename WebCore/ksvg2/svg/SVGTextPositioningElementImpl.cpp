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
#include <kdom/core/AttrImpl.h>

#include "SVGNames.h"
#include "SVGHelper.h"
//#include "SVGDocument.h"
#include "SVGTextPositioningElementImpl.h"
#include "SVGAnimatedLengthListImpl.h"
#include "SVGAnimatedNumberListImpl.h"

using namespace KSVG;

SVGTextPositioningElementImpl::SVGTextPositioningElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGTextContentElementImpl(tagName, doc)
{
}

SVGTextPositioningElementImpl::~SVGTextPositioningElementImpl()
{
}

SVGAnimatedLengthListImpl *SVGTextPositioningElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthListImpl>(m_x, this);
}

SVGAnimatedLengthListImpl *SVGTextPositioningElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthListImpl>(m_y, this);
}

SVGAnimatedLengthListImpl *SVGTextPositioningElementImpl::dx() const
{
    return lazy_create<SVGAnimatedLengthListImpl>(m_dx, this);
}

SVGAnimatedLengthListImpl *SVGTextPositioningElementImpl::dy() const
{
    return lazy_create<SVGAnimatedLengthListImpl>(m_dy, this);
}

SVGAnimatedNumberListImpl *SVGTextPositioningElementImpl::rotate() const
{
    return lazy_create<SVGAnimatedNumberListImpl>(m_rotate, this);
}

void SVGTextPositioningElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
    
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->parse(value.qstring(), this, LM_WIDTH);
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->parse(value.qstring(), this, LM_HEIGHT);
    else if (attr->name() == SVGNames::dxAttr)
        dx()->baseVal()->parse(value.qstring(), this, LM_WIDTH);
    else if (attr->name() == SVGNames::dyAttr)
        dy()->baseVal()->parse(value.qstring(), this, LM_HEIGHT);
    else if (attr->name() == SVGNames::rotateAttr)
        rotate()->baseVal()->parse(value.qstring(), this);
    else
        SVGTextContentElementImpl::parseMappedAttribute(attr);
}

// vim:ts=4:noet
