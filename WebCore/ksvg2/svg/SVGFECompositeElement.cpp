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
#include <DeprecatedStringList.h>

#include <kdom/core/Attr.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFECompositeElement.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedString.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGFECompositeElement::SVGFECompositeElement(const QualifiedName& tagName, Document *doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFECompositeElement::~SVGFECompositeElement()
{
    delete m_filterEffect;
}

SVGAnimatedString *SVGFECompositeElement::in1() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in1, dummy);
}

SVGAnimatedString *SVGFECompositeElement::in2() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in2, dummy);
}

SVGAnimatedEnumeration *SVGFECompositeElement::_operator() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedEnumeration>(m_operator, dummy);
}

SVGAnimatedNumber *SVGFECompositeElement::k1() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_k1, dummy);
}

SVGAnimatedNumber *SVGFECompositeElement::k2() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_k2, dummy);
}

SVGAnimatedNumber *SVGFECompositeElement::k3() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_k3, dummy);
}

SVGAnimatedNumber *SVGFECompositeElement::k4() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_k4, dummy);
}

void SVGFECompositeElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
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
        k1()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::k2Attr)
        k2()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::k3Attr)
        k3()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::k4Attr)
        k4()->setBaseVal(value.deprecatedString().toDouble());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

KCanvasFEComposite *SVGFECompositeElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEComposite *>(renderingDevice()->createFilterEffect(FE_COMPOSITE));
    if (!m_filterEffect)
        return 0;
    m_filterEffect->setOperation((KCCompositeOperationType)(_operator()->baseVal() - 1));
    m_filterEffect->setIn(String(in1()->baseVal()).deprecatedString());
    m_filterEffect->setIn2(String(in2()->baseVal()).deprecatedString());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setK1(k1()->baseVal());
    m_filterEffect->setK2(k2()->baseVal());
    m_filterEffect->setK3(k3()->baseVal());
    m_filterEffect->setK4(k4()->baseVal());
    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

