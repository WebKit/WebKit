/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PathStream.h"

#include "GeometryUtilities.h"

namespace WebCore {

UniqueRef<PathStream> PathStream::create()
{
    return makeUniqueRef<PathStream>();
}

UniqueRef<PathStream> PathStream::create(const PathStream& pathStream)
{
    return makeUniqueRef<PathStream>(pathStream);
}

UniqueRef<PathStream> PathStream::create(PathSegment&& segment)
{
    return makeUniqueRef<PathStream>(WTFMove(segment));
}

UniqueRef<PathStream> PathStream::create(Vector<PathSegment>&& segments)
{
    return makeUniqueRef<PathStream>(WTFMove(segments));
}

UniqueRef<PathStream> PathStream::create(const Vector<PathSegment>& segments)
{
    return makeUniqueRef<PathStream>(segments);
}

UniqueRef<PathStream> PathStream::create(const Vector<FloatPoint>& points)
{
    auto stream = PathStream::create();
    if (points.size() < 2)
        return stream;

    stream->moveTo(points[0]);
    for (size_t i = 1; i < points.size(); ++i)
        stream->addLineTo(points[i]);

    stream->closeSubpath();
    return stream;
}

PathStream::PathStream()
    : m_segmentsData(SegmentsData::create())
{
}

PathStream::PathStream(Vector<PathSegment>&& segments)
    : m_segmentsData(SegmentsData::create(WTFMove(segments)))
{
}

PathStream::PathStream(const Vector<PathSegment>& segments)
    : m_segmentsData(SegmentsData::create(segments))
{
}

PathStream::PathStream(const PathStream& pathStream)
    : m_segmentsData(pathStream.m_segmentsData)
{
}

PathStream::PathStream(PathSegment&& segment)
    : m_segmentsData(SegmentsData::create(WTFMove(segment)))
{
}

UniqueRef<PathImpl> PathStream::clone() const
{
    return create(*this);
}

bool PathStream::operator==(const PathImpl& other) const
{
    if (!is<PathStream>(other))
        return false;

    return m_segmentsData == downcast<PathStream>(other).m_segmentsData;
}

const PathMoveTo* PathStream::lastIfMoveTo() const
{
    if (isEmpty())
        return nullptr;

    return std::get_if<PathMoveTo>(&m_segmentsData->segments.last().data());
}

void PathStream::moveTo(const FloatPoint& point)
{
    segments().append(PathMoveTo { point });
}

void PathStream::addLineTo(const FloatPoint& point)
{
    if (const auto* moveTo = lastIfMoveTo())
        segments().last() = { PathDataLine { moveTo->point, point } };
    else
        segments().append(PathLineTo { point });
}

void PathStream::addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint)
{
    if (const auto* moveTo = lastIfMoveTo())
        segments().last() = { PathDataQuadCurve { moveTo->point, controlPoint, endPoint } };
    else
        segments().append(PathQuadCurveTo { controlPoint, endPoint });
}

void PathStream::addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint)
{
    if (const auto* moveTo = lastIfMoveTo())
        segments().last() = { PathDataBezierCurve { moveTo->point, controlPoint1, controlPoint2, endPoint } };
    else
        segments().append(PathBezierCurveTo { controlPoint1, controlPoint2, endPoint });
}

void PathStream::addArcTo(const FloatPoint& point1, const FloatPoint& point2, float radius)
{
    if (const auto* moveTo = lastIfMoveTo())
        segments().last() = { PathDataArc { moveTo->point, point1, point2, radius } };
    else
        segments().append(PathArcTo { point1, point2, radius });
}

void PathStream::addArc(const FloatPoint& point, float radius, float startAngle, float endAngle, RotationDirection direction)
{
    segments().append(PathArc { point, radius, startAngle, endAngle, direction });
}

void PathStream::addRect(const FloatRect& rect)
{
    segments().append(PathRect { rect });
}

void PathStream::addEllipse(const FloatPoint& point, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, RotationDirection direction)
{
    segments().append(PathEllipse { point, radiusX, radiusY, rotation, startAngle, endAngle, direction });
}

