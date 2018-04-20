/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "XLinkNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGGradientElement);

// Animated property definitions
DEFINE_ANIMATED_ENUMERATION(SVGGradientElement, SVGNames::spreadMethodAttr, SpreadMethod, spreadMethod, SVGSpreadMethodType)
DEFINE_ANIMATED_ENUMERATION(SVGGradientElement, SVGNames::gradientUnitsAttr, GradientUnits, gradientUnits, SVGUnitTypes::SVGUnitType)
DEFINE_ANIMATED_TRANSFORM_LIST(SVGGradientElement, SVGNames::gradientTransformAttr, GradientTransform, gradientTransform)
DEFINE_ANIMATED_STRING(SVGGradientElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGGradientElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGGradientElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(spreadMethod)
    REGISTER_LOCAL_ANIMATED_PROPERTY(gradientUnits)
    REGISTER_LOCAL_ANIMATED_PROPERTY(gradientTransform)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGElement)
END_REGISTER_ANIMATED_PROPERTIES
 
SVGGradientElement::SVGGradientElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , m_spreadMethod(SVGSpreadMethodPad)
    , m_gradientUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
{
    registerAnimatedPropertiesForSVGGradientElement();
}

bool SVGGradientElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static const auto supportedAttributes = makeNeverDestroyed([] {
        HashSet<QualifiedName> set;
        SVGURIReference::addSupportedAttributes(set);
        SVGExternalResourcesRequired::addSupportedAttributes(set);
        set.add({ SVGNames::gradientUnitsAttr.get(), SVGNames::gradientTransformAttr.get(), SVGNames::spreadMethodAttr.get() });
        return set;
    }());
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGGradientElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::gradientUnitsAttr) {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setGradientUnitsBaseValue(propertyValue);
        return;
    }

    if (name == SVGNames::gradientTransformAttr) {
        SVGTransformListValues newList;
        newList.parse(value);
        detachAnimatedGradientTransformListWrappers(newList.size());
        setGradientTransformBaseValue(newList);
        return;
    }

    if (name == SVGNames::spreadMethodAttr) {
        auto propertyValue = SVGPropertyTraits<SVGSpreadMethodType>::fromString(value);
        if (propertyValue > 0)
            setSpreadMethodBaseValue(propertyValue);
        return;
    }

    SVGElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    InstanceInvalidationGuard guard(*this);
    
    if (RenderObject* object = renderer())
        object->setNeedsLayout();
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
