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

void SVGFEDropShadowElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::stdDeviationAttr) {
        if (auto result = parseNumberOptionalNumber(value)) {
            m_stdDeviationX->setBaseValInternal(result->first);
            m_stdDeviationY->setBaseValInternal(result->second);
        }
        return;
    }

    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    if (name == SVGNames::dxAttr) {
        m_dx->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::dyAttr) {
        m_dy->setBaseValInternal(value.toFloat());
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

void SVGFEDropShadowElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::inAttr) {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        return;
    }

    if (attrName == SVGNames::stdDeviationAttr && (stdDeviationX() < 0 || stdDeviationY() < 0)) {
        InstanceInvalidationGuard guard(*this);
        markFilterEffectForRebuild();
        return;
    }

    if (attrName == SVGNames::stdDeviationAttr || attrName == SVGNames::dxAttr || attrName == SVGNames::dyAttr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

bool SVGFEDropShadowElement::setFilterEffectAttribute(FilterEffect& effect, const QualifiedName& attrName)
{
    auto& feDropShadow = downcast<FEDropShadow>(effect);

    if (attrName == SVGNames::stdDeviationAttr) {
        bool stdDeviationXChanged = feDropShadow.setStdDeviationX(stdDeviationX());
        bool stdDeviationYChanged = feDropShadow.setStdDeviationY(stdDeviationY());
        return stdDeviationXChanged || stdDeviationYChanged;
    }

    if (attrName == SVGNames::dxAttr)
        return feDropShadow.setDx(dx());
    if (attrName == SVGNames::dyAttr)
        return feDropShadow.setDy(dy());

    RenderObject* renderer = this->renderer();
    ASSERT(renderer);
    const RenderStyle& style = renderer->style();

    if (attrName == SVGNames::flood_colorAttr)
        return feDropShadow.setShadowColor(style.colorResolvingCurrentColor(style.svgStyle().floodColor()));
    if (attrName == SVGNames::flood_opacityAttr)
        return feDropShadow.setShadowOpacity(style.svgStyle().floodOpacity());

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
