/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGRadialGradientElement.h"

#include "FloatConversion.h"
#include "FloatPoint.h"
#include "LegacyRenderSVGResourceRadialGradient.h"
#include "NodeName.h"
#include "RadialGradientAttributes.h"
#include "RenderSVGResourceRadialGradient.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGStopElement.h"
#include "SVGUnitTypes.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGRadialGradientElement);

inline SVGRadialGradientElement::SVGRadialGradientElement(const QualifiedName& tagName, Document& document)
    : SVGGradientElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    // Spec: If the cx/cy/r/fr attribute is not specified, the effect is as if a value of "50%" were specified.
    ASSERT(hasTagName(SVGNames::radialGradientTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::cxAttr, &SVGRadialGradientElement::m_cx>();
        PropertyRegistry::registerProperty<SVGNames::cyAttr, &SVGRadialGradientElement::m_cy>();
        PropertyRegistry::registerProperty<SVGNames::rAttr, &SVGRadialGradientElement::m_r>();
        PropertyRegistry::registerProperty<SVGNames::fxAttr, &SVGRadialGradientElement::m_fx>();
        PropertyRegistry::registerProperty<SVGNames::fyAttr, &SVGRadialGradientElement::m_fy>();
        PropertyRegistry::registerProperty<SVGNames::frAttr, &SVGRadialGradientElement::m_fr>();
    });
}

Ref<SVGRadialGradientElement> SVGRadialGradientElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGRadialGradientElement(tagName, document));
}

void SVGRadialGradientElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGParsingError parseError = NoError;

    switch (name.nodeName()) {
    case AttributeNames::cxAttr:
        m_cx->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::cyAttr:
        m_cy->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::rAttr:
        m_r->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Other, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    case AttributeNames::fxAttr:
        m_fx->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::fyAttr:
        m_fy->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::frAttr:
        m_fr->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Other, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);
    SVGGradientElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGRadialGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        updateRelativeLengthsInformation();
        invalidateGradientResource();
        return;
    }

    SVGGradientElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGRadialGradientElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGResourceRadialGradient>(*this, WTFMove(style));
#endif
    return createRenderer<LegacyRenderSVGResourceRadialGradient>(*this, WTFMove(style));
}

static void setGradientAttributes(SVGGradientElement& element, RadialGradientAttributes& attributes, bool isRadial = true)
{
    if (!attributes.hasSpreadMethod() && element.hasAttribute(SVGNames::spreadMethodAttr))
        attributes.setSpreadMethod(element.spreadMethod());

    if (!attributes.hasGradientUnits() && element.hasAttribute(SVGNames::gradientUnitsAttr))
        attributes.setGradientUnits(element.gradientUnits());

    if (!attributes.hasGradientTransform() && element.hasAttribute(SVGNames::gradientTransformAttr))
        attributes.setGradientTransform(element.gradientTransform().concatenate());

    if (!attributes.hasStops())
        attributes.setStops(element.buildStops());

    if (isRadial) {
        SVGRadialGradientElement& radial = downcast<SVGRadialGradientElement>(element);

        if (!attributes.hasCx() && element.hasAttribute(SVGNames::cxAttr))
            attributes.setCx(radial.cx());

        if (!attributes.hasCy() && element.hasAttribute(SVGNames::cyAttr))
            attributes.setCy(radial.cy());

        if (!attributes.hasR() && element.hasAttribute(SVGNames::rAttr))
            attributes.setR(radial.r());

        if (!attributes.hasFx() && element.hasAttribute(SVGNames::fxAttr))
            attributes.setFx(radial.fx());

        if (!attributes.hasFy() && element.hasAttribute(SVGNames::fyAttr))
            attributes.setFy(radial.fy());

        if (!attributes.hasFr() && element.hasAttribute(SVGNames::frAttr))
            attributes.setFr(radial.fr());
    }
}

bool SVGRadialGradientElement::collectGradientAttributes(RadialGradientAttributes& attributes)
{
    if (!renderer())
        return false;

    HashSet<RefPtr<SVGGradientElement>> processedGradients;
    SVGGradientElement* current = this;

    setGradientAttributes(*current, attributes);
    processedGradients.add(current);

    while (true) {
        // Respect xlink:href, take attributes from referenced element
        auto target = SVGURIReference::targetElementFromIRIString(current->href(), treeScopeForSVGReferences());
        if (auto* gradientElement = dynamicDowncast<SVGGradientElement>(target.element.get())) {
            current = gradientElement;

            // Cycle detection
            if (processedGradients.contains(current))
                break;

            if (!current->renderer())
                return false;

            setGradientAttributes(*current, attributes, current->hasTagName(SVGNames::radialGradientTag));
            processedGradients.add(current);
        } else
            break;
    }

    // Handle default values for fx/fy
    if (!attributes.hasFx())
        attributes.setFx(attributes.cx());

    if (!attributes.hasFy())
        attributes.setFy(attributes.cy());

    return true;
}

bool SVGRadialGradientElement::selfHasRelativeLengths() const
{
    return cx().isRelative()
        || cy().isRelative()
        || r().isRelative()
        || fx().isRelative()
        || fy().isRelative()
        || fr().isRelative();
}

}
