/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGGradientElement.h"

#include "ElementIterator.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGResourceLinearGradient.h"
#include "RenderSVGResourceRadialGradient.h"
#include "SVGNames.h"
#include "SVGStopElement.h"
#include "SVGTransformable.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGGradientElement);

SVGGradientElement::SVGGradientElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , SVGURIReference(this)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::spreadMethodAttr, SVGSpreadMethodType, &SVGGradientElement::m_spreadMethod>();
        PropertyRegistry::registerProperty<SVGNames::gradientUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGGradientElement::m_gradientUnits>();
        PropertyRegistry::registerProperty<SVGNames::gradientTransformAttr, &SVGGradientElement::m_gradientTransform>();
    });
}

void SVGGradientElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::gradientUnitsAttr) {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            m_gradientUnits->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
        return;
    }

    if (name == SVGNames::gradientTransformAttr) {
        m_gradientTransform->baseVal()->parse(value);
        return;
    }

    if (name == SVGNames::spreadMethodAttr) {
        auto propertyValue = SVGPropertyTraits<SVGSpreadMethodType>::fromString(value);
        if (propertyValue > 0)
            m_spreadMethod->setBaseValInternal<SVGSpreadMethodType>(propertyValue);
        return;
    }

    SVGElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
}

void SVGGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName) || SVGURIReference::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        if (RenderObject* object = renderer())
            object->setNeedsLayout();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}
    
void SVGGradientElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChangeSource::Parser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

Gradient::ColorStopVector SVGGradientElement::buildStops()
{
    Gradient::ColorStopVector stops;
    float previousOffset = 0.0f;
    for (auto& stop : childrenOfType<SVGStopElement>(*this)) {
        auto monotonicallyIncreasingOffset = std::clamp(stop.offset(), previousOffset, 1.0f);
        previousOffset = monotonicallyIncreasingOffset;
        stops.append({ monotonicallyIncreasingOffset, stop.stopColorIncludingOpacity() });
    }
    return stops;
}

}
