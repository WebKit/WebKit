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

#include "SVGPathSegMoveto.h"
#include "SVGPathSegMovetoImpl.h"

#include "SVGConstants.h"
#include "SVGPathSegMoveto.lut.h"
using namespace KSVG;

/*
@begin SVGPathSegMovetoAbs::s_hashTable 3
 x		SVGPathSegMovetoConstants::X		DontDelete
 y		SVGPathSegMovetoConstants::Y		DontDelete
@end
*/

Value SVGPathSegMovetoAbs::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegMovetoConstants::X:
		return Number(x());
	case SVGPathSegMovetoConstants::Y:
		return Number(y());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegMovetoAbs::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	switch(token)
	{
		case SVGPathSegMovetoConstants::X:
		{
			setX(value.toNumber(exec));
			return;
		}
		case SVGPathSegMovetoConstants::Y:
		{
			setY(value.toNumber(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}

// The qdom way...
#define _impl (static_cast<SVGPathSegMovetoAbsImpl *>(impl))

SVGPathSegMovetoAbs SVGPathSegMovetoAbs::null;

SVGPathSegMovetoAbs::SVGPathSegMovetoAbs() : SVGPathSeg()
{
}

SVGPathSegMovetoAbs::SVGPathSegMovetoAbs(SVGPathSegMovetoAbsImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegMovetoAbs::SVGPathSegMovetoAbs(const SVGPathSegMovetoAbs &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegMovetoAbs::SVGPathSegMovetoAbs(const SVGPathSeg &other) : SVGPathSeg(other)
{
	(*this) = other;
}

SVGPathSegMovetoAbs::~SVGPathSegMovetoAbs()
{
}

SVGPathSegMovetoAbs &SVGPathSegMovetoAbs::operator=(const SVGPathSegMovetoAbs &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegMovetoAbs, PATHSEG_MOVETO_ABS)

void SVGPathSegMovetoAbs::setX(float x)
{
	if(impl)
		_impl->setX(x);
}

float SVGPathSegMovetoAbs::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

void SVGPathSegMovetoAbs::setY(float y)
{
	if(impl)
		_impl->setY(y);
}

float SVGPathSegMovetoAbs::y() const
{
	if(!impl)
		return -1;

	return _impl->y();
}


/*
@begin SVGPathSegMovetoRel::s_hashTable 3
 x		SVGPathSegMovetoConstants::X		DontDelete
 y		SVGPathSegMovetoConstants::Y		DontDelete
@end
*/

Value SVGPathSegMovetoRel::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegMovetoConstants::X:
		return Number(x());
	case SVGPathSegMovetoConstants::Y:
		return Number(y());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegMovetoRel::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	switch(token)
	{
		case SVGPathSegMovetoConstants::X:
		{
			setX(value.toNumber(exec));
			return;
		}
		case SVGPathSegMovetoConstants::Y:
		{
			setY(value.toNumber(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}

// The qdom way...
#undef _impl
#define _impl (static_cast<SVGPathSegMovetoRelImpl *>(impl))

SVGPathSegMovetoRel SVGPathSegMovetoRel::null;

SVGPathSegMovetoRel::SVGPathSegMovetoRel() : SVGPathSeg()
{
}


SVGPathSegMovetoRel::SVGPathSegMovetoRel(SVGPathSegMovetoRelImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegMovetoRel::SVGPathSegMovetoRel(const SVGPathSegMovetoRel &other) : SVGPathSeg(other)
{
	(*this) = other;
}

SVGPathSegMovetoRel::SVGPathSegMovetoRel(const SVGPathSeg &other) : SVGPathSeg(other)
{
	(*this) = other;
}

SVGPathSegMovetoRel::~SVGPathSegMovetoRel()
{
}

SVGPathSegMovetoRel &SVGPathSegMovetoRel::operator=(const SVGPathSegMovetoRel &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegMovetoRel, PATHSEG_MOVETO_REL)

void SVGPathSegMovetoRel::setX(float x)
{
	if(impl)
		_impl->setX(x);
}

float SVGPathSegMovetoRel::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

void SVGPathSegMovetoRel::setY(float y)
{
	if(impl)
		_impl->setY(y);
}

float SVGPathSegMovetoRel::y() const
{
	if(!impl)
		return -1;

	return _impl->y();
}

// vim:ts=4:noet
