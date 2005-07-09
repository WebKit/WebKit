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
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGElement.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGTransformable.h"
#include "SVGTransformableImpl.h"
#include "SVGAnimatedTransformList.h"

#include "SVGConstants.h"
#include "SVGTransformable.lut.h"
using namespace KSVG;

/*
@begin SVGTransformable::s_hashTable 3
 transform	SVGTransformableConstants::Transform	DontDelete|ReadOnly
@end
*/

Value SVGTransformable::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGTransformableConstants::Transform:
			return KDOM::safe_cache<SVGAnimatedTransformList>(exec, transform());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

SVGTransformable SVGTransformable::null;

SVGTransformable::SVGTransformable() : SVGLocatable(), impl(0)
{
}

SVGTransformable::SVGTransformable(SVGTransformableImpl *i) : SVGLocatable(i), impl(i)
{
}

SVGTransformable::SVGTransformable(const SVGTransformable &other) : SVGLocatable(), impl(0)
{
	(*this) = other;
}

SVGTransformable::~SVGTransformable()
{
}

SVGTransformable &SVGTransformable::operator=(const SVGTransformable &other)
{
	SVGLocatable::operator=(other);
	if(impl != other.impl)
		impl = other.impl;

	return *this;
}

SVGTransformable &SVGTransformable::operator=(SVGTransformableImpl *other)
{
	SVGLocatable::operator=(other);
	if(impl != other)
		impl = other;

	return *this;
}

SVGAnimatedTransformList SVGTransformable::transform() const
{
	if(!impl)
		return SVGAnimatedTransformList::null;

	return SVGAnimatedTransformList(impl->transform());
}

// vim:ts=4:noet
