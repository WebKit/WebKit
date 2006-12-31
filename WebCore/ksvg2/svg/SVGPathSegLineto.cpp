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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"

#ifdef SVG_SUPPORT
#include "SVGPathSegLineto.h"

#include "SVGStyledElement.h"

namespace WebCore {

SVGPathSegLinetoAbs::SVGPathSegLinetoAbs(double x, double y)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
{
}

SVGPathSegLinetoAbs::~SVGPathSegLinetoAbs()
{
}

void SVGPathSegLinetoAbs::setX(double x)
{
    m_x = x;
}

double SVGPathSegLinetoAbs::x() const
{
    return m_x;
}

void SVGPathSegLinetoAbs::setY(double y)
{
    m_y = y;
}

double SVGPathSegLinetoAbs::y() const
{
    return m_y;
}

SVGPathSegLinetoRel::SVGPathSegLinetoRel(double x, double y)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
{
}

SVGPathSegLinetoRel::~SVGPathSegLinetoRel()
{
}

void SVGPathSegLinetoRel::setX(double x)
{
    m_x = x;
}

double SVGPathSegLinetoRel::x() const
{
    return m_x;
}

void SVGPathSegLinetoRel::setY(double y)
{
    m_y = y;
}

double SVGPathSegLinetoRel::y() const
{
    return m_y;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
