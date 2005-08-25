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
#include "SVGFilterPrimitiveStandardAttributesImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGFilterElementImpl.h"

#include <kcanvas/KCanvasFilters.h>

using namespace KSVG;

SVGFilterPrimitiveStandardAttributesImpl::SVGFilterPrimitiveStandardAttributesImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix)
{
	m_x = m_y = m_width = m_height = 0;
	m_result = 0;
}

SVGFilterPrimitiveStandardAttributesImpl::~SVGFilterPrimitiveStandardAttributesImpl()
{
	if(m_x)
		m_x->deref();
	if(m_y)
		m_y->deref();
	if(m_width)
		m_width->deref();
	if(m_height)
		m_height->deref();
	if(m_result)
		m_result->deref();
}

SVGAnimatedLengthImpl *SVGFilterPrimitiveStandardAttributesImpl::x() const
{
	// Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
	const SVGStyledElementImpl *context = dynamic_cast<const SVGStyledElementImpl *>(this);
	return lazy_create<SVGAnimatedLengthImpl>(m_x, context, LM_WIDTH);
}

SVGAnimatedLengthImpl *SVGFilterPrimitiveStandardAttributesImpl::y() const
{
	// Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
	const SVGStyledElementImpl *context = dynamic_cast<const SVGStyledElementImpl *>(this);
	return lazy_create<SVGAnimatedLengthImpl>(m_y, context, LM_HEIGHT);
}

SVGAnimatedLengthImpl *SVGFilterPrimitiveStandardAttributesImpl::width() const
{
	const SVGStyledElementImpl *context = dynamic_cast<const SVGStyledElementImpl *>(this);
	// Spec : If the attribute is not specified, the effect is as if a value of "100%" were specified.
	if(!m_width)
	{
	 	lazy_create<SVGAnimatedLengthImpl>(m_width, context, LM_WIDTH);
		m_width->baseVal()->setValueAsString(KDOM::DOMString("100%").handle());
		return m_width;
	}

	return m_width;
}

SVGAnimatedLengthImpl *SVGFilterPrimitiveStandardAttributesImpl::height() const
{
	const SVGStyledElementImpl *context = dynamic_cast<const SVGStyledElementImpl *>(this);
	// Spec : If the attribute is not specified, the effect is as if a value of "100%" were specified.
	if(!m_height)
	{
	 	lazy_create<SVGAnimatedLengthImpl>(m_height, context, LM_HEIGHT);
		m_height->baseVal()->setValueAsString(KDOM::DOMString("100%").handle());
		return m_height;
	}

	return m_height;
}

SVGAnimatedStringImpl *SVGFilterPrimitiveStandardAttributesImpl::result() const
{
	const SVGStyledElementImpl *context = dynamic_cast<const SVGStyledElementImpl *>(this);
	return lazy_create<SVGAnimatedStringImpl>(m_result, context);
}

void SVGFilterPrimitiveStandardAttributesImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMStringImpl *value = attr->value();
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
		case ATTR_RESULT:
		{
			result()->setBaseVal(value);
			break;
		}
		default:
		{
			return SVGStyledElementImpl::parseAttribute(attr);
		}
	};

}

void SVGFilterPrimitiveStandardAttributesImpl::setStandardAttributes(KCanvasFilterEffect *filterEffect) const
{
	if (!filterEffect) return;
	bool bbox = false;
	if(parentNode() && parentNode()->id() == ID_FILTER)
		bbox = static_cast<SVGFilterElementImpl *>(parentNode())->primitiveUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

	x()->baseVal()->setBboxRelative(bbox);
	y()->baseVal()->setBboxRelative(bbox);
	width()->baseVal()->setBboxRelative(bbox);
	height()->baseVal()->setBboxRelative(bbox);
	float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
	float _width = width()->baseVal()->value(), _height = height()->baseVal()->value();
	if(bbox)
		filterEffect->setSubRegion(QRect(int(_x * 100.f), int(_y * 100.f), int(_width * 100.f), int(_height * 100.f)));
	else
		filterEffect->setSubRegion(QRect(int(_x), int(_y), int(_width), int(_height)));

	filterEffect->setResult(KDOM::DOMString(result()->baseVal()).string());
}

// vim:ts=4:noet
