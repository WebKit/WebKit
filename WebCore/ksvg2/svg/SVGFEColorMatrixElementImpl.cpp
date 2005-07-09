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

#include <qstringlist.h>

#include <kdom/impl/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEColorMatrixElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedNumberListImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFEColorMatrixElementImpl::SVGFEColorMatrixElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix) : 
SVGFilterPrimitiveStandardAttributesImpl(doc, id, prefix)
{
	m_in1 = 0;
	m_type = 0;
	m_values = 0;
	m_filterEffect = 0;
}

SVGFEColorMatrixElementImpl::~SVGFEColorMatrixElementImpl()
{
	if(m_in1)
		m_in1->deref();
	if(m_type)
		m_type->deref();
	if(m_values)
		m_values->deref();
}

SVGAnimatedStringImpl *SVGFEColorMatrixElementImpl::in1() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

SVGAnimatedEnumerationImpl *SVGFEColorMatrixElementImpl::type() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedEnumerationImpl>(m_type, dummy);
}

SVGAnimatedNumberListImpl *SVGFEColorMatrixElementImpl::values() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberListImpl>(m_values, dummy);
}

void SVGFEColorMatrixElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMString value(attr->value());
	switch(id)
	{
		case ATTR_TYPE:
		{
			if(value == "matrix")
				type()->setBaseVal(SVG_FECOLORMATRIX_TYPE_MATRIX);
			else if(value == "saturate")
				type()->setBaseVal(SVG_FECOLORMATRIX_TYPE_SATURATE);
			else if(value == "hueRotate")
				type()->setBaseVal(SVG_FECOLORMATRIX_TYPE_HUEROTATE);
			else if(value == "luminanceToAlpha")
				type()->setBaseVal(SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA);
			break;
		}
		case ATTR_IN:
		{
			in1()->setBaseVal(value.implementation());
			break;
		}
		case ATTR_VALUES:
		{
			values()->baseVal()->parse(value.string(), this);
			break;
		}
		default:
		{
			SVGFilterPrimitiveStandardAttributesImpl::parseAttribute(attr);
		}
	};
}

KCanvasItem *SVGFEColorMatrixElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *) const
{
	m_filterEffect = static_cast<KCanvasFEColorMatrix *>(canvas->renderingDevice()->createFilterEffect(FE_COLOR_MATRIX));
	m_filterEffect->setIn(KDOM::DOMString(in1()->baseVal()).string());
	setStandardAttributes(m_filterEffect);
	QValueList<float> _values;
	SVGNumberListImpl *numbers = values()->baseVal();
	unsigned int nr = numbers->numberOfItems();
	for(unsigned int i = 0;i < nr;i++)
		_values.append(numbers->getItem(i)->value());
	m_filterEffect->setValues(_values);
	m_filterEffect->setType((KCColorMatrixType)(type()->baseVal() - 1));
	return 0;
}

KCanvasFilterEffect *SVGFEColorMatrixElementImpl::filterEffect() const
{
	return m_filterEffect;
}

// vim:ts=4:noet
