/*
 * Copyright (C) 2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>
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
#include "SVGFEDiffuseLightingElement.h"

#include "FEDiffuseLighting.h"
#include "RenderStyle.h"
#include "SVGFELightElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEDiffuseLightingElement);

inline SVGFEDiffuseLightingElement::SVGFEDiffuseLightingElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feDiffuseLightingTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEDiffuseLightingElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::diffuseConstantAttr, &SVGFEDiffuseLightingElement::m_diffuseConstant>();
        PropertyRegistry::registerProperty<SVGNames::surfaceScaleAttr, &SVGFEDiffuseLightingElement::m_surfaceScale>();
        PropertyRegistry::registerProperty<SVGNames::kernelUnitLengthAttr, &SVGFEDiffuseLightingElement::m_kernelUnitLengthX, &SVGFEDiffuseLightingElement::m_kernelUnitLengthY>();
    });
}

Ref<SVGFEDiffuseLightingElement> SVGFEDiffuseLightingElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEDiffuseLightingElement(tagName, document));
}

void SVGFEDiffuseLightingElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    if (name == SVGNames::surfaceScaleAttr) {
        m_surfaceScale->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::diffuseConstantAttr) {
        m_diffuseConstant->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::kernelUnitLengthAttr) {
        if (auto result = parseNumberOptionalNumber(value)) {
            m_kernelUnitLengthX->setBaseValInternal(result->first);
            m_kernelUnitLengthY->setBaseValInternal(result->second);
        }
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFEDiffuseLightingElement::setFilterEffectAttribute(FilterEffect& effect, const QualifiedName& attrName)
{
    auto& feDiffuseLighting = downcast<FEDiffuseLighting>(effect);

    if (attrName == SVGNames::lighting_colorAttr) {
        RenderObject* renderer = this->renderer();
        ASSERT(renderer);
        auto& style = renderer->style();
        Color color = style.colorWithColorFilter(style.svgStyle().lightingColor());
        return feDiffuseLighting.setLightingColor(color);
    }
    if (attrName == SVGNames::surfaceScaleAttr)
        return feDiffuseLighting.setSurfaceScale(surfaceScale());
    if (attrName == SVGNames::diffuseConstantAttr)
        return feDiffuseLighting.setDiffuseConstant(diffuseConstant());

    auto& lightSource = const_cast<LightSource&>(feDiffuseLighting.lightSource());
    const SVGFELightElement* lightElement = SVGFELightElement::findLightElement(this);
    ASSERT(lightElement);

    if (attrName == SVGNames::azimuthAttr)
        return lightSource.setAzimuth(lightElement->azimuth());
    if (attrName == SVGNames::elevationAttr)
        return lightSource.setElevation(lightElement->elevation());
    if (attrName == SVGNames::xAttr)
        return lightSource.setX(lightElement->x());
    if (attrName == SVGNames::yAttr)
        return lightSource.setY(lightElement->y());
    if (attrName == SVGNames::zAttr)
        return lightSource.setZ(lightElement->z());
    if (attrName == SVGNames::pointsAtXAttr)
        return lightSource.setPointsAtX(lightElement->pointsAtX());
    if (attrName == SVGNames::pointsAtYAttr)
        return lightSource.setPointsAtY(lightElement->pointsAtY());
    if (attrName == SVGNames::pointsAtZAttr)
        return lightSource.setPointsAtZ(lightElement->pointsAtZ());
    if (attrName == SVGNames::specularExponentAttr)
        return lightSource.setSpecularExponent(lightElement->specularExponent());
    if (attrName == SVGNames::limitingConeAngleAttr)
        return lightSource.setLimitingConeAngle(lightElement->limitingConeAngle());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEDiffuseLightingElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::inAttr) {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        return;
    }

    if (attrName == SVGNames::diffuseConstantAttr || attrName == SVGNames::surfaceScaleAttr || attrName == SVGNames::kernelUnitLengthAttr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

void SVGFEDiffuseLightingElement::lightElementAttributeChanged(const SVGFELightElement* lightElement, const QualifiedName& attrName)
{
    if (SVGFELightElement::findLightElement(this) != lightElement)
        return;

    // The light element has different attribute names.
    primitiveAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEDiffuseLightingElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    RefPtr lightElement = SVGFELightElement::findLightElement(this);
    if (!lightElement)
        return nullptr;

    auto* renderer = this->renderer();
    if (!renderer)
        return nullptr;

    auto lightSource = lightElement->lightSource();
    auto& style = renderer->style();

    Color color = style.colorWithColorFilter(style.svgStyle().lightingColor());

    return FEDiffuseLighting::create(color, surfaceScale(), diffuseConstant(), kernelUnitLengthX(), kernelUnitLengthY(), WTFMove(lightSource));
}

} // namespace WebCore
