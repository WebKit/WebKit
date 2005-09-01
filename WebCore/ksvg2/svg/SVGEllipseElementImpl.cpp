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

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGEllipseElementImpl.h"
#include "SVGAnimatedLengthImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>

using namespace KSVG;

SVGEllipseElementImpl::SVGEllipseElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl()
{
    m_cx = m_cy = m_rx = m_ry = 0;
}

SVGEllipseElementImpl::~SVGEllipseElementImpl()
{
    if(m_cx)
        m_cx->deref();
    if(m_cy)
        m_cy->deref();
    if(m_rx)
        m_rx->deref();
    if(m_ry)
        m_ry->deref();
}

SVGAnimatedLengthImpl *SVGEllipseElementImpl::cx() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_cx, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGEllipseElementImpl::cy() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_cy, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGEllipseElementImpl::rx() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_rx, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGEllipseElementImpl::ry() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_ry, this, LM_HEIGHT, viewportElement());
}

void SVGEllipseElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMStringImpl *value = attr->value();
    switch(id)
    {
        case ATTR_CX:
        {
            cx()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_CY:
        {
            cy()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_RX:
        {
            rx()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_RY:
        {
            ry()->baseVal()->setValueAsString(value);
            break;
        }
        default:
        {
            if(SVGTestsImpl::parseAttribute(attr)) return;
            if(SVGLangSpaceImpl::parseAttribute(attr)) return;
            if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
            if(SVGTransformableImpl::parseAttribute(attr)) return;
            
            SVGStyledElementImpl::parseAttribute(attr);
        }
    };
}

KCPathDataList SVGEllipseElementImpl::toPathData() const
{
    float _cx = cx()->baseVal()->value(), _cy = cy()->baseVal()->value();
    float _rx = rx()->baseVal()->value(), _ry = ry()->baseVal()->value();

    return KCanvasCreator::self()->createEllipse(_cx, _cy, _rx, _ry);
}

const SVGStyledElementImpl *SVGEllipseElementImpl::pushAttributeContext(const SVGStyledElementImpl *context)
{
    // All attribute's contexts are equal (so just take the one from 'cx').
    const SVGStyledElementImpl *restore = cx()->baseVal()->context();

    cx()->baseVal()->setContext(context);
    cy()->baseVal()->setContext(context);
    rx()->baseVal()->setContext(context);
    ry()->baseVal()->setContext(context);

    SVGStyledElementImpl::pushAttributeContext(context);
    return restore;
}

// vim:ts=4:noet
