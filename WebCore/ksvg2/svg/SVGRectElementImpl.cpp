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

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGDocumentImpl.h"
#include "SVGRectElementImpl.h"
#include "SVGAnimatedLengthImpl.h"

#include <kcanvas/device/KRenderingStyle.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingFillPainter.h>
#include <kcanvas/device/KRenderingPaintServerSolid.h>
#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>

using namespace KSVG;

SVGRectElementImpl::SVGRectElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl()
{
    m_x = m_y = m_rx = m_ry = m_width = m_height = 0;
}

SVGRectElementImpl::~SVGRectElementImpl()
{
    if(m_x)
        m_x->deref();
    if(m_y)
        m_y->deref();
    if(m_width)
        m_width->deref();
    if(m_height)
        m_height->deref();
    if(m_rx)
        m_rx->deref();
    if(m_ry)
        m_ry->deref();
}

SVGAnimatedLengthImpl *SVGRectElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGRectElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGRectElementImpl::width() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGRectElementImpl::height() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGRectElementImpl::rx() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_rx, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGRectElementImpl::ry() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_ry, this, LM_HEIGHT, viewportElement());
}

void SVGRectElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMStringImpl *value = attr->value();
    switch(id)
    {
        case ATTR_X:
        {
            x()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_Y:
        {
            y()->baseVal()->setValueAsString(value);
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
        case ATTR_WIDTH:
        {
            width()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_HEIGHT:
        {
            height()->baseVal()->setValueAsString(value);
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

KCPathDataList SVGRectElementImpl::toPathData() const
{
    float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
    float _width = width()->baseVal()->value(), _height = height()->baseVal()->value();

    if(hasAttribute(KDOM::DOMString("rx").handle()) || hasAttribute(KDOM::DOMString("ry").handle()))
    {
        float _rx = rx()->baseVal()->value(), _ry = rx()->baseVal()->value();
        return KCanvasCreator::self()->createRoundedRectangle(_x, _y, _width, _height, _rx, _ry);
    }

    return KCanvasCreator::self()->createRectangle(_x, _y, _width, _height);
}

const SVGStyledElementImpl *SVGRectElementImpl::pushAttributeContext(const SVGStyledElementImpl *context)
{
    // All attribute's contexts are equal (so just take the one from 'x').
    const SVGStyledElementImpl *restore = x()->baseVal()->context();

    x()->baseVal()->setContext(context);
    y()->baseVal()->setContext(context);
    width()->baseVal()->setContext(context);
    height()->baseVal()->setContext(context);
    
    SVGStyledElementImpl::pushAttributeContext(context);
    return restore;
}

// vim:ts=4:noet
