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

#ifndef KSVG_SVGLineElementImpl_H
#define KSVG_SVGLineElementImpl_H

#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGTransformableImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

namespace KSVG
{
	class SVGAnimatedLengthImpl;
	class SVGLineElementImpl : public SVGStyledElementImpl,
							   public SVGTestsImpl,
							   public SVGLangSpaceImpl,
							   public SVGExternalResourcesRequiredImpl,
							   public SVGTransformableImpl
	{
	public:
		SVGLineElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix);
		virtual ~SVGLineElementImpl();

		// 'SVGLineElement' functions
		SVGAnimatedLengthImpl *x1() const;
		SVGAnimatedLengthImpl *y1() const;
		SVGAnimatedLengthImpl *x2() const;
		SVGAnimatedLengthImpl *y2() const;

		virtual void parseAttribute(KDOM::AttributeImpl *attr);

		virtual bool implementsCanvasItem() const { return true; }
		virtual KCPathDataList toPathData() const;

		virtual const SVGStyledElementImpl *pushAttributeContext(const SVGStyledElementImpl *context);

	private:
		mutable SVGAnimatedLengthImpl *m_x1;
		mutable SVGAnimatedLengthImpl *m_y1;
		mutable SVGAnimatedLengthImpl *m_x2;
		mutable SVGAnimatedLengthImpl *m_y2;
	};
};

#endif

// vim:ts=4:noet
