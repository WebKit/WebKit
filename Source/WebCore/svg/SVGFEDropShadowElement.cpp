/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "SVGFEDropShadowElement.h"

#include "NodeName.h"
#include "RenderStyle.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGRenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEDropShadowElement);

inline SVGFEDropShadowElement::SVGFEDropShadowElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feDropShadowTag));
    
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEDropShadowElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::dxAttr, &SVGFEDropShadowElement::m_dx>();
        PropertyRegistry::registerProperty<SVGNames::dyAttr, &SVGFEDropShadowElement::m_dy>();
        PropertyRegistry::registerProperty<SVGNames::stdDeviationAttr, &SVGFEDropShadowElement::m_stdDeviationX, &SVGFEDropShadowElement::m_stdDeviationY>();
    });
}

Ref<SVGFEDropShadowElement> SVGFEDropShadowElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEDropShadowElement(tagName, document));
}

void SVGFEDropShadowElement::setStdDeviation(float x, float y)
{
    m_stdDeviationX->setBaseValInternal(x);
    m_stdDeviationY->setBaseValInternal(y);
    updateSVGRendererForElementChange();
}

void SVGFEDropShadowElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::stdDeviationAttr:
        if (auto result = parseNumberOptionalNumber(newValue)) {
            m_stdDeviationX->setBaseValInternal(result->first);
            m_stdDeviationY->setBaseValInternal(result->second);
        }
        break;
    case AttributeNames::inAttr:
        m_in1->setBaseValInternal(newValue);
        break;
    case AttributeNames::dxAttr:
        m_dx->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::dyAttr:
        m_dy->setBaseValInternal(newValue.toFloat());
        break;
    default:
        break;
    }

    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGFEDropShadowElement::svgAttributeChanged(const QualifiedName& attrName)
{
    switch (attrName.nodeName()) {
    case AttributeNames::inAttr: {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        break;
    }
    case AttributeNames::stdDeviationAttr: {
        if (stdDeviationX() < 0 || stdDeviationY() < 0) {
            InstanceInvalidationGuard guard(*this);
            markFilterEffectForRebuild();
            return;
        }
        FALLTHROUGH;
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

bool SVGFEDropShadowElement::setFilterEffectAttribute(FilterEffect& filterEffect, const QualifiedName& attrName)
{
    auto& effect = downcast<FEDropShadow>(filterEffect);
    switch (attrName.nodeName()) {
    case AttributeNames::stdDeviationAttr:
        return effect.setStdDeviationX(stdDeviationX()) || effect.setStdDeviationY(stdDeviationY());
    case AttributeNames::dxAttr:
        return effect.setDx(dx());
    case AttributeNames::dyAttr:
        return effect.setDy(dy());
    case AttributeNames::flood_colorAttr: {
        auto& style = renderer()->style();
        return effect.setShadowColor(style.colorResolvingCurrentColor(style.svgStyle().floodColor()));
    }
    case AttributeNames::flood_opacityAttr:
        return effect.setShadowOpacity(renderer()->style().svgStyle().floodOpacity());
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool SVGFEDropShadowElement::isIdentity() const
{
    return !stdDeviationX() && !stdDeviationY() && !dx() && !dy();
}

IntOutsets SVGFEDropShadowElement::outsets(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits) const
{
    auto offset = SVGFilter::calculateResolvedSize({ dx(), dy() }, targetBoundingBox, primitiveUnits);
    auto stdDeviation = SVGFilter::calculateResolvedSize({ stdDeviationX(), stdDeviationY() }, targetBoundingBox, primitiveUnits);
    return FEDropShadow::calculateOutsets(offset, stdDeviation);
}

RefPtr<FilterEffect> SVGFEDropShadowElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    RenderObject* renderer = this->renderer();
    if (!renderer)
        return nullptr;

    if (stdDeviationX() < 0 || stdDeviationY() < 0)
        return nullptr;

    auto& style = renderer->style();
    const SVGRenderStyle& svgStyle = style.svgStyle();
    
    Color color = style.colorWithColorFilter(svgStyle.floodColor());
    float opacity = svgStyle.floodOpacity();

    return FEDropShadow::create(stdDeviationX(), stdDeviationY(), dx(), dy(), color, opacity);
}

} // namespace WebCore
