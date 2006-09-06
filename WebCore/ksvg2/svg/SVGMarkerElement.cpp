/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "SVGMarkerElement.h"

#include "Attr.h"
#include "PlatformString.h"
#include "SVGAngle.h"
#include "SVGAngle.h"
#include "SVGLength.h"
#include "SVGFitToViewBox.h"
#include "SVGHelper.h"
#include "SVGMatrix.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "ksvg.h"
#include <kcanvas/RenderSVGContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

namespace WebCore {

SVGMarkerElement::SVGMarkerElement(const QualifiedName& tagName, Document *doc)
    : SVGStyledElement(tagName, doc)
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGFitToViewBox()
    , m_refX(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_refY(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_markerWidth(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_markerHeight(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_markerUnits(SVG_MARKERUNITS_STROKEWIDTH)
    , m_orientType(0)
    , m_orientAngle(new SVGAngle(this))
{
    m_marker = 0;
}

SVGMarkerElement::~SVGMarkerElement()
{
    delete m_marker;
}

void SVGMarkerElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::markerUnitsAttr) {
        if (value == "userSpaceOnUse")
            setMarkerUnitsBaseValue(SVG_MARKERUNITS_USERSPACEONUSE);
    }
    else if (attr->name() == SVGNames::refXAttr)
        refXBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::refYAttr)
        refYBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::markerWidthAttr)
        markerWidthBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::markerHeightAttr)
        markerHeightBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::orientAttr)
    {
        if (value == "auto")
            setOrientToAuto();
        else {
            SVGAngle *angle = SVGSVGElement::createSVGAngle();
            angle->setValueAsString(value.impl());
            setOrientToAngle(angle);
        }
    }
    else
    {
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        if(SVGFitToViewBox::parseMappedAttribute(attr)) return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength*, Length, length, RefX, refX, SVGNames::refXAttr.localName(), m_refX.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength*, Length, length, RefY, refY, SVGNames::refYAttr.localName(), m_refY.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, int, Enumeration, enumeration, MarkerUnits, markerUnits, SVGNames::markerUnitsAttr.localName(), m_markerUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength*, Length, length, MarkerWidth, markerWidth, SVGNames::markerWidthAttr.localName(), m_markerWidth.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength*, Length, length, MarkerHeight, markerHeight, SVGNames::markerHeightAttr.localName(), m_markerHeight.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, int, Enumeration, enumeration, OrientType, orientType, "orientType", m_orientType)
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGAngle*, Angle, angle, OrientAngle, orientAngle, "orientAngle", m_orientAngle.get())

void SVGMarkerElement::setOrientToAuto()
{
    setOrientTypeBaseValue(SVG_MARKER_ORIENT_AUTO);
}

void SVGMarkerElement::setOrientToAngle(SVGAngle *angle)
{
    setOrientTypeBaseValue(SVG_MARKER_ORIENT_ANGLE);
    setOrientAngleBaseValue(angle);
}

KCanvasMarker *SVGMarkerElement::canvasResource()
{
    if(!m_marker)
        m_marker = static_cast<KCanvasMarker *>(renderingDevice()->createResource(RS_MARKER));
    
    m_marker->setMarker(renderer());

    // Spec: If the attribute is not specified, the effect is as if a
    // value of "0" were specified.
    if(!m_orientType)
        setOrientToAngle(SVGSVGElement::createSVGAngle());
    
    if(orientTypeBaseValue() == SVG_MARKER_ORIENT_ANGLE)
        m_marker->setAngle(orientAngleBaseValue()->value());
    else
        m_marker->setAutoAngle();

    m_marker->setRef(refXBaseValue()->value(), refYBaseValue()->value());
    
    m_marker->setUseStrokeWidth(markerUnitsBaseValue() == SVG_MARKERUNITS_STROKEWIDTH);
    double w = markerWidthBaseValue()->value();
    double h = markerHeightBaseValue()->value();
    RefPtr<SVGMatrix> viewBox = viewBoxToViewTransform(w, h);
    m_marker->setScale(viewBox->matrix().m11(), viewBox->matrix().m22());
    
    return m_marker;
}

RenderObject* SVGMarkerElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    RenderSVGContainer *markerContainer = new (arena) RenderSVGContainer(this);
    markerContainer->setDrawsContents(false); // Marker contents will be explicitly drawn.
    return markerContainer;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

