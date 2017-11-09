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
#include "SVGFECompositeElement.h"

#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFECompositeElement, SVGNames::inAttr, In1, in1)
DEFINE_ANIMATED_STRING(SVGFECompositeElement, SVGNames::in2Attr, In2, in2)
DEFINE_ANIMATED_ENUMERATION(SVGFECompositeElement, SVGNames::operatorAttr, SVGOperator, svgOperator, CompositeOperationType)
DEFINE_ANIMATED_NUMBER(SVGFECompositeElement, SVGNames::k1Attr, K1, k1)
DEFINE_ANIMATED_NUMBER(SVGFECompositeElement, SVGNames::k2Attr, K2, k2)
DEFINE_ANIMATED_NUMBER(SVGFECompositeElement, SVGNames::k3Attr, K3, k3)
DEFINE_ANIMATED_NUMBER(SVGFECompositeElement, SVGNames::k4Attr, K4, k4)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFECompositeElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in2)
    REGISTER_LOCAL_ANIMATED_PROPERTY(svgOperator)
    REGISTER_LOCAL_ANIMATED_PROPERTY(k1)
    REGISTER_LOCAL_ANIMATED_PROPERTY(k2)
    REGISTER_LOCAL_ANIMATED_PROPERTY(k3)
    REGISTER_LOCAL_ANIMATED_PROPERTY(k4)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFECompositeElement::SVGFECompositeElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , m_svgOperator(FECOMPOSITE_OPERATOR_OVER)
{
    ASSERT(hasTagName(SVGNames::feCompositeTag));
    registerAnimatedPropertiesForSVGFECompositeElement();
}

Ref<SVGFECompositeElement> SVGFECompositeElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFECompositeElement(tagName, document));
}

void SVGFECompositeElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::operatorAttr) {
        CompositeOperationType propertyValue = SVGPropertyTraits<CompositeOperationType>::fromString(value);
        if (propertyValue > 0)
            setSVGOperatorBaseValue(propertyValue);
        return;
    }

    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    if (name == SVGNames::in2Attr) {
        setIn2BaseValue(value);
        return;
    }

    if (name == SVGNames::k1Attr) {
        setK1BaseValue(value.toFloat());
        return;
    }

    if (name == SVGNames::k2Attr) {
        setK2BaseValue(value.toFloat());
        return;
    }

    if (name == SVGNames::k3Attr) {
        setK3BaseValue(value.toFloat());
        return;
    }

    if (name == SVGNames::k4Attr) {
        setK4BaseValue(value.toFloat());
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFECompositeElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEComposite* composite = static_cast<FEComposite*>(effect);
    if (attrName == SVGNames::operatorAttr)
        return composite->setOperation(svgOperator());
    if (attrName == SVGNames::k1Attr)
        return composite->setK1(k1());
    if (attrName == SVGNames::k2Attr)
        return composite->setK2(k2());
    if (attrName == SVGNames::k3Attr)
        return composite->setK3(k3());
    if (attrName == SVGNames::k4Attr)
        return composite->setK4(k4());

    ASSERT_NOT_REACHED();
    return false;
}


void SVGFECompositeElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::operatorAttr || attrName == SVGNames::k1Attr || attrName == SVGNames::k2Attr || attrName == SVGNames::k3Attr || attrName == SVGNames::k4Attr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr || attrName == SVGNames::in2Attr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFECompositeElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    auto input1 = filterBuilder->getEffectById(in1());
    auto input2 = filterBuilder->getEffectById(in2());
    
    if (!input1 || !input2)
        return nullptr;

    RefPtr<FilterEffect> effect = FEComposite::create(filter, svgOperator(), k1(), k2(), k3(), k4());
    FilterEffectVector& inputEffects = effect->inputEffects();
    inputEffects.reserveCapacity(2);
    inputEffects.append(input1);
    inputEffects.append(input2);    
    return effect;
}

}
