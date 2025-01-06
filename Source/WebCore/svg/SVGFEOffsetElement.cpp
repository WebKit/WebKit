/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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
#include "SVGFEOffsetElement.h"

#include "FEOffset.h"
#include "NodeName.h"
#include "SVGFilter.h"
#include "SVGNames.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGFEOffsetElement);

inline SVGFEOffsetElement::SVGFEOffsetElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feOffsetTag));
    
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEOffsetElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::dxAttr, &SVGFEOffsetElement::m_dx>();
        PropertyRegistry::registerProperty<SVGNames::dyAttr, &SVGFEOffsetElement::m_dy>();
    });
}

Ref<SVGFEOffsetElement> SVGFEOffsetElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEOffsetElement(tagName, document));
}

void SVGFEOffsetElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::dxAttr:
        Ref { m_dx }->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::dyAttr:
        Ref { m_dy }->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::inAttr:
        Ref { m_in1 }->setBaseValInternal(newValue);
        break;
    default:
        break;
    }

    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGFEOffsetElement::svgAttributeChanged(const QualifiedName& attrName)
{
    switch (attrName.nodeName()) {
    case AttributeNames::inAttr: {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        break;
    }
    case AttributeNames::dxAttr:
    case AttributeNames::dyAttr: {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        break;
    }
    default:
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        break;
    }
}

bool SVGFEOffsetElement::setFilterEffectAttribute(FilterEffect& effect, const QualifiedName& attrName)
{
    auto& offset = downcast<FEOffset>(effect);

    if (attrName == SVGNames::dxAttr)
        return offset.setDx(dx());
    if (attrName == SVGNames::dyAttr)
        return offset.setDy(dy());

    ASSERT_NOT_REACHED();
    return false;
}

bool SVGFEOffsetElement::isIdentity() const
{
    return !dx() && !dy();
}

IntOutsets SVGFEOffsetElement::outsets(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits) const
{
    auto offset = SVGFilter::calculateResolvedSize({ dx(), dy() }, targetBoundingBox, primitiveUnits);
    return FEOffset::calculateOutsets(offset);
}

RefPtr<FilterEffect> SVGFEOffsetElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    return FEOffset::create(dx(), dy());
}

} // namespace WebCore
