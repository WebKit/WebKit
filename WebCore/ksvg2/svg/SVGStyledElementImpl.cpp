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

#include <kdom/kdom.h>
#include <kdom/impl/AttrImpl.h>
#include <kdom/impl/domattrs.h>
#include <kdom/impl/CDFInterface.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasItem.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "KSVGSettings.h"
#include "SVGMatrixImpl.h"
#include "SVGRenderStyle.h"
#include "SVGElementImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGCSSStyleSelector.h"
#include "SVGTransformableImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "KCanvasRenderingStyle.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGCSSStyleDeclarationImpl.h"

#include "svgattrs.h"
#include <ksvg2/css/impl/cssproperties.h>

using namespace KSVG;

SVGStyledElementImpl::SVGStyledElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix)
: SVGElementImpl(doc, id, prefix), SVGStylableImpl(), m_pa(0), m_className(0)
{
	m_canvasItem = 0;
	m_renderStyle = 0;
	m_updateVectorial = false;
}

SVGStyledElementImpl::~SVGStyledElementImpl()
{
	if(m_className)
		m_className->deref();
	if(m_pa)
		m_pa->deref();

	delete m_renderStyle;
}

SVGAnimatedStringImpl *SVGStyledElementImpl::className() const
{
	if(!m_className)
	{
		m_className = new SVGAnimatedStringImpl(0); // TODO: use notification context?
		m_className->ref();
	}

	return m_className;
}

KDOM::CSSStyleDeclarationImpl *SVGStyledElementImpl::style()
{
	return styleRules();
}

KDOM::CSSStyleDeclarationImpl *SVGStyledElementImpl::pa() const
{
	if(!m_pa)
	{
		m_pa = new SVGCSSStyleDeclarationImpl(ownerDocument()->implementation()->cdfInterface(), 0);
		m_pa->ref();
	}

	return m_pa;
}

KDOM::CSSValueImpl *SVGStyledElementImpl::getPresentationAttribute(const KDOM::DOMString &name)
{
	return pa()->getPropertyCSSValue(name);
}

KCanvasItem *SVGStyledElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
	KCPathDataList pathData = toPathData();
	if(pathData.isEmpty())
		return 0;

	KCanvasItem *item = KCanvasCreator::self()->createPathItem(canvas, style, pathData);

	// Associate 'KCanvasItem' with 'SVGStyledElementImpl'
	item->setUserData(const_cast<SVGStyledElementImpl *>(this));

	return item;
}

void SVGStyledElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	KDOM::DOMStringImpl *value = attr->val();
	switch(id)
	{
		case ATTR_CLASS:
		{
			className()->setBaseVal(value);
			break;
		}
		case ATTR_STYLE:
		{
			style()->setProperty(KDOM::DOMString(value));
			break;
		}
		default:
		{
			KDOM::CDFInterface *interface = (ownerDocument() ? ownerDocument()->implementation()->cdfInterface() : 0);
			if(interface)
			{
				QString qProp = QString::fromLatin1(interface->getAttrName(id));
				int svgPropId = interface->getPropertyID(qProp.ascii(), qProp.length());
				if(svgPropId > 0)
				{
					if(value)
						pa()->setProperty(svgPropId, KDOM::DOMString(value));
					else
						pa()->removeProperty(svgPropId);

					if(!ownerDocument()->parsing())
						setChanged(true);

					return;
				}
			}

			SVGElementImpl::parseAttribute(attr);
		}
	};
}

KDOM::RenderStyle *SVGStyledElementImpl::renderStyle() const
{
	if(!m_renderStyle)
	{
		kdDebug(26002) << "[SVGStyledElementImpl::renderStyle] Updating rendering style of " << localName() << " ..." << endl;
		SVGDocumentImpl *doc = static_cast<SVGDocumentImpl *>(ownerDocument());
		if(!doc)
			return 0;

		KDOM::CSSStyleSelector *styleSelector = doc->styleSelector();
		if(!styleSelector)
			return 0;

		m_renderStyle = styleSelector->styleForElement(const_cast<SVGStyledElementImpl *>(this));
	}
	else
		kdDebug(26002) << "[SVGStyledElementImpl::renderStyle] Getting cached rendering style of " << localName() << " ..." << endl;

	return m_renderStyle;
}

