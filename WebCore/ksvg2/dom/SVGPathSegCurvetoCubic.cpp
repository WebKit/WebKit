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

#include "SVGPathSegCurvetoCubic.h"
#include "SVGPathSegCurvetoCubicImpl.h"

#include "SVGConstants.h"
#include "SVGPathSegCurvetoCubic.lut.h"
using namespace KSVG;

/*
@begin SVGPathSegCurvetoCubicAbs::s_hashTable 7
 x        SVGPathSegCurvetoCubicConstants::X        DontDelete
 y        SVGPathSegCurvetoCubicConstants::Y        DontDelete
 x1        SVGPathSegCurvetoCubicConstants::X1        DontDelete
 y1        SVGPathSegCurvetoCubicConstants::Y1        DontDelete
 x2        SVGPathSegCurvetoCubicConstants::X2        DontDelete
 y2        SVGPathSegCurvetoCubicConstants::Y2        DontDelete
@end
*/

ValueImp *SVGPathSegCurvetoCubicAbs::getValueProperty(ExecState *, int token) const
{
    switch(token)
    {
    case SVGPathSegCurvetoCubicConstants::X:
        return Number(x());
    case SVGPathSegCurvetoCubicConstants::Y:
        return Number(y());
    case SVGPathSegCurvetoCubicConstants::X1:
        return Number(x1());
    case SVGPathSegCurvetoCubicConstants::Y1:
        return Number(y1());
    case SVGPathSegCurvetoCubicConstants::X2:
        return Number(x2());
    case SVGPathSegCurvetoCubicConstants::Y2:
        return Number(y2());
    default:
        kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    return Undefined();
}

void SVGPathSegCurvetoCubicAbs::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
    switch(token)
    {
        case SVGPathSegCurvetoCubicConstants::X:
        {
            setX(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicConstants::Y:
        {
            setY(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicConstants::X1:
        {
            setX1(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicConstants::Y1:
        {
            setY1(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicConstants::X2:
        {
            setX2(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicConstants::Y2:
        {
            setY2(value->toNumber(exec));
            return;
        }
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }
}

// The qdom way...
#define _impl (static_cast<SVGPathSegCurvetoCubicAbsImpl *>(impl))

SVGPathSegCurvetoCubicAbs SVGPathSegCurvetoCubicAbs::null;

SVGPathSegCurvetoCubicAbs::SVGPathSegCurvetoCubicAbs() : SVGPathSeg()
{
}

SVGPathSegCurvetoCubicAbs::SVGPathSegCurvetoCubicAbs(SVGPathSegCurvetoCubicAbsImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegCurvetoCubicAbs::SVGPathSegCurvetoCubicAbs(const SVGPathSegCurvetoCubicAbs &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegCurvetoCubicAbs::SVGPathSegCurvetoCubicAbs(const SVGPathSeg &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegCurvetoCubicAbs::~SVGPathSegCurvetoCubicAbs()
{
}

SVGPathSegCurvetoCubicAbs &SVGPathSegCurvetoCubicAbs::operator=(const SVGPathSegCurvetoCubicAbs &other)
{
    SVGPathSeg::operator=(other);
    return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegCurvetoCubicAbs, PATHSEG_CURVETO_CUBIC_ABS)

void SVGPathSegCurvetoCubicAbs::setX(float x)
{
    if(impl)
        _impl->setX(x);
}

float SVGPathSegCurvetoCubicAbs::x() const
{
    if(!impl)
        return -1;

    return _impl->x();
}

void SVGPathSegCurvetoCubicAbs::setY(float y)
{
    if(impl)
        _impl->setY(y);
}

float SVGPathSegCurvetoCubicAbs::y() const
{
    if(!impl)
        return -1;

    return _impl->y();
}

void SVGPathSegCurvetoCubicAbs::setX1(float x1)
{
    if(impl)
        _impl->setX1(x1);
}

float SVGPathSegCurvetoCubicAbs::x1() const
{
    if(!impl)
        return -1;

    return _impl->x1();
}

void SVGPathSegCurvetoCubicAbs::setY1(float y1)
{
    if(impl)
        _impl->setY1(y1);
}

float SVGPathSegCurvetoCubicAbs::y1() const
{
    if(!impl)
        return -1;

    return _impl->y1();
}

void SVGPathSegCurvetoCubicAbs::setX2(float x2)
{
    if(impl)
        _impl->setX2(x2);
}

float SVGPathSegCurvetoCubicAbs::x2() const
{
    if(!impl)
        return -1;

    return _impl->x2();
}

void SVGPathSegCurvetoCubicAbs::setY2(float y2)
{
    if(impl)
        _impl->setY2(y2);
}

float SVGPathSegCurvetoCubicAbs::y2() const
{
    if(!impl)
        return -1;

    return _impl->y2();
}



/*
@begin SVGPathSegCurvetoCubicRel::s_hashTable 7
 x        SVGPathSegCurvetoCubicConstants::X        DontDelete
 y        SVGPathSegCurvetoCubicConstants::Y        DontDelete
 x1        SVGPathSegCurvetoCubicConstants::X1        DontDelete
 y1        SVGPathSegCurvetoCubicConstants::Y1        DontDelete
 x2        SVGPathSegCurvetoCubicConstants::X2        DontDelete
 y2        SVGPathSegCurvetoCubicConstants::Y2        DontDelete
@end
*/

ValueImp *SVGPathSegCurvetoCubicRel::getValueProperty(ExecState *, int token) const
{
    switch(token)
    {
    case SVGPathSegCurvetoCubicConstants::X:
        return Number(x());
    case SVGPathSegCurvetoCubicConstants::Y:
        return Number(y());
    case SVGPathSegCurvetoCubicConstants::X1:
        return Number(x1());
    case SVGPathSegCurvetoCubicConstants::Y1:
        return Number(y1());
    case SVGPathSegCurvetoCubicConstants::X2:
        return Number(x2());
    case SVGPathSegCurvetoCubicConstants::Y2:
        return Number(y2());
    default:
        kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    return Undefined();
}

void SVGPathSegCurvetoCubicRel::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
    switch(token)
    {
        case SVGPathSegCurvetoCubicConstants::X:
        {
            setX(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicConstants::Y:
        {
            setY(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicConstants::X1:
        {
            setX1(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicConstants::Y1:
        {
            setY1(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicConstants::X2:
        {
            setX2(value->toNumber(exec));
            return;
        }
        case SVGPathSegCurvetoCubicConstants::Y2:
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
#define _impl (static_cast<SVGPathSegCurvetoCubicRelImpl *>(impl))

SVGPathSegCurvetoCubicRel SVGPathSegCurvetoCubicRel::null;

SVGPathSegCurvetoCubicRel::SVGPathSegCurvetoCubicRel() : SVGPathSeg()
{
}

SVGPathSegCurvetoCubicRel::SVGPathSegCurvetoCubicRel(SVGPathSegCurvetoCubicRelImpl *other) : SVGPathSeg(other)
{
}

SVGPathSegCurvetoCubicRel::SVGPathSegCurvetoCubicRel(const SVGPathSegCurvetoCubicRel &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegCurvetoCubicRel::SVGPathSegCurvetoCubicRel(const SVGPathSeg &other) : SVGPathSeg()
{
    (*this) = other;
}

SVGPathSegCurvetoCubicRel::~SVGPathSegCurvetoCubicRel()
{
}

SVGPathSegCurvetoCubicRel &SVGPathSegCurvetoCubicRel::operator=(const SVGPathSegCurvetoCubicRel &other)
{
    SVGPathSeg::operator=(other);
    return *this;
}

KSVG_PATHSEG_DERIVED_ASSIGN_OP(SVGPathSegCurvetoCubicRel, PATHSEG_CURVETO_CUBIC_REL)

void SVGPathSegCurvetoCubicRel::setX(float x)
{
    if(impl)
        _impl->setX(x);
}

float SVGPathSegCurvetoCubicRel::x() const
{
    if(!impl)
        return -1;

    return _impl->x();
}

void SVGPathSegCurvetoCubicRel::setY(float y)
{
    if(impl)
        _impl->setY(y);
}

float SVGPathSegCurvetoCubicRel::y() const
{
    if(!impl)
        return -1;

    return _impl->y();
}

void SVGPathSegCurvetoCubicRel::setX1(float x1)
{
    if(impl)
        _impl->setX1(x1);
}

float SVGPathSegCurvetoCubicRel::x1() const
{
    if(!impl)
        return -1;

    return _impl->x1();
}

void SVGPathSegCurvetoCubicRel::setY1(float y1)
{
    if(impl)
        _impl->setY1(y1);
}

float SVGPathSegCurvetoCubicRel::y1() const
{
    if(!impl)
        return -1;

    return _impl->y1();
}

void SVGPathSegCurvetoCubicRel::setX2(float x2)
{
    if(impl)
        _impl->setX2(x2);
}

float SVGPathSegCurvetoCubicRel::x2() const
{
    if(!impl)
        return -1;

    return _impl->x2();
}

void SVGPathSegCurvetoCubicRel::setY2(float y2)
{
    if(impl)
        _impl->setY2(y2);
}

float SVGPathSegCurvetoCubicRel::y2() const
{
    if(!impl)
        return -1;

    return _impl->y2();
}

// vim:ts=4:noet
