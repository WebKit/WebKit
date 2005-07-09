/*
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

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

#include "XPathException.h"
#include "XPathExceptionImpl.h"

#include "XPathConstants.h"
#include "XPathException.lut.h"

using namespace KDOM;
using namespace KJS;

/*
@begin XPathException::s_hashTable 2
 code	XPathExceptionConstants::Code	DontDelete|ReadOnly
@end
*/

Value XPathException::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case XPathExceptionConstants::Code:
			return Number(code());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

XPathException XPathException::null;

XPathException::XPathException() : d(0)
{
}

XPathException::XPathException(XPathExceptionImpl *i) : d(i)
{
	if(d)
		d->ref();
}

XPathException::XPathException(const XPathException &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(XPathException)

unsigned short XPathException::code() const
{
	if(!d)
		return 0;

	return d->code();
}

// vim:ts=4:noet
