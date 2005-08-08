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

#include "SVGPathSegArc.h"
#include "SVGPathSegArcImpl.h"

#include "SVGConstants.h"
#include "SVGPathSegArc.lut.h"
using namespace KSVG;

/*
@begin SVGPathSegArcAbs::s_hashTable 11
 x				SVGPathSegArcConstants::X				DontDelete
 y				SVGPathSegArcConstants::Y				DontDelete
 r1				SVGPathSegArcConstants::R1				DontDelete
 r2				SVGPathSegArcConstants::R2				DontDelete
 angle			SVGPathSegArcConstants::Angle			DontDelete
 largeArcFlag	SVGPathSegArcConstants::LargeArcFlag	DontDelete
 sweepFlag		SVGPathSegArcConstants::SweepFlag		DontDelete
@end
*/

ValueImp *SVGPathSegArcAbs::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegArcConstants::X:
		return Number(x());
	case SVGPathSegArcConstants::Y:
		return Number(y());
	case SVGPathSegArcConstants::R1:
		return Number(r1());
	case SVGPathSegArcConstants::R2:
		return Number(r2());
	case SVGPathSegArcConstants::LargeArcFlag:
		return KJS::Boolean(largeArcFlag());
	case SVGPathSegArcConstants::SweepFlag:
		return KJS::Boolean(sweepFlag());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegArcAbs::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	switch(token)
	{
		case SVGPathSegArcConstants::X:
		{
			setX(value->toNumber(exec));
			return;
		}
		case SVGPathSegArcConstants::Y:
		{
			setY(value->toNumber(exec));
			return;
		}
		case SVGPathSegArcConstants::R1:
		{
			setR1(value->toNumber(exec));
			return;
		}
		case SVGPathSegArcConstants::R2:
		{
			setR2(value->toNumber(exec));
			return;
		}
		case SVGPathSegArcConstants::LargeArcFlag:
		{
			setLargeArcFlag(value->toBoolean(exec));
			return;
		}
		case SVGPathSegArcConstants::SweepFlag:
		{
			setSweepFlag(value->toBoolean(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}

// The qdom way...
#define _impl (static_cast<SVGPathSegArcAbsImpl *>(impl))

SVGPathSegArcAbs SVGPathSegArcAbs::null;

SVGPathSegArcAbs::SVGPathSegArcAbs() : SVGPathSeg()
{
}

SVGPathSegArcAbs::SVGPathSegArcAbs(SVGPathSegArcAbsImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegArcAbs::SVGPathSegArcAbs(const SVGPathSegArcAbs &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegArcAbs::SVGPathSegArcAbs(const SVGPathSeg &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegArcAbs::~SVGPathSegArcAbs()
{
}

SVGPathSegArcAbs &SVGPathSegArcAbs::operator=(const SVGPathSegArcAbs &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegArcAbs, PATHSEG_ARC_ABS)

void SVGPathSegArcAbs::setX(float x)
{
	if(impl)
		_impl->setX(x);
}

float SVGPathSegArcAbs::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

void SVGPathSegArcAbs::setY(float y)
{
	if(impl)
		_impl->setY(y);
}

float SVGPathSegArcAbs::y() const
{
	if(!impl)
		return -1;

	return _impl->y();
}

void SVGPathSegArcAbs::setR1(float r1)
{
	if(impl)
		_impl->setR1(r1);
}

float SVGPathSegArcAbs::r1() const
{
	if(!impl)
		return -1;

	return _impl->r1();
}

void SVGPathSegArcAbs::setR2(float r2)
{
	if(impl)
		_impl->setR2(r2);
}

float SVGPathSegArcAbs::r2() const
{
	if(!impl)
		return -1;

	return _impl->r2();
}

void SVGPathSegArcAbs::setAngle(float angle)
{
	if(impl)
		_impl->setAngle(angle);
}

float SVGPathSegArcAbs::angle() const
{
	if(!impl)
		return -1;

	return _impl->angle();
}

void SVGPathSegArcAbs::setLargeArcFlag(bool largeArcFlag)
{
	if(impl)
		_impl->setLargeArcFlag(largeArcFlag);
}

bool SVGPathSegArcAbs::largeArcFlag() const
{
	if(!impl)
		return false;

	return _impl->largeArcFlag();
}

void SVGPathSegArcAbs::setSweepFlag(bool sweepFlag)
{
	if(impl)
		_impl->setSweepFlag(sweepFlag);
}

bool SVGPathSegArcAbs::sweepFlag() const
{
	if(!impl)
		return false;

	return _impl->sweepFlag();
}



/*
@begin SVGPathSegArcRel::s_hashTable 11
 x				SVGPathSegArcConstants::X				DontDelete
 y				SVGPathSegArcConstants::Y				DontDelete
 r1				SVGPathSegArcConstants::R1				DontDelete
 r2				SVGPathSegArcConstants::R2				DontDelete
 angle			SVGPathSegArcConstants::Angle			DontDelete
 largeArcFlag	SVGPathSegArcConstants::LargeArcFlag	DontDelete
 sweepFlag		SVGPathSegArcConstants::SweepFlag		DontDelete
@end
*/

ValueImp *SVGPathSegArcRel::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegArcConstants::X:
		return Number(x());
	case SVGPathSegArcConstants::Y:
		return Number(y());
	case SVGPathSegArcConstants::R1:
		return Number(r1());
	case SVGPathSegArcConstants::R2:
		return Number(r2());
	case SVGPathSegArcConstants::LargeArcFlag:
		return KJS::Boolean(largeArcFlag());
	case SVGPathSegArcConstants::SweepFlag:
		return KJS::Boolean(sweepFlag());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegArcRel::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	switch(token)
	{
		case SVGPathSegArcConstants::X:
		{
			setX(value->toNumber(exec));
			return;
		}
		case SVGPathSegArcConstants::Y:
		{
			setY(value->toNumber(exec));
			return;
		}
		case SVGPathSegArcConstants::R1:
		{
			setR1(value->toNumber(exec));
			return;
		}
		case SVGPathSegArcConstants::R2:
		{
			setR2(value->toNumber(exec));
			return;
		}
		case SVGPathSegArcConstants::LargeArcFlag:
		{
			setLargeArcFlag(value->toBoolean(exec));
			return;
		}
		case SVGPathSegArcConstants::SweepFlag:
		{
			setSweepFlag(value->toBoolean(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}

// The qdom way...
#undef _impl
#define _impl (static_cast<SVGPathSegArcRelImpl *>(impl))

SVGPathSegArcRel SVGPathSegArcRel::null;

SVGPathSegArcRel::SVGPathSegArcRel() : SVGPathSeg()
{
}

SVGPathSegArcRel::SVGPathSegArcRel(SVGPathSegArcRelImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegArcRel::SVGPathSegArcRel(const SVGPathSegArcRel &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegArcRel::SVGPathSegArcRel(const SVGPathSeg &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegArcRel::~SVGPathSegArcRel()
{
}

SVGPathSegArcRel &SVGPathSegArcRel::operator=(const SVGPathSegArcRel &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegArcRel, PATHSEG_ARC_REL)

void SVGPathSegArcRel::setX(float x)
{
	if(impl)
		_impl->setX(x);
}

float SVGPathSegArcRel::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

void SVGPathSegArcRel::setY(float y)
{
	if(impl)
		_impl->setY(y);
}

float SVGPathSegArcRel::y() const
{
	if(!impl)
		return -1;

	return _impl->y();
}

void SVGPathSegArcRel::setR1(float r1)
{
	if(impl)
		_impl->setR1(r1);
}

float SVGPathSegArcRel::r1() const
{
	if(!impl)
		return -1;

	return _impl->r1();
}

void SVGPathSegArcRel::setR2(float r2)
{
	if(impl)
		_impl->setR2(r2);
}

float SVGPathSegArcRel::r2() const
{
	if(!impl)
		return -1;

	return _impl->r2();
}

void SVGPathSegArcRel::setAngle(float angle)
{
	if(impl)
		_impl->setAngle(angle);
}

float SVGPathSegArcRel::angle() const
{
	if(!impl)
		return -1;

	return _impl->angle();
}

void SVGPathSegArcRel::setLargeArcFlag(bool largeArcFlag)
{
	if(impl)
		_impl->setLargeArcFlag(largeArcFlag);
}

bool SVGPathSegArcRel::largeArcFlag() const
{
	if(!impl)
		return false;

	return _impl->largeArcFlag();
}

void SVGPathSegArcRel::setSweepFlag(bool sweepFlag)
{
	if(impl)
		_impl->setSweepFlag(sweepFlag);
}

bool SVGPathSegArcRel::sweepFlag() const
{
	if(!impl)
		return false;

	return _impl->sweepFlag();
}

// vim:ts=4:noet
