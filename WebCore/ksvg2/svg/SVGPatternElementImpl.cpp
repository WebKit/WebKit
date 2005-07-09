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

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasMatrix.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/KCanvasImage.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerPattern.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGMatrixImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGTransformableImpl.h"
#include "SVGTransformListImpl.h"
#include "KCanvasRenderingStyle.h"
#include "SVGPatternElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedTransformListImpl.h"

using namespace KSVG;

SVGPatternElementImpl::SVGPatternElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix) : SVGStyledElementImpl(doc, id, prefix), SVGURIReferenceImpl(), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGFitToViewBoxImpl(), KCanvasResourceListener()
{
	m_patternUnits = 0;
	m_patternTransform = 0;
	m_patternContentUnits = 0;
	m_x = m_y = m_width = m_height = 0;

	m_tile = 0;
	m_paintServer = 0;
	m_ignoreAttributeChanges = false;
}

SVGPatternElementImpl::~SVGPatternElementImpl()
{
	if(m_x)
		m_x->deref();
	if(m_y)
		m_y->deref();
	if(m_width)
		m_width->deref();
	if(m_height)
		m_height->deref();
	if(m_patternUnits)
		m_patternUnits->deref();
	if(m_patternContentUnits)
		m_patternContentUnits->deref();
	if(m_patternTransform)
		m_patternTransform->deref();
}

SVGAnimatedEnumerationImpl *SVGPatternElementImpl::patternUnits() const
{
	if(!m_patternUnits)
	{
		lazy_create<SVGAnimatedEnumerationImpl>(m_patternUnits, this);
		m_patternUnits->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
	}

	return m_patternUnits;
}

SVGAnimatedEnumerationImpl *SVGPatternElementImpl::patternContentUnits() const
{
	if(!m_patternContentUnits)
	{
		lazy_create<SVGAnimatedEnumerationImpl>(m_patternContentUnits, this);
		m_patternContentUnits->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
	}

	return m_patternContentUnits;
}

SVGAnimatedLengthImpl *SVGPatternElementImpl::x() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGPatternElementImpl::y() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGPatternElementImpl::width() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGPatternElementImpl::height() const
{
	return lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedTransformListImpl *SVGPatternElementImpl::patternTransform() const
{
	return lazy_create<SVGAnimatedTransformListImpl>(m_patternTransform, this);
}

void SVGPatternElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMString value(attr->value());
	switch(id)
	{
		case ATTR_PATTERNUNITS:
		{
			if(value == "userSpaceOnUse")
				patternUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
			else if(value == "objectBoundingBox")
				patternUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
			break;
		}
		case ATTR_PATTERNCONTENTUNITS:
		{
			if(value == "userSpaceOnUse")
				patternContentUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
			else if(value == "objectBoundingBox")
				patternContentUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
			break;
		}
		case ATTR_PATTERNTRANSFORM:
		{
			SVGTransformListImpl *patternTransforms = patternTransform()->baseVal();
			SVGTransformableImpl::parseTransformAttribute(patternTransforms, value);
			break;
		}
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
			if(SVGURIReferenceImpl::parseAttribute(attr)) return;
			if(SVGTestsImpl::parseAttribute(attr)) return;
			if(SVGLangSpaceImpl::parseAttribute(attr)) return;
			if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
			if(SVGFitToViewBoxImpl::parseAttribute(attr)) return;

			SVGStyledElementImpl::parseAttribute(attr);
		}
	};
}

const SVGStyledElementImpl *SVGPatternElementImpl::pushAttributeContext(const SVGStyledElementImpl *context)
{
	// All attribute's contexts are equal (so just take the one from 'x').
	const SVGStyledElementImpl *restore = x()->baseVal()->context();

	x()->baseVal()->setContext(context);
	y()->baseVal()->setContext(context);
	width()->baseVal()->setContext(context);
	height()->baseVal()->setContext(context);

	return restore;
}

void SVGPatternElementImpl::resourceNotification() const
{
	// We're referenced by a "client", calculate the tile now...
	notifyAttributeChange();
}