void PathStream::addEllipseInRect(const FloatRect& rect)
{
    segments().append(PathEllipseInRect { rect });
}

void PathStream::addRoundedRect(const FloatRoundedRect& roundedRect, PathRoundedRect::Strategy strategy)
{
    segments().append(PathRoundedRect { roundedRect, strategy });
}

void PathStream::closeSubpath()
{
    segments().append(PathCloseSubpath { });
}

const Vector<PathSegment>& PathStream::segments() const
{
    return m_segmentsData->segments;
}

void PathStream::applySegments(const PathSegmentApplier& applier) const
{
    for (auto& segment : m_segmentsData->segments)
        applier(segment);
}

bool PathStream::applyElements(const PathElementApplier& applier) const
{
    for (auto& segment : m_segmentsData->segments) {
        if (!segment.canApplyElements())
            return false;
    }

    for (auto& segment : m_segmentsData->segments)
        segment.applyElements(applier);

    return true;
}

bool PathStream::transform(const AffineTransform& transform)
{
    for (auto& segment : m_segmentsData->segments) {
        if (!segment.canTransform())
            return false;
    }

    for (auto& segment : m_segmentsData.access().segments)
        segment.transform(transform);

    return true;
}

std::optional<PathSegment> PathStream::singleSegment() const
{
    if (m_segmentsData->segments.size() != 1)
        return std::nullopt;

    return m_segmentsData->segments.first();
}

template<class DataType>
std::optional<DataType> PathStream::singleDataType() const
{
    const auto segment = singleSegment();
    if (!segment)
        return std::nullopt;
    const auto data = std::get_if<DataType>(&segment->data());
    if (!data)
        return std::nullopt;
    return *data;
}

std::optional<PathDataLine> PathStream::singleDataLine() const
{
    return singleDataType<PathDataLine>();
}

std::optional<PathArc> PathStream::singleArc() const
{
    return singleDataType<PathArc>();
}

std::optional<PathDataQuadCurve> PathStream::singleQuadCurve() const
{
    return singleDataType<PathDataQuadCurve>();
}

std::optional<PathDataBezierCurve> PathStream::singleBezierCurve() const
{
    return singleDataType<PathDataBezierCurve>();
}

bool PathStream::isClosed() const
{
    if (isEmpty())
        return false;

    return m_segmentsData->segments.last().isCloseSubPath();
}

FloatPoint PathStream::currentPoint() const
{
    FloatPoint lastMoveToPoint;
    FloatPoint currentPoint;

    for (auto& segment : m_segmentsData->segments)
        currentPoint = segment.calculateEndPoint(currentPoint, lastMoveToPoint);

    return currentPoint;
}

FloatRect PathStream::computeFastBoundingRect(std::span<const PathSegment> segments)
{
    FloatPoint lastMoveToPoint;
    FloatPoint currentPoint;
    FloatRect boundingRect = FloatRect::smallestRect();

    for (auto& segment : segments) {
        segment.extendFastBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
        currentPoint = segment.calculateEndPoint(currentPoint, lastMoveToPoint);
    }

    if (boundingRect.isSmallest())
        boundingRect.extend(currentPoint);

    return boundingRect;
}

FloatRect PathStream::fastBoundingRect() const
{
    return computeFastBoundingRect(m_segmentsData->segments.span());
}

FloatRect PathStream::computeBoundingRect(std::span<const PathSegment> segments)
{
    FloatPoint lastMoveToPoint;
    FloatPoint currentPoint;
    FloatRect boundingRect = FloatRect::smallestRect();

    for (auto& segment : segments) {
        segment.extendBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
        currentPoint = segment.calculateEndPoint(currentPoint, lastMoveToPoint);
    }

    if (boundingRect.isSmallest())
        boundingRect.extend(currentPoint);

    return boundingRect;
}

FloatRect PathStream::boundingRect() const
{
    return computeBoundingRect(m_segmentsData->segments.span());
}

} // namespace WebCore
