/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGGradientElement.h"

#include "CSSStyleSelector.h"
#include "MappedAttribute.h"
#include "RenderPath.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGResourceLinearGradient.h"
#include "RenderSVGResourceRadialGradient.h"
#include "SVGNames.h"
#include "SVGStopElement.h"
#include "SVGTransformList.h"
#include "SVGTransformable.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGGradientElement::SVGGradientElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGURIReference()
    , SVGExternalResourcesRequired()
    , m_gradientUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_gradientTransform(SVGTransformList::create(SVGNames::gradientTransformAttr))
{
}

SVGGradientElement::~SVGGradientElement()
{
}

void SVGGradientElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::gradientUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setGradientUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setGradientUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::gradientTransformAttr) {
        SVGTransformList* gradientTransforms = gradientTransformBaseValue();
        if (!SVGTransformable::parseTransformAttribute(gradientTransforms, attr->value())) {
            ExceptionCode ec = 0;
            gradientTransforms->clear(ec);
        }
    } else if (attr->name() == SVGNames::spreadMethodAttr) {
        if (attr->value() == "reflect")
            setSpreadMethodBaseValue(SpreadMethodReflect);
        else if (attr->value() == "repeat")
            setSpreadMethodBaseValue(SpreadMethodRepeat);
        else if (attr->value() == "pad")
            setSpreadMethodBaseValue(SpreadMethodPad);
    } else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        
        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledElement::svgAttributeChanged(attrName);

    if (attrName == SVGNames::gradientUnitsAttr
        || attrName == SVGNames::gradientTransformAttr
        || attrName == SVGNames::spreadMethodAttr
        || SVGURIReference::isKnownAttribute(attrName)
        || SVGExternalResourcesRequired::isKnownAttribute(attrName)
        || SVGStyledElement::isKnownAttribute(attrName))
        invalidateResourceClients();
}

void SVGGradientElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGStyledElement::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeGradientUnits();
        synchronizeGradientTransform();
        synchronizeSpreadMethod();
        synchronizeExternalResourcesRequired();
        synchronizeHref();
        return;
    }

    if (attrName == SVGNames::gradientUnitsAttr)
        synchronizeGradientUnits();
    else if (attrName == SVGNames::gradientTransformAttr)
        synchronizeGradientTransform();
    else if (attrName == SVGNames::spreadMethodAttr)
        synchronizeSpreadMethod();
    else if (SVGExternalResourcesRequired::isKnownAttribute(attrName))
        synchronizeExternalResourcesRequired();
    else if (SVGURIReference::isKnownAttribute(attrName))
        synchronizeHref();
}

void SVGGradientElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!changedByParser)
        invalidateResourceClients();
}

Vector<Gradient::ColorStop> SVGGradientElement::buildStops()
{
    Vector<Gradient::ColorStop> stops;

    float previousOffset = 0.0f;
    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        SVGElement* element = n->isSVGElement() ? static_cast<SVGElement*>(n) : 0;
        if (!element || !element->isGradientStop())
            continue;

        SVGStopElement* stop = static_cast<SVGStopElement*>(element);
        Color color = stop->stopColorIncludingOpacity();

        // Figure out right monotonic offset
        float offset = stop->offset();
        offset = std::min(std::max(previousOffset, offset), 1.0f);
        previousOffset = offset;

        // Extract individual channel values
        // FIXME: Why doesn't ColorStop take a Color and an offset??
        float r, g, b, a;
        color.getRGBA(r, g, b, a);

        stops.append(Gradient::ColorStop(offset, r, g, b, a));
    }

    return stops;
}

}

#endif // ENABLE(SVG)
