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

#include <ksvg2/ksvg.h>

#include "SVGRect.h"
#include "SVGHelper.h"
#include "SVGRectImpl.h"
#include "SVGElementImpl.h"

#include "SVGConstants.h"
#include "SVGRect.lut.h"
using namespace KSVG;

/*
@begin SVGRect::s_hashTable 5
 x				SVGRectConstants::X			DontDelete
 y				SVGRectConstants::Y			DontDelete
 width			SVGRectConstants::Width		DontDelete
 height			SVGRectConstants::Height	DontDelete
@end
*/

ValueImp *SVGRect::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGRectConstants::X:
			return Number(x());
		case SVGRectConstants::Y:
			return Number(y());
		case SVGRectConstants::Width:
			return Number(width());
		case SVGRectConstants::Height:
			return Number(height());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

void SVGRect::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE
	
	switch(token)
	{
		case SVGRectConstants::X:
		{
			setX(value->toNumber(exec));
			break;
		}
		case SVGRectConstants::Y:
		{
			setY(value->toNumber(exec));
			break;
		}
		case SVGRectConstants::Width:
		{
			setWidth(value->toNumber(exec));
			break;
		}
		case SVGRectConstants::Height:
		{
			setHeight(value->toNumber(exec));
			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

SVGRect SVGRect::null;

SVGRect::SVGRect() : impl(0)
{
}

SVGRect::SVGRect(SVGRectImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGRect::SVGRect(const SVGRect &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGRect)

double SVGRect::x() const
{
	if(!impl)
		return -1;

	return impl->x();
}

void SVGRect::setX(double value)
{
	if(impl)
		impl->setX(value);
}

double SVGRect::y() const
{
	if(!impl)
		return -1;

	return impl->y();
}

void SVGRect::setY(double value)
{
	if(impl)
		impl->setY(value);
}

double SVGRect::width() const
{
	if(!impl)
		return -1;

	return impl->width();
}

void SVGRect::setWidth(double value)
{
	if(impl)
		impl->setWidth(value);
}

double SVGRect::height() const
{
	if(!impl)
		return -1;

	return impl->height();
}

void SVGRect::setHeight(double value)
{
	if(impl)
		impl->setHeight(value);
}

// vim:ts=4:noet
