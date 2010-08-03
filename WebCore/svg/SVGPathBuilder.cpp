/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 *               2006       Alexander Kellett <lypanov@kde.org>
 *               2006, 2007 Rob Buis <buis@kde.org>
 * Copyrigth (C) 2007, 2009 Apple, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGPathBuilder.h"

#include "SVGPathParser.h"

namespace WebCore {

SVGPathBuilder::SVGPathBuilder(Path& path)
    : m_path(path)
{
}

bool SVGPathBuilder::build(const String& d)
{
    SVGPathParser parser(this);
    return parser.parsePathDataString(d, true);
}

void SVGPathBuilder::moveTo(const FloatPoint& point, bool closed, PathCoordinateMode mode)
{
    m_current = mode == AbsoluteCoordinates ? point : m_current + point;
    if (closed)
        m_path.closeSubpath();
    m_path.moveTo(m_current);
}

void SVGPathBuilder::lineTo(const FloatPoint& point, PathCoordinateMode mode)
{
    m_current = mode == AbsoluteCoordinates ? point : m_current + point;
    m_path.addLineTo(m_current);
}

void SVGPathBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& point, PathCoordinateMode mode)
{
    if (mode == RelativeCoordinates) {
        m_path.addBezierCurveTo(m_current + point1, m_current + point2, m_current + point);
        m_current += point;
    } else {
        m_current = point;
        m_path.addBezierCurveTo(point1, point2, m_current);
    }    
}

void SVGPathBuilder::closePath()
{
    m_path.closeSubpath();
}

}

#endif // ENABLE(SVG)
