/*
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
#include "SVGPathByteStreamSource.h"

namespace WebCore {

SVGPathByteStreamSource::SVGPathByteStreamSource(const SVGPathByteStream& stream)
    : m_streamCurrent(stream.bytes().span())
{
}

bool SVGPathByteStreamSource::hasMoreData() const
{
    return !m_streamCurrent.empty();
}

SVGPathSegType SVGPathByteStreamSource::nextCommand(SVGPathSegType)
{
    return readSVGSegmentType().value_or(SVGPathSegType::Unknown);
}

std::optional<SVGPathSegType> SVGPathByteStreamSource::parseSVGSegmentType()
{
    return readSVGSegmentType();
}

std::optional<SVGPathSource::MoveToSegment> SVGPathByteStreamSource::parseMoveToSegment(FloatPoint)
{
    auto targetPoint = readFloatPoint();
    if (!targetPoint)
        return std::nullopt;

    MoveToSegment segment;
    segment.targetPoint = *targetPoint;
    return segment;
}

std::optional<SVGPathSource::LineToSegment> SVGPathByteStreamSource::parseLineToSegment(FloatPoint)
{
    auto targetPoint = readFloatPoint();
    if (!targetPoint)
        return std::nullopt;

    LineToSegment segment;
    segment.targetPoint = *targetPoint;
    return segment;
}

std::optional<SVGPathSource::LineToHorizontalSegment> SVGPathByteStreamSource::parseLineToHorizontalSegment(FloatPoint)
{
    auto x = readFloat();
    if (!x)
        return std::nullopt;

    LineToHorizontalSegment segment;
    segment.x = *x;
    return segment;
}

std::optional<SVGPathSource::LineToVerticalSegment> SVGPathByteStreamSource::parseLineToVerticalSegment(FloatPoint)
{
    auto y = readFloat();
    if (!y)
        return std::nullopt;

    LineToVerticalSegment segment;
    segment.y = *y;
    return segment;
}

std::optional<SVGPathSource::CurveToCubicSegment> SVGPathByteStreamSource::parseCurveToCubicSegment(FloatPoint)
{
    auto point1 = readFloatPoint();
    auto point2 = readFloatPoint();
    auto targetPoint = readFloatPoint();
    if (!point1 || !point2 || !targetPoint)
        return std::nullopt;

    CurveToCubicSegment segment;
    segment.point1 = *point1;
    segment.point2 = *point2;
    segment.targetPoint = *targetPoint;
    return segment;
}

std::optional<SVGPathSource::CurveToCubicSmoothSegment> SVGPathByteStreamSource::parseCurveToCubicSmoothSegment(FloatPoint)
{
    auto point2 = readFloatPoint();
    auto targetPoint = readFloatPoint();
    if (!point2 || !targetPoint)
        return std::nullopt;

    CurveToCubicSmoothSegment segment;
    segment.point2 = *point2;
    segment.targetPoint = *targetPoint;
    return segment;
}

std::optional<SVGPathSource::CurveToQuadraticSegment> SVGPathByteStreamSource::parseCurveToQuadraticSegment(FloatPoint)
{
    auto point1 = readFloatPoint();
    auto targetPoint = readFloatPoint();
    if (!point1 || !targetPoint)
        return std::nullopt;

    CurveToQuadraticSegment segment;
    segment.point1 = *point1;
    segment.targetPoint = *targetPoint;
    return segment;
}

std::optional<SVGPathSource::CurveToQuadraticSmoothSegment> SVGPathByteStreamSource::parseCurveToQuadraticSmoothSegment(FloatPoint)
{
    auto targetPoint = readFloatPoint();
    if (!targetPoint)
        return std::nullopt;

    CurveToQuadraticSmoothSegment segment;
    segment.targetPoint = *targetPoint;
    return segment;
}

std::optional<SVGPathSource::ArcToSegment> SVGPathByteStreamSource::parseArcToSegment(FloatPoint)
{
    auto rx = readFloat();
    auto ry = readFloat();
    auto angle = readFloat();
    auto largeArc = readFlag();
    auto sweep = readFlag();
    auto targetPoint = readFloatPoint();
    if (!rx || !ry || !angle || !largeArc || !sweep || !targetPoint)
        return std::nullopt;

    ArcToSegment segment;
    segment.rx = *rx;
    segment.ry = *ry;
    segment.angle = *angle;
    segment.largeArc = *largeArc;
    segment.sweep = *sweep;
    segment.targetPoint = *targetPoint;
    return segment;
}

}
