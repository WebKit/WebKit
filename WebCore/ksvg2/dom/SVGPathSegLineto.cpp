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

#include "SVGPathSegLineto.h"
#include "SVGPathSegLinetoImpl.h"

#include "SVGConstants.h"
#include "SVGPathSegLineto.lut.h"
using namespace KSVG;

/*
@begin SVGPathSegLinetoAbs::s_hashTable 3
 x		SVGPathSegLinetoConstants::X		DontDelete
 y		SVGPathSegLinetoConstants::Y		DontDelete
@end
*/

ValueImp *SVGPathSegLinetoAbs::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegLinetoConstants::X:
		return Number(x());
	case SVGPathSegLinetoConstants::Y:
		return Number(y());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegLinetoAbs::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	switch(token)
	{
		case SVGPathSegLinetoConstants::X:
		{
			setX(value->toNumber(exec));
			return;
		}
		case SVGPathSegLinetoConstants::Y:
		{
			setY(value->toNumber(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}

// The qdom way...
#define _impl (static_cast<SVGPathSegLinetoAbsImpl *>(impl))

SVGPathSegLinetoAbs SVGPathSegLinetoAbs::null;

SVGPathSegLinetoAbs::SVGPathSegLinetoAbs() : SVGPathSeg()
{
}

SVGPathSegLinetoAbs::SVGPathSegLinetoAbs(SVGPathSegLinetoAbsImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegLinetoAbs::SVGPathSegLinetoAbs(const SVGPathSegLinetoAbs &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegLinetoAbs::SVGPathSegLinetoAbs(const SVGPathSeg &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegLinetoAbs::~SVGPathSegLinetoAbs()
{
}

SVGPathSegLinetoAbs &SVGPathSegLinetoAbs::operator=(const SVGPathSegLinetoAbs &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegLinetoAbs, PATHSEG_LINETO_ABS)

void SVGPathSegLinetoAbs::setX(float x)
{
	if(impl)
		_impl->setX(x);
}

float SVGPathSegLinetoAbs::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

void SVGPathSegLinetoAbs::setY(float y)
{
	if(impl)
		_impl->setY(y);
}

float SVGPathSegLinetoAbs::y() const
{
	if(!impl)
		return -1;

	return _impl->y();
}



/*
@begin SVGPathSegLinetoRel::s_hashTable 3
 x		SVGPathSegLinetoConstants::X		DontDelete
 y		SVGPathSegLinetoConstants::Y		DontDelete
@end
*/

ValueImp *SVGPathSegLinetoRel::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegLinetoConstants::X:
		return Number(x());
	case SVGPathSegLinetoConstants::Y:
		return Number(y());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegLinetoRel::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	switch(token)
	{
		case SVGPathSegLinetoConstants::X:
		{
			setX(value->toNumber(exec));
			return;
		}
		case SVGPathSegLinetoConstants::Y:
		{
			setY(value->toNumber(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}

// The qdom way...
#undef _impl
#define _impl (static_cast<SVGPathSegLinetoRelImpl *>(impl))

SVGPathSegLinetoRel SVGPathSegLinetoRel::null;

SVGPathSegLinetoRel::SVGPathSegLinetoRel() : SVGPathSeg()
{
}

SVGPathSegLinetoRel::SVGPathSegLinetoRel(SVGPathSegLinetoRelImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegLinetoRel::SVGPathSegLinetoRel(const SVGPathSegLinetoRel &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegLinetoRel::SVGPathSegLinetoRel(const SVGPathSeg &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegLinetoRel::~SVGPathSegLinetoRel()
{
}

SVGPathSegLinetoRel &SVGPathSegLinetoRel::operator=(const SVGPathSegLinetoRel &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegLinetoRel, PATHSEG_LINETO_REL)

void SVGPathSegLinetoRel::setX(float x)
{
	if(impl)
		_impl->setX(x);
}

float SVGPathSegLinetoRel::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

void SVGPathSegLinetoRel::setY(float y)
{
	if(impl)
		_impl->setY(y);
}

float SVGPathSegLinetoRel::y() const
{
	if(!impl)
		return -1;

	return _impl->y();
}

// vim:ts=4:noet
