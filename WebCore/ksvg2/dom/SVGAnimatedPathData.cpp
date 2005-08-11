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

#include <kdom/ecma/Ecma.h>

#include "SVGElement.h"
#include "SVGException.h"
#include "SVGElementImpl.h"
#include "SVGPathSegList.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimatedPathData.h"
#include "SVGAnimatedPathDataImpl.h"

#include "SVGConstants.h"
#include "SVGAnimatedPathData.lut.h"
using namespace KSVG;

/*
@begin SVGAnimatedPathData::s_hashTable 5
 pathSegList					SVGAnimatedPathDataConstants::PathSegList						DontDelete|ReadOnly
 normalizedPathSegList			SVGAnimatedPathDataConstants::NormalizedPathSegList				DontDelete|ReadOnly
 animatedPathSegList			SVGAnimatedPathDataConstants::AnimatedPathSegList				DontDelete|ReadOnly
 animatedNormalizedPathSegList	SVGAnimatedPathDataConstants::AnimatedNormalizedPathSegList		DontDelete|ReadOnly
@end
*/

ValueImp *SVGAnimatedPathData::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedPathDataConstants::PathSegList:
			return KDOM::safe_cache<SVGPathSegList>(exec, pathSegList());
		case SVGAnimatedPathDataConstants::NormalizedPathSegList:
			return KDOM::safe_cache<SVGPathSegList>(exec, normalizedPathSegList());
		case SVGAnimatedPathDataConstants::AnimatedPathSegList:
			return KDOM::safe_cache<SVGPathSegList>(exec, animatedPathSegList());
		case SVGAnimatedPathDataConstants::AnimatedNormalizedPathSegList:
			return KDOM::safe_cache<SVGPathSegList>(exec, animatedNormalizedPathSegList());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

SVGAnimatedPathData::SVGAnimatedPathData() : impl(0)
{
}

SVGAnimatedPathData::SVGAnimatedPathData(SVGAnimatedPathDataImpl *i) : impl(i)
{
}

SVGAnimatedPathData::SVGAnimatedPathData(const SVGAnimatedPathData &other) : impl(0)
{
	(*this) = other;
}

SVGAnimatedPathData::~SVGAnimatedPathData()
{
}

SVGAnimatedPathData &SVGAnimatedPathData::operator=(const SVGAnimatedPathData &other)
{
	if(impl != other.impl)
		impl = other.impl;

	return *this;
}

SVGAnimatedPathData &SVGAnimatedPathData::operator=(SVGAnimatedPathDataImpl *other)
{
	if(impl != other)
		impl = other;

	return *this;
}

SVGPathSegList SVGAnimatedPathData::pathSegList() const
{
	if(!impl)
		return SVGPathSegList::null;

	return SVGPathSegList(impl->pathSegList());
}

SVGPathSegList SVGAnimatedPathData::normalizedPathSegList() const
{
	if(!impl)
		return SVGPathSegList::null;

	return SVGPathSegList(impl->normalizedPathSegList());
}

SVGPathSegList SVGAnimatedPathData::animatedPathSegList() const
{
	if(!impl)
		return SVGPathSegList::null;

	return SVGPathSegList(impl->animatedPathSegList());
}

SVGPathSegList SVGAnimatedPathData::animatedNormalizedPathSegList() const
{
	if(!impl)
		return SVGPathSegList::null;

	return SVGPathSegList(impl->animatedNormalizedPathSegList());
}

// vim:ts=4:noet
