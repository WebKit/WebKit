/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.

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
#include "SVGMarkerElement.h"

#include "MappedAttribute.h"
#include "PlatformString.h"
#include "RenderSVGResourceMarker.h"
#include "SVGFitToViewBox.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"

namespace WebCore {

char SVGOrientTypeAttrIdentifier[] = "SVGOrientTypeAttr";
char SVGOrientAngleAttrIdentifier[] = "SVGOrientAngleAttr";

SVGMarkerElement::SVGMarkerElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGFitToViewBox()
    , m_refX(LengthModeWidth)
    , m_refY(LengthModeHeight)
    , m_markerWidth(LengthModeWidth, "3")
    , m_markerHeight(LengthModeHeight, "3") 
    , m_markerUnits(SVG_MARKERUNITS_STROKEWIDTH)
    , m_orientType(SVG_MARKER_ORIENT_ANGLE)
{
    // Spec: If the markerWidth/markerHeight attribute is not specified, the effect is as if a value of "3" were specified.
}

SVGMarkerElement::~SVGMarkerElement()
{
}

AffineTransform SVGMarkerElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    return SVGFitToViewBox::viewBoxToViewTransform(viewBox(), preserveAspectRatio(), viewWidth, viewHeight);
}

void SVGMarkerElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::markerUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setMarkerUnitsBaseValue(SVG_MARKERUNITS_USERSPACEONUSE);
        else if (attr->value() == "strokeWidth")
            setMarkerUnitsBaseValue(SVG_MARKERUNITS_STROKEWIDTH);
    } else if (attr->name() == SVGNames::refXAttr)
        setRefXBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::refYAttr)
        setRefYBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::markerWidthAttr)
        setMarkerWidthBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::markerHeightAttr)
        setMarkerHeightBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::orientAttr) {
        SVGAngle angle;

        if (attr->value() == "auto")
            setOrientTypeBaseValue(SVG_MARKER_ORIENT_AUTO);
        else {
            angle.setValueAsString(attr->value());
            setOrientTypeBaseValue(SVG_MARKER_ORIENT_ANGLE);
        }

        setOrientAngleBaseValue(angle);
    } else {
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGFitToViewBox::parseMappedAttribute(document(), attr))
            return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGMarkerElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledElement::svgAttributeChanged(attrName);

    if (attrName == SVGNames::markerUnitsAttr || attrName == SVGNames::refXAttr ||
        attrName == SVGNames::refYAttr || attrName == SVGNames::markerWidthAttr ||
        attrName == SVGNames::markerHeightAttr || attrName == SVGNames::orientAttr ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGFitToViewBox::isKnownAttribute(attrName) ||
        SVGStyledElement::isKnownAttribute(attrName))
        invalidateResourceClients();
}

void SVGMarkerElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGStyledElement::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeMarkerUnits();
        synchronizeRefX();
        synchronizeRefY();
        synchronizeMarkerWidth();
        synchronizeMarkerHeight();
        synchronizeOrientAngle();
        synchronizeOrientType();
        synchronizeExternalResourcesRequired();
        synchronizeViewBox();
        synchronizePreserveAspectRatio();
        return;
    }

    if (attrName == SVGNames::markerUnitsAttr)
        synchronizeMarkerUnits();
    else if (attrName == SVGNames::refXAttr)
        synchronizeRefX();
    else if (attrName == SVGNames::refYAttr)
        synchronizeRefY();
    else if (attrName == SVGNames::markerWidthAttr)
        synchronizeMarkerWidth();
    else if (attrName == SVGNames::markerHeightAttr)
        synchronizeMarkerHeight();
    else if (attrName == SVGNames::orientAttr) {
        synchronizeOrientAngle();
        synchronizeOrientType();
    } else if (SVGExternalResourcesRequired::isKnownAttribute(attrName))
        synchronizeExternalResourcesRequired();
    else if (SVGFitToViewBox::isKnownAttribute(attrName)) {
        synchronizeViewBox();
        synchronizePreserveAspectRatio();
    }
}

void SVGMarkerElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!changedByParser)
        invalidateResourceClients();
}

void SVGMarkerElement::setOrientToAuto()
{
    setOrientTypeBaseValue(SVG_MARKER_ORIENT_AUTO);
    setOrientAngleBaseValue(SVGAngle());

    invalidateResourceClients();
}

void SVGMarkerElement::setOrientToAngle(const SVGAngle& angle)
{
    setOrientTypeBaseValue(SVG_MARKER_ORIENT_ANGLE);
    setOrientAngleBaseValue(angle);

    invalidateResourceClients();
}

RenderObject* SVGMarkerElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGResourceMarker(this);
}

}

#endif
