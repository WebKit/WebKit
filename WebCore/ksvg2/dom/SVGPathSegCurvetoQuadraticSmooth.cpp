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

#include "SVGPathSegCurvetoQuadraticSmooth.h"
#include "SVGPathSegCurvetoQuadraticSmoothImpl.h"

#include "SVGConstants.h"
#include "SVGPathSegCurvetoQuadraticSmooth.lut.h"
using namespace KSVG;

/*
@begin SVGPathSegCurvetoQuadraticSmoothAbs::s_hashTable 3
 x	SVGPathSegCurvetoQuadraticSmoothConstants::X		DontDelete
 y	SVGPathSegCurvetoQuadraticSmoothConstants::Y		DontDelete
@end
*/

Value SVGPathSegCurvetoQuadraticSmoothAbs::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegCurvetoQuadraticSmoothConstants::X:
		return Number(x());
	case SVGPathSegCurvetoQuadraticSmoothConstants::Y:
		return Number(y());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegCurvetoQuadraticSmoothAbs::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	switch(token)
	{
		case SVGPathSegCurvetoQuadraticSmoothConstants::X:
		{
			setX(value.toNumber(exec));
			return;
		}
		case SVGPathSegCurvetoQuadraticSmoothConstants::Y:
		{
			setY(value.toNumber(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}
// The qdom way...
#define _impl (static_cast<SVGPathSegCurvetoQuadraticSmoothAbsImpl *>(impl))

SVGPathSegCurvetoQuadraticSmoothAbs SVGPathSegCurvetoQuadraticSmoothAbs::null;

SVGPathSegCurvetoQuadraticSmoothAbs::SVGPathSegCurvetoQuadraticSmoothAbs() : SVGPathSeg()
{
}

SVGPathSegCurvetoQuadraticSmoothAbs::SVGPathSegCurvetoQuadraticSmoothAbs(SVGPathSegCurvetoQuadraticSmoothAbsImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegCurvetoQuadraticSmoothAbs::SVGPathSegCurvetoQuadraticSmoothAbs(const SVGPathSegCurvetoQuadraticSmoothAbs &other) : SVGPathSeg()
{
	(*this)= other;
}

SVGPathSegCurvetoQuadraticSmoothAbs::SVGPathSegCurvetoQuadraticSmoothAbs(const SVGPathSeg &other) : SVGPathSeg()
{
	(*this)= other;
}

SVGPathSegCurvetoQuadraticSmoothAbs::~SVGPathSegCurvetoQuadraticSmoothAbs()
{
}

SVGPathSegCurvetoQuadraticSmoothAbs &SVGPathSegCurvetoQuadraticSmoothAbs::operator=(const SVGPathSegCurvetoQuadraticSmoothAbs &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegCurvetoQuadraticSmoothAbs, PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS)

void SVGPathSegCurvetoQuadraticSmoothAbs::setX(float x)
{
	if(impl)
		_impl->setX(x);
}

float SVGPathSegCurvetoQuadraticSmoothAbs::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

void SVGPathSegCurvetoQuadraticSmoothAbs::setY(float y)
{
	if(impl)
		_impl->setY(y);
}

float SVGPathSegCurvetoQuadraticSmoothAbs::y() const
{
	if(!impl)
		return -1;

	return _impl->y();
}



/*
@begin SVGPathSegCurvetoQuadraticSmoothRel::s_hashTable 3
 x	SVGPathSegCurvetoQuadraticSmoothConstants::X		DontDelete
 y	SVGPathSegCurvetoQuadraticSmoothConstants::Y		DontDelete
@end
*/

Value SVGPathSegCurvetoQuadraticSmoothRel::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegCurvetoQuadraticSmoothConstants::X:
		return Number(x());
	case SVGPathSegCurvetoQuadraticSmoothConstants::Y:
		return Number(y());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegCurvetoQuadraticSmoothRel::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	switch(token)
	{
		case SVGPathSegCurvetoQuadraticSmoothConstants::X:
		{
			setX(value.toNumber(exec));
			return;
		}
		case SVGPathSegCurvetoQuadraticSmoothConstants::Y:
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
#define _impl (static_cast<SVGPathSegCurvetoQuadraticSmoothRelImpl *>(impl))

SVGPathSegCurvetoQuadraticSmoothRel SVGPathSegCurvetoQuadraticSmoothRel::null;

SVGPathSegCurvetoQuadraticSmoothRel::SVGPathSegCurvetoQuadraticSmoothRel() : SVGPathSeg()
{
}

SVGPathSegCurvetoQuadraticSmoothRel::SVGPathSegCurvetoQuadraticSmoothRel(SVGPathSegCurvetoQuadraticSmoothRelImpl *other) : SVGPathSeg(other)
{
}


SVGPathSegCurvetoQuadraticSmoothRel::SVGPathSegCurvetoQuadraticSmoothRel(const SVGPathSegCurvetoQuadraticSmoothRel &other) : SVGPathSeg()
{
	(*this)= other;
}

SVGPathSegCurvetoQuadraticSmoothRel::SVGPathSegCurvetoQuadraticSmoothRel(const SVGPathSeg &other) : SVGPathSeg()
{
	(*this)= other;
}

SVGPathSegCurvetoQuadraticSmoothRel::~SVGPathSegCurvetoQuadraticSmoothRel()
{
}

SVGPathSegCurvetoQuadraticSmoothRel &SVGPathSegCurvetoQuadraticSmoothRel::operator=(const SVGPathSegCurvetoQuadraticSmoothRel &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegCurvetoQuadraticSmoothRel, PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL)

void SVGPathSegCurvetoQuadraticSmoothRel::setX(float x)
{
	if(impl)
		_impl->setX(x);
}

float SVGPathSegCurvetoQuadraticSmoothRel::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

void SVGPathSegCurvetoQuadraticSmoothRel::setY(float y)
{
	if(impl)
		_impl->setY(y);
}

float SVGPathSegCurvetoQuadraticSmoothRel::y() const
{
	if(!impl)
		return -1;

	return _impl->y();
}

// vim:ts=4:noet
