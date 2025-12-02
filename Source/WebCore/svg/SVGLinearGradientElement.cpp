/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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
#include "SVGLinearGradientElement.h"

#include "ContainerNodeInlines.h"
#include "Document.h"
#include "FloatPoint.h"
#include "LegacyRenderSVGResourceLinearGradient.h"
#include "LinearGradientAttributes.h"
#include "NodeName.h"
#include "RenderSVGResourceLinearGradient.h"
#include "SVGElementTypeHelpers.h"
#include "SVGLengthValue.h"
#include "SVGNames.h"
#include "SVGParsingError.h"
#include "SVGUnitTypes.h"
#include "Settings.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGLinearGradientElement);

inline SVGLinearGradientElement::SVGLinearGradientElement(const QualifiedName& tagName, Document& document)
    : SVGGradientElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    // Spec: If the x2 attribute is not specified, the effect is as if a value of "100%" were specified.
    ASSERT(hasTagName(SVGNames::linearGradientTag));

    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::x1Attr, &SVGLinearGradientElement::m_x1>();
        PropertyRegistry::registerProperty<SVGNames::y1Attr, &SVGLinearGradientElement::m_y1>();
        PropertyRegistry::registerProperty<SVGNames::x2Attr, &SVGLinearGradientElement::m_x2>();
        PropertyRegistry::registerProperty<SVGNames::y2Attr, &SVGLinearGradientElement::m_y2>();
    }
}

Ref<SVGLinearGradientElement> SVGLinearGradientElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGLinearGradientElement(tagName, document));
}

void SVGLinearGradientElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    auto parseError = SVGParsingError::None;

    switch (name.nodeName()) {
    case AttributeNames::x1Attr:
        Ref { m_x1 }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError, SVGLengthNegativeValuesMode::Allow, "0%"_s));
        break;
    case AttributeNames::y1Attr:
        Ref { m_y1 }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError, SVGLengthNegativeValuesMode::Allow, "0%"_s));
        break;
    case AttributeNames::x2Attr:
        Ref { m_x2 }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError, SVGLengthNegativeValuesMode::Allow, "100%"_s));
        break;
    case AttributeNames::y2Attr:
        Ref { m_y2 }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError, SVGLengthNegativeValuesMode::Allow, "0%"_s));
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGGradientElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGLinearGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        updateRelativeLengthsInformation();
        invalidateGradientResource();
        return;
    }

    SVGGradientElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGLinearGradientElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGResourceLinearGradient>(*this, WTFMove(style));
    return createRenderer<LegacyRenderSVGResourceLinearGradient>(*this, WTFMove(style));
}

static void setGradientAttributes(SVGGradientElement& element, LinearGradientAttributes& attributes, bool isLinear = true)
{
    if (!attributes.hasSpreadMethod() && element.hasAttribute(SVGNames::spreadMethodAttr))
        attributes.setSpreadMethod(element.spreadMethod());

    if (!attributes.hasGradientUnits() && element.hasAttribute(SVGNames::gradientUnitsAttr))
        attributes.setGradientUnits(element.gradientUnits());

    if (!attributes.hasGradientTransform() && element.hasAttribute(SVGNames::gradientTransformAttr))
        attributes.setGradientTransform(element.gradientTransform().concatenate());

    if (!attributes.hasStops())
        attributes.setStops(element.buildStops());

    if (isLinear) {
        SVGLinearGradientElement& linear = downcast<SVGLinearGradientElement>(element);

        if (!attributes.hasX1() && element.hasAttribute(SVGNames::x1Attr))
            attributes.setX1(linear.x1());

        if (!attributes.hasY1() && element.hasAttribute(SVGNames::y1Attr))
            attributes.setY1(linear.y1());

        if (!attributes.hasX2() && element.hasAttribute(SVGNames::x2Attr))
            attributes.setX2(linear.x2());

        if (!attributes.hasY2() && element.hasAttribute(SVGNames::y2Attr))
            attributes.setY2(linear.y2());
    }
}

bool SVGLinearGradientElement::collectGradientAttributes(LinearGradientAttributes& attributes)
{
    if (!renderer())
        return false;

    HashSet<Ref<SVGGradientElement>> processedGradients;
    Ref<SVGGradientElement> current { *this };

    setGradientAttributes(current.get(), attributes);
    processedGradients.add(current.copyRef());

    while (true) {
        // Respect xlink:href, take attributes from referenced element
        auto target = SVGURIReference::targetElementFromIRIString(current->href(), treeScopeForSVGReferences());
        if (auto* gradientElement = dynamicDowncast<SVGGradientElement>(target.element.get())) {
            current = *gradientElement;

            // Cycle detection
            if (processedGradients.contains(current))
                return true;

            if (!current->renderer())
                return false;

            setGradientAttributes(current.get(), attributes, current->hasTagName(SVGNames::linearGradientTag));
            processedGradients.add(current.copyRef());
        } else
            return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool SVGLinearGradientElement::selfHasRelativeLengths() const
{
    return x1().isRelative()
        || y1().isRelative()
        || x2().isRelative()
        || y2().isRelative();
}

}
