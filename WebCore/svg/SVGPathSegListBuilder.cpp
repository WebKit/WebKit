/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGPathSegListBuilder.h"

#include "ExceptionCode.h"
#include "SVGPathElement.h"
#include "SVGPathParser.h"
#include "SVGPathSegArc.h"
#include "SVGPathSegClosePath.h"
#include "SVGPathSegCurvetoCubic.h"
#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGPathSegCurvetoQuadraticSmooth.h"
#include "SVGPathSegLineto.h"
#include "SVGPathSegLinetoHorizontal.h"
#include "SVGPathSegLinetoVertical.h"
#include "SVGPathSegList.h"
#include "SVGPathSegMoveto.h"

namespace WebCore {

SVGPathSegListBuilder::SVGPathSegListBuilder(SVGPathSegList* segList)
    : m_pathSegList(segList)
{
}

bool SVGPathSegListBuilder::build(const String& d, PathParsingMode parsingMode)
{
    if (!m_pathSegList)
        return false;

    SVGPathParser parser(this);
    return parser.parsePathDataString(d, parsingMode == NormalizedParsing);
}

void SVGPathSegListBuilder::moveTo(const FloatPoint& point, bool, PathCoordinateMode mode)
{
    ExceptionCode ec = 0;
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegMovetoAbs(point.x(), point.y()), ec);
    else
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegMovetoRel(point.x(), point.y()), ec);
    ASSERT(!ec);
}

void SVGPathSegListBuilder::lineTo(const FloatPoint& point, PathCoordinateMode mode)
{
    ExceptionCode ec = 0;
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegLinetoAbs(point.x(), point.y()), ec);
    else
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegLinetoRel(point.x(), point.y()), ec);
    ASSERT(!ec);
}

void SVGPathSegListBuilder::lineToHorizontal(float x, PathCoordinateMode mode)
{
    ExceptionCode ec = 0;
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegLinetoHorizontalAbs(x), ec);
    else
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegLinetoHorizontalRel(x), ec);
    ASSERT(!ec);
}

void SVGPathSegListBuilder::lineToVertical(float y, PathCoordinateMode mode)
{
    ExceptionCode ec = 0;
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegLinetoVerticalAbs(y), ec);
    else
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegLinetoVerticalRel(y), ec);
    ASSERT(!ec);
}

void SVGPathSegListBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& point, PathCoordinateMode mode)
{
    ExceptionCode ec = 0;
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegCurvetoCubicAbs(point.x(), point.y(), point1.x(), point1.y(), point2.x(), point2.y()), ec);
    else
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegCurvetoCubicRel(point.x(), point.y(), point1.x(), point1.y(), point2.x(), point2.y()), ec);
    ASSERT(!ec);
}

void SVGPathSegListBuilder::curveToCubicSmooth(const FloatPoint& point, const FloatPoint& point2, PathCoordinateMode mode)
{
    ExceptionCode ec = 0;
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegCurvetoCubicSmoothAbs(point2.x(), point2.y(), point.x(), point.y()), ec);
    else
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegCurvetoCubicSmoothRel(point2.x(), point2.y(), point.x(), point.y()), ec);
    ASSERT(!ec);
}

void SVGPathSegListBuilder::curveToQuadratic(const FloatPoint& point, const FloatPoint& point1, PathCoordinateMode mode)
{
    ExceptionCode ec = 0;
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegCurvetoQuadraticAbs(point1.x(), point1.y(), point.x(), point.y()), ec);
    else
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegCurvetoQuadraticRel(point1.x(), point1.y(), point.x(), point.y()), ec);
    ASSERT(!ec);
}

void SVGPathSegListBuilder::curveToQuadraticSmooth(const FloatPoint& point, PathCoordinateMode mode)
{
    ExceptionCode ec = 0;
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothAbs(point.x(), point.y()), ec);
    else
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothRel(point.x(), point.y()), ec);
    ASSERT(!ec);
}

void SVGPathSegListBuilder::arcTo(const FloatPoint& point, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, PathCoordinateMode mode)
{
    ExceptionCode ec = 0;
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegArcAbs(point.x(), point.y(), r1, r2, angle, largeArcFlag, sweepFlag), ec);
    else
        m_pathSegList->appendItem(SVGPathElement::createSVGPathSegArcRel(point.x(), point.y(), r1, r2, angle, largeArcFlag, sweepFlag), ec);
    ASSERT(!ec);
}

void SVGPathSegListBuilder::closePath()
{
    ExceptionCode ec = 0;
    m_pathSegList->appendItem(SVGPathElement::createSVGPathSegClosePath(), ec);
    ASSERT(!ec);
}

}

#endif // ENABLE(SVG)
