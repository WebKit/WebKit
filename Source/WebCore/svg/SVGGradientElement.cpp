/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "SVGGradientElement.h"

#include "ElementIterator.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGResourceLinearGradient.h"
#include "RenderSVGResourceRadialGradient.h"
#include "SVGNames.h"
#include "SVGStopElement.h"
#include "SVGTransformListValues.h"
#include "SVGTransformable.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGGradientElement);

SVGGradientElement::SVGGradientElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , SVGExternalResourcesRequired(this)
    , SVGURIReference(this)
{
    registerAttributes();
}

void SVGGradientElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::spreadMethodAttr, SVGSpreadMethodType, &SVGGradientElement::m_spreadMethod>();
    registry.registerAttribute<SVGNames::gradientUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGGradientElement::m_gradientUnits>();
    registry.registerAttribute<SVGNames::gradientTransformAttr, &SVGGradientElement::m_gradientTransform>();
}

void SVGGradientElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::gradientUnitsAttr) {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            m_gradientUnits.setValue(propertyValue);
        return;
    }

    if (name == SVGNames::gradientTransformAttr) {
        SVGTransformListValues newList;
        newList.parse(value);
        m_gradientTransform.detachAnimatedListWrappers(attributeOwnerProxy(), newList.size());
        m_gradientTransform.setValue(WTFMove(newList));
        return;
    }

    if (name == SVGNames::spreadMethodAttr) {
        auto propertyValue = SVGPropertyTraits<SVGSpreadMethodType>::fromString(value);
        if (propertyValue > 0)
            m_spreadMethod.setValue(propertyValue);
        return;
    }

    SVGElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (isKnownAttribute(attrName) || SVGURIReference::isKnownAttribute(attrName)) {
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

Vector<Gradient::ColorStop> SVGGradientElement::buildStops()
{
    Vector<Gradient::ColorStop> stops;
    float previousOffset = 0.0f;

    for (auto& stop : childrenOfType<SVGStopElement>(*this)) {
        const Color& color = stop.stopColorIncludingOpacity();

        // Figure out right monotonic offset.
        float offset = stop.offset();
        offset = std::min(std::max(previousOffset, offset), 1.0f);
        previousOffset = offset;

        stops.append(Gradient::ColorStop(offset, color));
    }

    return stops;
}

}
