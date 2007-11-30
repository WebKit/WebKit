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
#include "SVGPathSegMoveto.h"

#include "SVGStyledElement.h"

namespace WebCore {

SVGPathSegMovetoAbs::SVGPathSegMovetoAbs(float x, float y)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
{
}

SVGPathSegMovetoAbs::~SVGPathSegMovetoAbs()
{
}

void SVGPathSegMovetoAbs::setX(float x)
{
    m_x = x;
}

float SVGPathSegMovetoAbs::x() const
{
    return m_x;
}

void SVGPathSegMovetoAbs::setY(float y)
{
    m_y = y;
}

float SVGPathSegMovetoAbs::y() const
{
    return m_y;
}




SVGPathSegMovetoRel::SVGPathSegMovetoRel(float x, float y)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
{
}

SVGPathSegMovetoRel::~SVGPathSegMovetoRel()
{
}

void SVGPathSegMovetoRel::setX(float x)
{
    m_x = x;
}

float SVGPathSegMovetoRel::x() const
{
    return m_x;
}

void SVGPathSegMovetoRel::setY(float y)
{
    m_y = y;
}

float SVGPathSegMovetoRel::y() const
{
    return m_y;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
