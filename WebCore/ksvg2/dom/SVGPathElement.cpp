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

#include <kdom/ecma/Ecma.h>

#include "SVGPathElement.h"
#include "SVGPathElementImpl.h"
#include "SVGPoint.h"
#include "SVGPathSegClosePath.h"
#include "SVGPathSegLineto.h"
#include "SVGPathSegLinetoHorizontal.h"
#include "SVGPathSegLinetoVertical.h"
#include "SVGPathSegMoveto.h"
#include "SVGPathSegArc.h"
#include "SVGPathSegCurvetoCubic.h"
#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGPathSegCurvetoQuadraticSmooth.h"
#include "SVGAnimatedNumber.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"

#include "SVGConstants.h"
#include "SVGPathElement.lut.h"
using namespace KSVG;

/*
@begin SVGPathElement::s_hashTable 3
 pathLength	SVGPathElementConstants::PathLength	DontDelete|ReadOnly
@end
@begin SVGPathElementProto::s_hashTable 23
 getTotalLength								SVGPathElementConstants::GetTotalLength							DontDelete|Function 0
 getPointAtLength							SVGPathElementConstants::GetPointAtLength						DontDelete|Function 1
 getPathSegAtLength							SVGPathElementConstants::GetPathSegAtLength						DontDelete|Function 1
 createSVGPathSegClosePath					SVGPathElementConstants::CreateSVGPathSegClosePath				DontDelete|Function 0
 createSVGPathSegMovetoAbs					SVGPathElementConstants::CreateSVGPathSegMovetoAbs				DontDelete|Function 2
 createSVGPathSegMovetoRel					SVGPathElementConstants::CreateSVGPathSegMovetoRel				DontDelete|Function 2
 createSVGPathSegLinetoAbs					SVGPathElementConstants::CreateSVGPathSegLinetoAbs				DontDelete|Function 2
 createSVGPathSegLinetoRel					SVGPathElementConstants::CreateSVGPathSegLinetoRel				DontDelete|Function 2
 createSVGPathSegArcAbs						SVGPathElementConstants::CreateSVGPathSegArcAbs					DontDelete|Function 7
 createSVGPathSegArcRel						SVGPathElementConstants::CreateSVGPathSegArcRel					DontDelete|Function 7
 createSVGPathSegCurvetoCubicAbs			SVGPathElementConstants::CreateSVGPathSegCurvetoCubicAbs			DontDelete|Function 6
 createSVGPathSegCurvetoCubicRel			SVGPathElementConstants::CreateSVGPathSegCurvetoCubicRel			DontDelete|Function 6
 createSVGPathSegCurvetoQuadraticAbs		SVGPathElementConstants::CreateSVGPathSegCurvetoQuadraticAbs		DontDelete|Function 4
 createSVGPathSegCurvetoQuadraticRel		SVGPathElementConstants::CreateSVGPathSegCurvetoQuadraticRel		DontDelete|Function 4
 createSVGPathSegLinetoHorizontalAbs		SVGPathElementConstants::CreateSVGPathSegLinetoHorizontalAbs		DontDelete|Function 1
 createSVGPathSegLinetoHorizontalRel		SVGPathElementConstants::CreateSVGPathSegLinetoHorizontalRel		DontDelete|Function 1
 createSVGPathSegLinetoVerticalAbs			SVGPathElementConstants::CreateSVGPathSegLinetoVerticalAbs		DontDelete|Function 1
 createSVGPathSegLinetoVerticalRel			SVGPathElementConstants::CreateSVGPathSegLinetoVerticalRel		DontDelete|Function 1
 createSVGPathSegCurvetoCubicSmoothAbs			SVGPathElementConstants::CreateSVGPathSegCurvetoCubicSmoothAbs			DontDelete|Function 4
 createSVGPathSegCurvetoCubicSmoothRel			SVGPathElementConstants::CreateSVGPathSegCurvetoCubicSmoothRel			DontDelete|Function 4
 createSVGPathSegCurvetoQuadraticSmoothAbs		SVGPathElementConstants::CreateSVGPathSegCurvetoQuadraticSmoothAbs		DontDelete|Function 2
 createSVGPathSegCurvetoQuadraticSmoothRel		SVGPathElementConstants::CreateSVGPathSegCurvetoQuadraticSmoothRel		DontDelete|Function 2
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGPathElement", SVGPathElementProto, SVGPathElementProtoFunc)

ValueImp *SVGPathElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE
	
	switch(token)
	{
		case SVGPathElementConstants::PathLength:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, pathLength());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

ValueImp *SVGPathElementProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	SVGPathElement obj(cast(exec, thisObj));
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGPathElementConstants::GetTotalLength:
			return Number(obj.getTotalLength());
		case SVGPathElementConstants::GetPointAtLength:
			return KDOM::safe_cache<SVGPoint>(exec, obj.getPointAtLength(args[0]->toNumber(exec)));
		case SVGPathElementConstants::GetPathSegAtLength:
			return Number(obj.getPathSegAtLength(args[0]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegClosePath:
			return KDOM::safe_cache<SVGPathSeg>(exec, obj.createSVGPathSegClosePath());
		case SVGPathElementConstants::CreateSVGPathSegMovetoAbs:
			return KDOM::safe_cache<SVGPathSegMovetoAbs>(exec, obj.createSVGPathSegMovetoAbs(args[0]->toNumber(exec), args[1]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegMovetoRel:
			return KDOM::safe_cache<SVGPathSegMovetoRel>(exec, obj.createSVGPathSegMovetoRel(args[0]->toNumber(exec), args[1]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegLinetoAbs:
			return KDOM::safe_cache<SVGPathSegLinetoAbs>(exec, obj.createSVGPathSegLinetoAbs(args[0]->toNumber(exec), args[1]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegLinetoRel:
			return KDOM::safe_cache<SVGPathSegLinetoRel>(exec, obj.createSVGPathSegLinetoRel(args[0]->toNumber(exec), args[1]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegCurvetoCubicAbs:
			return KDOM::safe_cache<SVGPathSegCurvetoCubicAbs>(exec, obj.createSVGPathSegCurvetoCubicAbs(args[0]->toNumber(exec), args[1]->toNumber(exec), args[2]->toNumber(exec), args[3]->toNumber(exec), args[4]->toNumber(exec), args[5]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegCurvetoCubicRel:
			return KDOM::safe_cache<SVGPathSegCurvetoCubicRel>(exec, obj.createSVGPathSegCurvetoCubicRel(args[0]->toNumber(exec), args[1]->toNumber(exec), args[2]->toNumber(exec), args[3]->toNumber(exec), args[4]->toNumber(exec), args[5]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegCurvetoQuadraticAbs:
			return KDOM::safe_cache<SVGPathSegCurvetoQuadraticAbs>(exec, obj.createSVGPathSegCurvetoQuadraticAbs(args[0]->toNumber(exec), args[1]->toNumber(exec), args[2]->toNumber(exec), args[3]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegCurvetoQuadraticRel:
			return KDOM::safe_cache<SVGPathSegCurvetoQuadraticRel>(exec, obj.createSVGPathSegCurvetoQuadraticRel(args[0]->toNumber(exec), args[1]->toNumber(exec), args[2]->toNumber(exec), args[3]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegArcAbs:
			return KDOM::safe_cache<SVGPathSegArcAbs>(exec, obj.createSVGPathSegArcAbs(args[0]->toNumber(exec), args[1]->toNumber(exec), args[2]->toNumber(exec), args[3]->toNumber(exec), args[4]->toNumber(exec), args[5]->toBoolean(exec), args[6]->toBoolean(exec)));
		case SVGPathElementConstants::CreateSVGPathSegArcRel:
			return KDOM::safe_cache<SVGPathSegArcRel>(exec, obj.createSVGPathSegArcRel(args[0]->toNumber(exec), args[1]->toNumber(exec), args[2]->toNumber(exec), args[3]->toNumber(exec), args[4]->toNumber(exec), args[5]->toBoolean(exec), args[6]->toBoolean(exec)));
		case SVGPathElementConstants::CreateSVGPathSegLinetoHorizontalAbs:
			return KDOM::safe_cache<SVGPathSegLinetoHorizontalAbs>(exec, obj.createSVGPathSegLinetoHorizontalAbs(args[0]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegLinetoHorizontalRel:
			return KDOM::safe_cache<SVGPathSegLinetoHorizontalRel>(exec, obj.createSVGPathSegLinetoHorizontalRel(args[0]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegLinetoVerticalAbs:
			return KDOM::safe_cache<SVGPathSegLinetoVerticalAbs>(exec, obj.createSVGPathSegLinetoVerticalAbs(args[0]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegLinetoVerticalRel:
			return KDOM::safe_cache<SVGPathSegLinetoVerticalRel>(exec, obj.createSVGPathSegLinetoVerticalRel(args[0]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegCurvetoCubicSmoothAbs:
			return KDOM::safe_cache<SVGPathSegCurvetoCubicSmoothAbs>(exec, obj.createSVGPathSegCurvetoCubicSmoothAbs(args[0]->toNumber(exec), args[1]->toNumber(exec), args[2]->toNumber(exec), args[3]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegCurvetoCubicSmoothRel:
			return KDOM::safe_cache<SVGPathSegCurvetoCubicSmoothRel>(exec, obj.createSVGPathSegCurvetoCubicSmoothRel(args[0]->toNumber(exec), args[1]->toNumber(exec), args[2]->toNumber(exec), args[3]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegCurvetoQuadraticSmoothAbs:
			return KDOM::safe_cache<SVGPathSegCurvetoQuadraticSmoothAbs>(exec, obj.createSVGPathSegCurvetoQuadraticSmoothAbs(args[0]->toNumber(exec), args[1]->toNumber(exec)));
		case SVGPathElementConstants::CreateSVGPathSegCurvetoQuadraticSmoothRel:
			return KDOM::safe_cache<SVGPathSegCurvetoQuadraticSmoothRel>(exec, obj.createSVGPathSegCurvetoQuadraticSmoothRel(args[0]->toNumber(exec), args[1]->toNumber(exec)));
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGPathElementImpl *>(d))

SVGPathElement SVGPathElement::null;

SVGPathElement::SVGPathElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGAnimatedPathData()
{
}

SVGPathElement::SVGPathElement(SVGPathElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i), SVGAnimatedPathData(i)
{
}

SVGPathElement::SVGPathElement(const SVGPathElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGAnimatedPathData()
{
	(*this) = other;
}

SVGPathElement::SVGPathElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGAnimatedPathData()
{
	(*this) = other;
}

SVGPathElement::~SVGPathElement()
{
}

SVGPathElement &SVGPathElement::operator=(const SVGPathElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGTransformable::operator=(other);
	SVGAnimatedPathData::operator=(other);
	return *this;
}

SVGPathElement &SVGPathElement::operator=(const KDOM::Node &other)
{
	SVGPathElementImpl *ohandle = static_cast<SVGPathElementImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
		{
			if(d)
				d->deref();
				
			d = 0;
		}
		else
		{
			SVGElement::operator=(other);
			SVGTests::operator=(ohandle);
			SVGLangSpace::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
			SVGStylable::operator=(ohandle);
			SVGTransformable::operator=(ohandle);
			SVGAnimatedPathData::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedNumber SVGPathElement::pathLength() const
{
	return SVGAnimatedNumber(impl->pathLength());
}

float SVGPathElement::getTotalLength()
{
	return impl->getTotalLength();
}

SVGPoint SVGPathElement::getPointAtLength(float distance)
{
	return SVGPoint(impl->getPointAtLength(distance));
}

unsigned long SVGPathElement::getPathSegAtLength(float distance)
{
	return impl->getPathSegAtLength(distance);
}

SVGPathSegClosePath SVGPathElement::createSVGPathSegClosePath()
{
	return SVGPathSegClosePath(impl->createSVGPathSegClosePath());
}

SVGPathSegMovetoAbs SVGPathElement::createSVGPathSegMovetoAbs(float x,float y)
{
	return SVGPathSegMovetoAbs(impl->createSVGPathSegMovetoAbs(x, y));
}

SVGPathSegMovetoRel SVGPathElement::createSVGPathSegMovetoRel(float x,float y)
{
	return SVGPathSegMovetoRel(impl->createSVGPathSegMovetoRel(x, y));
}

SVGPathSegLinetoAbs SVGPathElement::createSVGPathSegLinetoAbs(float x,float y)
{
	return SVGPathSegLinetoAbs(impl->createSVGPathSegLinetoAbs(x, y));
}

SVGPathSegLinetoRel SVGPathElement::createSVGPathSegLinetoRel(float x,float y)
{
	return SVGPathSegLinetoRel(impl->createSVGPathSegLinetoRel(x, y));
}

SVGPathSegCurvetoCubicAbs SVGPathElement::createSVGPathSegCurvetoCubicAbs(float x,float y,float x1,float y1,float x2,float y2)
{
	return SVGPathSegCurvetoCubicAbs(impl->createSVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2));
}

SVGPathSegCurvetoCubicRel SVGPathElement::createSVGPathSegCurvetoCubicRel(float x,float y,float x1,float y1,float x2,float y2)
{
	return SVGPathSegCurvetoCubicRel(impl->createSVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2));
}

SVGPathSegCurvetoQuadraticAbs SVGPathElement::createSVGPathSegCurvetoQuadraticAbs(float x,float y,float x1,float y1)
{
	return SVGPathSegCurvetoQuadraticAbs(impl->createSVGPathSegCurvetoQuadraticAbs(x, y, x1, y1));
}

SVGPathSegCurvetoQuadraticRel SVGPathElement::createSVGPathSegCurvetoQuadraticRel(float x,float y,float x1,float y1)
{
	return SVGPathSegCurvetoQuadraticRel(impl->createSVGPathSegCurvetoQuadraticRel(x, y, x1, y1));
}

SVGPathSegArcAbs SVGPathElement::createSVGPathSegArcAbs(float x,float y,float r1,float r2,float angle,bool largeArcFlag,bool sweepFlag)
{
	return SVGPathSegArcAbs(impl->createSVGPathSegArcAbs(x, y, r1, r2, angle, largeArcFlag, sweepFlag));
}

SVGPathSegArcRel SVGPathElement::createSVGPathSegArcRel(float x,float y,float r1,float r2,float angle,bool largeArcFlag,bool sweepFlag)
{
	return SVGPathSegArcRel(impl->createSVGPathSegArcRel(x, y, r1, r2, angle, largeArcFlag, sweepFlag));
}

SVGPathSegLinetoHorizontalAbs SVGPathElement::createSVGPathSegLinetoHorizontalAbs(float x)
{
	return SVGPathSegLinetoHorizontalAbs(impl->createSVGPathSegLinetoHorizontalAbs(x));
}

SVGPathSegLinetoHorizontalRel SVGPathElement::createSVGPathSegLinetoHorizontalRel(float x)
{
	return SVGPathSegLinetoHorizontalRel(impl->createSVGPathSegLinetoHorizontalRel(x));
}

SVGPathSegLinetoVerticalAbs SVGPathElement::createSVGPathSegLinetoVerticalAbs(float y)
{
	return SVGPathSegLinetoVerticalAbs(impl->createSVGPathSegLinetoVerticalAbs(y));
}

SVGPathSegLinetoVerticalRel SVGPathElement::createSVGPathSegLinetoVerticalRel(float y)
{
	return SVGPathSegLinetoVerticalRel(impl->createSVGPathSegLinetoVerticalRel(y));
}

SVGPathSegCurvetoCubicSmoothAbs SVGPathElement::createSVGPathSegCurvetoCubicSmoothAbs(float x,float y,float x2,float y2)
{
	return SVGPathSegCurvetoCubicSmoothAbs(impl->createSVGPathSegCurvetoCubicSmoothAbs(x, y, x2, y2));
}

SVGPathSegCurvetoCubicSmoothRel SVGPathElement::createSVGPathSegCurvetoCubicSmoothRel(float x,float y,float x2,float y2)
{
	return SVGPathSegCurvetoCubicSmoothRel(impl->createSVGPathSegCurvetoCubicSmoothRel(x, y, x2, y2));
}

SVGPathSegCurvetoQuadraticSmoothAbs SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothAbs(float x,float y)
{
	return SVGPathSegCurvetoQuadraticSmoothAbs(impl->createSVGPathSegCurvetoQuadraticSmoothAbs(x, y));
}

SVGPathSegCurvetoQuadraticSmoothRel SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothRel(float x,float y)
{
	return SVGPathSegCurvetoQuadraticSmoothRel(impl->createSVGPathSegCurvetoQuadraticSmoothRel(x, y));
}

// vim:ts=4:noet
