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

#include "SVGPathSegLinetoHorizontal.h"
#include "SVGPathSegLinetoHorizontalImpl.h"

#include "SVGConstants.h"
#include "SVGPathSegLinetoHorizontal.lut.h"
using namespace KSVG;

/*
@begin SVGPathSegLinetoHorizontalAbs::s_hashTable 3
 x		SVGPathSegLinetoHorizontalConstants::X		DontDelete
@end
*/

Value SVGPathSegLinetoHorizontalAbs::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegLinetoHorizontalConstants::X:
		return Number(x());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegLinetoHorizontalAbs::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	switch(token)
	{
		case SVGPathSegLinetoHorizontalConstants::X:
		{
			setX(value.toNumber(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}
// The qdom way...
#define _impl (static_cast<SVGPathSegLinetoHorizontalAbsImpl *>(impl))

SVGPathSegLinetoHorizontalAbs SVGPathSegLinetoHorizontalAbs::null;

SVGPathSegLinetoHorizontalAbs::SVGPathSegLinetoHorizontalAbs() : SVGPathSeg()
{
}
 
SVGPathSegLinetoHorizontalAbs::SVGPathSegLinetoHorizontalAbs(SVGPathSegLinetoHorizontalAbsImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegLinetoHorizontalAbs::SVGPathSegLinetoHorizontalAbs(const SVGPathSegLinetoHorizontalAbs &other) : SVGPathSeg(other)
{
	(*this) = other;
}

SVGPathSegLinetoHorizontalAbs::SVGPathSegLinetoHorizontalAbs(const SVGPathSeg &other) : SVGPathSeg(other)
{
	(*this) = other;
}

SVGPathSegLinetoHorizontalAbs::~SVGPathSegLinetoHorizontalAbs()
{
}
   
SVGPathSegLinetoHorizontalAbs &SVGPathSegLinetoHorizontalAbs::operator=(const SVGPathSegLinetoHorizontalAbs &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegLinetoHorizontalAbs, PATHSEG_LINETO_HORIZONTAL_ABS)

void SVGPathSegLinetoHorizontalAbs::setX(float x)
{
	if(impl)
		_impl->setX(x);
}
	    
float SVGPathSegLinetoHorizontalAbs::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}



/*
@begin SVGPathSegLinetoHorizontalRel::s_hashTable 3
 x		SVGPathSegLinetoHorizontalConstants::X		DontDelete
@end
*/

Value SVGPathSegLinetoHorizontalRel::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegLinetoHorizontalConstants::X:
		return Number(x());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void SVGPathSegLinetoHorizontalRel::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	switch(token)
	{
		case SVGPathSegLinetoHorizontalConstants::X:
		{
			setX(value.toNumber(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}

// The qdom way...
#undef _impl
#define _impl (static_cast<SVGPathSegLinetoHorizontalRelImpl *>(impl))

SVGPathSegLinetoHorizontalRel SVGPathSegLinetoHorizontalRel::null;

SVGPathSegLinetoHorizontalRel::SVGPathSegLinetoHorizontalRel() : SVGPathSeg()
{
}
 
SVGPathSegLinetoHorizontalRel::SVGPathSegLinetoHorizontalRel(SVGPathSegLinetoHorizontalRelImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegLinetoHorizontalRel::SVGPathSegLinetoHorizontalRel(const SVGPathSegLinetoHorizontalRel &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegLinetoHorizontalRel::SVGPathSegLinetoHorizontalRel(const SVGPathSeg &other) : SVGPathSeg()
{
	(*this) = other;
}

SVGPathSegLinetoHorizontalRel::~SVGPathSegLinetoHorizontalRel()
{
}
   
SVGPathSegLinetoHorizontalRel &SVGPathSegLinetoHorizontalRel::operator=(const SVGPathSegLinetoHorizontalRel &other)
{
	SVGPathSeg::operator=(other);
	return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegLinetoHorizontalRel, PATHSEG_LINETO_HORIZONTAL_REL)

void SVGPathSegLinetoHorizontalRel::setX(float x)
{
	if(impl)
		_impl->setX(x);
}
	    
float SVGPathSegLinetoHorizontalRel::x() const
{
	if(!impl)
		return -1;

	return _impl->x();
}

// vim:ts=4:noet