void SVGPatternElementImpl::notifyAttributeChange() const
{
	if(!m_paintServer || !m_paintServer->activeClient())
		return;

	if(m_ignoreAttributeChanges)
		return;

	float w = width()->baseVal()->value();
	float h = height()->baseVal()->value();

	QSize newSize = QSize(qRound(w), qRound(h));
	if(m_tile && (m_tile->size() == newSize))
		return;

	m_ignoreAttributeChanges = true;

	// Find first pattern def that has children
	const KDOM::ElementImpl *target = this;

	const KDOM::NodeImpl *test = static_cast<const KDOM::NodeImpl *>(target);
	while(test && !test->hasChildNodes())
	{
		QString ref = KDOM::DOMString(href()->baseVal()).string();
		test = ownerDocument()->getElementById(ref.mid(1));
		if(test && test->id() == ID_PATTERN)
			target = static_cast<const KDOM::ElementImpl *>(test);
	}

	unsigned short savedPatternUnits = patternUnits()->baseVal();
	unsigned short savedPatternContentUnits = patternContentUnits()->baseVal();

	KRenderingPaintServer *refServer = 0;
	QString ref = KDOM::DOMString(href()->baseVal()).string();
	if(!ref.isEmpty())
	{
		SVGDocumentImpl *document = static_cast<SVGDocumentImpl *>(ownerDocument());
		KCanvas *canvas = (document ? document->canvas() : 0);
		Q_ASSERT(canvas != 0);
		
		refServer = canvas->registry()->getPaintServerById(ref.mid(1));
	}

	KCanvasMatrix patternTransformMatrix;
	if(patternTransform()->baseVal()->numberOfItems() > 0)
		patternTransformMatrix = KCanvasMatrix(patternTransform()->baseVal()->consolidate()->matrix()->qmatrix());

	if(refServer && refServer->type() == PS_PATTERN)
	{
		KRenderingPaintServerPattern *refPattern = static_cast<KRenderingPaintServerPattern *>(refServer);
		
		if(!hasAttribute("patternUnits"))
		{
			KDOM::DOMString value(target->getAttribute("patternUnits"));
			if(value == "userSpaceOnUse")
				patternUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
			else if(value == "objectBoundingBox")
				patternUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
		}
		
		if(!hasAttribute("patternContentUnits"))
		{
			KDOM::DOMString value(target->getAttribute("patternContentUnits"));
			if(value == "userSpaceOnUse")
				patternContentUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
			else if(value == "objectBoundingBox")
				patternContentUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
		}

		if(!hasAttribute("patternTransform"))
			patternTransformMatrix = refPattern->patternTransform();
	}

	SVGStyledElementImpl *activeElement = static_cast<SVGStyledElementImpl *>(m_paintServer->activeClient()->userData());

	bool bbox = (patternUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);

	const SVGStyledElementImpl *savedContext = 0;
	if(bbox)
	{
		if(activeElement)
			savedContext = const_cast<SVGPatternElementImpl *>(this)->pushAttributeContext(activeElement);
	}	

	delete m_tile;
	m_tile = static_cast<KCanvasImage *>(canvas()->renderingDevice()->createResource(RS_IMAGE));
	m_tile->init(newSize);

	KRenderingDeviceContext *patternContext = canvas()->renderingDevice()->contextForImage(m_tile);
	canvas()->renderingDevice()->pushContext(patternContext);
//	KCanvasCommonArgs args;
//	args.setCanvas(canvas());
//	args.setStyle(item->style());
	
	KRenderingPaintServerPattern *pattern = static_cast<KRenderingPaintServerPattern *>(m_paintServer);
	pattern->setX(x()->baseVal()->value());
	pattern->setY(y()->baseVal()->value());
	pattern->setWidth(width()->baseVal()->value());
	pattern->setHeight(height()->baseVal()->value());
	pattern->setPatternTransform(patternTransformMatrix);
	pattern->setTile(m_tile);

	for(KDOM::NodeImpl *n = target->firstChild(); n != 0; n = n->nextSibling())
	{
		SVGStyledElementImpl *e = dynamic_cast<SVGStyledElementImpl *>(n);
		KCanvasItem *item = (e ? e->canvasItem() : 0);
		if(item && item->style())
		{
			KCanvasMatrix savedMatrix = item->style()->objectMatrix();

			const SVGStyledElementImpl *savedContext = 0;
			if(patternContentUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
			{
				if(activeElement)
					savedContext = e->pushAttributeContext(activeElement);
			}

			// Take into account viewportElement's viewBox, if existant...
			if(viewportElement() && viewportElement()->id() == ID_SVG)
			{
				SVGSVGElementImpl *svgElement = static_cast<SVGSVGElementImpl *>(viewportElement());

				SVGMatrixImpl *svgCTM = svgElement->getCTM();
				svgCTM->ref();

				SVGMatrixImpl *ctm = SVGLocatableImpl::getCTM();
				ctm->ref();

				KCanvasMatrix newMatrix(svgCTM->qmatrix());
				newMatrix.multiply(savedMatrix);
				newMatrix.scale(1.0 / ctm->a(), 1.0 / ctm->d());

				item->style()->setObjectMatrix(newMatrix);

				ctm->deref();
				svgCTM->deref();
			}

			item->draw(QRect());

			if(savedContext)
				e->pushAttributeContext(savedContext);

			item->style()->setObjectMatrix(savedMatrix);
		}
	}

	if(savedContext)
		const_cast<SVGPatternElementImpl *>(this)->pushAttributeContext(savedContext);

	canvas()->renderingDevice()->popContext();
	delete patternContext;

	patternUnits()->setBaseVal(savedPatternUnits);
	patternContentUnits()->setBaseVal(savedPatternContentUnits);

	// Update all users of this resource...
	const KCanvasItemList &clients = pattern->clients();

	KCanvasItemList::ConstIterator it = clients.begin();
	KCanvasItemList::ConstIterator end = clients.end();

	for(; it != end; ++it)
	{
		const KCanvasItem *current = (*it);

		SVGStyledElementImpl *styled = (current ? static_cast<SVGStyledElementImpl *>(current->userData()) : 0);
		if(styled)
		{
			styled->setChanged(true);

			if(styled->canvasItem())
				styled->canvasItem()->invalidate();
		}
	}

	m_ignoreAttributeChanges = false;
}

KCanvasItem *SVGPatternElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *) const
{
	m_paintServer = canvas->renderingDevice()->createPaintServer(KCPaintServerType(PS_PATTERN));
	KRenderingPaintServerPattern *pserver = static_cast<KRenderingPaintServerPattern *>(m_paintServer);

	pserver->setListener(const_cast<SVGPatternElementImpl *>(this));

	canvas->registry()->addPaintServerById(getId().string(), pserver);
	return 0;
}

SVGMatrixImpl *SVGPatternElementImpl::getCTM() const
{
	SVGMatrixImpl *mat = SVGSVGElementImpl::createSVGMatrix();
	if(mat)
	{
		SVGMatrixImpl *viewBox = viewBoxToViewTransform(width()->baseVal()->value(),
														height()->baseVal()->value());

		viewBox->ref();
		mat->multiply(viewBox);
		viewBox->deref();
	}

	return mat;
}

// vim:ts=4:noet
