/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#include "config.h"

#ifdef SVG_SUPPORT
#include "SVGPathSegArc.h"

#include "SVGStyledElement.h"

namespace WebCore {

SVGPathSegArcAbs::SVGPathSegArcAbs()
    : SVGPathSeg()
{
    m_x = m_y = m_r1 = m_r2 = m_angle = 0.0;
    m_largeArcFlag = m_sweepFlag = false;
}

SVGPathSegArcAbs::~SVGPathSegArcAbs()
{
}

void SVGPathSegArcAbs::setX(double x)
{
    m_x = x;
}

double SVGPathSegArcAbs::x() const
{
    return m_x;
}

void SVGPathSegArcAbs::setY(double y)
{
    m_y = y;
}

double SVGPathSegArcAbs::y() const
{
    return m_y;
}

void SVGPathSegArcAbs::setR1(double r1)
{
    m_r1 = r1;
}

double SVGPathSegArcAbs::r1() const
{
    return m_r1;
}

void SVGPathSegArcAbs::setR2(double r2)
{
    m_r2 = r2;
}

double SVGPathSegArcAbs::r2() const
{
    return m_r2;
}

void SVGPathSegArcAbs::setAngle(double angle)
{
    m_angle = angle;
}

double SVGPathSegArcAbs::angle() const
{
    return m_angle;
}

void SVGPathSegArcAbs::setLargeArcFlag(bool largeArcFlag)
{
    m_largeArcFlag = largeArcFlag;
}

bool SVGPathSegArcAbs::largeArcFlag() const
{
    return m_largeArcFlag;
}

void SVGPathSegArcAbs::setSweepFlag(bool sweepFlag)
{
    m_sweepFlag = sweepFlag;
}

bool SVGPathSegArcAbs::sweepFlag() const
{
    return m_sweepFlag;
}



SVGPathSegArcRel::SVGPathSegArcRel()
    : SVGPathSeg()
{
    m_x = m_y = m_r1 = m_r2 = m_angle = 0.0;
    m_largeArcFlag = m_sweepFlag = false;
}

SVGPathSegArcRel::~SVGPathSegArcRel()
{
}

void SVGPathSegArcRel::setX(double x)
{
    m_x = x;
}

double SVGPathSegArcRel::x() const
{
    return m_x;
}

void SVGPathSegArcRel::setY(double y)
{
    m_y = y;
}

double SVGPathSegArcRel::y() const
{
    return m_y;
}

void SVGPathSegArcRel::setR1(double r1)
{
    m_r1 = r1;
}

double SVGPathSegArcRel::r1() const
{
    return m_r1;
}

void SVGPathSegArcRel::setR2(double r2)
{
    m_r2 = r2;
}

double SVGPathSegArcRel::r2() const
{
    return m_r2;
}

void SVGPathSegArcRel::setAngle(double angle)
{
    m_angle = angle;
}

double SVGPathSegArcRel::angle() const
{
    return m_angle;
}

void SVGPathSegArcRel::setLargeArcFlag(bool largeArcFlag)
{
    m_largeArcFlag = largeArcFlag;
}

bool SVGPathSegArcRel::largeArcFlag() const
{
    return m_largeArcFlag;
}

void SVGPathSegArcRel::setSweepFlag(bool sweepFlag)
{
    m_sweepFlag = sweepFlag;
}

bool SVGPathSegArcRel::sweepFlag() const
{
    return m_sweepFlag;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
