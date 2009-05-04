/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
                  2008 Eric Seidel <eric@webkit.org>
                  2008 Dirk Schulze <krit@webkit.org>

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
#include "SVGRadialGradientElement.h"

#include "FloatConversion.h"
#include "FloatPoint.h"
#include "MappedAttribute.h"
#include "RadialGradientAttributes.h"
#include "RenderObject.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPaintServerRadialGradient.h"
#include "SVGStopElement.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGUnitTypes.h"

namespace WebCore {

SVGRadialGradientElement::SVGRadialGradientElement(const QualifiedName& tagName, Document* doc)
    : SVGGradientElement(tagName, doc)
    , m_cx(this, SVGNames::cxAttr, LengthModeWidth, "50%")
    , m_cy(this, SVGNames::cyAttr, LengthModeHeight, "50%")
    , m_r(this, SVGNames::rAttr, LengthModeOther, "50%")
    , m_fx(this, SVGNames::fxAttr, LengthModeWidth)
    , m_fy(this, SVGNames::fyAttr, LengthModeHeight)
{
    // Spec: If the cx/cy/r attribute is not specified, the effect is as if a value of "50%" were specified.
}

SVGRadialGradientElement::~SVGRadialGradientElement()
{
}

void SVGRadialGradientElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::cxAttr)
        setCxBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::cyAttr)
        setCyBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::rAttr) {
        setRBaseValue(SVGLength(LengthModeOther, attr->value()));
        if (rBaseValue().value(this) < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for radial gradient radius <r> is not allowed");
    } else if (attr->name() == SVGNames::fxAttr)
        setFxBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::fyAttr)
        setFyBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else
        SVGGradientElement::parseMappedAttribute(attr);
}

void SVGRadialGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGGradientElement::svgAttributeChanged(attrName);

    if (!m_resource)
        return;

    if (attrName == SVGNames::cxAttr || attrName == SVGNames::cyAttr ||
        attrName == SVGNames::fxAttr || attrName == SVGNames::fyAttr ||
        attrName == SVGNames::rAttr)
        m_resource->invalidate();
}

void SVGRadialGradientElement::buildGradient() const
{
    RadialGradientAttributes attributes = collectGradientProperties();

    RefPtr<SVGPaintServerRadialGradient> radialGradient = WTF::static_pointer_cast<SVGPaintServerRadialGradient>(m_resource);

    double adjustedFocusX = attributes.fx();
    double adjustedFocusY = attributes.fy();

    double fdx = attributes.fx() - attributes.cx();
    double fdy = attributes.fy() - attributes.cy();

    // Spec: If (fx, fy) lies outside the circle defined by (cx, cy) and
    // r, set (fx, fy) to the point of intersection of the line through
    // (fx, fy) and the circle.
    if (sqrt(fdx * fdx + fdy * fdy) > attributes.r()) {
        double angle = atan2(attributes.fy() * 100.0, attributes.fx() * 100.0);
        adjustedFocusX = cos(angle) * attributes.r();
        adjustedFocusY = sin(angle) * attributes.r();
    }

    FloatPoint focalPoint = FloatPoint::narrowPrecision(attributes.fx(), attributes.fy());
    FloatPoint centerPoint = FloatPoint::narrowPrecision(attributes.cx(), attributes.cy());

    RefPtr<Gradient> gradient = Gradient::create(
        FloatPoint::narrowPrecision(adjustedFocusX, adjustedFocusY),
        0.f, // SVG does not support a "focus radius"
        centerPoint,
        narrowPrecisionToFloat(attributes.r()));
    gradient->setSpreadMethod(attributes.spreadMethod());

    Vector<SVGGradientStop> stops = attributes.stops();
    float previousOffset = 0.0f;
    for (unsigned i = 0; i < stops.size(); ++i) {
         float offset = std::min(std::max(previousOffset, stops[i].first), 1.0f);
         previousOffset = offset;
         gradient->addColorStop(offset, stops[i].second);
    }

    radialGradient->setGradient(gradient);

    if (attributes.stops().isEmpty())
        return;

    radialGradient->setBoundingBoxMode(attributes.boundingBoxMode());
    radialGradient->setGradientTransform(attributes.gradientTransform());
    radialGradient->setGradientCenter(centerPoint);
    radialGradient->setGradientFocal(focalPoint);
    radialGradient->setGradientRadius(narrowPrecisionToFloat(attributes.r()));
    radialGradient->setGradientStops(attributes.stops());
}

RadialGradientAttributes SVGRadialGradientElement::collectGradientProperties() const
{
    RadialGradientAttributes attributes;
    HashSet<const SVGGradientElement*> processedGradients;

    bool isRadial = true;
    const SVGGradientElement* current = this;

    while (current) {
        if (!attributes.hasSpreadMethod() && current->hasAttribute(SVGNames::spreadMethodAttr))
            attributes.setSpreadMethod((GradientSpreadMethod) current->spreadMethod());

        if (!attributes.hasBoundingBoxMode() && current->hasAttribute(SVGNames::gradientUnitsAttr))
            attributes.setBoundingBoxMode(current->gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);

        if (!attributes.hasGradientTransform() && current->hasAttribute(SVGNames::gradientTransformAttr))
            attributes.setGradientTransform(current->gradientTransform()->consolidate().matrix());

        if (!attributes.hasStops()) {
            const Vector<SVGGradientStop>& stops(current->buildStops());
            if (!stops.isEmpty())
                attributes.setStops(stops);
        }

        if (isRadial) {
            const SVGRadialGradientElement* radial = static_cast<const SVGRadialGradientElement*>(current);

            if (!attributes.hasCx() && current->hasAttribute(SVGNames::cxAttr))
                attributes.setCx(radial->cx().valueAsPercentage());

            if (!attributes.hasCy() && current->hasAttribute(SVGNames::cyAttr))
                attributes.setCy(radial->cy().valueAsPercentage());

            if (!attributes.hasR() && current->hasAttribute(SVGNames::rAttr))
                attributes.setR(radial->r().valueAsPercentage());

            if (!attributes.hasFx() && current->hasAttribute(SVGNames::fxAttr))
                attributes.setFx(radial->fx().valueAsPercentage());

            if (!attributes.hasFy() && current->hasAttribute(SVGNames::fyAttr))
                attributes.setFy(radial->fy().valueAsPercentage());
        }

        processedGradients.add(current);

        // Respect xlink:href, take attributes from referenced element
        Node* refNode = ownerDocument()->getElementById(SVGURIReference::getTarget(current->href()));
        if (refNode && (refNode->hasTagName(SVGNames::radialGradientTag) || refNode->hasTagName(SVGNames::linearGradientTag))) {
            current = static_cast<const SVGGradientElement*>(const_cast<const Node*>(refNode));

            // Cycle detection
            if (processedGradients.contains(current))
                return RadialGradientAttributes();

            isRadial = current->gradientType() == RadialGradientPaintServer;
        } else
            current = 0;
    }

    // Handle default values for fx/fy
    if (!attributes.hasFx())
        attributes.setFx(attributes.cx());

    if (!attributes.hasFy())
        attributes.setFy(attributes.cy());

    return attributes;
}
}

#endif // ENABLE(SVG)
