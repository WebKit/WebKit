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

#ifndef KSVG_SVGPathElement_H
#define KSVG_SVGPathElement_H

#include "SVGAnimatedPathData.h"
#include <ksvg2/ecma/SVGLookup.h>
#include <ksvg2/dom/SVGElement.h>
#include <ksvg2/dom/SVGTests.h>
#include <ksvg2/dom/SVGLangSpace.h>
#include <ksvg2/dom/SVGExternalResourcesRequired.h>
#include <ksvg2/dom/SVGStylable.h>
#include <ksvg2/dom/SVGTransformable.h>

namespace KSVG
{
	class SVGPoint;
	class SVGPathSegClosePath;
	class SVGPathSegLinetoAbs;
	class SVGPathSegLinetoRel;
	class SVGPathSegLinetoHorizontalAbs;
	class SVGPathSegLinetoHorizontalRel;
	class SVGPathSegLinetoVerticalAbs;
	class SVGPathSegLinetoVerticalRel;
	class SVGPathSegMovetoAbs;
	class SVGPathSegMovetoRel;
	class SVGPathSegArcAbs;
	class SVGPathSegArcRel;
	class SVGPathSegCurvetoCubicAbs;
	class SVGPathSegCurvetoCubicRel;
	class SVGPathSegCurvetoCubicSmoothAbs;
	class SVGPathSegCurvetoCubicSmoothRel;
	class SVGPathSegCurvetoQuadraticAbs;
	class SVGPathSegCurvetoQuadraticRel;
	class SVGPathSegCurvetoQuadraticSmoothAbs;
	class SVGPathSegCurvetoQuadraticSmoothRel;
	class SVGPathElementImpl;
	class SVGAnimatedNumber;
	class SVGPathElement : public SVGElement,
						   public SVGTests,
						   public SVGLangSpace,
						   public SVGExternalResourcesRequired,
						   public SVGStylable,
						   public SVGTransformable,
						   public SVGAnimatedPathData
	{ 
	public:
		SVGPathElement();
		explicit SVGPathElement(SVGPathElementImpl *);
		SVGPathElement(const SVGPathElement &);
		SVGPathElement(const KDOM::Node &other);
		virtual ~SVGPathElement();

		// Operators
		SVGPathElement &operator=(const SVGPathElement &other);
		SVGPathElement &operator=(const KDOM::Node &other);

		SVGAnimatedNumber pathLength() const;
		float getTotalLength();
		SVGPoint getPointAtLength(float distance);
		unsigned long getPathSegAtLength(float distance);

		SVGPathSegClosePath createSVGPathSegClosePath();
		SVGPathSegMovetoAbs createSVGPathSegMovetoAbs(float x,float y);
		SVGPathSegMovetoRel createSVGPathSegMovetoRel(float x,float y);
		SVGPathSegLinetoAbs createSVGPathSegLinetoAbs(float x,float y);
		SVGPathSegLinetoRel createSVGPathSegLinetoRel(float x,float y);
		SVGPathSegCurvetoCubicAbs createSVGPathSegCurvetoCubicAbs(float x,float y,float x1,float y1,float x2,float y2);
		SVGPathSegCurvetoCubicRel createSVGPathSegCurvetoCubicRel(float x,float y,float x1,float y1,float x2,float y2);
		SVGPathSegCurvetoQuadraticAbs createSVGPathSegCurvetoQuadraticAbs(float x,float y,float x1,float y1);
		SVGPathSegCurvetoQuadraticRel createSVGPathSegCurvetoQuadraticRel(float x,float y,float x1,float y1);
		SVGPathSegArcAbs createSVGPathSegArcAbs(float x,float y,float r1,float r2,float angle,bool largeArcFlag,bool sweepFlag);
		SVGPathSegArcRel createSVGPathSegArcRel(float x,float y,float r1,float r2,float angle,bool largeArcFlag,bool sweepFlag);
		SVGPathSegLinetoHorizontalAbs createSVGPathSegLinetoHorizontalAbs(float x);
		SVGPathSegLinetoHorizontalRel createSVGPathSegLinetoHorizontalRel(float x);
		SVGPathSegLinetoVerticalAbs createSVGPathSegLinetoVerticalAbs(float y);
		SVGPathSegLinetoVerticalRel createSVGPathSegLinetoVerticalRel(float y);
		SVGPathSegCurvetoCubicSmoothAbs createSVGPathSegCurvetoCubicSmoothAbs(float x,float y,float x2,float y2);
		SVGPathSegCurvetoCubicSmoothRel createSVGPathSegCurvetoCubicSmoothRel(float x,float y,float x2,float y2);
		SVGPathSegCurvetoQuadraticSmoothAbs createSVGPathSegCurvetoQuadraticSmoothAbs(float x,float y);
		SVGPathSegCurvetoQuadraticSmoothRel createSVGPathSegCurvetoQuadraticSmoothRel(float x,float y);

		// Internal
		KSVG_INTERNAL(SVGPathElement)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGPathElementProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGPathElementProtoFunc, SVGPathElement)

#endif

// vim:ts=4:noet
