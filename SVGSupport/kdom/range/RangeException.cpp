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


#include "kdomrange.h"
#include "RangeException.h"
#include "RangeExceptionImpl.h"

#include "RangeConstants.h"
#include "RangeException.lut.h"
using namespace KDOM;

/*
@begin RangeException::s_hashTable 2
 code	RangeExceptionConstants::Code	DontDelete|ReadOnly
@end
*/

KJS::ValueImp *RangeException::getValueProperty(KJS::ExecState *, int token) const
{
	switch(token)
	{
		case RangeExceptionConstants::Code:
			return KJS::Number(code());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return KJS::Undefined();
}

RangeException RangeException::null;

RangeException::RangeException() : d(0)
{
}

RangeException::RangeException(RangeExceptionImpl *i) : d(i)
{
	if(d)
		d->ref();
}

RangeException::RangeException(const RangeException &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(RangeException)

unsigned short RangeException::code() const
{
	if(!d)
		return 0;

	return d->code();
}

// vim:ts=4:noet
