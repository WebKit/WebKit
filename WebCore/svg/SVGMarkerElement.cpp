/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "SVGMarkerElement.h"

#include "PlatformString.h"
#include "RenderSVGViewportContainer.h"
#include "SVGAngle.h"
#include "SVGFitToViewBox.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"

namespace WebCore {

SVGMarkerElement::SVGMarkerElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGFitToViewBox()
    , m_refX(this, LengthModeWidth)
    , m_refY(this, LengthModeHeight)
    , m_markerWidth(this, LengthModeWidth)
    , m_markerHeight(this, LengthModeHeight) 
    , m_markerUnits(SVG_MARKERUNITS_STROKEWIDTH)
    , m_orientType(0)
    , m_orientAngle(new SVGAngle())
{
    // Spec: If the attribute is not specified, the effect is as if a value of "3" were specified.
    setMarkerWidthBaseValue(SVGLength(this, LengthModeWidth, "3"));
    setMarkerHeightBaseValue(SVGLength(this, LengthModeHeight, "3"));
}

SVGMarkerElement::~SVGMarkerElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength, Length, length, RefX, refX, SVGNames::refXAttr, m_refX)
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength, Length, length, RefY, refY, SVGNames::refYAttr, m_refY)
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, int, Enumeration, enumeration, MarkerUnits, markerUnits, SVGNames::markerUnitsAttr, m_markerUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength, Length, length, MarkerWidth, markerWidth, SVGNames::markerWidthAttr, m_markerWidth)
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength, Length, length, MarkerHeight, markerHeight, SVGNames::markerHeightAttr, m_markerHeight)
ANIMATED_PROPERTY_DEFINITIONS_WITH_CUSTOM_IDENTIFIER(SVGMarkerElement, int, Enumeration, enumeration, OrientType, orientType, SVGNames::orientAttr, "orientType", m_orientType)
ANIMATED_PROPERTY_DEFINITIONS_WITH_CUSTOM_IDENTIFIER(SVGMarkerElement, SVGAngle*, Angle, angle, OrientAngle, orientAngle, SVGNames::orientAttr, "orientAngle", m_orientAngle.get())

void SVGMarkerElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::markerUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setMarkerUnitsBaseValue(SVG_MARKERUNITS_USERSPACEONUSE);
    } else if (attr->name() == SVGNames::refXAttr)
        setRefXBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::refYAttr)
        setRefYBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::markerWidthAttr)
        setMarkerWidthBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::markerHeightAttr)
        setMarkerHeightBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::orientAttr) {
        if (attr->value() == "auto")
            setOrientToAuto();
        else {
            SVGAngle* angle = new SVGAngle();
            angle->setValueAsString(attr->value());
            setOrientToAngle(angle);
        }
    } else {
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGFitToViewBox::parseMappedAttribute(attr))
            return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGMarkerElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledElement::svgAttributeChanged(attrName);

    if (!m_marker)
        return;

    if (attrName == SVGNames::markerUnitsAttr || attrName == SVGNames::refXAttr ||
        attrName == SVGNames::refYAttr || attrName == SVGNames::markerWidthAttr ||
        attrName == SVGNames::markerHeightAttr || attrName == SVGNames::orientAttr ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGFitToViewBox::isKnownAttribute(attrName) ||
        SVGStyledElement::isKnownAttribute(attrName)) {
        if (renderer())
            renderer()->setNeedsLayout(true);
        
        m_marker->invalidate();
    }
}

void SVGMarkerElement::childrenChanged(bool changedByParser)
{
    SVGStyledElement::childrenChanged(changedByParser);

    if (renderer())
        renderer()->setNeedsLayout(true);

    if (m_marker)
        m_marker->invalidate();
}

void SVGMarkerElement::setOrientToAuto()
{
    setOrientTypeBaseValue(SVG_MARKER_ORIENT_AUTO);
}

void SVGMarkerElement::setOrientToAngle(SVGAngle* angle)
{
    setOrientTypeBaseValue(SVG_MARKER_ORIENT_ANGLE);
    setOrientAngleBaseValue(angle);
}

SVGResource* SVGMarkerElement::canvasResource()
{
    if (!m_marker)
        m_marker = new SVGResourceMarker();

    m_marker->setMarker(static_cast<RenderSVGViewportContainer*>(renderer()));

    // Spec: If the attribute is not specified, the effect is as if a
    // value of "0" were specified.
    if (!m_orientType)
        setOrientToAngle(SVGSVGElement::createSVGAngle());

    if (orientType() == SVG_MARKER_ORIENT_ANGLE)
        m_marker->setAngle(orientAngle()->value());
    else
        m_marker->setAutoAngle();

    m_marker->setRef(refX().value(), refY().value());
    m_marker->setUseStrokeWidth(markerUnits() == SVG_MARKERUNITS_STROKEWIDTH);

    return m_marker.get();
}

RenderObject* SVGMarkerElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    RenderSVGViewportContainer* markerContainer = new (arena) RenderSVGViewportContainer(this);
    markerContainer->setDrawsContents(false); // Marker contents will be explicitly drawn.
    return markerContainer;
}

}

#endif // ENABLE(SVG)
