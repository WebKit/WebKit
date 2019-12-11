/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
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
#include "SVGFESpecularLightingElement.h"

#include "FilterEffect.h"
#include "RenderStyle.h"
#include "SVGFELightElement.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFESpecularLightingElement);

inline SVGFESpecularLightingElement::SVGFESpecularLightingElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feSpecularLightingTag));
    
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFESpecularLightingElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::specularConstantAttr, &SVGFESpecularLightingElement::m_specularConstant>();
        PropertyRegistry::registerProperty<SVGNames::specularExponentAttr, &SVGFESpecularLightingElement::m_specularExponent>();
        PropertyRegistry::registerProperty<SVGNames::surfaceScaleAttr, &SVGFESpecularLightingElement::m_surfaceScale>();
        PropertyRegistry::registerProperty<SVGNames::kernelUnitLengthAttr, &SVGFESpecularLightingElement::m_kernelUnitLengthX, &SVGFESpecularLightingElement::m_kernelUnitLengthY>();
    });
}

Ref<SVGFESpecularLightingElement> SVGFESpecularLightingElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFESpecularLightingElement(tagName, document));
}

void SVGFESpecularLightingElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    if (name == SVGNames::surfaceScaleAttr) {
        m_surfaceScale->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::specularConstantAttr) {
        m_specularConstant->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::specularExponentAttr) {
        m_specularExponent->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::kernelUnitLengthAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            m_kernelUnitLengthX->setBaseValInternal(x);
            m_kernelUnitLengthY->setBaseValInternal(y);
        }
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFESpecularLightingElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FESpecularLighting* specularLighting = static_cast<FESpecularLighting*>(effect);

    if (attrName == SVGNames::lighting_colorAttr) {
        RenderObject* renderer = this->renderer();
        ASSERT(renderer);
        Color color = renderer->style().colorByApplyingColorFilter(renderer->style().svgStyle().lightingColor());
        return specularLighting->setLightingColor(color);
    }
    if (attrName == SVGNames::surfaceScaleAttr)
        return specularLighting->setSurfaceScale(surfaceScale());
    if (attrName == SVGNames::specularConstantAttr)
        return specularLighting->setSpecularConstant(specularConstant());
    if (attrName == SVGNames::specularExponentAttr)
        return specularLighting->setSpecularExponent(specularExponent());

    auto& lightSource = const_cast<LightSource&>(specularLighting->lightSource());
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

void SVGFESpecularLightingElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::surfaceScaleAttr || attrName == SVGNames::specularConstantAttr || attrName == SVGNames::specularExponentAttr || attrName == SVGNames::kernelUnitLengthAttr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

void SVGFESpecularLightingElement::lightElementAttributeChanged(const SVGFELightElement* lightElement, const QualifiedName& attrName)
{
    if (SVGFELightElement::findLightElement(this) != lightElement)
        return;

    // The light element has different attribute names so attrName can identify the requested attribute.
    primitiveAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFESpecularLightingElement::build(SVGFilterBuilder* filterBuilder, Filter& filter) const
{
    auto input1 = filterBuilder->getEffectById(in1());

    if (!input1)
        return nullptr;

    auto lightElement = makeRefPtr(SVGFELightElement::findLightElement(this));
    if (!lightElement)
        return nullptr;
    
    auto lightSource = lightElement->lightSource(*filterBuilder);

    RenderObject* renderer = this->renderer();
    if (!renderer)
        return nullptr;
    
    Color color = renderer->style().colorByApplyingColorFilter(renderer->style().svgStyle().lightingColor());

    auto effect = FESpecularLighting::create(filter, color, surfaceScale(), specularConstant(), specularExponent(), kernelUnitLengthX(), kernelUnitLengthY(), WTFMove(lightSource));
    effect->inputEffects().append(input1);
    return effect;
}

}
