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

#include <kdom/impl/AttrImpl.h>
#include <kdom/Namespace.h>

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGPointListImpl.h"
#include "SVGPolyElementImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGPolyElementImpl::SVGPolyElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl(), SVGAnimatedPointsImpl(), SVGPolyParser()
{
	m_points = 0;
}

SVGPolyElementImpl::~SVGPolyElementImpl()
{
	if(m_points)
		m_points->deref();
}

SVGPointListImpl *SVGPolyElementImpl::points() const
{
	return lazy_create<SVGPointListImpl>(m_points, this);
}

SVGPointListImpl *SVGPolyElementImpl::animatedPoints() const
{
	return 0;
}

void SVGPolyElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	switch(id)
	{
		case ATTR_POINTS:
		{
			parsePoints(KDOM::DOMString(attr->value()).string());
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

void SVGPolyElementImpl::svgPolyTo(double x1, double y1, int) const
{
	points()->appendItem(new SVGPointImpl(x1, y1, this));
}

void SVGPolyElementImpl::notifyAttributeChange() const
{
	if(ownerDocument()->parsing())
		return;

	SVGStyledElementImpl::notifyAttributeChange();

	// Spec: Additionally, the 'points' attribute on the original element
	// accessed via the XML DOM (e.g., using the getAttribute() method call)
	// will reflect any changes made to points.
	KDOM::DOMString _points;
	int len = points()->numberOfItems();
	for(int i = 0; i < len; ++i)
	{
		SVGPointImpl *p = points()->getItem(i);
		_points += QString::fromLatin1("%1 %2 ").arg(p->x()).arg(p->y());
	}

	KDOM::DOMString p("points");
	KDOM::AttrImpl *attr = const_cast<SVGPolyElementImpl *>(this)->getAttributeNode(p.implementation());
	if(attr)
	{
		attr->setOwnerElement(0);
		attr->setValue(_points);
		attr->setOwnerElement(const_cast<SVGPolyElementImpl *>(this));
	}
}

// vim:ts=4:noet
