/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFECompositeElement.h"

#include "MappedAttribute.h"
#include "SVGNames.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFECompositeElement::SVGFECompositeElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_in1(this, SVGNames::inAttr)
    , m_in2(this, SVGNames::in2Attr)
    , m__operator(this, SVGNames::operatorAttr, FECOMPOSITE_OPERATOR_OVER)
    , m_k1(this, SVGNames::k1Attr)
    , m_k2(this, SVGNames::k2Attr)
    , m_k3(this, SVGNames::k3Attr)
    , m_k4(this, SVGNames::k4Attr)
{
}

SVGFECompositeElement::~SVGFECompositeElement()
{
}

void SVGFECompositeElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::operatorAttr) {
        if (value == "over")
            set_operatorBaseValue(FECOMPOSITE_OPERATOR_OVER);
        else if (value == "in")
            set_operatorBaseValue(FECOMPOSITE_OPERATOR_IN);
        else if (value == "out")
            set_operatorBaseValue(FECOMPOSITE_OPERATOR_OUT);
        else if (value == "atop")
            set_operatorBaseValue(FECOMPOSITE_OPERATOR_ATOP);
        else if (value == "xor")
            set_operatorBaseValue(FECOMPOSITE_OPERATOR_XOR);
        else if (value == "arithmetic")
            set_operatorBaseValue(FECOMPOSITE_OPERATOR_ARITHMETIC);
    }
    else if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else if (attr->name() == SVGNames::in2Attr)
        setIn2BaseValue(value);
    else if (attr->name() == SVGNames::k1Attr)
        setK1BaseValue(value.toFloat());
    else if (attr->name() == SVGNames::k2Attr)
        setK2BaseValue(value.toFloat());
    else if (attr->name() == SVGNames::k3Attr)
        setK3BaseValue(value.toFloat());
    else if (attr->name() == SVGNames::k4Attr)
        setK4BaseValue(value.toFloat());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

bool SVGFECompositeElement::build(SVGResourceFilter* filterResource)
{
    FilterEffect* input1 = filterResource->builder()->getEffectById(in1());
    FilterEffect* input2 = filterResource->builder()->getEffectById(in2());
    
    if(!input1 || !input2)
        return false;
    
    RefPtr<FilterEffect> effect = FEComposite::create(input1, input2, static_cast<CompositeOperationType>(_operator()),
                                        k1(), k2(), k3(), k4());
    filterResource->addFilterEffect(this, effect.release());

    return true;
}

}

#endif // ENABLE(SVG)
