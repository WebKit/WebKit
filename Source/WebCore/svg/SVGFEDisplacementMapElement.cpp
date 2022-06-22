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
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEDisplacementMapElement);

inline SVGFEDisplacementMapElement::SVGFEDisplacementMapElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
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

void SVGFEDisplacementMapElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::xChannelSelectorAttr) {
        auto propertyValue = SVGPropertyTraits<ChannelSelectorType>::fromString(value);
        if (propertyValue > 0)
            m_xChannelSelector->setBaseValInternal<ChannelSelectorType>(propertyValue);
        return;
    }

    if (name == SVGNames::yChannelSelectorAttr) {
        auto propertyValue = SVGPropertyTraits<ChannelSelectorType>::fromString(value);
        if (propertyValue > 0)
            m_yChannelSelector->setBaseValInternal<ChannelSelectorType>(propertyValue);
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

    if (name == SVGNames::scaleAttr) {
        m_scale->setBaseValInternal(value.toFloat());
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFEDisplacementMapElement::setFilterEffectAttribute(FilterEffect& effect, const QualifiedName& attrName)
{
    auto& feDisplacementMap = downcast<FEDisplacementMap>(effect);
    if (attrName == SVGNames::xChannelSelectorAttr)
        return feDisplacementMap.setXChannelSelector(xChannelSelector());
    if (attrName == SVGNames::yChannelSelectorAttr)
        return feDisplacementMap.setYChannelSelector(yChannelSelector());
    if (attrName == SVGNames::scaleAttr)
        return feDisplacementMap.setScale(scale());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEDisplacementMapElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::inAttr || attrName == SVGNames::in2Attr) {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        return;
    }

    if (attrName == SVGNames::xChannelSelectorAttr || attrName == SVGNames::yChannelSelectorAttr || attrName == SVGNames::scaleAttr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEDisplacementMapElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    return FEDisplacementMap::create(xChannelSelector(), yChannelSelector(), scale());
}

} // namespace WebCore
