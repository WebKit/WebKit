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
#include "SVGPathSegCurvetoCubic.h"

#include "SVGStyledElement.h"

namespace WebCore {

SVGPathSegCurvetoCubicAbs::SVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
    , m_x1(x1)
    , m_y1(y1)
    , m_x2(x2)
    , m_y2(y2)
{
}

SVGPathSegCurvetoCubicAbs::~SVGPathSegCurvetoCubicAbs()
{
}

void SVGPathSegCurvetoCubicAbs::setX(float x)
{
    m_x = x;
}

float SVGPathSegCurvetoCubicAbs::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicAbs::setY(float y)
{
    m_y = y;
}

float SVGPathSegCurvetoCubicAbs::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicAbs::setX1(float x1)
{
    m_x1 = x1;
}

float SVGPathSegCurvetoCubicAbs::x1() const
{
    return m_x1;
}

void SVGPathSegCurvetoCubicAbs::setY1(float y1)
{
    m_y1 = y1;
}

float SVGPathSegCurvetoCubicAbs::y1() const
{
    return m_y1;
}

void SVGPathSegCurvetoCubicAbs::setX2(float x2)
{
    m_x2 = x2;
}

float SVGPathSegCurvetoCubicAbs::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicAbs::setY2(float y2)
{
    m_y2 = y2;
}

float SVGPathSegCurvetoCubicAbs::y2() const
{
    return m_y2;
}




SVGPathSegCurvetoCubicRel::SVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
    , m_x1(x1)
    , m_y1(y1)
    , m_x2(x2)
    , m_y2(y2)
{
}

SVGPathSegCurvetoCubicRel::~SVGPathSegCurvetoCubicRel()
{
}

void SVGPathSegCurvetoCubicRel::setX(float x)
{
    m_x = x;
}

float SVGPathSegCurvetoCubicRel::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicRel::setY(float y)
{
    m_y = y;
}

float SVGPathSegCurvetoCubicRel::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicRel::setX1(float x1)
{
    m_x1 = x1;
}

float SVGPathSegCurvetoCubicRel::x1() const
{
    return m_x1;
}

void SVGPathSegCurvetoCubicRel::setY1(float y1)
{
    m_y1 = y1;
}

float SVGPathSegCurvetoCubicRel::y1() const
{
    return m_y1;
}

void SVGPathSegCurvetoCubicRel::setX2(float x2)
{
    m_x2 = x2;
}

float SVGPathSegCurvetoCubicRel::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicRel::setY2(float y2)
{
    m_y2 = y2;
}

float SVGPathSegCurvetoCubicRel::y2() const
{
    return m_y2;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
