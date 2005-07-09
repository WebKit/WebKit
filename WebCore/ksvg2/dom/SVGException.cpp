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

#include "SVGException.h"
#include "SVGElementImpl.h"
#include "SVGExceptionImpl.h"

#include "SVGConstants.h"
#include "SVGException.lut.h"
using namespace KSVG;

/*
@begin SVGException::s_hashTable 2
 code	SVGExceptionConstants::Code	DontDelete|ReadOnly
@end
*/

Value SVGException::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case SVGExceptionConstants::Code:
			return Number(code());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

SVGException SVGException::null;

SVGException::SVGException() : impl(0)
{
}

SVGException::SVGException(SVGExceptionImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGException::SVGException(const SVGException &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGException)

unsigned short SVGException::code() const
{
	if(!impl)
		return 0;

	return impl->code();
}

// vim:ts=4:noet
