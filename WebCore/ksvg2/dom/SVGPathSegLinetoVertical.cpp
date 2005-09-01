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

#include "SVGPathSegLinetoVertical.h"
#include "SVGPathSegLinetoVerticalImpl.h"

#include "SVGConstants.h"
#include "SVGPathSegLinetoVertical.lut.h"
using namespace KSVG;

/*
@begin SVGPathSegLinetoVerticalAbs::s_hashTable 3
 y        SVGPathSegLinetoVerticalConstants::Y        DontDelete
@end
*/

ValueImp *SVGPathSegLinetoVerticalAbs::getValueProperty(ExecState *, int token) const
{
    switch(token)
    {
    case SVGPathSegLinetoVerticalConstants::Y:
        return Number(y());
    default:
        kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    return Undefined();
}

void SVGPathSegLinetoVerticalAbs::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
    switch(token)
    {
        case SVGPathSegLinetoVerticalConstants::Y:
        {
            setY(value->toNumber(exec));
            return;
        }
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }
}
// The qdom way...
#define _impl (static_cast<SVGPathSegLinetoVerticalAbsImpl *>(impl))

SVGPathSegLinetoVerticalAbs SVGPathSegLinetoVerticalAbs::null;

SVGPathSegLinetoVerticalAbs::SVGPathSegLinetoVerticalAbs() : SVGPathSeg()
{
}
 

SVGPathSegLinetoVerticalAbs::SVGPathSegLinetoVerticalAbs(SVGPathSegLinetoVerticalAbsImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegLinetoVerticalAbs::SVGPathSegLinetoVerticalAbs(const SVGPathSegLinetoVerticalAbs &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegLinetoVerticalAbs::SVGPathSegLinetoVerticalAbs(const SVGPathSeg &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegLinetoVerticalAbs::~SVGPathSegLinetoVerticalAbs()
{
}
   
SVGPathSegLinetoVerticalAbs &SVGPathSegLinetoVerticalAbs::operator=(const SVGPathSegLinetoVerticalAbs &other)
{
    SVGPathSeg::operator=(other);
    return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegLinetoVerticalAbs, PATHSEG_LINETO_VERTICAL_ABS)

void SVGPathSegLinetoVerticalAbs::setY(float y)
{
    if(impl)
        _impl->setY(y);
}

float SVGPathSegLinetoVerticalAbs::y() const
{
    if(!impl)
        return -1;

    return _impl->y();
}



/*
@begin SVGPathSegLinetoVerticalRel::s_hashTable 3
 y        SVGPathSegLinetoVerticalConstants::Y        DontDelete
@end
*/

ValueImp *SVGPathSegLinetoVerticalRel::getValueProperty(ExecState *, int token) const
{
    switch(token)
    {
    case SVGPathSegLinetoVerticalConstants::Y:
        return Number(y());
    default:
        kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    return Undefined();
}

void SVGPathSegLinetoVerticalRel::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
    switch(token)
    {
        case SVGPathSegLinetoVerticalConstants::Y:
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
#define _impl (static_cast<SVGPathSegLinetoVerticalRelImpl *>(impl))

SVGPathSegLinetoVerticalRel SVGPathSegLinetoVerticalRel::null;

SVGPathSegLinetoVerticalRel::SVGPathSegLinetoVerticalRel() : SVGPathSeg()
{
}

SVGPathSegLinetoVerticalRel::SVGPathSegLinetoVerticalRel(SVGPathSegLinetoVerticalRelImpl *other) : SVGPathSeg(other)
{
} 

SVGPathSegLinetoVerticalRel::SVGPathSegLinetoVerticalRel(const SVGPathSegLinetoVerticalRel &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegLinetoVerticalRel::SVGPathSegLinetoVerticalRel(const SVGPathSeg &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegLinetoVerticalRel::~SVGPathSegLinetoVerticalRel()
{
}
   
SVGPathSegLinetoVerticalRel &SVGPathSegLinetoVerticalRel::operator=(const SVGPathSegLinetoVerticalRel &other)
{
    SVGPathSeg::operator=(other);
    return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegLinetoVerticalRel, PATHSEG_LINETO_VERTICAL_REL)

void SVGPathSegLinetoVerticalRel::setY(float y)
{
    if(impl)
        _impl->setY(y);
}

float SVGPathSegLinetoVerticalRel::y() const
{
    if(!impl)
        return -1;

    return _impl->y();
}

// vim:ts=4:noet
