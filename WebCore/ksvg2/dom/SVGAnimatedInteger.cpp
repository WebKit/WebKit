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

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGElementImpl.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedIntegerImpl.h"

#include "SVGConstants.h"
#include "SVGAnimatedInteger.lut.h"
using namespace KSVG;

/*
@begin SVGAnimatedInteger::s_hashTable 3
 baseVal	SVGAnimatedIntegerConstants::BaseVal	DontDelete
 animVal	SVGAnimatedIntegerConstants::AnimVal	DontDelete|ReadOnly
@end
*/

Value SVGAnimatedInteger::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedIntegerConstants::BaseVal:
			return Number(baseVal());
		case SVGAnimatedIntegerConstants::AnimVal:
			return Number(animVal());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

void SVGAnimatedInteger::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedIntegerConstants::BaseVal:
		{
			if(impl)
				impl->setBaseVal(static_cast<long>(value.toNumber(exec)));

			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

SVGAnimatedInteger SVGAnimatedInteger::null;

SVGAnimatedInteger::SVGAnimatedInteger() : impl(0)
{
}

SVGAnimatedInteger::SVGAnimatedInteger(SVGAnimatedIntegerImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGAnimatedInteger::SVGAnimatedInteger(const SVGAnimatedInteger &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGAnimatedInteger)

long SVGAnimatedInteger::baseVal() const
{
	if(!impl)
		return 0;

	return impl->baseVal();
}

long SVGAnimatedInteger::animVal() const
{
	if(!impl)
		return 0;

	return impl->animVal();
}

// vim:ts=4:noet
