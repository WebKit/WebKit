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
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGException.h"
#include "SVGElementImpl.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedEnumerationImpl.h"

using namespace KSVG;

#include "SVGConstants.h"
#include "SVGAnimatedEnumeration.lut.h"
using namespace KSVG;

/*
@begin SVGAnimatedEnumeration::s_hashTable 3
 baseVal	SVGAnimatedEnumerationConstants::BaseVal	DontDelete
 animVal	SVGAnimatedEnumerationConstants::AnimVal	DontDelete|ReadOnly
@end
*/

ValueImp *SVGAnimatedEnumeration::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedEnumerationConstants::BaseVal:
			return Number(baseVal());
		case SVGAnimatedEnumerationConstants::AnimVal:
			return Number(animVal());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

void SVGAnimatedEnumeration::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedEnumerationConstants::BaseVal:
		{
			if(impl)
				impl->setBaseVal(static_cast<unsigned short>(value->toNumber(exec)));

			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

SVGAnimatedEnumeration SVGAnimatedEnumeration::null;

SVGAnimatedEnumeration::SVGAnimatedEnumeration() : impl(0)
{
}

SVGAnimatedEnumeration::SVGAnimatedEnumeration(SVGAnimatedEnumerationImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGAnimatedEnumeration::SVGAnimatedEnumeration(const SVGAnimatedEnumeration &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGAnimatedEnumeration)

unsigned short SVGAnimatedEnumeration::baseVal() const
{
	if(!impl)
		return false;

	return impl->baseVal();
}

unsigned short SVGAnimatedEnumeration::animVal() const
{
	if(!impl)
		return false;

	return impl->animVal();
}

// vim:ts=4:noet
