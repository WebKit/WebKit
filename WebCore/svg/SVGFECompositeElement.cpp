/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFECompositeElement.h"

#include "Attribute.h"
#include "SVGNames.h"

namespace WebCore {

inline SVGFECompositeElement::SVGFECompositeElement(const QualifiedName& tagName, Document* document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , m__operator(FECOMPOSITE_OPERATOR_OVER)
{
}

PassRefPtr<SVGFECompositeElement> SVGFECompositeElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGFECompositeElement(tagName, document));
}

void SVGFECompositeElement::parseMappedAttribute(Attribute* attr)
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
    } else if (attr->name() == SVGNames::inAttr)
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

void SVGFECompositeElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGFilterPrimitiveStandardAttributes::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronize_operator();
        synchronizeIn1();
        synchronizeIn2();
        synchronizeK1();
        synchronizeK2();
        synchronizeK3();
        synchronizeK4();
        return;
    }

    if (attrName == SVGNames::operatorAttr)
        synchronize_operator();
    else if (attrName == SVGNames::inAttr)
        synchronizeIn1();
    else if (attrName == SVGNames::in2Attr)
        synchronizeIn2();
    else if (attrName == SVGNames::k1Attr)
        synchronizeK1();
    else if (attrName == SVGNames::k2Attr)
        synchronizeK2();
    else if (attrName == SVGNames::k3Attr)
        synchronizeK3();
    else if (attrName == SVGNames::k4Attr)
        synchronizeK4();
}

PassRefPtr<FilterEffect> SVGFECompositeElement::build(SVGFilterBuilder* filterBuilder)
{
    FilterEffect* input1 = filterBuilder->getEffectById(in1());
    FilterEffect* input2 = filterBuilder->getEffectById(in2());
    
    if (!input1 || !input2)
        return 0;

    RefPtr<FilterEffect> effect = FEComposite::create(static_cast<CompositeOperationType>(_operator()),
                                                          k1(), k2(), k3(), k4());
    FilterEffectVector& inputEffects = effect->inputEffects();
    inputEffects.reserveCapacity(2);
    inputEffects.append(input1);
    inputEffects.append(input2);    
    return effect.release();
}

}

#endif // ENABLE(SVG)
