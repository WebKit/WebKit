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

#include <qstringlist.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEOffsetElementImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFEOffsetElementImpl::SVGFEOffsetElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix) : 
SVGFilterPrimitiveStandardAttributesImpl(doc, id, prefix)
{
    m_in1 = 0;
    m_dx = 0;
    m_dy = 0;
    m_filterEffect = 0;
}

SVGFEOffsetElementImpl::~SVGFEOffsetElementImpl()
{
    if(m_in1)
        m_in1->deref();
    if(m_dx)
        m_dx->deref();
    if(m_dy)
        m_dy->deref();
}

SVGAnimatedStringImpl *SVGFEOffsetElementImpl::in1() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

SVGAnimatedNumberImpl *SVGFEOffsetElementImpl::dx() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_dx, dummy);
}

SVGAnimatedNumberImpl *SVGFEOffsetElementImpl::dy() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_dy, dummy);
}

void SVGFEOffsetElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMString value(attr->value());
    switch(id)
    {
        case ATTR_DX:
        {
            dx()->setBaseVal(value.string().toDouble());
            break;
        }
        case ATTR_DY:
        {
            dy()->setBaseVal(value.string().toDouble());
            break;
        }
        case ATTR_IN:
        {
            in1()->setBaseVal(value.handle());
            break;
        }
        default:
        {
            SVGFilterPrimitiveStandardAttributesImpl::parseAttribute(attr);
        }
    };
}

KCanvasItem *SVGFEOffsetElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
    m_filterEffect = static_cast<KCanvasFEOffset *>(canvas->renderingDevice()->createFilterEffect(FE_OFFSET));
    m_filterEffect->setIn(KDOM::DOMString(in1()->baseVal()).string());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setDx(dx()->baseVal());
    m_filterEffect->setDy(dy()->baseVal());
    return 0;
}

KCanvasFilterEffect *SVGFEOffsetElementImpl::filterEffect() const
{
    return m_filterEffect;
}

// vim:ts=4:noet
