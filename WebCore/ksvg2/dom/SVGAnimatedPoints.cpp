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

#include "SVGElement.h"
#include "SVGAnimatedPoints.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGElementImpl.h"
#include "SVGAnimatedPointsImpl.h"
#include "SVGPointList.h"

#include "SVGConstants.h"
#include "SVGAnimatedPoints.lut.h"
using namespace KSVG;

/*
@begin SVGAnimatedPoints::s_hashTable 3
 points			SVGAnimatedPointsConstants::Points			DontDelete|ReadOnly
 animatedPoints	SVGAnimatedPointsConstants::AnimatedPoints	DontDelete|ReadOnly
@end
*/

Value SVGAnimatedPoints::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedPointsConstants::Points:
			return KDOM::safe_cache<SVGPointList>(exec, points());
		case SVGAnimatedPointsConstants::AnimatedPoints:
			return KDOM::safe_cache<SVGPointList>(exec, animatedPoints());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

SVGAnimatedPoints::SVGAnimatedPoints() : impl(0)
{
}

SVGAnimatedPoints::SVGAnimatedPoints(SVGAnimatedPointsImpl *i) : impl(i)
{
}

SVGAnimatedPoints::SVGAnimatedPoints(const SVGAnimatedPoints &other) : impl(0)
{
	(*this) = other;
}

SVGAnimatedPoints::~SVGAnimatedPoints()
{
}

SVGAnimatedPoints &SVGAnimatedPoints::operator=(const SVGAnimatedPoints &other)
{
	if(impl != other.impl)
		impl = other.impl;

	return *this;
}

SVGAnimatedPoints &SVGAnimatedPoints::operator=(SVGAnimatedPointsImpl *other)
{
	if(impl != other)
		impl = other;

	return *this;
}

SVGPointList SVGAnimatedPoints::points() const
{
	if(!impl)
		return SVGPointList::null;

	return SVGPointList(impl->points());
}

SVGPointList SVGAnimatedPoints::animatedPoints() const
{
	if(!impl)
		return SVGPointList::null;

	return SVGPointList(impl->animatedPoints());
}

// vim:ts=4:noet
