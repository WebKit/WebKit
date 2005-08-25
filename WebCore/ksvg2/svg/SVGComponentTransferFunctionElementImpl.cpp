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

#include <qstringlist.h>

#include <kdom/impl/AttrImpl.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGComponentTransferFunctionElementImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedNumberListImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGComponentTransferFunctionElementImpl::SVGComponentTransferFunctionElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix) : 
SVGElementImpl(doc, id, prefix)
{
	m_type = 0;
	m_tableValues = 0;
	m_slope = 0;
	m_intercept = 0;
	m_amplitude = 0;
	m_exponent = 0;
	m_offset = 0;
}

SVGComponentTransferFunctionElementImpl::~SVGComponentTransferFunctionElementImpl()
{
	if(m_type)
		m_type->deref();
	if(m_tableValues)
		m_tableValues->deref();
	if(m_slope)
		m_slope->deref();
	if(m_intercept)
		m_intercept->deref();
	if(m_amplitude)
		m_amplitude->deref();
	if(m_exponent)
		m_exponent->deref();
	if(m_offset)
		m_offset->deref();
}

SVGAnimatedEnumerationImpl *SVGComponentTransferFunctionElementImpl::type() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedEnumerationImpl>(m_type, dummy);
}

SVGAnimatedNumberListImpl *SVGComponentTransferFunctionElementImpl::tableValues() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberListImpl>(m_tableValues, dummy);
}

SVGAnimatedNumberImpl *SVGComponentTransferFunctionElementImpl::slope() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberImpl>(m_slope, dummy);
}

SVGAnimatedNumberImpl *SVGComponentTransferFunctionElementImpl::intercept() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberImpl>(m_intercept, dummy);
}

SVGAnimatedNumberImpl *SVGComponentTransferFunctionElementImpl::amplitude() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberImpl>(m_amplitude, dummy);
}

SVGAnimatedNumberImpl *SVGComponentTransferFunctionElementImpl::exponent() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberImpl>(m_exponent, dummy);
}

SVGAnimatedNumberImpl *SVGComponentTransferFunctionElementImpl::offset() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberImpl>(m_offset, dummy);
}

void SVGComponentTransferFunctionElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMString value(attr->value());
	switch(id)
	{
		case ATTR_TYPE:
		{
			if(value == "identity")
				type()->setBaseVal(SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY);
			else if(value == "table")
				type()->setBaseVal(SVG_FECOMPONENTTRANSFER_TYPE_TABLE);
			else if(value == "discrete")
				type()->setBaseVal(SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE);
			else if(value == "linear")
				type()->setBaseVal(SVG_FECOMPONENTTRANSFER_TYPE_LINEAR);
			else if(value == "gamma")
				type()->setBaseVal(SVG_FECOMPONENTTRANSFER_TYPE_GAMMA);
			break;
		}
		case ATTR_VALUES:
		{
			tableValues()->baseVal()->parse(value.string());
			break;
		}
		case ATTR_SLOPE:
		{
			slope()->setBaseVal(value.string().toFloat());
			break;
		}
		case ATTR_INTERCEPT:
		{
			intercept()->setBaseVal(value.string().toFloat());
			break;
		}
		case ATTR_AMPLITUDE:
		{
			amplitude()->setBaseVal(value.string().toFloat());
			break;
		}
		case ATTR_EXPONENT:
		{
			exponent()->setBaseVal(value.string().toFloat());
			break;
		}
		case ATTR_OFFSET:
		{
			offset()->setBaseVal(value.string().toFloat());
			break;
		}
		default:
		{
			SVGElementImpl::parseAttribute(attr);
		}
	};
}

// vim:ts=4:noet
