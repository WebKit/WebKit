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

#ifndef KSVG_SVGStyledElementImpl_H
#define KSVG_SVGStyledElementImpl_H

#include "SVGElementImpl.h"
#include "SVGStylableImpl.h"

#include <qwmatrix.h>
#include <kcanvas/KCanvasPath.h>
#include <kdom/css/impl/RenderStyle.h>

class KCanvas;
class KCanvasView;
class KCanvasItem;
class KRenderingStyle;

namespace KSVG
{
	class KCanvasRenderingStyle;
	class SVGStyledElementImpl;
	class SVGCSSStyleDeclarationImpl;
	class SVGStyledElementImpl : public SVGElementImpl,
								 public SVGStylableImpl
	{
	public:
		SVGStyledElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix);
		virtual ~SVGStyledElementImpl();

		// 'SVGStylable' functions
		virtual SVGAnimatedStringImpl *className() const;
		virtual KDOM::CSSStyleDeclarationImpl *style();
		virtual KDOM::CSSStyleDeclarationImpl *pa() const;
		virtual KDOM::CSSValueImpl *getPresentationAttribute(KDOM::DOMStringImpl *name);

		virtual void attach();
		virtual void detach();

		virtual bool allowAttachChildren(KDOM::ElementImpl *) const { return true; }

		// This needs to be implemented.
		virtual bool implementsCanvasItem() const { return false; }
		virtual KCPathDataList toPathData() const { return KCPathDataList(); }
		virtual KCanvasItem *createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const;

		virtual void parseAttribute(KDOM::AttributeImpl *attr);

		void updateCTM(const QWMatrix &ctm);

		KCanvasItem *canvasItem() const;
		virtual void notifyAttributeChange() const;
		virtual void recalcStyle(StyleChange = NoChange);

		// Imagine we're a <rect> inside of a <pattern> section with patternContentUnits="objectBoundingBox"
		// and our 'width' attribute is set to 50%. When the pattern gets referenced it knows the "bbox"
		// of it's user and has to push the "active client's bbox" as new attribute context to all attributes
		// of the 'rect'. This function also returns the old attribute context, to be able to restore it...
		virtual const SVGStyledElementImpl *pushAttributeContext(const SVGStyledElementImpl *context);

	protected:
		friend class SVGDocumentImpl; // Needs renderStyle accesss...
		friend class SVGClipPathElementImpl; // Needs renderStyle access..

		KCanvas *canvas() const;
		KCanvasView *canvasView() const;

		virtual KDOM::RenderStyle *renderStyle() const;
		virtual void finalizeStyle(KCanvasRenderingStyle *style, bool needFillStrokeUpdate = true);

		void setStyle(KDOM::RenderStyle *newStyle);
		void updateCanvasItem(); // Handles "path data" object changes... (not for style/transform!)

		KCanvasItem *m_canvasItem;

	private:
		mutable SVGCSSStyleDeclarationImpl *m_pa;
		mutable SVGAnimatedStringImpl *m_className;

		mutable KDOM::RenderStyle *m_renderStyle;

		// Optimized updating logic
		bool m_updateVectorial : 1;
	};
};

#endif

// vim:ts=4:noet
