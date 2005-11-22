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
#include <kdom/core/AttrImpl.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGTransformableImpl.h"
#include "SVGTransformListImpl.h"
#include "SVGGradientElementImpl.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedTransformListImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGStopElementImpl.h"
#include "SVGRenderStyle.h"

#include <kcanvas/device/KRenderingPaintServerGradient.h>
#include <kcanvas/device/KRenderingDevice.h>

using namespace KSVG;

SVGGradientElementImpl::SVGGradientElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : SVGStyledElementImpl(tagName, doc), SVGURIReferenceImpl(), SVGExternalResourcesRequiredImpl()
{
    m_spreadMethod = 0;
    m_gradientUnits = 0;
    m_gradientTransform = 0;
    m_resource = 0;
}

SVGGradientElementImpl::~SVGGradientElementImpl()
{
    if(m_gradientUnits)
        m_gradientUnits->deref();
    if(m_gradientTransform)
        m_gradientTransform->deref();
    if(m_spreadMethod)
        m_spreadMethod->deref();
}

SVGAnimatedEnumerationImpl *SVGGradientElementImpl::gradientUnits() const
{
    if(!m_gradientUnits)
    {
        lazy_create<SVGAnimatedEnumerationImpl>(m_gradientUnits, this);
        m_gradientUnits->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }
    
    return m_gradientUnits;
}

SVGAnimatedTransformListImpl *SVGGradientElementImpl::gradientTransform() const
{
    return lazy_create<SVGAnimatedTransformListImpl>(m_gradientTransform, this);
}

SVGAnimatedEnumerationImpl *SVGGradientElementImpl::spreadMethod() const
{
    return lazy_create<SVGAnimatedEnumerationImpl>(m_spreadMethod, this);
}

void SVGGradientElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
    if (attr->name() == SVGNames::gradientUnitsAttr)
    {
        if(value == "userSpaceOnUse")
            gradientUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
        else if(value == "objectBoundingBox")
            gradientUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }
    else if (attr->name() == SVGNames::gradientTransformAttr)
    {
        SVGTransformListImpl *gradientTransforms = gradientTransform()->baseVal();
        SVGTransformableImpl::parseTransformAttribute(gradientTransforms, attr->value().impl());
    }
    else if (attr->name() == SVGNames::spreadMethodAttr)
    {
        if(value == "reflect")
            spreadMethod()->setBaseVal(SVG_SPREADMETHOD_REFLECT);
        else if(value == "repeat")
            spreadMethod()->setBaseVal(SVG_SPREADMETHOD_REPEAT);
        else if(value == "pad")
            spreadMethod()->setBaseVal(SVG_SPREADMETHOD_PAD);
    }
    else
    {
        if(SVGURIReferenceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;
        
        SVGStyledElementImpl::parseMappedAttribute(attr);
    }
}

void SVGGradientElementImpl::notifyAttributeChange() const
{
    if(ownerDocument()->parsing() || !attached())
        return;

    // Update clients of this gradient resource...
    buildGradient(m_resource);
    
    m_resource->invalidate();  // should this be added to build gradient?
    
    const KCanvasItemList &clients = m_resource->clients();
    for(KCanvasItemList::ConstIterator it = clients.begin(); it != clients.end(); ++it)
    {
        const RenderPath *current = (*it);
        SVGStyledElementImpl *styled = (current ? static_cast<SVGStyledElementImpl *>(current->element()) : 0);
        if(styled)
            styled->setChanged(true);
    }
}

KRenderingPaintServerGradient *SVGGradientElementImpl::canvasResource()
{
    if (!m_resource) {
        KRenderingPaintServer *temp = canvas()->renderingDevice()->createPaintServer(gradientType());
        m_resource = static_cast<KRenderingPaintServerGradient *>(temp);
        m_resource->setListener(this);
        buildGradient(m_resource);
    }
    return m_resource;
}

void SVGGradientElementImpl::resourceNotification() const
{
    // We're referenced by a "client", build the gradient now...
    buildGradient(m_resource);
}

void SVGGradientElementImpl::rebuildStops() const
{
    if (m_resource && !ownerDocument()->parsing()) {
        KCSortedGradientStopList &stops = m_resource->gradientStops();
        stops.clear();
        for (KDOM::NodeImpl *n = firstChild(); n; n = n->nextSibling()) {
            SVGElementImpl *element = svg_dynamic_cast(n);
            if (element && element->isGradientStop()) {
                SVGStopElementImpl *stop = static_cast<SVGStopElementImpl *>(element);
                float stopOffset = stop->offset()->baseVal();
                
                SVGRenderStyle *stopStyle = getDocument()->styleSelector()->styleForElement(stop)->svgStyle();
                QColor c = stopStyle->stopColor();
                float opacity = stopStyle->stopOpacity();
                
                stops.addStop(stopOffset, qRgba(c.red(), c.green(), c.blue(), int(opacity * 255.)));
            }
        }
    }
}

// vim:ts=4:noet
