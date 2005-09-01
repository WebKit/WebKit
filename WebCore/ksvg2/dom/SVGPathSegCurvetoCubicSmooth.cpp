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

#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGPathSegCurvetoCubicSmoothImpl.h"

#include "SVGConstants.h"
#include "SVGPathSegCurvetoCubicSmooth.lut.h"
using namespace KSVG;

/*
@begin SVGPathSegCurvetoCubicSmoothAbs::s_hashTable 5
 x        SVGPathSegCurvetoCubicSmoothConstants::X        DontDelete
 y        SVGPathSegCurvetoCubicSmoothConstants::Y        DontDelete
 x2        SVGPathSegCurvetoCubicSmoothConstants::X2        DontDelete
 y2        SVGPathSegCurvetoCubicSmoothConstants::Y2        DontDelete
@end
*/

ValueImp *SVGPathSegCurvetoCubicSmoothAbs::getValueProperty(ExecState *, int token) const
{
    switch(token)
    {
    case SVGPathSegCurvetoCubicSmoothConstants::X:
        return Number(x());
    case SVGPathSegCurvetoCubicSmoothConstants::Y:
        return Number(y());
    case SVGPathSegCurvetoCubicSmoothConstants::X2:
        return Number(x2());
    case SVGPathSegCurvetoCubicSmoothConstants::Y2:
        return Number(y2());
    default:
        kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    return Undefined();
}

void SVGPathSegCurvetoCubicSmoothAbs::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
    switch(token)
    {
        case SVGPathSegCurvetoCubicSmoothConstants::X:
        {
            setX(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicSmoothConstants::Y:
        {
            setY(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicSmoothConstants::X2:
        {
            setX2(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicSmoothConstants::Y2:
        {
            setY2(value->toNumber(exec));
            return;
        }
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }
}

// The qdom way...
#define _impl (static_cast<SVGPathSegCurvetoCubicSmoothAbsImpl *>(impl))

SVGPathSegCurvetoCubicSmoothAbs SVGPathSegCurvetoCubicSmoothAbs::null;

SVGPathSegCurvetoCubicSmoothAbs::SVGPathSegCurvetoCubicSmoothAbs() : SVGPathSeg()
{
}

SVGPathSegCurvetoCubicSmoothAbs::SVGPathSegCurvetoCubicSmoothAbs(SVGPathSegCurvetoCubicSmoothAbsImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegCurvetoCubicSmoothAbs::SVGPathSegCurvetoCubicSmoothAbs(const SVGPathSegCurvetoCubicSmoothAbs &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegCurvetoCubicSmoothAbs::SVGPathSegCurvetoCubicSmoothAbs(const SVGPathSeg &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegCurvetoCubicSmoothAbs::~SVGPathSegCurvetoCubicSmoothAbs()
{
}

SVGPathSegCurvetoCubicSmoothAbs &SVGPathSegCurvetoCubicSmoothAbs::operator=(const SVGPathSegCurvetoCubicSmoothAbs &other)
{
    SVGPathSeg::operator=(other);
    return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegCurvetoCubicSmoothAbs, PATHSEG_CURVETO_CUBIC_SMOOTH_ABS)

void SVGPathSegCurvetoCubicSmoothAbs::setX(float x)
{
    if(impl)
        _impl->setX(x);
}

float SVGPathSegCurvetoCubicSmoothAbs::x() const
{
    if(!impl)
        return -1;

    return _impl->x();
}

void SVGPathSegCurvetoCubicSmoothAbs::setY(float y)
{
    if(impl)
        _impl->setY(y);
}

float SVGPathSegCurvetoCubicSmoothAbs::y() const
{
    if(!impl)
        return -1;

    return _impl->y();
}

void SVGPathSegCurvetoCubicSmoothAbs::setX2(float x2)
{
    if(impl)
        _impl->setX2(x2);
}

float SVGPathSegCurvetoCubicSmoothAbs::x2() const
{
    if(!impl)
        return -1;

    return _impl->x2();
}

void SVGPathSegCurvetoCubicSmoothAbs::setY2(float y2)
{
    if(impl)
        _impl->setY2(y2);
}

float SVGPathSegCurvetoCubicSmoothAbs::y2() const
{
    if(!impl)
        return -1;

    return _impl->y2();
}



/*
@begin SVGPathSegCurvetoCubicSmoothRel::s_hashTable 5
 x        SVGPathSegCurvetoCubicSmoothConstants::X        DontDelete
 y        SVGPathSegCurvetoCubicSmoothConstants::Y        DontDelete
 x2        SVGPathSegCurvetoCubicSmoothConstants::X2        DontDelete
 y2        SVGPathSegCurvetoCubicSmoothConstants::Y2        DontDelete
@end
*/

ValueImp *SVGPathSegCurvetoCubicSmoothRel::getValueProperty(ExecState *, int token) const
{
    switch(token)
    {
    case SVGPathSegCurvetoCubicSmoothConstants::X:
        return Number(x());
    case SVGPathSegCurvetoCubicSmoothConstants::Y:
        return Number(y());
    case SVGPathSegCurvetoCubicSmoothConstants::X2:
        return Number(x2());
    case SVGPathSegCurvetoCubicSmoothConstants::Y2:
        return Number(y2());
    default:
        kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    return Undefined();
}

void SVGPathSegCurvetoCubicSmoothRel::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
    switch(token)
    {
        case SVGPathSegCurvetoCubicSmoothConstants::X:
        {
            setX(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicSmoothConstants::Y:
        {
            setY(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicSmoothConstants::X2:
        {
            setX2(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicSmoothConstants::Y2:
        {
            setY2(value->toNumber(exec));
            return;
        }
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }
}


// The qdom way...
#undef _impl
#define _impl (static_cast<SVGPathSegCurvetoCubicSmoothRelImpl *>(impl))

SVGPathSegCurvetoCubicSmoothRel SVGPathSegCurvetoCubicSmoothRel::null;

SVGPathSegCurvetoCubicSmoothRel::SVGPathSegCurvetoCubicSmoothRel() : SVGPathSeg()
{
}

SVGPathSegCurvetoCubicSmoothRel::SVGPathSegCurvetoCubicSmoothRel(SVGPathSegCurvetoCubicSmoothRelImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegCurvetoCubicSmoothRel::SVGPathSegCurvetoCubicSmoothRel(const SVGPathSegCurvetoCubicSmoothRel &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegCurvetoCubicSmoothRel::SVGPathSegCurvetoCubicSmoothRel(const SVGPathSeg &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegCurvetoCubicSmoothRel::~SVGPathSegCurvetoCubicSmoothRel()
{
}

SVGPathSegCurvetoCubicSmoothRel &SVGPathSegCurvetoCubicSmoothRel::operator=(const SVGPathSegCurvetoCubicSmoothRel &other)
{
    SVGPathSeg::operator=(other);
    return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegCurvetoCubicSmoothRel, PATHSEG_CURVETO_CUBIC_SMOOTH_REL)

void SVGPathSegCurvetoCubicSmoothRel::setX(float x)
{
    if(impl)
        _impl->setX(x);
}

float SVGPathSegCurvetoCubicSmoothRel::x() const
{
    if(!impl)
        return -1;

    return _impl->x();
}

void SVGPathSegCurvetoCubicSmoothRel::setY(float y)
{
    if(impl)
        _impl->setY(y);
}

float SVGPathSegCurvetoCubicSmoothRel::y() const
{
    if(!impl)
        return -1;

    return _impl->y();
}

void SVGPathSegCurvetoCubicSmoothRel::setX2(float x2)
{
    if(impl)
        _impl->setX2(x2);
}

float SVGPathSegCurvetoCubicSmoothRel::x2() const
{
    if(!impl)
        return -1;

    return _impl->x2();
}

void SVGPathSegCurvetoCubicSmoothRel::setY2(float y2)
{
    if(impl)
        _impl->setY2(y2);
}

float SVGPathSegCurvetoCubicSmoothRel::y2() const
{
    if(!impl)
        return -1;

    return _impl->y2();
}

// vim:ts=4:noet
