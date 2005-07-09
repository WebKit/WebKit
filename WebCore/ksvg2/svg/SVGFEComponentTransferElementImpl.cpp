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

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEComponentTransferElementImpl.h"
#include "SVGFEFuncRElementImpl.h"
#include "SVGFEFuncGElementImpl.h"
#include "SVGFEFuncBElementImpl.h"
#include "SVGFEFuncAElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFEComponentTransferElementImpl::SVGFEComponentTransferElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix) : 
SVGFilterPrimitiveStandardAttributesImpl(doc, id, prefix)
{
	m_in1 = 0;
	m_filterEffect = 0;
}

SVGFEComponentTransferElementImpl::~SVGFEComponentTransferElementImpl()
{
	if(m_in1)
		m_in1->deref();
}

SVGAnimatedStringImpl *SVGFEComponentTransferElementImpl::in1() const
{
	SVGStyledElementImpl *dummy = 0;
	return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

void SVGFEComponentTransferElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMString value(attr->value());
	switch(id)
	{
		case ATTR_IN:
		{
			in1()->setBaseVal(value.implementation());
			break;
		}
		default:
		{
			SVGFilterPrimitiveStandardAttributesImpl::parseAttribute(attr);
		}
	};
}

KCanvasItem *SVGFEComponentTransferElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
	m_filterEffect = static_cast<KCanvasFEComponentTransfer *>(canvas->renderingDevice()->createFilterEffect(FE_COMPONENT_TRANSFER));
	m_filterEffect->setIn(KDOM::DOMString(in1()->baseVal()).string());
	setStandardAttributes(m_filterEffect);
	return 0;
}

KCanvasFilterEffect *SVGFEComponentTransferElementImpl::filterEffect() const
{
	return m_filterEffect;
}

void SVGFEComponentTransferElementImpl::close()
{
	for(KDOM::NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
	{
		KCComponentTransferFunction func;
		if(n->id() == ID_FEFUNCR)
		{
			SVGFEFuncRElementImpl *funcR = static_cast<SVGFEFuncRElementImpl *>(n);
			func.type = (KCComponentTransferType)(funcR->type()->baseVal() - 1);
			func.slope = funcR->slope()->baseVal()->value();
			func.intercept = funcR->intercept()->baseVal()->value();
			func.amplitude = funcR->amplitude()->baseVal()->value();
			func.exponent = funcR->exponent()->baseVal()->value();
			func.offset = funcR->offset()->baseVal()->value();
			m_filterEffect->setRedFunction(func);
		}
		else if(n->id() == ID_FEFUNCG)
		{
			SVGFEFuncGElementImpl *funcG = static_cast<SVGFEFuncGElementImpl *>(n);
			func.type = (KCComponentTransferType)(funcG->type()->baseVal() - 1);
			func.slope = funcG->slope()->baseVal()->value();
			func.intercept = funcG->intercept()->baseVal()->value();
			func.amplitude = funcG->amplitude()->baseVal()->value();
			func.exponent = funcG->exponent()->baseVal()->value();
			func.offset = funcG->offset()->baseVal()->value();
			m_filterEffect->setGreenFunction(func);
		}
		else if(n->id() == ID_FEFUNCB)
		{
			SVGFEFuncBElementImpl *funcB = static_cast<SVGFEFuncBElementImpl *>(n);
			func.type = (KCComponentTransferType)(funcB->type()->baseVal() - 1);
			func.slope = funcB->slope()->baseVal()->value();
			func.intercept = funcB->intercept()->baseVal()->value();
			func.amplitude = funcB->amplitude()->baseVal()->value();
			func.exponent = funcB->exponent()->baseVal()->value();
			func.offset = funcB->offset()->baseVal()->value();
			m_filterEffect->setBlueFunction(func);
		}
		else if(n->id() == ID_FEFUNCA)
		{
			SVGFEFuncAElementImpl *funcA = static_cast<SVGFEFuncAElementImpl *>(n);
			func.type = (KCComponentTransferType)(funcA->type()->baseVal() - 1);
			func.slope = funcA->slope()->baseVal()->value();
			func.intercept = funcA->intercept()->baseVal()->value();
			func.amplitude = funcA->amplitude()->baseVal()->value();
			func.exponent = funcA->exponent()->baseVal()->value();
			func.offset = funcA->offset()->baseVal()->value();
			m_filterEffect->setAlphaFunction(func);
		}
	}
}

// vim:ts=4:noet
