/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "SVGPathByteStreamBuilder.h"

#include "SVGPathSeg.h"
#include "SVGPathStringViewSource.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

SVGPathByteStreamBuilder::SVGPathByteStreamBuilder(SVGPathByteStream& byteStream)
    : m_byteStream(byteStream)
{
}

void SVGPathByteStreamBuilder::moveTo(const FloatPoint& targetPoint, bool, PathCoordinateMode mode)
{
    writeType(mode == RelativeCoordinates ?  SVGPathSegType::MoveToRel : SVGPathSegType::MoveToAbs);
    writeType(targetPoint);
}

void SVGPathByteStreamBuilder::lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    writeSegmentType(mode == RelativeCoordinates ? SVGPathSegType::LineToRel : SVGPathSegType::LineToAbs);
    writeType(targetPoint);
}

void SVGPathByteStreamBuilder::lineToHorizontal(float x, PathCoordinateMode mode)
{
    writeType(mode == RelativeCoordinates ? SVGPathSegType::LineToHorizontalRel : SVGPathSegType::LineToHorizontalAbs);
    writeType(x);
}

void SVGPathByteStreamBuilder::lineToVertical(float y, PathCoordinateMode mode)
{
    writeType(mode == RelativeCoordinates ? SVGPathSegType::LineToVerticalRel : SVGPathSegType::LineToVerticalAbs);
    writeType(y);
}

void SVGPathByteStreamBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    writeType(mode == RelativeCoordinates ? SVGPathSegType::CurveToCubicRel : SVGPathSegType::CurveToCubicAbs);
    writeType(point1);
    writeType(point2);
    writeType(targetPoint);
}

void SVGPathByteStreamBuilder::curveToCubicSmooth(const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    writeType(mode == RelativeCoordinates ? SVGPathSegType::CurveToCubicSmoothRel : SVGPathSegType::CurveToCubicSmoothAbs);
    writeType(point2);
    writeType(targetPoint);
}

void SVGPathByteStreamBuilder::curveToQuadratic(const FloatPoint& point1, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    writeType(mode == RelativeCoordinates ? SVGPathSegType::CurveToQuadraticRel : SVGPathSegType::CurveToQuadraticAbs);
    writeType(point1);
    writeType(targetPoint);
}

void SVGPathByteStreamBuilder::curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    writeType(mode == RelativeCoordinates ? SVGPathSegType::CurveToQuadraticSmoothRel : SVGPathSegType::CurveToQuadraticSmoothAbs);
    writeType(targetPoint);
}

void SVGPathByteStreamBuilder::arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    writeType(mode == RelativeCoordinates ? SVGPathSegType::ArcRel : SVGPathSegType::ArcAbs);
    writeType(r1);
    writeType(r2);
    writeType(angle);
    writeType(largeArcFlag);
    writeType(sweepFlag);
    writeType(targetPoint);
}

void SVGPathByteStreamBuilder::closePath()
{
    writeType(SVGPathSegType::ClosePath);
}

} // namespace WebCore
