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
#if SVG_SUPPORT
#include "PlatformString.h"
#include <kdom/core/Attr.h>
#include <kdom/core/NamedAttrMap.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGAngle.h"
#include "SVGLength.h"
#include "SVGMatrix.h"
#include "SVGSVGElement.h"
#include "SVGFitToViewBox.h"
#include "SVGMarkerElement.h"
#include "SVGAnimatedAngle.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedEnumeration.h"

namespace WebCore {

SVGMarkerElement::SVGMarkerElement(const QualifiedName& tagName, Document *doc)
: SVGStyledElement(tagName, doc), SVGLangSpace(),
  SVGExternalResourcesRequired(), SVGFitToViewBox()
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
            markerUnits()->setBaseVal(SVG_MARKERUNITS_USERSPACEONUSE);
    }
    else if (attr->name() == SVGNames::refXAttr)
        refX()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::refYAttr)
        refY()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::markerWidthAttr)
        markerWidth()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::markerHeightAttr)
        markerHeight()->baseVal()->setValueAsString(value.impl());
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

SVGAnimatedLength *SVGMarkerElement::refX() const
{
    return lazy_create<SVGAnimatedLength>(m_refX, static_cast<const SVGStyledElement *>(0), LM_WIDTH, this);
}

SVGAnimatedLength *SVGMarkerElement::refY() const
{
    return lazy_create<SVGAnimatedLength>(m_refY, static_cast<const SVGStyledElement *>(0), LM_HEIGHT, this);
}

SVGAnimatedEnumeration *SVGMarkerElement::markerUnits() const
{
    if (!m_markerUnits) {
        lazy_create<SVGAnimatedEnumeration>(m_markerUnits, this);
        m_markerUnits->setBaseVal(SVG_MARKERUNITS_STROKEWIDTH);
    }

    return m_markerUnits.get();
}

SVGAnimatedLength *SVGMarkerElement::markerWidth() const
{
    return lazy_create<SVGAnimatedLength>(m_markerWidth, static_cast<const SVGStyledElement *>(0), LM_WIDTH, this);
}

SVGAnimatedLength *SVGMarkerElement::markerHeight() const
{
    return lazy_create<SVGAnimatedLength>(m_markerHeight, static_cast<const SVGStyledElement *>(0), LM_HEIGHT, this);
}

SVGAnimatedEnumeration *SVGMarkerElement::orientType() const
{
    return lazy_create<SVGAnimatedEnumeration>(m_orientType, this);
}

SVGAnimatedAngle *SVGMarkerElement::orientAngle() const
{
    return lazy_create<SVGAnimatedAngle>(m_orientAngle, this);
}

void SVGMarkerElement::setOrientToAuto()
{
    orientType()->setBaseVal(SVG_MARKER_ORIENT_AUTO);
}

void SVGMarkerElement::setOrientToAngle(SVGAngle *angle)
{
    orientType()->setBaseVal(SVG_MARKER_ORIENT_ANGLE);
    orientAngle()->setBaseVal(angle);
}

KCanvasMarker *SVGMarkerElement::canvasResource()
{
    if(!m_marker)
        m_marker = static_cast<KCanvasMarker *>(renderingDevice()->createResource(RS_MARKER));
    
    m_marker->setMarker(renderer());

    // Spec: If the attribute is not specified, the effect is as if a
    // value of "0" were specified.
    if(!m_orientType)
    {
        SVGAngle *angle = SVGSVGElement::createSVGAngle();
        angle->setValueAsString(String("0").impl());
        setOrientToAngle(angle);
    }
    
    if(orientType()->baseVal() == SVG_MARKER_ORIENT_ANGLE)
        m_marker->setAngle(orientAngle()->baseVal()->value());
    else
        m_marker->setAutoAngle();

    m_marker->setRef(refX()->baseVal()->value(), refY()->baseVal()->value());
    
    m_marker->setUseStrokeWidth(markerUnits()->baseVal() == SVG_MARKERUNITS_STROKEWIDTH);
    double w = markerWidth()->baseVal()->value();
    double h = markerHeight()->baseVal()->value();
    RefPtr<SVGMatrix> viewBox = viewBoxToViewTransform(w, h);
    m_marker->setScale(viewBox->qmatrix().m11(), viewBox->qmatrix().m22());
    
    return m_marker;
}

RenderObject *SVGMarkerElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    KCanvasContainer *markerContainer = renderingDevice()->createContainer(arena, style, this);
    markerContainer->setDrawsContents(false); // Marker contents will be explicitly drawn.
    return markerContainer;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

