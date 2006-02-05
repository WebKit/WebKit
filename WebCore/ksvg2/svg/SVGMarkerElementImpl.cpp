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
#include <kdom/core/AttrImpl.h>
#include <kdom/core/NamedAttrMapImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGAngleImpl.h"
#include "SVGLengthImpl.h"
#include "SVGMatrixImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGMarkerElementImpl.h"
#include "SVGAnimatedAngleImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedEnumerationImpl.h"

using namespace KSVG;

SVGMarkerElementImpl::SVGMarkerElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGStyledElementImpl(tagName, doc), SVGLangSpaceImpl(),
  SVGExternalResourcesRequiredImpl(), SVGFitToViewBoxImpl()
{
    m_marker = 0;
}

SVGMarkerElementImpl::~SVGMarkerElementImpl()
{
    delete m_marker;
}

void SVGMarkerElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    const KDOM::AtomicString& value = attr->value();
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
            SVGAngleImpl *angle = SVGSVGElementImpl::createSVGAngle();
            angle->setValueAsString(value.impl());
            setOrientToAngle(angle);
        }
    }
    else
    {
        if(SVGLangSpaceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;
        if(SVGFitToViewBoxImpl::parseMappedAttribute(attr)) return;

        SVGStyledElementImpl::parseMappedAttribute(attr);
    }
}

SVGAnimatedLengthImpl *SVGMarkerElementImpl::refX() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_refX, static_cast<const SVGStyledElementImpl *>(0), LM_WIDTH, this);
}

SVGAnimatedLengthImpl *SVGMarkerElementImpl::refY() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_refY, static_cast<const SVGStyledElementImpl *>(0), LM_HEIGHT, this);
}

SVGAnimatedEnumerationImpl *SVGMarkerElementImpl::markerUnits() const
{
    if (!m_markerUnits) {
        lazy_create<SVGAnimatedEnumerationImpl>(m_markerUnits, this);
        m_markerUnits->setBaseVal(SVG_MARKERUNITS_STROKEWIDTH);
    }

    return m_markerUnits.get();
}

SVGAnimatedLengthImpl *SVGMarkerElementImpl::markerWidth() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_markerWidth, static_cast<const SVGStyledElementImpl *>(0), LM_WIDTH, this);
}

SVGAnimatedLengthImpl *SVGMarkerElementImpl::markerHeight() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_markerHeight, static_cast<const SVGStyledElementImpl *>(0), LM_HEIGHT, this);
}

SVGAnimatedEnumerationImpl *SVGMarkerElementImpl::orientType() const
{
    return lazy_create<SVGAnimatedEnumerationImpl>(m_orientType, this);
}

SVGAnimatedAngleImpl *SVGMarkerElementImpl::orientAngle() const
{
    return lazy_create<SVGAnimatedAngleImpl>(m_orientAngle, this);
}

void SVGMarkerElementImpl::setOrientToAuto()
{
    orientType()->setBaseVal(SVG_MARKER_ORIENT_AUTO);
}

void SVGMarkerElementImpl::setOrientToAngle(SVGAngleImpl *angle)
{
    orientType()->setBaseVal(SVG_MARKER_ORIENT_ANGLE);
    orientAngle()->setBaseVal(angle);
}

KCanvasMarker *SVGMarkerElementImpl::canvasResource()
{
    if(!m_marker)
        m_marker = static_cast<KCanvasMarker *>(QPainter::renderingDevice()->createResource(RS_MARKER));
    
    m_marker->setMarker(renderer());

    // Spec: If the attribute is not specified, the effect is as if a
    // value of "0" were specified.
    if(!m_orientType)
    {
        SVGAngleImpl *angle = SVGSVGElementImpl::createSVGAngle();
        angle->setValueAsString(KDOM::DOMString("0").impl());
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
    RefPtr<SVGMatrixImpl> viewBox = viewBoxToViewTransform(w, h);
    m_marker->setScale(viewBox->qmatrix().m11(), viewBox->qmatrix().m22());
    
    return m_marker;
}

khtml::RenderObject *SVGMarkerElementImpl::createRenderer(RenderArena *arena, khtml::RenderStyle *style)
{
    KCanvasContainer *markerContainer = QPainter::renderingDevice()->createContainer(arena, style, this);
    markerContainer->setDrawsContents(false); // Marker contents will be explicitly drawn.
    return markerContainer;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

