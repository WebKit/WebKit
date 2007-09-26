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
#include "SVGPathSegCurvetoCubicSmooth.h"

#include "SVGStyledElement.h"

namespace WebCore {

SVGPathSegCurvetoCubicSmoothAbs::SVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
    , m_x2(x2)
    , m_y2(y2)
{
}

SVGPathSegCurvetoCubicSmoothAbs::~SVGPathSegCurvetoCubicSmoothAbs()
{
}

void SVGPathSegCurvetoCubicSmoothAbs::setX(float x)
{
    m_x = x;
}

float SVGPathSegCurvetoCubicSmoothAbs::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicSmoothAbs::setY(float y)
{
    m_y = y;
}

float SVGPathSegCurvetoCubicSmoothAbs::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicSmoothAbs::setX2(float x2)
{
    m_x2 = x2;
}

float SVGPathSegCurvetoCubicSmoothAbs::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicSmoothAbs::setY2(float y2)
{
    m_y2 = y2;
}

float SVGPathSegCurvetoCubicSmoothAbs::y2() const
{
    return m_y2;
}



SVGPathSegCurvetoCubicSmoothRel::SVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
    , m_x2(x2)
    , m_y2(y2)
{
}

SVGPathSegCurvetoCubicSmoothRel::~SVGPathSegCurvetoCubicSmoothRel()
{
}

void SVGPathSegCurvetoCubicSmoothRel::setX(float x)
{
    m_x = x;
}

float SVGPathSegCurvetoCubicSmoothRel::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicSmoothRel::setY(float y)
{
    m_y = y;
}

float SVGPathSegCurvetoCubicSmoothRel::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicSmoothRel::setX2(float x2)
{
    m_x2 = x2;
}

float SVGPathSegCurvetoCubicSmoothRel::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicSmoothRel::setY2(float y2)
{
    m_y2 = y2;
}

float SVGPathSegCurvetoCubicSmoothRel::y2() const
{
    return m_y2;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
