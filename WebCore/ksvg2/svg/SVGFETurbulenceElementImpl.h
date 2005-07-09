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

#ifndef KSVG_SVGFETurbulenceElementImpl_H
#define KSVG_SVGFETurbulenceElementImpl_H

#include "SVGFilterPrimitiveStandardAttributesImpl.h"

class KCanvasFETurbulence;
class KCanvasFilterEffect;

namespace KSVG
{
	class SVGAnimatedIntegerImpl;
	class SVGAnimatedNumberImpl;
	class SVGAnimatedEnumerationImpl;

	class SVGFETurbulenceElementImpl : public SVGFilterPrimitiveStandardAttributesImpl
	{
	public:
		SVGFETurbulenceElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix);
		virtual ~SVGFETurbulenceElementImpl();

		// 'SVGFETurbulenceElement' functions
		SVGAnimatedNumberImpl *baseFrequencyX() const;
		SVGAnimatedNumberImpl *baseFrequencyY() const;
		SVGAnimatedIntegerImpl *numOctaves() const;
		SVGAnimatedNumberImpl *seed() const;
		SVGAnimatedEnumerationImpl *stitchTiles() const;
		SVGAnimatedEnumerationImpl *type() const;

		// Derived from: 'ElementImpl'
		virtual void parseAttribute(KDOM::AttributeImpl *attr);

		virtual KCanvasItem *createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const;

		virtual bool implementsCanvasItem() const { return true; }

		virtual KCanvasFilterEffect *filterEffect() const;

	private:
		mutable SVGAnimatedNumberImpl *m_baseFrequencyX;
		mutable SVGAnimatedNumberImpl *m_baseFrequencyY;
		mutable SVGAnimatedIntegerImpl *m_numOctaves;
		mutable SVGAnimatedNumberImpl *m_seed;
		mutable SVGAnimatedEnumerationImpl *m_stitchTiles;
		mutable SVGAnimatedEnumerationImpl *m_type;
		mutable KCanvasFETurbulence *m_filterEffect;
	};
};

#endif

// vim:ts=4:noet
