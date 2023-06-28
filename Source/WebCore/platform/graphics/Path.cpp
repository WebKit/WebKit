/*
 * Copyright (C) 2003-2023 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "Path.h"

#include "AffineTransform.h"
#include "PathStream.h"
#include "PathTraversalState.h"
#include "PlatformPathImpl.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Path::Path()
    : m_impl(PathStream::create())
{
}

Path::Path(const Vector<FloatPoint>& points)
    : m_impl(PathStream::create(points))
{
}

Path::Path(Vector<PathSegment>&& segments)
    : m_impl(PathStream::create(WTFMove(segments)))
{
}

Path::Path(UniqueRef<PathImpl>&& impl)
    : m_impl(WTFMove(impl))
{
}

Path::Path(const Path& other)
    : m_impl(other.m_impl->clone())
{
}

Path& Path::operator=(const Path& other)
{
    if (this != &other)
        m_impl = other.m_impl->clone();
    return *this;
}

bool Path::operator==(const Path& other) const
{
    return *m_impl == *other.m_impl;
}

PlatformPathImpl& Path::ensurePlatformPathImpl()
{
    if (auto* stream = dynamicDowncast<PathStream>(*m_impl))
        m_impl = PlatformPathImpl::create(*stream);

    return downcast<PlatformPathImpl>(*m_impl);
}

Path Path::polygonPathFromPoints(const Vector<FloatPoint>& points)
{
    return Path(points);
}

void Path::moveTo(const FloatPoint& point)
{
    m_impl->moveTo(point);
}

void Path::addLineTo(const FloatPoint& point)
{
    if (isEmpty())
        m_impl->moveTo({ });
    m_impl->addLineTo(point);
}

void Path::addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint)
{
    if (isEmpty())
        m_impl->moveTo({ });
    m_impl->addQuadCurveTo(controlPoint, endPoint);
}

void Path::addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint)
{
    if (isEmpty())
        m_impl->moveTo({ });
    m_impl->addBezierCurveTo(controlPoint1, controlPoint2, endPoint);
}

void Path::addArcTo(const FloatPoint& point1, const FloatPoint& point2, float radius)
{
    if (isEmpty())
        m_impl->moveTo({ });
    m_impl->addArcTo(point1, point2, radius);
}

void Path::addArc(const FloatPoint& point, float radius, float startAngle, float endAngle, RotationDirection direction)
{
    m_impl->addArc(point, radius, startAngle, endAngle, direction);
}

void Path::addEllipse(const FloatPoint& point, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, RotationDirection direction)
{
    m_impl->addEllipse(point, radiusX, radiusY, rotation, startAngle, endAngle, direction);
}

void Path::addEllipseInRect(const FloatRect& rect)
{
    m_impl->addEllipseInRect(rect);
}

void Path::addRect(const FloatRect& rect)
{
    m_impl->addRect(rect);
}

static FloatRoundedRect calculateEvenRoundedRect(const FloatRect& rect, const FloatSize& roundingRadii)
{
    FloatSize radius(roundingRadii);
    FloatSize halfSize = rect.size() / 2;

    // Apply the SVG corner radius constraints, per the rect section of the SVG shapes spec: if
    // one of rx,ry is negative, then the other corner radius value is used. If both values are
    // negative then rx = ry = 0. If rx is greater than half of the width of the rectangle
    // then set rx to half of the width; ry is handled similarly.

    if (radius.width() < 0)
        radius.setWidth(std::max<float>(radius.height(), 0));

    if (radius.height() < 0)
        radius.setHeight(radius.width());

    if (radius.width() > halfSize.width())
        radius.setWidth(halfSize.width());

    if (radius.height() > halfSize.height())
        radius.setHeight(halfSize.height());

    return FloatRoundedRect(rect, radius, radius, radius, radius);
}

void Path::addRoundedRect(const FloatRoundedRect& roundedRect, PathRoundedRect::Strategy strategy)
{
    if (roundedRect.isEmpty())
        return;

    if (!roundedRect.isRenderable()) {
        // If all the radii cannot be accommodated, return a rect.
        addRect(roundedRect.rect());
        return;
    }

    m_impl->addRoundedRect(roundedRect, strategy);
}

void Path::addRoundedRect(const FloatRect& rect, const FloatSize& roundingRadii, PathRoundedRect::Strategy strategy)
{
    if (rect.isEmpty())
        return;

    addRoundedRect(calculateEvenRoundedRect(rect, roundingRadii), strategy);
}

void Path::addRoundedRect(const RoundedRect& rect)
{
    addRoundedRect(FloatRoundedRect(rect));
}

void Path::closeSubpath()
{
    if (isEmpty())
        return;

    m_impl->closeSubpath();
}

void Path::addPath(const Path& path, const AffineTransform& transform)
{
    if (path.isEmpty() || !transform.isInvertible())
        return;

    ensurePlatformPathImpl().addPath(const_cast<Path&>(path).ensurePlatformPathImpl(), transform);
}

void Path::applySegments(const PathSegmentApplier& applier) const
{
    m_impl->applySegments(applier);
}

void Path::applyElements(const PathElementApplier& applier) const
{
    const_cast<Path&>(*this).ensurePlatformPathImpl().applyElements(applier);
}

void Path::clear()
{
    m_impl = PathStream::create();
}

void Path::translate(const FloatSize& delta)
{
    transform(AffineTransform::makeTranslation(delta));
}

void Path::transform(const AffineTransform& transform)
{
    if (transform.isIdentity() || isEmpty())
        return;

    ensurePlatformPathImpl().transform(transform);
}

std::optional<PathSegment> Path::singleSegment() const
{
    return m_impl->singleSegment();
}

std::optional<PathDataLine> Path::singleDataLine() const
{
    return m_impl->singleDataLine();
}

std::optional<PathArc> Path::singleArc() const
{
    return m_impl->singleArc();
}

std::optional<PathDataQuadCurve> Path::singleQuadCurve() const
{
    return m_impl->singleQuadCurve();
}

std::optional<PathDataBezierCurve> Path::singleBezierCurve() const
{
    return m_impl->singleBezierCurve();
}

bool Path::isEmpty() const
{
    return m_impl->isEmpty();
}

PlatformPathPtr Path::platformPath() const
{
    return const_cast<Path&>(*this).ensurePlatformPathImpl().platformPath();
}

const Vector<PathSegment>* Path::segmentsIfExists() const
{
    if (auto* stream = dynamicDowncast<PathStream>(*m_impl))
        return &stream->segments();

    return nullptr;
}

Vector<PathSegment> Path::segments() const
{
    if (const auto* segments = segmentsIfExists())
        return *segments;

    Vector<PathSegment> segments;
    applySegments([&](const PathSegment& segment) {
        segments.append(segment);
    });

    return segments;
}

float Path::length() const
{
    PathTraversalState traversalState(PathTraversalState::Action::TotalLength);

    applyElements([&traversalState](const PathElement& element) {
        traversalState.processPathElement(element);
    });

    return traversalState.totalLength();
}

bool Path::isClosed() const
{
    if (isEmpty())
        return false;

    return m_impl->isClosed();
}

FloatPoint Path::currentPoint() const
{
    if (isEmpty())
        return { };

    return m_impl->currentPoint();
}

PathTraversalState Path::traversalStateAtLength(float length) const
{
    PathTraversalState traversalState(PathTraversalState::Action::VectorAtLength, length);

    applyElements([&traversalState](const PathElement& element) {
        traversalState.processPathElement(element);
    });

    return traversalState;
}

FloatPoint Path::pointAtLength(float length) const
{
    return traversalStateAtLength(length).current();
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    if (isEmpty())
        return false;

    return const_cast<Path&>(*this).ensurePlatformPathImpl().contains(point, rule);
}

bool Path::strokeContains(const FloatPoint& point, const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    ASSERT(strokeStyleApplier);

    if (isEmpty())
        return false;

    return const_cast<Path&>(*this).ensurePlatformPathImpl().strokeContains(point, strokeStyleApplier);
}

FloatRect Path::fastBoundingRect() const
{
    if (isEmpty())
        return { };

    return m_impl->fastBoundingRect();
}

FloatRect Path::boundingRect() const
{
    if (isEmpty())
        return { };

    return m_impl->boundingRect();
}

FloatRect Path::strokeBoundingRect(const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    return const_cast<Path&>(*this).ensurePlatformPathImpl().strokeBoundingRect(strokeStyleApplier);
}

TextStream& operator<<(TextStream& ts, const Path& path)
{
    bool isFirst = true;
    path.applySegments([&ts, &isFirst](const PathSegment& segment) {
        if (!isFirst)
            ts << ", ";
        else
            isFirst = false;
        ts << segment;
    });
    return ts;
}

} // namespace WebCore
