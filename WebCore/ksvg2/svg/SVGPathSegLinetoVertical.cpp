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

#if ENABLE(SVG)
#include "SVGPathSegLinetoVertical.h"

#include "SVGStyledElement.h"

namespace WebCore {

SVGPathSegLinetoVerticalAbs::SVGPathSegLinetoVerticalAbs(double y)
    : SVGPathSeg()
    , m_y(y)
{
}

SVGPathSegLinetoVerticalAbs::~SVGPathSegLinetoVerticalAbs()
{
}

void SVGPathSegLinetoVerticalAbs::setY(double y)
{
    m_y = y;
}

double SVGPathSegLinetoVerticalAbs::y() const
{
    return m_y;
}




SVGPathSegLinetoVerticalRel::SVGPathSegLinetoVerticalRel(double y)
    : SVGPathSeg()
    , m_y(y)
{
}

SVGPathSegLinetoVerticalRel::~SVGPathSegLinetoVerticalRel()
{
}

void SVGPathSegLinetoVerticalRel::setY(double y)
{
    m_y = y;
}

double SVGPathSegLinetoVerticalRel::y() const
{
    return m_y;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
