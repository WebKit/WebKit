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

#include "ksvg.h"
#include "SVGNumber.h"
#include "SVGHelper.h"
#include "SVGNumberImpl.h"
#include "SVGException.h"
#include "SVGElementImpl.h"
#include "SVGExceptionImpl.h"

#include "SVGConstants.h"
#include "SVGNumber.lut.h"
using namespace KSVG;

/*
@begin SVGNumber::s_hashTable 3
 value	SVGNumberConstants::Value	DontDelete
@end
*/

ValueImp *SVGNumber::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGNumberConstants::Value:
			return Number(value());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

void SVGNumber::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGNumberConstants::Value:
		{
			setValue(value->toNumber(exec));
			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

SVGNumber SVGNumber::null;

SVGNumber::SVGNumber() : impl(0)
{
}

SVGNumber::SVGNumber(SVGNumberImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGNumber::SVGNumber(const SVGNumber &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGNumber)

float SVGNumber::value() const
{
	if(!impl)
		return -1;

	return impl->value();
}

void SVGNumber::setValue(float value)
{
	if(impl)
		impl->setValue(value);
}

// vim:ts=4:noet
