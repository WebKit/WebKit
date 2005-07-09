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
#include "SVGElementImpl.h"
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedBooleanImpl.h"

using namespace KSVG;

#include "SVGConstants.h"
#include "SVGAnimatedBoolean.lut.h"
using namespace KSVG;

/*
@begin SVGAnimatedBoolean::s_hashTable 3
 baseVal	SVGAnimatedBooleanConstants::BaseVal	DontDelete
 animVal	SVGAnimatedBooleanConstants::AnimVal	DontDelete|ReadOnly
@end
*/

Value SVGAnimatedBoolean::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedBooleanConstants::BaseVal:
			return KJS::Boolean(baseVal());
		case SVGAnimatedBooleanConstants::AnimVal:
			return KJS::Boolean(animVal());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

void SVGAnimatedBoolean::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedBooleanConstants::BaseVal:
		{
			if(impl)
				impl->setBaseVal(value.toBoolean(exec));

			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

SVGAnimatedBoolean SVGAnimatedBoolean::null;

SVGAnimatedBoolean::SVGAnimatedBoolean() : impl(0)
{
}

SVGAnimatedBoolean::SVGAnimatedBoolean(SVGAnimatedBooleanImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGAnimatedBoolean::SVGAnimatedBoolean(const SVGAnimatedBoolean &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGAnimatedBoolean)

bool SVGAnimatedBoolean::baseVal() const
{
	if(!impl)
		return false;

	return impl->baseVal();
}

bool SVGAnimatedBoolean::animVal() const
{
	if(!impl)
		return false;

	return impl->animVal();
}

// vim:ts=4:noet
