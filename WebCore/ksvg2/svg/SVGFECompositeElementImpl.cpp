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
#include <qstringlist.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFECompositeElementImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace WebCore;

SVGFECompositeElementImpl::SVGFECompositeElementImpl(const QualifiedName& tagName, DocumentImpl *doc) : 
SVGFilterPrimitiveStandardAttributesImpl(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFECompositeElementImpl::~SVGFECompositeElementImpl()
{
    delete m_filterEffect;
}

SVGAnimatedStringImpl *SVGFECompositeElementImpl::in1() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

SVGAnimatedStringImpl *SVGFECompositeElementImpl::in2() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedStringImpl>(m_in2, dummy);
}

SVGAnimatedEnumerationImpl *SVGFECompositeElementImpl::_operator() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedEnumerationImpl>(m_operator, dummy);
}

SVGAnimatedNumberImpl *SVGFECompositeElementImpl::k1() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_k1, dummy);
}

SVGAnimatedNumberImpl *SVGFECompositeElementImpl::k2() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_k2, dummy);
}

SVGAnimatedNumberImpl *SVGFECompositeElementImpl::k3() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_k3, dummy);
}

SVGAnimatedNumberImpl *SVGFECompositeElementImpl::k4() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_k4, dummy);
}

void SVGFECompositeElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    DOMString value(attr->value());
    if (attr->name() == SVGNames::operatorAttr)
    {
        if(value == "over")
            _operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_OVER);
        else if(value == "in")
            _operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_IN);
        else if(value == "out")
            _operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_OUT);
        else if(value == "atop")
            _operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_ATOP);
        else if(value == "xor")
            _operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_XOR);
        else if(value == "arithmetic")
            _operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_ARITHMETIC);
    }
    else if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::in2Attr)
        in2()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::k1Attr)
        k1()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::k2Attr)
        k2()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::k3Attr)
        k3()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::k4Attr)
        k4()->setBaseVal(value.qstring().toDouble());
    else
        SVGFilterPrimitiveStandardAttributesImpl::parseMappedAttribute(attr);
}

KCanvasFEComposite *SVGFECompositeElementImpl::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEComposite *>(QPainter::renderingDevice()->createFilterEffect(FE_COMPOSITE));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setOperation((KCCompositeOperationType)(_operator()->baseVal() - 1));
    m_filterEffect->setIn(DOMString(in1()->baseVal()).qstring());
    m_filterEffect->setIn2(DOMString(in2()->baseVal()).qstring());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setK1(k1()->baseVal());
    m_filterEffect->setK2(k2()->baseVal());
    m_filterEffect->setK3(k3()->baseVal());
    m_filterEffect->setK4(k4()->baseVal());
    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