void SVGStyledElementImpl::notifyAttributeChange() const
{
	// For most cases we need to handle vectorial data changes (ie. rect x changed)
	if(!ownerDocument()->parsing())
	{
		// TODO: Use a more optimized way of updating, means not calling updateCanvasItem() here!
		const_cast<SVGStyledElementImpl *>(this)->m_updateVectorial = true;
		const_cast<SVGStyledElementImpl *>(this)->updateCanvasItem();
	}
}

void SVGStyledElementImpl::finalizeStyle(KCanvasRenderingStyle *style, bool needFillStrokeUpdate)
{
	if(needFillStrokeUpdate)
	{
		style->updateFill(m_canvasItem);
		style->updateStroke(m_canvasItem);
	}
}

void SVGStyledElementImpl::attach()
{
	kdDebug(26002) << "[SVGStyledElementImpl::attach] About to attach canvas item for element " << localName() << endl;

	SVGDocumentImpl *doc = static_cast<SVGDocumentImpl *>(ownerDocument());
	KCanvas *canvas = (doc ? doc->canvas() : 0);	
	if(canvas && implementsCanvasItem())
	{
		SVGRenderStyle *svgStyle = static_cast<SVGRenderStyle *>(renderStyle());
		KCanvasRenderingStyle *renderingStyle = new KCanvasRenderingStyle(canvas, svgStyle);

		m_canvasItem = createCanvasItem(canvas, renderingStyle);
		if(m_canvasItem)
		{
			finalizeStyle(renderingStyle);

			// Apply transformations if possible...
			SVGLocatableImpl *transformable = dynamic_cast<SVGLocatableImpl *>(this);
			if(transformable)
			{
				SVGMatrixImpl *ctm = transformable->getScreenCTM();
				renderingStyle->setObjectMatrix(KCanvasMatrix(ctm->qmatrix()));
				ctm->deref();
			}

			SVGStyledElementImpl *parent = dynamic_cast<SVGStyledElementImpl *>(parentNode());
			if(parent && parent->canvasItem() && parent->allowAttachChildren(this))
				parent->canvasItem()->appendItem(m_canvasItem);

#ifndef APPLE_COMPILE_HACK
			KSVGSettings settings;
			if(settings.progressiveRendering() && m_canvasItem->isVisible())
				m_canvasItem->invalidate(); // Causes defered redraw...
#endif
		}
		else
		{
			delete renderingStyle;
			renderingStyle = NULL;
		}

		kdDebug(26002) << "[SVGStyledElementImpl::attach] Attachted canvas item: " << m_canvasItem << endl;
	}
	else
		kdDebug(26002) << "[SVGStyledElementImpl::attach] Unable to attach canvas item. (not necessarily wrong!)" << endl;

	KDOM::NodeBaseImpl::attach();
}

void SVGStyledElementImpl::detach()
{
	kdDebug(26002) << "[SVGStyledElementImpl::detach] About to detach canvas item for element " << localName() << endl;

	if(m_canvasItem)
	{
		SVGStyledElementImpl *parent = dynamic_cast<SVGStyledElementImpl *>(parentNode());
		if(parent && parent->canvasItem())
			parent->canvasItem()->removeItem(m_canvasItem);
	}

	kdDebug(26002) << "[SVGStyledElementImpl::detach] Detached canvas item: " << m_canvasItem << endl;
	m_canvasItem = 0;

	delete m_renderStyle;
	m_renderStyle = 0;

	KDOM::NodeBaseImpl::detach();
}

KCanvasItem *SVGStyledElementImpl::canvasItem() const
{
	return m_canvasItem;
}

KCanvas *SVGStyledElementImpl::canvas() const
{
	SVGDocumentImpl *doc = static_cast<SVGDocumentImpl *>(ownerDocument());
	return doc ? doc->canvas() : 0;
}

KCanvasView *SVGStyledElementImpl::canvasView() const
{
	SVGDocumentImpl *doc = static_cast<SVGDocumentImpl *>(ownerDocument());
	return doc ? doc->canvasView() : 0;
}

