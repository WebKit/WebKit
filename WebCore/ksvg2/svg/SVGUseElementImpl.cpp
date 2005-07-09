/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include <kdom/Namespace.h>
#include <kdom/impl/AttrImpl.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGDocumentImpl.h"
#include "SVGGElementImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGUseElementImpl.h"
#include "SVGSymbolElementImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "KCanvasRenderingStyle.h"
#include "SVGAnimatedPreserveAspectRatioImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingStyle.h>
#include <kcanvas/device/KRenderingDevice.h>

using namespace KSVG;

SVGUseElementImpl::SVGUseElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl(), SVGURIReferenceImpl()
{
	m_x = m_y = m_width = m_height = 0;
}

SVGUseElementImpl::~SVGUseElementImpl()
{
	if(m_x)
		m_x->deref();
	if(m_y)
		m_y->deref();
	if(m_width)
		m_width->deref();
	if(m_height)
		m_height->deref();
}

SVGAnimatedLengthImpl *SVGUseElementImpl::x() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGUseElementImpl::y() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGUseElementImpl::width() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGUseElementImpl::height() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT, viewportElement());
}

void SVGUseElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMString value(attr->value());
	switch(id)
	{
		case ATTR_X:
		{
			x()->baseVal()->setValueAsString(value);
			break;
		}
		case ATTR_Y:
		{
			y()->baseVal()->setValueAsString(value);
			break;
		}
		case ATTR_WIDTH:
		{
			width()->baseVal()->setValueAsString(value);
			break;
		}
		case ATTR_HEIGHT:
		{
			height()->baseVal()->setValueAsString(value);
			break;
		}
		default:
		{
			if(SVGTestsImpl::parseAttribute(attr)) return;
			if(SVGLangSpaceImpl::parseAttribute(attr)) return;
			if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
			if(SVGTransformableImpl::parseAttribute(attr)) return;
			if(SVGURIReferenceImpl::parseAttribute(attr)) return;

			SVGStyledElementImpl::parseAttribute(attr);
		}
	};
}

void SVGUseElementImpl::close()
{
	QString ref = KDOM::DOMString(href()->baseVal()).string();
	QString targetId = SVGURIReferenceImpl::getTarget(ref);
	SVGElementImpl *target = dynamic_cast<SVGElementImpl *>(ownerDocument()->getElementById(targetId));
	if(!target)
		return;
		
	float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
	float _w = width()->baseVal()->value(), _h = height()->baseVal()->value();
	
	QString w = QString::number(_w);
	QString h = QString::number(_h);
	
	QString trans = QString::fromLatin1("translate(%1, %2)").arg(_x).arg(_y);	
	if(target->id() == ID_SYMBOL)
	{
		SVGElementImpl *dummy = new SVGSVGElementImpl(ownerDocument(), ID_SVG, KDOM::DOMString());
		dummy->setAttributeNS(KDOM::NS_SVG, "width", w);
		dummy->setAttributeNS(KDOM::NS_SVG, "height", h);
		
		SVGSymbolElementImpl *symbol = static_cast<SVGSymbolElementImpl *>(target);
		dummy->setAttributeNS(KDOM::NS_SVG, "viewBox", symbol->getAttributeNS(KDOM::NS_SVG, "viewBox"));
		target->cloneChildNodes(dummy, ownerDocument());
		
		SVGElementImpl *dummy2 = new SVGDummyElementImpl(ownerDocument(), ID_G, KDOM::DOMString());
		dummy2->setAttributeNS(KDOM::NS_SVG, "transform", trans);
		
		appendChild(dummy2);		
		dummy2->appendChild(dummy);
	}
	else if(target->id() == ID_SVG)
	{
		SVGDummyElementImpl *dummy = new SVGDummyElementImpl(ownerDocument(), ID_G, KDOM::DOMString());
		dummy->setAttributeNS(KDOM::NS_SVG, "transform", trans);
		
		SVGElementImpl *root = static_cast<SVGElementImpl *>(target->cloneNode(true, ownerDocument()));
		if(hasAttribute("width"))
			root->setAttributeNS(KDOM::NS_SVG, "width", w);
			
		if(hasAttribute("height"))
			root->setAttributeNS(KDOM::NS_SVG, "height", h);
			
		appendChild(dummy);
		dummy->appendChild(root);
	}
	else
	{
		SVGDummyElementImpl *dummy = new SVGDummyElementImpl(ownerDocument(), ID_G, KDOM::DOMString());
		dummy->setAttributeNS(KDOM::NS_SVG, "transform", trans);
		
		KDOM::NodeImpl *root = target->cloneNode(true, ownerDocument());
		
		appendChild(dummy);
		dummy->appendChild(root);
	}
}

bool SVGUseElementImpl::hasChildNodes() const
{
	return false;
}

KCanvasItem *SVGUseElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
	return KCanvasCreator::self()->createContainer(canvas, style);
}

// vim:ts=4:noet
