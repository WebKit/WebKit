/*
 * Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
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
#include "SVGFEDisplacementMapElement.h"

#include "FEDisplacementMap.h"
#include "NodeName.h"
#include "SVGNames.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGFEDisplacementMapElement);

inline SVGFEDisplacementMapElement::SVGFEDisplacementMapElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feDisplacementMapTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEDisplacementMapElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::in2Attr, &SVGFEDisplacementMapElement::m_in2>();
        PropertyRegistry::registerProperty<SVGNames::xChannelSelectorAttr, ChannelSelectorType, &SVGFEDisplacementMapElement::m_xChannelSelector>();
        PropertyRegistry::registerProperty<SVGNames::yChannelSelectorAttr, ChannelSelectorType, &SVGFEDisplacementMapElement::m_yChannelSelector>();
        PropertyRegistry::registerProperty<SVGNames::scaleAttr, &SVGFEDisplacementMapElement::m_scale>();
    });
}

Ref<SVGFEDisplacementMapElement> SVGFEDisplacementMapElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEDisplacementMapElement(tagName, document));
}

void SVGFEDisplacementMapElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::xChannelSelectorAttr: {
        auto propertyValue = SVGPropertyTraits<ChannelSelectorType>::fromString(newValue);
        if (enumToUnderlyingType(propertyValue))
            Ref { m_xChannelSelector }->setBaseValInternal<ChannelSelectorType>(propertyValue);
        break;
    }
    case AttributeNames::yChannelSelectorAttr: {
        auto propertyValue = SVGPropertyTraits<ChannelSelectorType>::fromString(newValue);
        if (enumToUnderlyingType(propertyValue))
            Ref { m_yChannelSelector }->setBaseValInternal<ChannelSelectorType>(propertyValue);
        break;
    }
    case AttributeNames::inAttr:
        Ref { m_in1 }->setBaseValInternal(newValue);
        break;
    case AttributeNames::in2Attr:
        Ref { m_in2 }->setBaseValInternal(newValue);
        break;
    case AttributeNames::scaleAttr:
        Ref { m_scale }->setBaseValInternal(newValue.toFloat());
        break;
    default:
        break;
    }

    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

bool SVGFEDisplacementMapElement::setFilterEffectAttribute(FilterEffect& filterEffect, const QualifiedName& attrName)
{
    auto& effect = downcast<FEDisplacementMap>(filterEffect);
    switch (attrName.nodeName()) {
    case AttributeNames::xChannelSelectorAttr:
        return effect.setXChannelSelector(xChannelSelector());
    case AttributeNames::yChannelSelectorAttr:
        return effect.setYChannelSelector(yChannelSelector());
    case AttributeNames::scaleAttr:
        return effect.setScale(scale());
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEDisplacementMapElement::svgAttributeChanged(const QualifiedName& attrName)
{
    switch (attrName.nodeName()) {
    case AttributeNames::inAttr:
    case AttributeNames::in2Attr: {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        break;
    }
    case AttributeNames::xChannelSelectorAttr:
    case AttributeNames::yChannelSelectorAttr:
    case AttributeNames::scaleAttr: {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        break;
    }
    default:
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        break;
    }
}

RefPtr<FilterEffect> SVGFEDisplacementMapElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    return FEDisplacementMap::create(xChannelSelector(), yChannelSelector(), scale());
}

} // namespace WebCore
