/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
    , m_cx(this, LengthModeWidth)
    , m_cy(this, LengthModeHeight)
    , m_r(this, LengthModeOther)
    , m_fx(this, LengthModeWidth)
    , m_fy(this, LengthModeHeight)
{
    // Spec: If the attribute is not specified, the effect is as if a value of "50%" were specified.
    setCxBaseValue(SVGLength(this, LengthModeWidth, "50%"));
    setCyBaseValue(SVGLength(this, LengthModeHeight, "50%"));
    setRBaseValue(SVGLength(this, LengthModeOther, "50%"));
}

SVGRadialGradientElement::~SVGRadialGradientElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength, Length, length, Cx, cx, SVGNames::cxAttr, m_cx)
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength, Length, length, Cy, cy, SVGNames::cyAttr, m_cy)
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength, Length, length, Fx, fx, SVGNames::fxAttr, m_fx)
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength, Length, length, Fy, fy, SVGNames::fyAttr, m_fy)
ANIMATED_PROPERTY_DEFINITIONS(SVGRadialGradientElement, SVGLength, Length, length, R, r, SVGNames::rAttr, m_r)

void SVGRadialGradientElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::cxAttr)
        setCxBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::cyAttr)
        setCyBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::rAttr) {
        setRBaseValue(SVGLength(this, LengthModeOther, attr->value()));
        if (r().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for radial gradient radius <r> is not allowed");
    } else if (attr->name() == SVGNames::fxAttr)
        setFxBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::fyAttr)
        setFyBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else
        SVGGradientElement::parseMappedAttribute(attr);
}

void SVGRadialGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGGradientElement::svgAttributeChanged(attrName);

    if (!m_resource)
        return;

    if (attrName == SVGNames::cxAttr || attrName == SVGNames::cyAttr ||
        attrName == SVGNames::fyAttr || attrName == SVGNames::fyAttr ||
        attrName == SVGNames::rAttr)
        m_resource->invalidate();
}

void SVGRadialGradientElement::buildGradient() const
{
    RadialGradientAttributes attributes = collectGradientProperties();

    // If we didn't find any gradient containing stop elements, ignore the request.
    if (attributes.stops().isEmpty())
        return;

    RefPtr<SVGPaintServerRadialGradient> radialGradient = WTF::static_pointer_cast<SVGPaintServerRadialGradient>(m_resource);

    radialGradient->setGradientStops(attributes.stops());
    radialGradient->setBoundingBoxMode(attributes.boundingBoxMode());
    radialGradient->setGradientSpreadMethod(attributes.spreadMethod()); 
    radialGradient->setGradientTransform(attributes.gradientTransform());
    radialGradient->setGradientCenter(FloatPoint::narrowPrecision(attributes.cx(), attributes.cy()));
    radialGradient->setGradientFocal(FloatPoint::narrowPrecision(attributes.fx(), attributes.fy()));
    radialGradient->setGradientRadius(narrowPrecisionToFloat(attributes.r()));
}

RadialGradientAttributes SVGRadialGradientElement::collectGradientProperties() const
{
    RadialGradientAttributes attributes;
    HashSet<const SVGGradientElement*> processedGradients;

    bool isRadial = true;
    const SVGGradientElement* current = this;

    while (current) {
        if (!attributes.hasSpreadMethod() && current->hasAttribute(SVGNames::spreadMethodAttr))
            attributes.setSpreadMethod((SVGGradientSpreadMethod) current->spreadMethod());

        if (!attributes.hasBoundingBoxMode() && current->hasAttribute(SVGNames::gradientUnitsAttr))
            attributes.setBoundingBoxMode(current->getAttribute(SVGNames::gradientUnitsAttr) == "objectBoundingBox");

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
