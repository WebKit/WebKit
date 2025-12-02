/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "SVGFilterElement.h"

#include "ContainerNodeInlines.h"
#include "LegacyRenderSVGResourceFilter.h"
#include "NodeName.h"
#include "RenderSVGResourceFilter.h"
#include "SVGElementInlines.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGParsingError.h"
#include "Settings.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGFilterElement);

inline SVGFilterElement::SVGFilterElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGURIReference(this)
{
    // Spec: If the x/y attribute is not specified, the effect is as if a value of "-10%" were specified.
    // Spec: If the width/height attribute is not specified, the effect is as if a value of "120%" were specified.
    ASSERT(hasTagName(SVGNames::filterTag));

    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::filterUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGFilterElement::m_filterUnits>();
        PropertyRegistry::registerProperty<SVGNames::primitiveUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGFilterElement::m_primitiveUnits>();
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGFilterElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGFilterElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::widthAttr, &SVGFilterElement::m_width>();
        PropertyRegistry::registerProperty<SVGNames::heightAttr, &SVGFilterElement::m_height>();
    }
}

Ref<SVGFilterElement> SVGFilterElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFilterElement(tagName, document));
}

void SVGFilterElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    auto parseError = SVGParsingError::None;

    switch (name.nodeName()) {
    case AttributeNames::filterUnitsAttr: {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(*this, newValue);
        if (propertyValue > 0)
            Ref { m_filterUnits }->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
        break;
    }
    case AttributeNames::primitiveUnitsAttr: {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(*this, newValue);
        if (propertyValue > 0)
            Ref { m_primitiveUnits }->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
        break;
    }
    case AttributeNames::xAttr:
        Ref { m_x }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError, SVGLengthNegativeValuesMode::Allow, "-10%"_s));
        break;
    case AttributeNames::yAttr:
        Ref { m_y }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError, SVGLengthNegativeValuesMode::Allow, "-10%"_s));
        break;
    case AttributeNames::widthAttr:
        Ref { m_width }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError, SVGLengthNegativeValuesMode::Allow, "120%"_s));
        break;
    case AttributeNames::heightAttr:
        Ref { m_height }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError, SVGLengthNegativeValuesMode::Allow, "120%"_s));
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGURIReference::parseAttribute(name, newValue);
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGFilterElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isAnimatedLengthAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        setPresentationalHintStyleIsDirty();
        return;
    }

    if (PropertyRegistry::isKnownAttribute(attrName) || SVGURIReference::isKnownAttribute(attrName)) {
        updateSVGRendererForElementChange();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

void SVGFilterElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChange::Source::Parser)
        return;

    if (document().settings().layerBasedSVGEngineEnabled()) {
        if (CheckedPtr filterRenderer = dynamicDowncast<RenderSVGResourceFilter>(renderer()))
            filterRenderer->invalidateFilter();
        return;
    }

    updateSVGRendererForElementChange();
}

RenderPtr<RenderElement> SVGFilterElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGResourceFilter>(*this, WTFMove(style));

    return createRenderer<LegacyRenderSVGResourceFilter>(*this, WTFMove(style));
}

bool SVGFilterElement::childShouldCreateRenderer(const Node& child) const
{
    using namespace ElementNames;

    auto* childElement = dynamicDowncast<SVGElement>(child);
    if (!childElement)
        return false;

    switch (childElement->elementName()) {
    case SVG::feBlend:
    case SVG::feColorMatrix:
    case SVG::feComponentTransfer:
    case SVG::feComposite:
    case SVG::feConvolveMatrix:
    case SVG::feDiffuseLighting:
    case SVG::feDisplacementMap:
    case SVG::feDistantLight:
    case SVG::feDropShadow:
    case SVG::feFlood:
    case SVG::feFuncA:
    case SVG::feFuncB:
    case SVG::feFuncG:
    case SVG::feFuncR:
    case SVG::feGaussianBlur:
    case SVG::feImage:
    case SVG::feMerge:
    case SVG::feMergeNode:
    case SVG::feMorphology:
    case SVG::feOffset:
    case SVG::fePointLight:
    case SVG::feSpecularLighting:
    case SVG::feSpotLight:
    case SVG::feTile:
    case SVG::feTurbulence:
        return true;
    default:
        break;
    }

    return false;
}

}
