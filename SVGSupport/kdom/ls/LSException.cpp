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

#include "LSException.h"
#include "LSExceptionImpl.h"

#include "LSConstants.h"
#include "LSException.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin LSException::s_hashTable 2
 code	LSExceptionConstants::Code	DontDelete|ReadOnly
@end
*/

ValueImp *LSException::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case LSExceptionConstants::Code:
			return Number(code());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

LSException LSException::null;

LSException::LSException() : d(0)
{
}

LSException::LSException(LSExceptionImpl *i) : d(i)
{
	if(d)
		d->ref();
}

LSException::LSException(const LSException &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(LSException)

unsigned short LSException::code() const
{
	if(!d)
		return 0;

	return d->code();
}

// vim:ts=4:noet
