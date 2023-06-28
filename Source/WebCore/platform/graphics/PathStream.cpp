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

PathStream::PathStream(Vector<PathSegment>&& segments)
    : m_segments(WTFMove(segments))
{
}

PathStream::PathStream(const Vector<PathSegment>& segments)
    : m_segments(segments)
{
}

UniqueRef<PathImpl> PathStream::clone() const
{
    return create(m_segments);
}

bool PathStream::operator==(const PathImpl& other) const
{
    if (!is<PathStream>(other))
        return false;
    return m_segments == downcast<PathStream>(other).m_segments;
}

template<class DataType1, class DataType2>
bool PathStream::mergeIntoComposite(const DataType2& data2)
{
    if (m_segments.isEmpty())
        return false;

    const auto* data1 = std::get_if<DataType1>(&m_segments.last().data());
    if (!data1)
        return false;

    m_segments.last() = { PathDataComposite<DataType1, DataType2> { *data1, data2 } };
    return true;
}

void PathStream::moveTo(const FloatPoint& point)
{
    m_segments.append(PathMoveTo { point });
}

void PathStream::addLineTo(const FloatPoint& point)
{
    auto lineTo = PathLineTo { point };
    if (mergeIntoComposite<PathMoveTo>(lineTo))
        return;
    m_segments.append(lineTo);
}

void PathStream::addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint)
{
    auto quadCurveTo = PathQuadCurveTo { controlPoint, endPoint };
    if (mergeIntoComposite<PathMoveTo>(quadCurveTo))
        return;
    m_segments.append(quadCurveTo);
}

void PathStream::addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint)
{
    auto bezierCurveTo = PathBezierCurveTo { controlPoint1, controlPoint2, endPoint };
    if (mergeIntoComposite<PathMoveTo>(bezierCurveTo))
        return;
    m_segments.append(bezierCurveTo);
}

void PathStream::addArcTo(const FloatPoint& point1, const FloatPoint& point2, float radius)
{
    auto arcTo = PathArcTo { point1, point2, radius };
    if (mergeIntoComposite<PathMoveTo>(arcTo))
        return;
    m_segments.append(arcTo);
}

void PathStream::addArc(const FloatPoint& point, float radius, float startAngle, float endAngle, RotationDirection direction)
{
    // Workaround for <rdar://problem/5189233> CGPathAddArc hangs or crashes when passed inf as start or end angle,
    // as well as http://bugs.webkit.org/show_bug.cgi?id=16449, since cairo_arc() functions hang or crash when
    // passed inf as radius or start/end angle.
    if (!std::isfinite(radius) || !std::isfinite(startAngle) || !std::isfinite(endAngle))
        return;

    m_segments.append(PathArc { point, radius, startAngle, endAngle, direction });
}

void PathStream::addRect(const FloatRect& rect)
{
    m_segments.append(PathRect { rect });
}

void PathStream::addEllipse(const FloatPoint& point, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, RotationDirection direction)
{
    m_segments.append(PathEllipse { point, radiusX, radiusY, rotation, startAngle, endAngle, direction });
}

void PathStream::addEllipseInRect(const FloatRect& rect)
{
    m_segments.append(PathEllipseInRect { rect });
}

void PathStream::addRoundedRect(const FloatRoundedRect& roundedRect, PathRoundedRect::Strategy strategy)
{
    m_segments.append(PathRoundedRect { roundedRect, strategy });
}

void PathStream::closeSubpath()
{
    m_segments.append(std::monostate());
}

const Vector<PathSegment>& PathStream::segments() const
{
    return m_segments;
}

void PathStream::applySegments(const PathSegmentApplier& applier) const
{
    for (auto& segment : m_segments)
        applier(segment);
}

void PathStream::applyElements(const PathElementApplier& applier) const
{
    for (auto& segment : m_segments)
        segment.applyElements(applier);
}

std::optional<PathSegment> PathStream::singleSegment() const
{
    if (m_segments.size() != 1)
        return std::nullopt;
    return m_segments.first();
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

    return m_segments.last().isCloseSubPath();
}

FloatPoint PathStream::currentPoint() const
{
    FloatPoint lastMoveToPoint;
    FloatPoint currentPoint;

    for (auto& segment : m_segments)
        currentPoint = segment.calculateEndPoint(currentPoint, lastMoveToPoint);

    return currentPoint;
}

FloatRect PathStream::fastBoundingRect() const
{
    FloatPoint lastMoveToPoint;
    FloatPoint currentPoint;
    FloatRect boundingRect = FloatRect::smallestRect();

    for (auto& segment : m_segments) {
        segment.extendFastBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
        currentPoint = segment.calculateEndPoint(currentPoint, lastMoveToPoint);
    }

    if (boundingRect.isSmallest())
        boundingRect.extend(currentPoint);

    return boundingRect;
}

FloatRect PathStream::boundingRect() const
{
    FloatPoint lastMoveToPoint;
    FloatPoint currentPoint;
    FloatRect boundingRect = FloatRect::smallestRect();

    for (auto& segment : m_segments) {
        segment.extendBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
        currentPoint = segment.calculateEndPoint(currentPoint, lastMoveToPoint);
    }

    if (boundingRect.isSmallest())
        boundingRect.extend(currentPoint);

    return boundingRect;
}

} // namespace WebCore
