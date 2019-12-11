/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009, 2015 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "SVGPathSegListBuilder.h"

#include "SVGPathSegImpl.h"
#include "SVGPathSegList.h"

namespace WebCore {

SVGPathSegListBuilder::SVGPathSegListBuilder(SVGPathSegList& pathSegList)
    : m_pathSegList(pathSegList)
{
}

void SVGPathSegListBuilder::moveTo(const FloatPoint& targetPoint, bool, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_pathSegList.append(SVGPathSegMovetoAbs::create(targetPoint.x(), targetPoint.y()));
    else
        m_pathSegList.append(SVGPathSegMovetoRel::create(targetPoint.x(), targetPoint.y()));
}

void SVGPathSegListBuilder::lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_pathSegList.append(SVGPathSegLinetoAbs::create(targetPoint.x(), targetPoint.y()));
    else
        m_pathSegList.append(SVGPathSegLinetoRel::create(targetPoint.x(), targetPoint.y()));
}

void SVGPathSegListBuilder::lineToHorizontal(float x, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_pathSegList.append(SVGPathSegLinetoHorizontalAbs::create(x));
    else
        m_pathSegList.append(SVGPathSegLinetoHorizontalRel::create(x));
}

void SVGPathSegListBuilder::lineToVertical(float y, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_pathSegList.append(SVGPathSegLinetoVerticalAbs::create(y));
    else
        m_pathSegList.append(SVGPathSegLinetoVerticalRel::create(y));
}

void SVGPathSegListBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_pathSegList.append(SVGPathSegCurvetoCubicAbs::create(targetPoint.x(), targetPoint.y(), point1.x(), point1.y(), point2.x(), point2.y()));
    else
        m_pathSegList.append(SVGPathSegCurvetoCubicRel::create(targetPoint.x(), targetPoint.y(), point1.x(), point1.y(), point2.x(), point2.y()));
}

void SVGPathSegListBuilder::curveToCubicSmooth(const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_pathSegList.append(SVGPathSegCurvetoCubicSmoothAbs::create(targetPoint.x(), targetPoint.y(), point2.x(), point2.y()));
    else
        m_pathSegList.append(SVGPathSegCurvetoCubicSmoothRel::create(targetPoint.x(), targetPoint.y(), point2.x(), point2.y()));
}

void SVGPathSegListBuilder::curveToQuadratic(const FloatPoint& point1, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_pathSegList.append(SVGPathSegCurvetoQuadraticAbs::create(targetPoint.x(), targetPoint.y(), point1.x(), point1.y()));
    else
        m_pathSegList.append(SVGPathSegCurvetoQuadraticRel::create(targetPoint.x(), targetPoint.y(), point1.x(), point1.y()));
}

void SVGPathSegListBuilder::curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_pathSegList.append(SVGPathSegCurvetoQuadraticSmoothAbs::create(targetPoint.x(), targetPoint.y()));
    else
        m_pathSegList.append(SVGPathSegCurvetoQuadraticSmoothRel::create(targetPoint.x(), targetPoint.y()));
}

void SVGPathSegListBuilder::arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_pathSegList.append(SVGPathSegArcAbs::create(targetPoint.x(), targetPoint.y(), r1, r2, angle, largeArcFlag, sweepFlag));
    else
        m_pathSegList.append(SVGPathSegArcRel::create(targetPoint.x(), targetPoint.y(), r1, r2, angle, largeArcFlag, sweepFlag));
}

void SVGPathSegListBuilder::closePath()
{
    m_pathSegList.append(SVGPathSegClosePath::create());
}

}
