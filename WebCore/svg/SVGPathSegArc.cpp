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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGPathSegArc.h"

#include "SVGStyledElement.h"

namespace WebCore {

SVGPathSegArcAbs::SVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
    , m_r1(r1)
    , m_r2(r2)
    , m_angle(angle)
    , m_largeArcFlag(largeArcFlag)
    , m_sweepFlag(sweepFlag)
{
}

SVGPathSegArcAbs::~SVGPathSegArcAbs()
{
}

void SVGPathSegArcAbs::setX(float x)
{
    m_x = x;
}

float SVGPathSegArcAbs::x() const
{
    return m_x;
}

void SVGPathSegArcAbs::setY(float y)
{
    m_y = y;
}

float SVGPathSegArcAbs::y() const
{
    return m_y;
}

void SVGPathSegArcAbs::setR1(float r1)
{
    m_r1 = r1;
}

float SVGPathSegArcAbs::r1() const
{
    return m_r1;
}

void SVGPathSegArcAbs::setR2(float r2)
{
    m_r2 = r2;
}

float SVGPathSegArcAbs::r2() const
{
    return m_r2;
}

void SVGPathSegArcAbs::setAngle(float angle)
{
    m_angle = angle;
}

float SVGPathSegArcAbs::angle() const
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



SVGPathSegArcRel::SVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
    , m_r1(r1)
    , m_r2(r2)
    , m_angle(angle)
    , m_largeArcFlag(largeArcFlag)
    , m_sweepFlag(sweepFlag)
{
}

SVGPathSegArcRel::~SVGPathSegArcRel()
{
}

void SVGPathSegArcRel::setX(float x)
{
    m_x = x;
}

float SVGPathSegArcRel::x() const
{
    return m_x;
}

void SVGPathSegArcRel::setY(float y)
{
    m_y = y;
}

float SVGPathSegArcRel::y() const
{
    return m_y;
}

void SVGPathSegArcRel::setR1(float r1)
{
    m_r1 = r1;
}

float SVGPathSegArcRel::r1() const
{
    return m_r1;
}

void SVGPathSegArcRel::setR2(float r2)
{
    m_r2 = r2;
}

float SVGPathSegArcRel::r2() const
{
    return m_r2;
}

void SVGPathSegArcRel::setAngle(float angle)
{
    m_angle = angle;
}

float SVGPathSegArcRel::angle() const
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

#endif // ENABLE(SVG)

// vim:ts=4:noet
