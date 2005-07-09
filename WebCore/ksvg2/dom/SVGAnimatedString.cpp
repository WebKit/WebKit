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
#include "SVGAnimatedString.h"
#include "SVGAnimatedStringImpl.h"

using namespace KSVG;

#include "SVGConstants.h"
#include "SVGAnimatedString.lut.h"
using namespace KSVG;

/*
@begin SVGAnimatedString::s_hashTable 3
 baseVal	SVGAnimatedStringConstants::BaseVal	DontDelete
 animVal	SVGAnimatedStringConstants::AnimVal	DontDelete|ReadOnly
@end
*/

Value SVGAnimatedString::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedStringConstants::BaseVal:
			return getDOMString(baseVal());
		case SVGAnimatedStringConstants::AnimVal:
			return getDOMString(animVal());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

void SVGAnimatedString::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedStringConstants::BaseVal:
		{
			if(impl)
				impl->setBaseVal(KDOM::toDOMString(exec, value).implementation());

			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

SVGAnimatedString SVGAnimatedString::null;

SVGAnimatedString::SVGAnimatedString() : impl(0)
{
}

SVGAnimatedString::SVGAnimatedString(SVGAnimatedStringImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGAnimatedString::SVGAnimatedString(const SVGAnimatedString &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGAnimatedString)

KDOM::DOMString SVGAnimatedString::baseVal() const
{
	if(!impl)
		return KDOM::DOMString();

	return KDOM::DOMString(impl->baseVal());
}

KDOM::DOMString SVGAnimatedString::animVal() const
{
	if(!impl)
		return KDOM::DOMString();

	return KDOM::DOMString(impl->animVal());
}

// vim:ts=4:noet
