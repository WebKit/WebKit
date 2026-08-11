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
#include "SVGFEColorMatrixElement.h"

#include "FEColorMatrix.h"
#include "NodeName.h"
#include "SVGNames.h"
#include "SVGPropertyOwnerRegistry.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGFEColorMatrixElement);

inline SVGFEColorMatrixElement::SVGFEColorMatrixElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feColorMatrixTag));

    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEColorMatrixElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::typeAttr, ColorMatrixType, &SVGFEColorMatrixElement::m_type>();
        PropertyRegistry::registerProperty<SVGNames::valuesAttr, &SVGFEColorMatrixElement::m_values>();
    }
}

Ref<SVGFEColorMatrixElement> SVGFEColorMatrixElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEColorMatrixElement(tagName, document));
}

void SVGFEColorMatrixElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::typeAttr: {
        auto propertyValue = SVGPropertyTraits<ColorMatrixType>::fromString(*this, newValue);
        if (std::to_underlying(propertyValue))
            Ref { m_type }->setBaseValInternal<ColorMatrixType>(propertyValue);
        break;
    }
    case AttributeNames::inAttr:
        Ref { m_in1 }->setBaseValInternal(newValue);
        break;
    case AttributeNames::valuesAttr:
        protect(m_values)->baseVal()->parse(newValue);
        break;
    default:
        break;
    }

    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

bool SVGFEColorMatrixElement::setFilterEffectAttribute(FilterEffect& effect, const QualifiedName& attrName)
{
    auto& feColorMatrix = downcast<FEColorMatrix>(effect);

    if (attrName == SVGNames::typeAttr)
        return feColorMatrix.setType(type());
    if (attrName == SVGNames::valuesAttr)
        return feColorMatrix.setValues(values());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEColorMatrixElement::svgAttributeChanged(const QualifiedName& attrName)
{
    switch (attrName.nodeName()) {
    case AttributeNames::inAttr: {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        break;
    }
    case AttributeNames::typeAttr:
    case AttributeNames::valuesAttr: {
        InstanceInvalidationGuard guard(*this);
        if (!FEColorMatrix::areValuesValidForType(type(), values()))
            markFilterEffectForRebuild();
        else
            primitiveAttributeChanged(attrName);
        break;
    }
    default:
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        break;
    }
}

RefPtr<FilterEffect> SVGFEColorMatrixElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    Vector<float> filterValues;
    ColorMatrixType filterType = type();

    // Use defaults if values is empty (SVG 1.1 15.10).
    if (!hasAttribute(SVGNames::valuesAttr)) {
        switch (filterType) {
        case ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX: {
            static constexpr unsigned matrixValueCount = 20;
            filterValues = Vector<float>(matrixValueCount, [](size_t i) {
                return (i % 6) ? 0.0 : 1.0;
            });
            break;
        }
        case ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE:
            filterValues = { 0 };
            break;
        case ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE:
            filterValues = { 1 };
            break;
        default:
            break;
        }
    } else {
        filterValues = values();
        filterValues.shrinkToFit();

        if (!FEColorMatrix::areValuesValidForType(type(), filterValues))
            return nullptr;
    }

    return FEColorMatrix::create(filterType, WTF::move(filterValues));
}

} // namespace WebCore
