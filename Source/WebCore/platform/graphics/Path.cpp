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

Path::Path(const Vector<FloatPoint>& points)
    : m_data(DataRef<PathImpl> { PathStream::create(points) })
{
}

Path::Path(Vector<PathSegment>&& segments)
{
    if (segments.isEmpty())
        return;

    if (segments.size() == 1)
        m_data = WTFMove(segments[0]);
    else
        m_data = DataRef<PathImpl> { PathStream::create(WTFMove(segments)) };
}

Path::Path(Ref<PathImpl>&& impl)
    : m_data(WTFMove(impl))
{
}

Path::Path(const Path& other)
{
    *this = other;
}

Path::Path(PathSegment&& segment)
{
    m_data = WTFMove(segment);
}

PathImpl& Path::setImpl(Ref<PathImpl>&& impl)
{
    auto& platformPathImpl = impl.get();
    m_data = WTFMove(impl);
    return platformPathImpl;
}

PlatformPathImpl& Path::ensurePlatformPathImpl()
{
    if (auto segment = asSingle())
        return downcast<PlatformPathImpl>(setImpl(PlatformPathImpl::create(WTFMove(*segment))));

    if (auto impl = asImpl()) {
        if (auto* stream = dynamicDowncast<PathStream>(*impl))
            return downcast<PlatformPathImpl>(setImpl(PlatformPathImpl::create(*stream)));
        return downcast<PlatformPathImpl>(*impl);
    }

    return downcast<PlatformPathImpl>(setImpl(PlatformPathImpl::create()));
}

PathImpl& Path::ensureImpl()
{
    if (auto segment = asSingle())
        return setImpl(PathStream::create(WTFMove(*segment)));

    if (auto impl = asImpl())
        return *impl;

    return setImpl(PathStream::create());
}

PathImpl* Path::asImpl()
{
    if (auto ref = std::get_if<DataRef<PathImpl>>(&m_data))
        return &ref->access();
    return nullptr;
}

const PathImpl* Path::asImpl() const
{
    if (auto ref = std::get_if<DataRef<PathImpl>>(&m_data))
        return ref->ptr();
    return nullptr;
}

void Path::moveTo(const FloatPoint& point)
{
    if (isEmpty())
        m_data = PathSegment(PathMoveTo { point });
    else
        ensureImpl().add(PathMoveTo { point });
}

const PathMoveTo* Path::asSingleMoveTo() const
{
    if (auto segment = asSingle())
        return std::get_if<PathMoveTo>(&segment->data());
    return nullptr;
}

const PathArc* Path::asSingleArc() const
{
    if (auto segment = asSingle())
        return std::get_if<PathArc>(&segment->data());
    return nullptr;
}

void Path::addLineTo(const FloatPoint& point)
{
    if (isEmpty())
        m_data = PathSegment(PathDataLine { { }, point });
    else if (auto moveTo = asSingleMoveTo())
        m_data = PathSegment(PathDataLine { moveTo->point, point });
    else
        ensureImpl().add(PathLineTo { point });
}

void Path::addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint)
{
    if (isEmpty())
        m_data = PathSegment(PathDataQuadCurve { { }, controlPoint, endPoint });
    else if (auto moveTo = asSingleMoveTo())
        m_data = PathSegment(PathDataQuadCurve { moveTo->point, controlPoint, endPoint });
    else
        ensureImpl().add(PathQuadCurveTo { controlPoint, endPoint });
}

void Path::addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint)
{
    if (isEmpty())
        m_data = PathSegment(PathDataBezierCurve { { }, controlPoint1, controlPoint2, endPoint });
    else if (auto moveTo = asSingleMoveTo())
        m_data = PathSegment(PathDataBezierCurve { moveTo->point, controlPoint1, controlPoint2, endPoint });
    else
        ensureImpl().add(PathBezierCurveTo { controlPoint1, controlPoint2, endPoint });
}

