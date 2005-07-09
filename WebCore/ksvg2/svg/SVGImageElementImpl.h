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

#ifndef KSVG_SVGImageElementImpl_H
#define KSVG_SVGImageElementImpl_H

#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGURIReferenceImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGTransformableImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"
#include <kdom/cache/KDOMCachedImage.h>
#include <kdom/cache/KDOMCachedDocument.h>
#include <kdom/cache/KDOMCachedObjectClient.h>

namespace KSVG
{
	class SVGAnimatedPreserveAspectRatioImpl;
	class SVGAnimatedLengthImpl;
	class SVGDocumentImpl;

	class SVGImageElementImpl : public SVGStyledElementImpl,
								public SVGTestsImpl,
								public SVGLangSpaceImpl,
								public SVGExternalResourcesRequiredImpl,
								public SVGTransformableImpl,
								public SVGURIReferenceImpl,
								public KDOM::CachedObjectClient
	{
	public:
		SVGImageElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id,  const KDOM::DOMString &prefix);
		virtual ~SVGImageElementImpl();

		// 'SVGImageElement' functions
		SVGAnimatedLengthImpl *x() const;
		SVGAnimatedLengthImpl *y() const;

		SVGAnimatedLengthImpl *width() const;
		SVGAnimatedLengthImpl *height() const;

		SVGAnimatedPreserveAspectRatioImpl *preserveAspectRatio() const;

		virtual void parseAttribute(KDOM::AttributeImpl *attr);

		virtual bool implementsCanvasItem() const { return true; }
		virtual KCanvasItem *createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const;

		virtual void notifyFinished(KDOM::CachedObject *finishedObj);

	protected:
		virtual void finalizeStyle(KCanvasRenderingStyle *style, bool needFillStrokeUpdate = true);

	private:
		mutable SVGAnimatedLengthImpl *m_x;
		mutable SVGAnimatedLengthImpl *m_y;
		mutable SVGAnimatedLengthImpl *m_width;
		mutable SVGAnimatedLengthImpl *m_height;
		mutable SVGAnimatedPreserveAspectRatioImpl *m_preserveAspectRatio;
		mutable KDOM::CachedDocument *m_cachedDocument;
		KDOM::CachedImage *m_cachedImage;
		SVGDocumentImpl *m_svgDoc;
	};
};

#endif

// vim:ts=4:noet
