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

#include <kdom/DOMString.h>
#include <kdom/impl/AttrImpl.h>
#include <kdom/impl/NamedAttrMapImpl.h>

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGStringListImpl.h"
#include "SVGViewElementImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGZoomAndPanImpl.h"

using namespace KSVG;

SVGViewElementImpl::SVGViewElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGExternalResourcesRequiredImpl(),
SVGFitToViewBoxImpl(), SVGZoomAndPanImpl()
{
	m_viewTarget = 0;
}

SVGViewElementImpl::~SVGViewElementImpl()
{
	if(m_viewTarget)
		m_viewTarget->deref();
}

SVGStringListImpl *SVGViewElementImpl::viewTarget() const
{
	return lazy_create<SVGStringListImpl>(m_viewTarget);
}

void SVGViewElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMString value(attr->value());
	switch(id)
	{
		case ATTR_VIEWTARGET:
		{
			viewTarget()->reset(value.string());
			break;
		}
		default:
		{
			if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
			if(SVGFitToViewBoxImpl::parseAttribute(attr)) return;
			if(SVGZoomAndPanImpl::parseAttribute(attr)) return;

			SVGStyledElementImpl::parseAttribute(attr);
		}
	};
}

