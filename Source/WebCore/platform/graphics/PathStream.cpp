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

Ref<PathStream> PathStream::create()
{
    return adoptRef(*new PathStream);
}

Ref<PathStream> PathStream::create(PathSegment&& segment)
{
    return adoptRef(*new PathStream(WTFMove(segment)));
}

Ref<PathStream> PathStream::create(Vector<PathSegment>&& segments)
{
    return adoptRef(*new PathStream(WTFMove(segments)));
}

Ref<PathStream> PathStream::create(const Vector<PathSegment>& segments)
{
    return adoptRef(*new PathStream(segments));
}

Ref<PathStream> PathStream::create(const Vector<FloatPoint>& points)
{
    auto stream = PathStream::create();
    if (points.size() < 2)
        return stream;

    stream->add(PathMoveTo { points[0] });
    for (size_t i = 1; i < points.size(); ++i)
        stream->add(PathLineTo { points[i] });

    stream->add(PathCloseSubpath { });
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

PathStream::PathStream(PathSegment&& segment)
    : m_segments({ WTFMove(segment) })
{
}

Ref<PathImpl> PathStream::copy() const
{
    return create(m_segments);
}

const PathMoveTo* PathStream::lastIfMoveTo() const
{
    if (isEmpty())
        return nullptr;

    return std::get_if<PathMoveTo>(&m_segments.last().data());
}

void PathStream::add(PathMoveTo moveTo)
{
    segments().append(moveTo);
}

void PathStream::add(PathLineTo lineTo)
{
    if (const auto* moveTo = lastIfMoveTo())
        segments().last() = { PathDataLine { moveTo->point, lineTo.point } };
    else
        segments().append(lineTo);
}

void PathStream::add(PathQuadCurveTo quadTo)
{
    if (const auto* moveTo = lastIfMoveTo())
        segments().last() = { PathDataQuadCurve { moveTo->point, quadTo.controlPoint, quadTo.endPoint } };
    else
        segments().append(quadTo);
}

void PathStream::add(PathBezierCurveTo bezierTo)
{
    if (const auto* moveTo = lastIfMoveTo())
        segments().last() = { PathDataBezierCurve { moveTo->point, bezierTo.controlPoint1, bezierTo.controlPoint2, bezierTo.endPoint } };
    else
        segments().append(bezierTo);
}

void PathStream::add(PathArcTo arcTo)
{
    if (const auto* moveTo = lastIfMoveTo())
        segments().last() = { PathDataArc { moveTo->point, arcTo.controlPoint1, arcTo.controlPoint2, arcTo.radius } };
    else
        segments().append(arcTo);
}

void PathStream::add(PathArc arc)
{
    segments().append(arc);
}

void PathStream::add(PathClosedArc closedArc)
{
    segments().append(closedArc);
}

void PathStream::add(PathRect rect)
{
    segments().append(rect);
}

void PathStream::add(PathEllipse ellipse)
{
    segments().append(ellipse);
}

void PathStream::add(PathEllipseInRect ellipseInRect)
{
    segments().append(ellipseInRect);
}

void PathStream::add(PathRoundedRect roundedRect)
{
    segments().append(roundedRect);
}

void PathStream::add(PathCloseSubpath)
{
    segments().append(PathCloseSubpath { });
}

void PathStream::applySegments(const PathSegmentApplier& applier) const
{
    for (auto& segment : m_segments)
        applier(segment);
}

bool PathStream::applyElements(const PathElementApplier& applier) const
{
    for (auto& segment : m_segments) {
        if (!segment.canApplyElements())
            return false;
    }

    for (auto& segment : m_segments)
        segment.applyElements(applier);

    return true;
}

bool PathStream::transform(const AffineTransform& transform)
{
    for (auto& segment : m_segments) {
        if (!segment.canTransform())
            return false;
    }

    for (auto& segment : m_segments)
        segment.transform(transform);

    return true;
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

std::optional<PathClosedArc> PathStream::singleClosedArc() const
{
    return singleDataType<PathClosedArc>();
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

    return m_segments.last().closesSubpath();
}

FloatPoint PathStream::currentPoint() const
{
    if (m_segments.isEmpty())
        return { };

    if (auto result = m_segments.last().tryGetEndPointWithoutContext())
        return result.value();

    FloatPoint lastMoveToPoint;
    FloatPoint currentPoint;

    for (auto& segment : m_segments)
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

bool PathStream::computeHasSubpaths(std::span<const PathSegment> segments)
{
    FloatPoint lastMoveToPoint;
    FloatPoint currentPoint;
    FloatRect boundingRect = FloatRect::smallestRect();

    for (auto& segment : segments) {
        segment.extendFastBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
        if (!boundingRect.isSmallest() && (boundingRect.height() || boundingRect.width()))
            return true;
        currentPoint = segment.calculateEndPoint(currentPoint, lastMoveToPoint);
    }

    if (boundingRect.isSmallest())
        boundingRect.extend(currentPoint);

    return boundingRect.height() || boundingRect.width();
}

bool PathStream::hasSubpaths() const
{
    return computeHasSubpaths(m_segments.span());
}

FloatRect PathStream::fastBoundingRect() const
{
    return computeFastBoundingRect(m_segments.span());
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
    return computeBoundingRect(m_segments.span());
}

} // namespace WebCore
