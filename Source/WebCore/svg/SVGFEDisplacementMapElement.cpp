/*
 * Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEDisplacementMapElement);

inline SVGFEDisplacementMapElement::SVGFEDisplacementMapElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feDisplacementMapTag));
    registerAttributes();
}

Ref<SVGFEDisplacementMapElement> SVGFEDisplacementMapElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEDisplacementMapElement(tagName, document));
}

void SVGFEDisplacementMapElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::inAttr, &SVGFEDisplacementMapElement::m_in1>();
    registry.registerAttribute<SVGNames::in2Attr, &SVGFEDisplacementMapElement::m_in2>();
    registry.registerAttribute<SVGNames::xChannelSelectorAttr, ChannelSelectorType, &SVGFEDisplacementMapElement::m_xChannelSelector>();
    registry.registerAttribute<SVGNames::yChannelSelectorAttr, ChannelSelectorType, &SVGFEDisplacementMapElement::m_yChannelSelector>();
    registry.registerAttribute<SVGNames::scaleAttr, &SVGFEDisplacementMapElement::m_scale>();
}

void SVGFEDisplacementMapElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::xChannelSelectorAttr) {
        auto propertyValue = SVGPropertyTraits<ChannelSelectorType>::fromString(value);
        if (propertyValue > 0)
            m_xChannelSelector.setValue(propertyValue);
        return;
    }

    if (name == SVGNames::yChannelSelectorAttr) {
        auto propertyValue = SVGPropertyTraits<ChannelSelectorType>::fromString(value);
        if (propertyValue > 0)
            m_yChannelSelector.setValue(propertyValue);
        return;
    }

    if (name == SVGNames::inAttr) {
        m_in1.setValue(value);
        return;
    }

    if (name == SVGNames::in2Attr) {
        m_in2.setValue(value);
        return;
    }

    if (name == SVGNames::scaleAttr) {
        m_scale.setValue(value.toFloat());
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFEDisplacementMapElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEDisplacementMap* displacementMap = static_cast<FEDisplacementMap*>(effect);
    if (attrName == SVGNames::xChannelSelectorAttr)
        return displacementMap->setXChannelSelector(xChannelSelector());
    if (attrName == SVGNames::yChannelSelectorAttr)
        return displacementMap->setYChannelSelector(yChannelSelector());
    if (attrName == SVGNames::scaleAttr)
        return displacementMap->setScale(scale());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEDisplacementMapElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::xChannelSelectorAttr || attrName == SVGNames::yChannelSelectorAttr || attrName == SVGNames::scaleAttr) {
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

RefPtr<FilterEffect> SVGFEDisplacementMapElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    auto input1 = filterBuilder->getEffectById(in1());
    auto input2 = filterBuilder->getEffectById(in2());
    
    if (!input1 || !input2)
        return nullptr;

    auto effect = FEDisplacementMap::create(filter, xChannelSelector(), yChannelSelector(), scale());
    FilterEffectVector& inputEffects = effect->inputEffects();
    inputEffects.reserveCapacity(2);
    inputEffects.append(input1);
    inputEffects.append(input2);    
    return WTFMove(effect);
}

}