void Path::addArcTo(const FloatPoint& point1, const FloatPoint& point2, float radius)
{
    if (isEmpty())
        m_data = PathSegment(PathDataArc { { }, point1, point2, radius });
    else if (auto moveTo = asSingleMoveTo())
        m_data = PathSegment(PathDataArc { moveTo->point, point1, point2, radius });
    else
        ensureImpl().add(PathArcTo { point1, point2, radius });
}

void Path::addArc(const FloatPoint& point, float radius, float startAngle, float endAngle, RotationDirection direction)
{
    // Workaround for <rdar://problem/5189233> CGPathAddArc hangs or crashes when passed inf as start or end angle,
    // as well as http://bugs.webkit.org/show_bug.cgi?id=16449, since cairo_arc() functions hang or crash when
    // passed inf as radius or start/end angle.
    if (!std::isfinite(radius) || !std::isfinite(startAngle) || !std::isfinite(endAngle))
        return;

    if (isEmpty())
        m_data = PathSegment(PathArc { point, radius, startAngle, endAngle, direction });
    else
        ensureImpl().add(PathArc { point, radius, startAngle, endAngle, direction });
}

void Path::addEllipse(const FloatPoint& point, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, RotationDirection direction)
{
    if (isEmpty())
        m_data = PathSegment(PathEllipse { point, radiusX, radiusY, rotation, startAngle, endAngle, direction });
    else
        ensureImpl().add(PathEllipse { point, radiusX, radiusY, rotation, startAngle, endAngle, direction });
}

void Path::addEllipseInRect(const FloatRect& rect)
{
    if (isEmpty())
        m_data = PathSegment(PathEllipseInRect { rect });
    else
        ensureImpl().add(PathEllipseInRect { rect });
}

void Path::addRect(const FloatRect& rect)
{
    if (isEmpty())
        m_data = PathSegment(PathRect { rect });
    else
        ensureImpl().add(PathRect { rect });
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

    if (isEmpty())
        m_data = PathSegment(PathRoundedRect { roundedRect, strategy });
    else
        ensureImpl().add(PathRoundedRect { roundedRect, strategy });
}

void Path::addRoundedRect(const FloatRect& rect, const FloatSize& roundingRadii, PathRoundedRect::Strategy strategy)
{
    if (rect.isEmpty())
        return;

    if (isEmpty())
        m_data = PathSegment(PathRoundedRect { calculateEvenRoundedRect(rect, roundingRadii), strategy });
    else
        ensureImpl().add(PathRoundedRect { calculateEvenRoundedRect(rect, roundingRadii), strategy });
}

void Path::addRoundedRect(const RoundedRect& rect)
{
    addRoundedRect(FloatRoundedRect(rect));
}

void Path::closeSubpath()
{
    if (isEmpty() || isClosed())
        return;

    if (auto arc = asSingleArc())
        m_data = PathSegment(PathClosedArc { *arc });
    else
        ensureImpl().add(PathCloseSubpath { });
}

void Path::addPath(const Path& path, const AffineTransform& transform)
{
    if (path.isEmpty() || !transform.isInvertible())
        return;

    ensurePlatformPathImpl().addPath(const_cast<Path&>(path).ensurePlatformPathImpl(), transform);
}

void Path::applySegments(const PathSegmentApplier& applier) const
{
    if (auto segment = asSingle())
        applier(*segment);
    else if (auto impl = asImpl())
        impl->applySegments(applier);
}

void Path::applyElements(const PathElementApplier& applier) const
{
    if (isEmpty())
        return;

    auto segment = asSingle();
    if (segment && segment->applyElements(applier))
        return;

    auto impl = asImpl();
    if (impl && impl->applyElements(applier))
        return;

    const_cast<Path&>(*this).ensurePlatformPathImpl().applyElements(applier);
}

void Path::clear()
{
    m_data = std::monostate { };
}

void Path::translate(const FloatSize& delta)
{
    transform(AffineTransform::makeTranslation(delta));
}

