/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#include "FEComposite.h"
#include "NodeName.h"
#include "SVGNames.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGFECompositeElement);

inline SVGFECompositeElement::SVGFECompositeElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
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

void SVGFECompositeElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::operatorAttr: {
        CompositeOperationType propertyValue = SVGPropertyTraits<CompositeOperationType>::fromString(newValue);
        if (enumToUnderlyingType(propertyValue))
            Ref { m_svgOperator }->setBaseValInternal<CompositeOperationType>(propertyValue);
        break;
    }
    case AttributeNames::inAttr:
        Ref { m_in1 }->setBaseValInternal(newValue);
        break;
    case AttributeNames::in2Attr:
        Ref { m_in2 }->setBaseValInternal(newValue);
        break;
    case AttributeNames::k1Attr:
        Ref { m_k1 }->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::k2Attr:
        Ref { m_k2 }->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::k3Attr:
        Ref { m_k3 }->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::k4Attr:
        Ref { m_k4 }->setBaseValInternal(newValue.toFloat());
        break;
    default:
        break;
    }

    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

bool SVGFECompositeElement::setFilterEffectAttribute(FilterEffect& filterEffect, const QualifiedName& attrName)
{
    auto& effect = downcast<FEComposite>(filterEffect);
    switch (attrName.nodeName()) {
    case AttributeNames::operatorAttr:
        return effect.setOperation(svgOperator());
    case AttributeNames::k1Attr:
        return effect.setK1(k1());
    case AttributeNames::k2Attr:
        return effect.setK2(k2());
    case AttributeNames::k3Attr:
        return effect.setK3(k3());
    case AttributeNames::k4Attr:
        return effect.setK4(k4());
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}


void SVGFECompositeElement::svgAttributeChanged(const QualifiedName& attrName)
{
    switch (attrName.nodeName()) {
    case AttributeNames::inAttr:
    case AttributeNames::in2Attr: {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        break;
    }
    case AttributeNames::k1Attr:
    case AttributeNames::k2Attr:
    case AttributeNames::k3Attr:
    case AttributeNames::k4Attr:
    case AttributeNames::operatorAttr: {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        break;
    }
    default:
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        break;
    }
}

RefPtr<FilterEffect> SVGFECompositeElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    return FEComposite::create(svgOperator(), k1(), k2(), k3(), k4());
}

} // namespace WebCore
