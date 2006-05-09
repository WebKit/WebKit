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
#include "SVGGradientElement.h"

#include "Attr.h"
#include "Document.h"
#include "RenderCanvas.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedTransformList.h"
#include "SVGDOMImplementation.h"
#include "SVGHelper.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGStopElement.h"
#include "SVGTransformList.h"
#include "SVGTransformable.h"
#include "cssstyleselector.h"
#include "ksvg.h"
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

using namespace WebCore;

SVGGradientElement::SVGGradientElement(const QualifiedName& tagName, Document *doc) : SVGStyledElement(tagName, doc), SVGURIReference(), SVGExternalResourcesRequired()
{
    m_resource = 0;
}

SVGGradientElement::~SVGGradientElement()
{
    delete m_resource;
}

SVGAnimatedEnumeration *SVGGradientElement::gradientUnits() const
{
    if (!m_gradientUnits) {
        lazy_create<SVGAnimatedEnumeration>(m_gradientUnits, this);
        m_gradientUnits->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }
    
    return m_gradientUnits.get();
}

SVGAnimatedTransformList *SVGGradientElement::gradientTransform() const
{
    return lazy_create<SVGAnimatedTransformList>(m_gradientTransform, this);
}

SVGAnimatedEnumeration *SVGGradientElement::spreadMethod() const
{
    return lazy_create<SVGAnimatedEnumeration>(m_spreadMethod, this);
}

void SVGGradientElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::gradientUnitsAttr) {
        if(value == "userSpaceOnUse")
            gradientUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
        else if(value == "objectBoundingBox")
            gradientUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::gradientTransformAttr) {
        SVGTransformList *gradientTransforms = gradientTransform()->baseVal();
        SVGTransformable::parseTransformAttribute(gradientTransforms, attr->value());
    } else if (attr->name() == SVGNames::spreadMethodAttr) {
        if(value == "reflect")
            spreadMethod()->setBaseVal(SVG_SPREADMETHOD_REFLECT);
        else if(value == "repeat")
            spreadMethod()->setBaseVal(SVG_SPREADMETHOD_REPEAT);
        else if(value == "pad")
            spreadMethod()->setBaseVal(SVG_SPREADMETHOD_PAD);
    } else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        
        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGGradientElement::notifyAttributeChange() const
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
        SVGStyledElement *styled = (current ? static_cast<SVGStyledElement *>(current->element()) : 0);
        if(styled)
            styled->setChanged(true);
    }
}

KRenderingPaintServerGradient *SVGGradientElement::canvasResource()
{
    if (!m_resource) {
        KRenderingPaintServer *temp = renderingDevice()->createPaintServer(gradientType());
        m_resource = static_cast<KRenderingPaintServerGradient *>(temp);
        m_resource->setListener(this);
        buildGradient(m_resource);
    }
    return m_resource;
}

void SVGGradientElement::resourceNotification() const
{
    // We're referenced by a "client", build the gradient now...
    buildGradient(m_resource);
}

void SVGGradientElement::rebuildStops() const
{
    if (m_resource && !ownerDocument()->parsing()) {
        Vector<KCGradientStop> stops;
         // FIXME: Manual style resolution is a hack
        RenderStyle* gradientStyle = const_cast<SVGGradientElement*>(this)->styleForRenderer(parent()->renderer());
        for (Node *n = firstChild(); n; n = n->nextSibling()) {
            SVGElement *element = svg_dynamic_cast(n);
            if (element && element->isGradientStop()) {
                SVGStopElement *stop = static_cast<SVGStopElement *>(element);
                float stopOffset = stop->offset()->baseVal();
                
                RenderStyle *stopStyle = document()->styleSelector()->styleForElement(stop, gradientStyle);
                Color c = stopStyle->svgStyle()->stopColor();
                float opacity = stopStyle->svgStyle()->stopOpacity();
                
                stops.append(makeGradientStop(stopOffset, makeRGBA(c.red(), c.green(), c.blue(), int(opacity * 255.))));
                stopStyle->deref(canvas()->renderArena());
            }
        }
        gradientStyle->deref(canvas()->renderArena());
        m_resource->setGradientStops(stops);
    }
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

