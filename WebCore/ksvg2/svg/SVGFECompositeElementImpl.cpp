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
#include "SVGFECompositeElementImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFECompositeElementImpl::SVGFECompositeElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix) : 
SVGFilterPrimitiveStandardAttributesImpl(doc, id, prefix)
{
	m_in1 = m_in2 = 0;
	m_k1 = m_k2 = m_k3 = m_k4 = 0;
	m_operator = 0;
	m_filterEffect = 0;
}

SVGFECompositeElementImpl::~SVGFECompositeElementImpl()
{
	if(m_in1)
		m_in1->deref();
	if(m_in2)
		m_in2->deref();
	if(m_operator)
		m_operator->deref();
	if(m_k1)
		m_k1->deref();
	if(m_k2)
		m_k2->deref();
	if(m_k3)
		m_k3->deref();
	if(m_k4)
		m_k4->deref();
}

SVGAnimatedStringImpl *SVGFECompositeElementImpl::in1() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

SVGAnimatedStringImpl *SVGFECompositeElementImpl::in2() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedStringImpl>(m_in2, dummy);
}

SVGAnimatedEnumerationImpl *SVGFECompositeElementImpl::_operator() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedEnumerationImpl>(m_operator, dummy);
}

SVGAnimatedNumberImpl *SVGFECompositeElementImpl::k1() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberImpl>(m_k1, dummy);
}

SVGAnimatedNumberImpl *SVGFECompositeElementImpl::k2() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberImpl>(m_k2, dummy);
}

SVGAnimatedNumberImpl *SVGFECompositeElementImpl::k3() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberImpl>(m_k3, dummy);
}

SVGAnimatedNumberImpl *SVGFECompositeElementImpl::k4() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedNumberImpl>(m_k4, dummy);
}

void SVGFECompositeElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMString value(attr->value());
	switch(id)
	{
		case ATTR_OPERATOR:
		{
			if(value == "over")
				_operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_OVER);
			else if(value == "in")
				_operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_IN);
			else if(value == "out")
				_operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_OUT);
			else if(value == "atop")
				_operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_ATOP);
			else if(value == "xor")
				_operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_XOR);
			else if(value == "arithmatic")
				_operator()->setBaseVal(SVG_FECOMPOSITE_OPERATOR_ARITHMETIC);
			break;
		}
		case ATTR_IN:
		{
			in1()->setBaseVal(value.implementation());
			break;
		}
		case ATTR_IN2:
		{
			in2()->setBaseVal(value.implementation());
			break;
		}
		default:
		{
			SVGFilterPrimitiveStandardAttributesImpl::parseAttribute(attr);
		}
	};
}

KCanvasItem *SVGFECompositeElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
	m_filterEffect = static_cast<KCanvasFEComposite *>(canvas->renderingDevice()->createFilterEffect(FE_COMPOSITE));
	m_filterEffect->setOperation((KCCompositeOperationType)(_operator()->baseVal() - 1));
	m_filterEffect->setIn(KDOM::DOMString(in1()->baseVal()).string());
	m_filterEffect->setIn2(KDOM::DOMString(in2()->baseVal()).string());
	setStandardAttributes(m_filterEffect);
	m_filterEffect->setK1(k1()->baseVal()->value());
	m_filterEffect->setK2(k2()->baseVal()->value());
	m_filterEffect->setK3(k3()->baseVal()->value());
	m_filterEffect->setK4(k4()->baseVal()->value());
	return 0;
}

KCanvasFilterEffect *SVGFECompositeElementImpl::filterEffect() const
{
	return m_filterEffect;
}

// vim:ts=4:noet
