/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFECompositeElement);

inline SVGFECompositeElement::SVGFECompositeElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feCompositeTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFECompositeElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::in2Attr, &SVGFECompositeElement::m_in2>();
        PropertyRegistry::registerProperty<SVGNames::operatorAttr, CompositeOperationType, &SVGFECompositeElement::m_svgOperator>();
        PropertyRegistry::registerProperty<SVGNames::k1Attr, &SVGFECompositeElement::m_k1>();
        PropertyRegistry::registerProperty<SVGNames::k2Attr, &SVGFECompositeElement::m_k2>();
        PropertyRegistry::registerProperty<SVGNames::k3Attr, &SVGFECompositeElement::m_k3>();
        PropertyRegistry::registerProperty<SVGNames::k4Attr, &SVGFECompositeElement::m_k4>();
    });
}

Ref<SVGFECompositeElement> SVGFECompositeElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFECompositeElement(tagName, document));
}

void SVGFECompositeElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::operatorAttr) {
        CompositeOperationType propertyValue = SVGPropertyTraits<CompositeOperationType>::fromString(value);
        if (propertyValue > 0)
            m_svgOperator->setBaseValInternal<CompositeOperationType>(propertyValue);
        return;
    }

    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    if (name == SVGNames::in2Attr) {
        m_in2->setBaseValInternal(value);
        return;
    }

    if (name == SVGNames::k1Attr) {
        m_k1->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::k2Attr) {
        m_k2->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::k3Attr) {
        m_k3->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::k4Attr) {
        m_k4->setBaseValInternal(value.toFloat());
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

RefPtr<FilterEffect> SVGFECompositeElement::build(SVGFilterBuilder* filterBuilder, Filter& filter) const
{
    auto input1 = filterBuilder->getEffectById(in1());
    auto input2 = filterBuilder->getEffectById(in2());
    
    if (!input1 || !input2)
        return nullptr;

    auto effect = FEComposite::create(filter, svgOperator(), k1(), k2(), k3(), k4());
    FilterEffectVector& inputEffects = effect->inputEffects();
    inputEffects.reserveCapacity(2);
    inputEffects.append(input1);
    inputEffects.append(input2);    
    return effect;
}

}
