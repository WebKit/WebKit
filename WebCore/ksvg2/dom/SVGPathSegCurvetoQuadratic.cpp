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

#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGPathSegCurvetoQuadraticImpl.h"

#include "SVGConstants.h"
#include "SVGPathSegCurvetoQuadratic.lut.h"
using namespace KSVG;

/*
@begin SVGPathSegCurvetoQuadraticAbs::s_hashTable 5
 x	SVGPathSegCurvetoQuadraticConstants::X		DontDelete
 y	SVGPathSegCurvetoQuadraticConstants::Y		DontDelete
 x1	SVGPathSegCurvetoQuadraticConstants::X1		DontDelete
 y1	SVGPathSegCurvetoQuadraticConstants::Y1		DontDelete
@end
*/

ValueImp *SVGPathSegCurvetoQuadraticAbs::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegCurvetoQuadraticConstants::X:
		return Number(x());
	case SVGPathSegCurvetoQuadraticConstants::Y:
		return Number(y());
	case SVGPathSegCurvetoQuadraticConstants::X1:
		return Number(x1());
	case SVGPathSegCurvetoQuadraticConstants::Y1:
		return Number(y1());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegCurvetoQuadraticAbs::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	switch(token)
	{
		case SVGPathSegCurvetoQuadraticConstants::X:
		{
			setX(value->toNumber(exec));
			return;
		}
		case SVGPathSegCurvetoQuadraticConstants::Y:
		{
			setY(value->toNumber(exec));
			return;
		}
		case SVGPathSegCurvetoQuadraticConstants::X1:
		{
			setX1(value->toNumber(exec));
			return;
		}
		case SVGPathSegCurvetoQuadraticConstants::Y1:
		{
			setY1(value->toNumber(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}

// The qdom way...
#define _impl (static_cast<SVGPathSegCurvetoQuadraticAbsImpl *>(impl))

SVGPathSegCurvetoQuadraticAbs SVGPathSegCurvetoQuadraticAbs::null;

SVGPathSegCurvetoQuadraticAbs::SVGPathSegCurvetoQuadraticAbs() : SVGPathSeg()
{
}

SVGPathSegCurvetoQuadraticAbs::SVGPathSegCurvetoQuadraticAbs(SVGPathSegCurvetoQuadraticAbsImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegCurvetoQuadraticAbs::SVGPathSegCurvetoQuadraticAbs(const SVGPathSegCurvetoQuadraticAbs &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegCurvetoQuadraticAbs::SVGPathSegCurvetoQuadraticAbs(const SVGPathSeg &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegCurvetoQuadraticAbs::~SVGPathSegCurvetoQuadraticAbs()
{
}

SVGPathSegCurvetoQuadraticAbs &SVGPathSegCurvetoQuadraticAbs::operator=(const SVGPathSegCurvetoQuadraticAbs &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegCurvetoQuadraticAbs, PATHSEG_CURVETO_QUADRATIC_ABS)

void SVGPathSegCurvetoQuadraticAbs::setX(float x)
{
	if(impl)
		_impl->setX(x);
}

float SVGPathSegCurvetoQuadraticAbs::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

void SVGPathSegCurvetoQuadraticAbs::setY(float y)
{
	if(impl)
		_impl->setY(y);
}

float SVGPathSegCurvetoQuadraticAbs::y() const
{
	if(!impl)
		return -1;

	return _impl->y();
}

void SVGPathSegCurvetoQuadraticAbs::setX1(float x1)
{
	if(impl)
		_impl->setX1(x1);
}

float SVGPathSegCurvetoQuadraticAbs::x1() const
{
	if(!impl)
		return -1;

	return _impl->x1();
}

void SVGPathSegCurvetoQuadraticAbs::setY1(float y1)
{
	if(impl)
		_impl->setY1(y1);
}

float SVGPathSegCurvetoQuadraticAbs::y1() const
{
	if(!impl)
		return -1;

	return _impl->y1();
}



/*
@begin SVGPathSegCurvetoQuadraticRel::s_hashTable 5
 x		SVGPathSegCurvetoQuadraticConstants::X		DontDelete
 y		SVGPathSegCurvetoQuadraticConstants::Y		DontDelete
 x1		SVGPathSegCurvetoQuadraticConstants::X1		DontDelete
 y1		SVGPathSegCurvetoQuadraticConstants::Y1		DontDelete
@end
*/

ValueImp *SVGPathSegCurvetoQuadraticRel::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegCurvetoQuadraticConstants::X:
		return Number(x());
	case SVGPathSegCurvetoQuadraticConstants::Y:
		return Number(y());
	case SVGPathSegCurvetoQuadraticConstants::X1:
		return Number(x1());
	case SVGPathSegCurvetoQuadraticConstants::Y1:
		return Number(y1());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegCurvetoQuadraticRel::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	switch(token)
	{
		case SVGPathSegCurvetoQuadraticConstants::X:
		{
			setX(value->toNumber(exec));
			return;
		}
		case SVGPathSegCurvetoQuadraticConstants::Y:
		{
			setY(value->toNumber(exec));
			return;
		}
		case SVGPathSegCurvetoQuadraticConstants::X1:
		{
			setX1(value->toNumber(exec));
			return;
		}
		case SVGPathSegCurvetoQuadraticConstants::Y1:
		{
			setY1(value->toNumber(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}


// The qdom way...
#undef _impl
#define _impl (static_cast<SVGPathSegCurvetoQuadraticRelImpl *>(impl))

SVGPathSegCurvetoQuadraticRel SVGPathSegCurvetoQuadraticRel::null;

SVGPathSegCurvetoQuadraticRel::SVGPathSegCurvetoQuadraticRel() : SVGPathSeg()
{
}

SVGPathSegCurvetoQuadraticRel::SVGPathSegCurvetoQuadraticRel(SVGPathSegCurvetoQuadraticRelImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegCurvetoQuadraticRel::SVGPathSegCurvetoQuadraticRel(const SVGPathSegCurvetoQuadraticRel &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegCurvetoQuadraticRel::SVGPathSegCurvetoQuadraticRel(const SVGPathSeg &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegCurvetoQuadraticRel::~SVGPathSegCurvetoQuadraticRel()
{
}

SVGPathSegCurvetoQuadraticRel &SVGPathSegCurvetoQuadraticRel::operator=(const SVGPathSegCurvetoQuadraticRel &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegCurvetoQuadraticRel, PATHSEG_CURVETO_QUADRATIC_REL)

void SVGPathSegCurvetoQuadraticRel::setX(float x)
{
	if(impl)
		_impl->setX(x);
}

float SVGPathSegCurvetoQuadraticRel::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

void SVGPathSegCurvetoQuadraticRel::setY(float y)
{
	if(impl)
		_impl->setY(y);
}

float SVGPathSegCurvetoQuadraticRel::y() const
{
	if(!impl)
		return -1;

	return _impl->y();
}

void SVGPathSegCurvetoQuadraticRel::setX1(float x1)
{
	if(impl)
		_impl->setX1(x1);
}

float SVGPathSegCurvetoQuadraticRel::x1() const
{
	if(!impl)
		return -1;

	return _impl->x1();
}

void SVGPathSegCurvetoQuadraticRel::setY1(float y1)
{
	if(impl)
		_impl->setY1(y1);
}

float SVGPathSegCurvetoQuadraticRel::y1() const
{
	if(!impl)
		return -1;

	return _impl->y1();
}

// vim:ts=4:noet
