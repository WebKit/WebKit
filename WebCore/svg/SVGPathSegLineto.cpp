/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGPathSegLineto.h"

#include "SVGStyledElement.h"

namespace WebCore {

SVGPathSegLinetoAbs::SVGPathSegLinetoAbs(float x, float y)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
{
}

SVGPathSegLinetoAbs::~SVGPathSegLinetoAbs()
{
}

void SVGPathSegLinetoAbs::setX(float x)
{
    m_x = x;
}

float SVGPathSegLinetoAbs::x() const
{
    return m_x;
}

void SVGPathSegLinetoAbs::setY(float y)
{
    m_y = y;
}

float SVGPathSegLinetoAbs::y() const
{
    return m_y;
}

SVGPathSegLinetoRel::SVGPathSegLinetoRel(float x, float y)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
{
}

SVGPathSegLinetoRel::~SVGPathSegLinetoRel()
{
}

void SVGPathSegLinetoRel::setX(float x)
{
    m_x = x;
}

float SVGPathSegLinetoRel::x() const
{
    return m_x;
}

void SVGPathSegLinetoRel::setY(float y)
{
    m_y = y;
}

float SVGPathSegLinetoRel::y() const
{
    return m_y;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
