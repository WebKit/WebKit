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

#include <kdom/impl/AttrImpl.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGDocumentImpl.h"
#include "SVGTransformableImpl.h"
#include "SVGTransformListImpl.h"
#include "SVGGradientElementImpl.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedTransformListImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

using namespace KSVG;

SVGGradientElementImpl::SVGGradientElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix) : SVGStyledElementImpl(doc, id, prefix), SVGURIReferenceImpl(), SVGExternalResourcesRequiredImpl()
{
	m_spreadMethod = 0;
	m_gradientUnits = 0;
	m_gradientTransform = 0;
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

void SVGGradientElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMString value(attr->value());
	switch(id)
	{
		case ATTR_GRADIENTUNITS:
		{
			if(value == "userSpaceOnUse")
				gradientUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
			else if(value == "objectBoundingBox")
				gradientUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
			break;
		}
		case ATTR_GRADIENTTRANSFORM:
		{
			SVGTransformListImpl *gradientTransforms = gradientTransform()->baseVal();
			SVGTransformableImpl::parseTransformAttribute(gradientTransforms, value);
			break;
		}
		case ATTR_SPREADMETHOD:
		{
			if(value == "reflect")
				spreadMethod()->setBaseVal(SVG_SPREADMETHOD_REFLECT);
			else if(value == "repeat")
				spreadMethod()->setBaseVal(SVG_SPREADMETHOD_REPEAT);
			else if(value == "pad")
				spreadMethod()->setBaseVal(SVG_SPREADMETHOD_PAD);
			break;
		}
		default:
		{
			if(SVGURIReferenceImpl::parseAttribute(attr)) return;
			if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
			
			SVGStyledElementImpl::parseAttribute(attr);
		}
	};
}

void SVGGradientElementImpl::notifyAttributeChange() const
{
	if(ownerDocument()->parsing())
		return;

	// Update clients of this gradient resource...
	SVGDocumentImpl *document = static_cast<SVGDocumentImpl *>(ownerDocument());
	KCanvas *canvas = (document ? document->canvas() : 0);
	if(canvas)
	{
		KRenderingPaintServerGradient *pserver = static_cast<KRenderingPaintServerGradient *>(canvas->registry()->getPaintServerById(KDOM::DOMString(getId()).string()));
		if(pserver)
		{
			buildGradient(pserver, canvas);
			
			pserver->invalidate();  // should this be added to build gradient?
			
			const KCanvasItemList &clients = pserver->clients();

			KCanvasItemList::ConstIterator it = clients.begin();
			KCanvasItemList::ConstIterator end = clients.end();

			for(; it != end; ++it)
			{
				const KCanvasItem *current = (*it);
				SVGStyledElementImpl *styled = (current ? static_cast<SVGStyledElementImpl *>(current->userData()) : 0);
				if(styled)
					styled->setChanged(true);
			}
		}
	}
}

// vim:ts=4:noet
