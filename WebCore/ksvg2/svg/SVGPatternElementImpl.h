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

#ifndef KSVG_SVGPatternElementImpl_H
#define KSVG_SVGPatternElementImpl_H

#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGLocatableImpl.h"
#include "SVGURIReferenceImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

#include <kcanvas/KCanvasTypes.h>
#include <kcanvas/KCanvasResourceListener.h>
#include <kcanvas/device/KRenderingPaintServer.h>

class KCanvas;
class KCanvasImage;
class KRenderingPaintServerPattern;

namespace KSVG
{
	class SVGAnimatedLengthImpl;
	class SVGPatternElementImpl;
	class SVGAnimatedEnumerationImpl;
	class SVGAnimatedTransformListImpl;
	class SVGPatternElementImpl : public SVGStyledElementImpl,
								  public SVGURIReferenceImpl,
								  public SVGTestsImpl,
								  public SVGLangSpaceImpl,
								  public SVGExternalResourcesRequiredImpl,
								  public SVGFitToViewBoxImpl,
								  public SVGLocatableImpl, // TODO : really needed?
								  public KCanvasResourceListener
	{
	public:
		SVGPatternElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix);
		virtual ~SVGPatternElementImpl();

		// 'SVGPatternElement' functions
		SVGAnimatedEnumerationImpl *patternUnits() const;
		SVGAnimatedEnumerationImpl *patternContentUnits() const;
		SVGAnimatedTransformListImpl *patternTransform() const;

		SVGAnimatedLengthImpl *x() const;
		SVGAnimatedLengthImpl *y() const;

		SVGAnimatedLengthImpl *width() const;
		SVGAnimatedLengthImpl *height() const;

		virtual void parseAttribute(KDOM::AttributeImpl *attr);

		const SVGStyledElementImpl *pushAttributeContext(const SVGStyledElementImpl *context);

		virtual void resourceNotification() const;
		virtual void notifyAttributeChange() const;

		// Derived from: 'SVGStyledElementImpl'
		virtual bool allowAttachChildren(KDOM::ElementImpl *) const { return false; }

		virtual bool implementsCanvasItem() const { return true; }
		virtual KCanvasItem *createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const;

		// 'virtual SVGLocatable' functions
		virtual SVGMatrixImpl *getCTM() const;

	protected:
		mutable SVGAnimatedLengthImpl *m_x;
		mutable SVGAnimatedLengthImpl *m_y;
		mutable SVGAnimatedLengthImpl *m_width;
		mutable SVGAnimatedLengthImpl *m_height;
		
		mutable SVGAnimatedEnumerationImpl *m_patternUnits;
		mutable SVGAnimatedEnumerationImpl *m_patternContentUnits;

		mutable SVGAnimatedTransformListImpl *m_patternTransform;

		mutable KCanvasImage *m_tile;
		mutable bool m_ignoreAttributeChanges;
		mutable KRenderingPaintServer *m_paintServer;
	};
};

#endif

// vim:ts=4:noet
