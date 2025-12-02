/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2015 Google Inc. All rights reserved.
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
#include "SVGFEMorphologyElement.h"

#include "FEMorphology.h"
#include "NodeName.h"
#include "SVGFilterRenderer.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGPropertyOwnerRegistry.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGFEMorphologyElement);

inline SVGFEMorphologyElement::SVGFEMorphologyElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feMorphologyTag));
    
    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEMorphologyElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::operatorAttr, MorphologyOperatorType, &SVGFEMorphologyElement::m_svgOperator>();
        PropertyRegistry::registerProperty<SVGNames::radiusAttr, &SVGFEMorphologyElement::m_radiusX, &SVGFEMorphologyElement::m_radiusY>();
    }
}

Ref<SVGFEMorphologyElement> SVGFEMorphologyElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEMorphologyElement(tagName, document));
}

void SVGFEMorphologyElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::operatorAttr: {
        MorphologyOperatorType propertyValue = SVGPropertyTraits<MorphologyOperatorType>::fromString(*this, newValue);
        if (propertyValue != MorphologyOperatorType::Unknown)
            Ref { m_svgOperator }->setBaseValInternal<MorphologyOperatorType>(propertyValue);
        break;
    }
    case AttributeNames::inAttr:
        Ref { m_in1 }->setBaseValInternal(newValue);
        break;
    case AttributeNames::radiusAttr:
        if (auto result = parseNumberOptionalNumber(newValue)) {
            Ref { m_radiusX }->setBaseValInternal(result->first);
            Ref { m_radiusY }->setBaseValInternal(result->second);
        }
        break;
    default:
        break;
    }

    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

bool SVGFEMorphologyElement::setFilterEffectAttribute(FilterEffect& effect, const QualifiedName& attrName)
{
    auto& feMorphology = downcast<FEMorphology>(effect);
    if (attrName == SVGNames::operatorAttr)
        return feMorphology.setMorphologyOperator(svgOperator());
    if (attrName == SVGNames::radiusAttr) {
        // Both setRadius functions should be evaluated separately.
        bool isRadiusXChanged = feMorphology.setRadiusX(radiusX());
        bool isRadiusYChanged = feMorphology.setRadiusY(radiusY());
        return isRadiusXChanged || isRadiusYChanged;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEMorphologyElement::svgAttributeChanged(const QualifiedName& attrName)
{
    switch (attrName.nodeName()) {
    case AttributeNames::inAttr: {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        break;
    }
    case AttributeNames::operatorAttr:
    case AttributeNames::radiusAttr: {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        break;
    }
    default:
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        break;
    }
}

bool SVGFEMorphologyElement::isIdentity() const
{
    return !std::max(0.0f, radiusX()) && !std::max(0.0f, radiusY());
}

IntOutsets SVGFEMorphologyElement::outsets(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits) const
{
    auto radius = SVGFilterRenderer::calculateResolvedSize({ radiusX(), radiusY() }, targetBoundingBox, primitiveUnits);
    return { static_cast<int>(radius.height()), static_cast<int>(radius.width()), static_cast<int>(radius.height()), static_cast<int>(radius.width()) };
}

RefPtr<FilterEffect> SVGFEMorphologyElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    // "A negative or zero value disables the effect of the given filter
    // primitive (i.e., the result is the filter input image)."
    // https://drafts.fxtf.org/filters/#element-attrdef-femorphology-radius
    // This is handled by FEMorphology.
    return FEMorphology::create(svgOperator(), radiusX(), radiusY());
}

} // namespace WebCore
