/*
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Path);

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

bool Path::definitelyEqual(const Path& other) const
{
    if (&other == this)
        return true;

    return WTF::switchOn(m_data,
        [&](std::monostate) {
            return other.isEmpty();
        },
        [&](const PathSegment& segment) {
            auto otherSegment = other.singleSegment();
            return otherSegment && segment == otherSegment.value();
        },
        [&](const DataRef<PathImpl>& impl) {
            if (auto singleSegment = impl->singleSegment()) {
                auto otherSegment = other.singleSegment();
                return otherSegment && singleSegment == otherSegment.value();
            }

            return impl.ptr() && other.asImpl() && impl->definitelyEqual(*other.asImpl());
        });
}

PathImpl& Path::setImpl(Ref<PathImpl>&& impl)
{
    auto& platformPathImpl = impl.get();
    m_data = WTFMove(impl);
    return platformPathImpl;
}

PlatformPathImpl& Path::ensurePlatformPathImpl()
{
    if (auto* segment = asSingle())
        return downcast<PlatformPathImpl>(setImpl(PlatformPathImpl::create(singleElementSpan(*segment))));

    if (RefPtr impl = asImpl()) {
        if (const auto* stream = dynamicDowncast<PathStream>(*impl))
            return downcast<PlatformPathImpl>(setImpl(PlatformPathImpl::create(stream->segments())));
        return downcast<PlatformPathImpl>(*impl);
    }
    // Generally platform path is never empty. This should only be called during Path::add() on an empty path.
    return downcast<PlatformPathImpl>(setImpl(PlatformPathImpl::create()));
}

PathImpl& Path::ensureImpl()
{
    if (auto segment = asSingle())
        return setImpl(PathStream::create(WTFMove(*segment)));

    if (auto impl = asImpl())
        return *impl;
    ASSERT_NOT_REACHED(); // Impl is never empty.
    return setImpl(PathStream::create());
}

Ref<PathImpl> Path::ensureProtectedImpl()
{
    return ensureImpl();
}

void Path::ensureImplForTesting()
{
    if (isEmpty())
        return;
    ensureImpl();
}

RefPtr<PathImpl> Path::asImpl()
{
    if (auto ref = std::get_if<DataRef<PathImpl>>(&m_data))
        return &ref->access();
    return nullptr;
}

RefPtr<const PathImpl> Path::asImpl() const
{
    if (auto ref = std::get_if<DataRef<PathImpl>>(&m_data))
        return ref->ptr();
    return nullptr;
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
        ensureProtectedImpl()->add(PathRoundedRect { roundedRect, strategy });
}

void Path::addRoundedRect(const FloatRect& rect, const FloatSize& roundingRadii, PathRoundedRect::Strategy strategy)
{
    if (rect.isEmpty())
        return;

    if (isEmpty())
        m_data = PathSegment(PathRoundedRect { calculateEvenRoundedRect(rect, roundingRadii), strategy });
    else
        ensureProtectedImpl()->add(PathRoundedRect { calculateEvenRoundedRect(rect, roundingRadii), strategy });
}

void Path::addRoundedRect(const LayoutRoundedRect& rect)
{
    addRoundedRect(FloatRoundedRect(rect));
}

void Path::addContinuousRoundedRect(const FloatRect& rect, const float cornerRadius)
{
    if (rect.isEmpty())
        return;

    PathContinuousRoundedRect continuousRoundedRect { rect, cornerRadius, cornerRadius };
    if (isEmpty())
        m_data = PathSegment(continuousRoundedRect);
    else
        ensureProtectedImpl()->add(continuousRoundedRect);
}

void Path::addContinuousRoundedRect(const FloatRect& rect, const float cornerWidth, const float cornerHeight)
{
    if (rect.isEmpty())
        return;

    PathContinuousRoundedRect continuousRoundedRect { rect, cornerWidth, cornerHeight };
    if (isEmpty())
        m_data = PathSegment(continuousRoundedRect);
    else
        ensureProtectedImpl()->add(continuousRoundedRect);
}

void Path::addPath(const Path& path, const AffineTransform& transform)
{
    if (path.isEmpty() || !transform.isInvertible())
        return;

    // FIXME: This should inspect the incoming path and add just the segments if possible.
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
    if (auto segment = singleSegment()) {
        if (auto* line = std::get_if<PathDataLine>(&segment->data()))
            return *line;
    }
    return std::nullopt;
}

bool Path::definitelySingleLine() const
{
    return !!singleDataLine();
}

PlatformPathPtr Path::platformPath() const
{
    if (isEmpty())
        return PlatformPathImpl::emptyPlatformPath();

    return const_cast<Path&>(*this).ensurePlatformPathImpl().platformPath();
}

const Vector<PathSegment>* Path::segmentsIfExists() const
{
    if (auto impl = asImpl()) {
        if (auto* stream = dynamicDowncast<PathStream>((*impl)))
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

bool Path::strokeContains(const FloatPoint& point, NOESCAPE const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    ASSERT(strokeStyleApplier);

    if (isEmpty())
        return false;

    return const_cast<Path&>(*this).ensurePlatformPathImpl().strokeContains(point, strokeStyleApplier);
}

bool Path::hasSubpaths() const
{
    if (auto* segment = asSingle())
        return PathStream::computeHasSubpaths(singleElementSpan(*segment));

    if (auto impl = asImpl())
        return impl->hasSubpaths();

    return false;
}

FloatRect Path::fastBoundingRect() const
{
    if (auto* segment = asSingle())
        return segment->fastBoundingRect();

    if (auto impl = asImpl())
        return impl->fastBoundingRect();

    return { };
}

FloatRect Path::boundingRect() const
{
    if (auto* segment = asSingle())
        return PathStream::computeBoundingRect(singleElementSpan(*segment));

    if (auto impl = asImpl())
        return impl->boundingRect();

    return { };
}

FloatRect Path::strokeBoundingRect(NOESCAPE const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    return const_cast<Path&>(*this).ensurePlatformPathImpl().strokeBoundingRect(strokeStyleApplier);
}

TextStream& operator<<(TextStream& ts, const Path& path)
{
    bool isFirst = true;
    path.applySegments([&ts, &isFirst](const PathSegment& segment) {
        if (!isFirst)
            ts << ", "_s;
        else
            isFirst = false;
        ts << segment;
    });
    return ts;
}

} // namespace WebCore
