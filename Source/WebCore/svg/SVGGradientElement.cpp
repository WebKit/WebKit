/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2023 Igalia S.L.
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
#include "SVGGradientElement.h"

#include "ElementChildIteratorInlines.h"
#include "LegacyRenderSVGResourceLinearGradient.h"
#include "LegacyRenderSVGResourceRadialGradient.h"
#include "NodeName.h"
#include "RenderSVGResourceGradient.h"
#include "SVGElementTypeHelpers.h"
#include "SVGStopElement.h"
#include "SVGTransformable.h"
#include "StyleResolver.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGGradientElement);

SVGGradientElement::SVGGradientElement(const QualifiedName& tagName, Document& document, UniqueRef<SVGPropertyRegistry>&& propertyRegistry)
    : SVGElement(tagName, document, WTFMove(propertyRegistry))
    , SVGURIReference(this)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::spreadMethodAttr, SVGSpreadMethodType, &SVGGradientElement::m_spreadMethod>();
        PropertyRegistry::registerProperty<SVGNames::gradientUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGGradientElement::m_gradientUnits>();
        PropertyRegistry::registerProperty<SVGNames::gradientTransformAttr, &SVGGradientElement::m_gradientTransform>();
    });
}

void SVGGradientElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::gradientUnitsAttr: {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(newValue);
        if (propertyValue > 0)
            Ref { m_gradientUnits }->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
        break;
    }
    case AttributeNames::gradientTransformAttr:
        Ref { m_gradientTransform }->baseVal()->parse(newValue);
        break;
    case AttributeNames::spreadMethodAttr: {
        auto propertyValue = SVGPropertyTraits<SVGSpreadMethodType>::fromString(newValue);
        if (propertyValue > 0)
            Ref { m_spreadMethod }->setBaseValInternal<SVGSpreadMethodType>(propertyValue);
        break;
    }
    default:
        break;
    }

    SVGURIReference::parseAttribute(name, newValue);
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGGradientElement::invalidateGradientResource()
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        if (CheckedPtr gradientRenderer = dynamicDowncast<RenderSVGResourceGradient>(renderer()))
            gradientRenderer->invalidateGradient();
        return;
    }

    updateSVGRendererForElementChange();
}

void SVGGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName) || SVGURIReference::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        invalidateGradientResource();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}
    
void SVGGradientElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);
    if (change.source == ChildChange::Source::Parser)
        return;

    invalidateGradientResource();
}

GradientColorStops SVGGradientElement::buildStops()
{
    GradientColorStops stops;
    float previousOffset = 0.0f;
    for (auto& stop : childrenOfType<SVGStopElement>(*this)) {
        auto monotonicallyIncreasingOffset = std::clamp(stop.offset(), previousOffset, 1.0f);
        previousOffset = monotonicallyIncreasingOffset;
        stops.addColorStop({ monotonicallyIncreasingOffset, stop.stopColorIncludingOpacity() });
    }
    return stops;
}

}