void SVGStyledElementImpl::updateCanvasItem()
{
	if(!m_canvasItem || !m_updateVectorial)
		return;

	if(canvas())
	{
		bool renderSection = false;

		SVGStyledElementImpl *parent = dynamic_cast<SVGStyledElementImpl *>(parentNode());
		if(parent && parent->canvasItem() && parent->allowAttachChildren(this))
			renderSection = true;

		// Invalidate before the change...
		if(renderSection)
			m_canvasItem->invalidate();

		KCanvasUserData newPath = KCanvasCreator::self()->createCanvasPathData(canvas(), toPathData());
		m_canvasItem->changePath(newPath);

		// Invalidate after the change...
		if(renderSection)
			m_canvasItem->invalidate();

		m_updateVectorial = false;
	}
}

void SVGStyledElementImpl::recalcStyle(StyleChange change)
{
	KDOM::RenderStyle *_style = m_renderStyle;
	bool hasParentRenderer = parentNode() ? parentNode()->attached() : false;

	if(hasParentRenderer && (change >= Inherit) || changed())
	{
		KDOM::EDisplay oldDisplay = _style ? _style->display() : KDOM::DS_NONE;

		KDOM::RenderStyle *newStyle = ownerDocument()->styleSelector()->styleForElement(this);
		newStyle->ref();
		
		StyleChange ch = diff(_style, newStyle);
		if(ch != NoChange)
		{
			if(oldDisplay != newStyle->display())
			{
				if(attached()) detach();
				// ### uuhm, suboptimal. style gets calculated again
				attach();
				// attach recalulates the style for all children. No need to do it twice.
				setChanged(false);
				setHasChangedChild(false);
				newStyle->deref();
				return;
			}

			if(newStyle)
				setStyle(newStyle);
		}

		newStyle->deref();

		if(change != Force)
		{
			if(ownerDocument()->usesDescendantRules())
				change = Force;
			else
				change = ch;
		}
	}

	for(NodeImpl *n = firstChild(); n; n = n->nextSibling())
	{
		if(change >= Inherit || n->hasChangedChild() || n->changed())
			n->recalcStyle(change);
	}
}

const SVGStyledElementImpl *SVGStyledElementImpl::pushAttributeContext(const SVGStyledElementImpl *)
{
	if(canvas())
	{
		KCanvasUserData newPath = KCanvasCreator::self()->createCanvasPathData(canvas(), toPathData());
		m_canvasItem->changePath(newPath);
	}

	return 0;
}

void SVGStyledElementImpl::setStyle(KDOM::RenderStyle *newStyle)
{
	KDOM_SAFE_SET(m_renderStyle, newStyle);

	KCanvasRenderingStyle *style = (m_canvasItem ? static_cast<KCanvasRenderingStyle *>(m_canvasItem->style()) : 0);
	if(style)
	{
		bool renderSection = false;

		SVGStyledElementImpl *parent = dynamic_cast<SVGStyledElementImpl *>(parentNode());
		if(parent && parent->canvasItem() && parent->allowAttachChildren(this))
			renderSection = true;

		// Invalidate before the change...
		if(renderSection)
			m_canvasItem->invalidate();

		style->removeClipPaths();
		style->updateStyle(static_cast<SVGRenderStyle *>(m_renderStyle), m_canvasItem);
		finalizeStyle(style, false);

		// Invalidate after the change...
		if(renderSection)
			m_canvasItem->invalidate();
	}
}

void SVGStyledElementImpl::updateCTM(const QWMatrix &ctm)
{
	if(!m_canvasItem)
		return;

	KCanvas *canvas = (m_canvasItem ? m_canvasItem->canvas() : 0);
	if(canvas)
	{
		// Invalidate before the change...
		m_canvasItem->invalidate();

		KCanvasRenderingStyle *style = static_cast<KCanvasRenderingStyle *>(m_canvasItem->style());
		style->setObjectMatrix(KCanvasMatrix(ctm));

		// Invalidate after the change...
		m_canvasItem->invalidate();
	}
}

// vim:ts=4:noet
