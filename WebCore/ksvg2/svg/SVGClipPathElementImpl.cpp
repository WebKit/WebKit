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

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasPath.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGDocumentImpl.h"
#include "SVGClipPathElementImpl.h"
#include "SVGAnimatedEnumerationImpl.h"

using namespace KSVG;

SVGClipPathElementImpl::SVGClipPathElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl()
{
	m_clipPathUnits = 0;
	m_clipper = 0;
}

SVGClipPathElementImpl::~SVGClipPathElementImpl()
{
	if(m_clipPathUnits)
		m_clipPathUnits->deref();
}

SVGAnimatedEnumerationImpl *SVGClipPathElementImpl::clipPathUnits() const
{
	if(!m_clipPathUnits)
	{
		lazy_create<SVGAnimatedEnumerationImpl>(m_clipPathUnits, this);
		m_clipPathUnits->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
	}

	return m_clipPathUnits;
}

void SVGClipPathElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMString value(attr->value());
	switch(id)
	{
		case ATTR_CLIPPATHUNITS:
		{
			if(value == "userSpaceOnUse")
				clipPathUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
			else if(value == "objectBoundingBox")
				clipPathUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
			break;
		}
		default:
		{
			if(SVGTestsImpl::parseAttribute(attr)) return;
			if(SVGLangSpaceImpl::parseAttribute(attr)) return;
			if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
			if(SVGTransformableImpl::parseAttribute(attr)) return;
			
			SVGStyledElementImpl::parseAttribute(attr);
		}
	};
}

void SVGClipPathElementImpl::close()
{
	if(!m_clipper)
	{
		SVGDocumentImpl *doc = static_cast<SVGDocumentImpl *>(ownerDocument());
		KCanvas *canvas = (doc ? doc->canvas() : 0);
		if(!canvas)
			return;

		m_clipper = static_cast<KCanvasClipper *>(canvas->renderingDevice()->createResource(RS_CLIPPER));
		canvas->registry()->addResourceById(getId().string(), m_clipper);
	}
	else
		m_clipper->resetClipData();

	bool bbox = clipPathUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

	for(KDOM::NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
	{
		SVGStyledElementImpl *e = dynamic_cast<SVGStyledElementImpl *>(n);
		if(e)
		{
			SVGRenderStyle *renderStyle = static_cast<SVGRenderStyle *>(e->renderStyle());
			m_clipper->addClipData(e->toPathData(), (KCWindRule) renderStyle->clipRule(), bbox);
		}
	}
}

// vim:ts=4:noet