void Path::transform(const AffineTransform& transform)
{
    if (transform.isIdentity() || isEmpty())
        return;

    auto segment = asSingle();
    if (segment && segment->transform(transform))
        return;

    auto impl = asImpl();
    if (impl && impl->transform(transform))
        return;

    ensurePlatformPathImpl().transform(transform);
}

std::optional<PathSegment> Path::singleSegment() const
{
    if (auto segment = asSingle())
        return *segment;

    if (auto impl = asImpl())
        return impl->singleSegment();

    return std::nullopt;
}

std::optional<PathDataLine> Path::singleDataLine() const
{
    if (auto segment = asSingle()) {
        if (auto data = std::get_if<PathDataLine>(&segment->data()))
            return *data;
    }

    if (auto impl = asImpl())
        return impl->singleDataLine();

    return std::nullopt;
}

std::optional<PathArc> Path::singleArc() const
{
    if (auto segment = asSingle()) {
        if (auto data = std::get_if<PathArc>(&segment->data()))
            return *data;
    }

    if (auto impl = asImpl())
        return impl->singleArc();

    return std::nullopt;
}

std::optional<PathClosedArc> Path::singleClosedArc() const
{
    if (auto segment = asSingle()) {
        if (auto data = std::get_if<PathClosedArc>(&segment->data()))
            return *data;
    }

    if (auto impl = asImpl())
        return impl->singleClosedArc();

    return std::nullopt;
}

std::optional<PathDataQuadCurve> Path::singleQuadCurve() const
{
    if (auto segment = asSingle()) {
        if (auto data = std::get_if<PathDataQuadCurve>(&segment->data()))
            return *data;
    }

    if (auto impl = asImpl())
        return impl->singleQuadCurve();

    return std::nullopt;
}

std::optional<PathDataBezierCurve> Path::singleBezierCurve() const
{
    if (auto segment = asSingle()) {
        if (auto data = std::get_if<PathDataBezierCurve>(&segment->data()))
            return *data;
    }

    if (auto impl = asImpl())
        return impl->singleBezierCurve();

    return std::nullopt;
}

bool Path::isEmpty() const
{
    if (std::holds_alternative<std::monostate>(m_data))
        return true;

    if (auto impl = asImpl())
        return impl->isEmpty();

    return false;
}

bool Path::definitelySingleLine() const
{
    return !!singleDataLine();
}

PlatformPathPtr Path::platformPath() const
{
    return const_cast<Path&>(*this).ensurePlatformPathImpl().platformPath();
}

const Vector<PathSegment>* Path::segmentsIfExists() const
{
    if (auto impl = asImpl()) {
        if (auto* stream = dynamicDowncast<PathStream>(*impl))
            return &stream->segments();
    }

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
    if (auto segment = asSingle())
        return segment->closesSubpath();

    if (auto impl = asImpl())
        return impl->isClosed();

    return false;
}

FloatPoint Path::currentPoint() const
{
    if (auto segment = asSingle()) {
        FloatPoint lastMoveToPoint;
        return segment->calculateEndPoint({ }, lastMoveToPoint);
    }

    if (auto impl = asImpl())
        return impl->currentPoint();

    return { };
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

bool Path::hasSubpaths() const
{
    if (auto segment = asSingle())
        return PathStream::computeHasSubpaths({ segment, 1 });

    if (auto impl = asImpl())
        return impl->hasSubpaths();

    return false;
}

FloatRect Path::fastBoundingRect() const
{
    if (auto segment = asSingle())
        return PathStream::computeFastBoundingRect({ segment, 1 });

    if (auto impl = asImpl())
        return impl->fastBoundingRect();

    return { };
}

FloatRect Path::boundingRect() const
{
    if (auto segment = asSingle())
        return PathStream::computeBoundingRect({ segment, 1 });

    if (auto impl = asImpl())
        return impl->boundingRect();

    return { };
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
