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

#include <kdom/DOMString.h>
#include <kdom/impl/AttrImpl.h>
#include <kdom/impl/NamedAttrMapImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGAngleImpl.h"
#include "SVGLengthImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGMarkerElementImpl.h"
#include "SVGAnimatedAngleImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedEnumerationImpl.h"

using namespace KSVG;

SVGMarkerElementImpl::SVGMarkerElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGLangSpaceImpl(),
  SVGExternalResourcesRequiredImpl(), SVGFitToViewBoxImpl()
{
    m_refX = m_refY = m_markerWidth = m_markerHeight = 0;
    m_markerUnits = m_orientType = 0;
    m_orientAngle = 0;
    m_marker = 0;
}

SVGMarkerElementImpl::~SVGMarkerElementImpl()
{
    if(m_refX)
        m_refX->deref();
    if(m_refY)
        m_refY->deref();
    if(m_markerUnits)
        m_markerUnits->deref();
    if(m_markerWidth)
        m_markerWidth->deref();
    if(m_markerHeight)
        m_markerHeight->deref();
    if(m_orientType)
        m_orientType->deref();
    if(m_orientAngle)
        m_orientAngle->deref();
}

void SVGMarkerElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMStringImpl *value = attr->value();
    switch(id)
    {
        case ATTR_REFX:
        {
            refX()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_REFY:
        {
            refY()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_MARKERWIDTH:
        {
            markerWidth()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_MARKERHEIGHT:
        {
            markerHeight()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_ORIENT:
        {
            if(KDOM::DOMString(value) == "auto")
                setOrientToAuto();
            else
            {
                SVGAngleImpl *angle = SVGSVGElementImpl::createSVGAngle();
                angle->setValueAsString(value);
                setOrientToAngle(angle);
            }
            break;
        }
        default:
        {
            if(SVGLangSpaceImpl::parseAttribute(attr)) return;
            if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
            if(SVGFitToViewBoxImpl::parseAttribute(attr)) return;

            SVGStyledElementImpl::parseAttribute(attr);
        }
    };
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
    if(!m_markerUnits)
    {
        lazy_create<SVGAnimatedEnumerationImpl>(m_markerUnits, this);
        m_markerUnits->setBaseVal(SVG_MARKERUNITS_STROKEWIDTH);
    }

    return m_markerUnits;
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

void SVGMarkerElementImpl::close()
{
    if(!m_marker)
    {
        m_marker = static_cast<KCanvasMarker *>(canvas()->renderingDevice()->createResource(RS_MARKER));
        canvas()->registry()->addResourceById(KDOM::DOMString(getId()).string(), m_marker);
    }
    
    m_marker->setMarker(m_canvasItem);

    // Spec: If the attribute is not specified, the effect is as if a
    // value of "0" were specified.
    if(!m_orientType)
    {
        SVGAngleImpl *angle = SVGSVGElementImpl::createSVGAngle();
        angle->setValueAsString(KDOM::DOMString("0").handle());
        setOrientToAngle(angle);
    }
    
    if(orientType()->baseVal() == SVG_MARKER_ORIENT_ANGLE)
        m_marker->setAngle(orientAngle()->baseVal()->value());
    else
        m_marker->setAutoAngle();

    m_marker->setRefX(refX()->baseVal()->value());
    m_marker->setRefY(refY()->baseVal()->value());
}

KCanvasItem *SVGMarkerElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
    KCanvasContainer *container = KCanvasCreator::self()->createContainer(canvas, style);
    container->setDrawContents(false); // Marker contents will be explicitly drawn.
    return container;
}

// vim:ts=4:noet
