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

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGLineElementImpl.h"
#include "SVGAnimatedLengthImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>

using namespace KSVG;

SVGLineElementImpl::SVGLineElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl()
{
	m_x1 = m_y1 = m_x2 = m_y2 = 0;
}

SVGLineElementImpl::~SVGLineElementImpl()
{
	if(m_x1)
		m_x1->deref();
	if(m_y1)
		m_y1->deref();
	if(m_x2)
		m_x2->deref();
	if(m_y2)
		m_y2->deref();
}

SVGAnimatedLengthImpl *SVGLineElementImpl::x1() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_x1, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGLineElementImpl::y1() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_y1, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGLineElementImpl::x2() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_x2, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGLineElementImpl::y2() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_y2, this, LM_HEIGHT, viewportElement());
}

void SVGLineElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMString value(attr->value());
	switch(id)
	{
		case ATTR_X1:
		{
			x1()->baseVal()->setValueAsString(value);
			break;
		}
		case ATTR_Y1:
		{
			y1()->baseVal()->setValueAsString(value);
			break;
		}
		case ATTR_X2:
		{
			x2()->baseVal()->setValueAsString(value);
			break;
		}
		case ATTR_Y2:
		{
			y2()->baseVal()->setValueAsString(value);
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

KCPathDataList SVGLineElementImpl::toPathData() const
{
	float _x1 = x1()->baseVal()->value(), _y1 = y1()->baseVal()->value();
	float _x2 = x2()->baseVal()->value(), _y2 = y2()->baseVal()->value();

	return KCanvasCreator::self()->createLine(_x1, _y1, _x2, _y2);
}

const SVGStyledElementImpl *SVGLineElementImpl::pushAttributeContext(const SVGStyledElementImpl *context)
{
	// All attribute's contexts are equal (so just take the one from 'x1').
	const SVGStyledElementImpl *restore = x1()->baseVal()->context();

	x1()->baseVal()->setContext(context);
	y1()->baseVal()->setContext(context);
	x2()->baseVal()->setContext(context);
	y2()->baseVal()->setContext(context);
	
	SVGStyledElementImpl::pushAttributeContext(context);
	return restore;
}

// vim:ts=4:noet
