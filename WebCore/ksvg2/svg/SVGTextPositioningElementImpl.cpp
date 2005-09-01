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

#include <kdom/impl/AttrImpl.h>

#include "svgattrs.h"
#include "SVGHelper.h"
//#include "SVGDocument.h"
#include "SVGTextPositioningElementImpl.h"
#include "SVGAnimatedLengthListImpl.h"
#include "SVGAnimatedNumberListImpl.h"

using namespace KSVG;

SVGTextPositioningElementImpl::SVGTextPositioningElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGTextContentElementImpl(doc, id, prefix)
{
    m_x = m_y = m_dx = m_dy = 0;
    m_rotate = 0;
}

SVGTextPositioningElementImpl::~SVGTextPositioningElementImpl()
{
    if(m_x)
        m_x->deref();
    if(m_y)
        m_y->deref();
    if(m_dx)
        m_dx->deref();
    if(m_dy)
        m_dy->deref();
    if(m_rotate)
        m_rotate->deref();
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

void SVGTextPositioningElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMString value(attr->value());
    switch(id)
    {
        case ATTR_X:
        {
            x()->baseVal()->parse(value.string(), this, LM_WIDTH);
            break;
        }
        case ATTR_Y:
        {
            y()->baseVal()->parse(value.string(), this, LM_HEIGHT);
            break;
        }
        case ATTR_DX:
        {
            dx()->baseVal()->parse(value.string(), this, LM_WIDTH);
            break;
        }
        case ATTR_DY:
        {
            dy()->baseVal()->parse(value.string(), this, LM_HEIGHT);
            break;
        }
        case ATTR_ROTATE:
        {
            rotate()->baseVal()->parse(value.string(), this);
            break;
        }
        default:
        {
            SVGTextContentElementImpl::parseAttribute(attr);
        }
    };
}

// vim:ts=4:noet
