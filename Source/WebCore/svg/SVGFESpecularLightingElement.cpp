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
    registerAttributes();
}

Ref<SVGFESpecularLightingElement> SVGFESpecularLightingElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFESpecularLightingElement(tagName, document));
}

const AtomicString& SVGFESpecularLightingElement::kernelUnitLengthXIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGKernelUnitLengthX", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

const AtomicString& SVGFESpecularLightingElement::kernelUnitLengthYIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGKernelUnitLengthY", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

void SVGFESpecularLightingElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::inAttr, &SVGFESpecularLightingElement::m_in1>();
    registry.registerAttribute<SVGNames::specularConstantAttr, &SVGFESpecularLightingElement::m_specularConstant>();
    registry.registerAttribute<SVGNames::specularExponentAttr, &SVGFESpecularLightingElement::m_specularExponent>();
    registry.registerAttribute<SVGNames::surfaceScaleAttr, &SVGFESpecularLightingElement::m_surfaceScale>();
    registry.registerAttribute<SVGNames::kernelUnitLengthAttr,
        &SVGFESpecularLightingElement::kernelUnitLengthXIdentifier, &SVGFESpecularLightingElement::m_kernelUnitLengthX,
        &SVGFESpecularLightingElement::kernelUnitLengthYIdentifier, &SVGFESpecularLightingElement::m_kernelUnitLengthY>();
}

void SVGFESpecularLightingElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::inAttr) {
        m_in1.setValue(value);
        return;
    }

    if (name == SVGNames::surfaceScaleAttr) {
        m_surfaceScale.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::specularConstantAttr) {
        m_specularConstant.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::specularExponentAttr) {
        m_specularExponent.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::kernelUnitLengthAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            m_kernelUnitLengthX.setValue(x);
            m_kernelUnitLengthY.setValue(y);
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

RefPtr<FilterEffect> SVGFESpecularLightingElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
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
    return WTFMove(effect);
}

}
