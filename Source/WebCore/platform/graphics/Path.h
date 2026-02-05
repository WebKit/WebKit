/*
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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

#pragma once

#include <WebCore/PathElement.h>
#include <WebCore/PathImpl.h>
#include <WebCore/PathSegment.h>
#include <WebCore/PlatformPath.h>
#include <WebCore/WindRule.h>
#include <wtf/DataRef.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Variant.h>

namespace WebCore {

class GraphicsContext;
class LayoutRoundedRect;
class PathTraversalState;

class Path {
    WTF_MAKE_TZONE_ALLOCATED(Path);
public:
    Path() = default;
    Path(PathSegment&&);
    WEBCORE_EXPORT Path(Vector<PathSegment>&&);
    explicit Path(const Vector<FloatPoint>& points);
    WEBCORE_EXPORT Path(Ref<PathImpl>&&);

    Path(const Path&) = default;
    Path(Path&&) = default;
    Path& operator=(const Path&) = default;
    Path& operator=(Path&&) = default;

    WEBCORE_EXPORT bool definitelyEqual(const Path&) const;

    void moveTo(const FloatPoint&);
    void addLineTo(const FloatPoint&);
    void addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint);
    void addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint);
    void addArcTo(const FloatPoint& point1, const FloatPoint& point2, float radius);
    void addArc(const FloatPoint&, float radius, float startAngle, float endAngle, RotationDirection);
    void addEllipse(const FloatPoint&, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, RotationDirection);
    void addEllipseInRect(const FloatRect&);
    void addRect(const FloatRect&);
    WEBCORE_EXPORT void addRoundedRect(const FloatRoundedRect&, PathRoundedRect::Strategy = PathRoundedRect::Strategy::PreferNative);
    void addRoundedRect(const FloatRect&, const FloatSize& roundingRadii, PathRoundedRect::Strategy = PathRoundedRect::Strategy::PreferNative);
    void addContinuousRoundedRect(const FloatRect&, float cornerRadius);
    void addContinuousRoundedRect(const FloatRect&, float cornerWidth, float cornerHeight);
    void addRoundedRect(const LayoutRoundedRect&);
    void closeSubpath();

    WEBCORE_EXPORT void addPath(const Path&, const AffineTransform&);

    void applySegments(const PathSegmentApplier&) const;
    WEBCORE_EXPORT void applyElements(const PathElementApplier&) const;
    void clear();

    void translate(const FloatSize& delta);
    void transform(const AffineTransform&);

    static constexpr float circleControlPoint() { return PathImpl::circleControlPoint(); }

    WEBCORE_EXPORT std::optional<PathSegment> singleSegment() const;
    std::optional<PathDataLine> singleDataLine() const;

    bool isEmpty() const;
    bool definitelySingleLine() const;
    WEBCORE_EXPORT PlatformPathPtr platformPath() const;
#if USE(CG)
    WEBCORE_EXPORT RetainPtr<CGPathRef> protectedPlatformPath() const;
#endif

    const PathSegment* singleSegmentIfExists() const { return asSingle(); }
    WEBCORE_EXPORT const Vector<PathSegment>* segmentsIfExists() const;
    WEBCORE_EXPORT Vector<PathSegment> segments() const;

    float length() const;
    bool isClosed() const;
    bool hasSubpaths() const;
    FloatPoint currentPoint() const;
    PathTraversalState traversalStateAtLength(float length) const;
    FloatPoint pointAtLength(float length) const;

    WEBCORE_EXPORT bool contains(const FloatPoint&, WindRule = WindRule::NonZero) const;
    WEBCORE_EXPORT bool strokeContains(const FloatPoint&, NOESCAPE const Function<void(GraphicsContext&)>& strokeStyleApplier) const;

    WEBCORE_EXPORT FloatRect fastBoundingRect() const;
    WEBCORE_EXPORT FloatRect boundingRect() const;
    FloatRect strokeBoundingRect(NOESCAPE const Function<void(GraphicsContext&)>& strokeStyleApplier = { }) const;

    WEBCORE_EXPORT void ensureImplForTesting();
    WEBCORE_EXPORT const PathImpl* asImpl() const;

    void setNotTransient();

private:
    PlatformPathImpl& ensurePlatformPathImpl();
    Ref<PlatformPathImpl> ensureProtectedPlatformPathImpl();
    PathImpl& setImpl(Ref<PathImpl>&&);
    WEBCORE_EXPORT PathImpl& ensureImpl();
    WEBCORE_EXPORT Ref<PathImpl> ensureProtectedImpl();

    PathSegment* asSingle() { return std::get_if<PathSegment>(&m_data); }
    const PathSegment* asSingle() const { return std::get_if<PathSegment>(&m_data); }

    PathImpl* asImpl();
    RefPtr<PathImpl> asProtectedImpl();
    RefPtr<const PathImpl> asProtectedImpl() const;

    std::optional<FloatPoint> initialMoveToPoint() const;

    Variant<std::monostate, PathSegment, DataRef<PathImpl>> m_data;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Path&);

inline Path::Path(PathSegment&& segment)
    : m_data(WTF::move(segment))
{
}

inline bool Path::isEmpty() const
{
    return std::holds_alternative<std::monostate>(m_data);
}

inline void Path::moveTo(const FloatPoint& point)
{
    if (isEmpty())
        m_data = PathSegment(PathMoveTo { point });
    else
        ensureProtectedImpl()->add(PathMoveTo { point });
}

inline void Path::addLineTo(const FloatPoint& point)
{
    if (auto initial = initialMoveToPoint())
        m_data = PathSegment(PathDataLine { *initial, point });
    else
        ensureProtectedImpl()->add(PathLineTo { point });
}

inline void Path::addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint)
{
    if (auto initial = initialMoveToPoint())
        m_data = PathSegment(PathDataQuadCurve { *initial, controlPoint, endPoint });
    else
        ensureProtectedImpl()->add(PathQuadCurveTo { controlPoint, endPoint });
}

inline void Path::addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint)
{
    if (auto initial = initialMoveToPoint())
        m_data = PathSegment(PathDataBezierCurve { *initial, controlPoint1, controlPoint2, endPoint });
    else
        ensureProtectedImpl()->add(PathBezierCurveTo { controlPoint1, controlPoint2, endPoint });
}

inline void Path::addArcTo(const FloatPoint& point1, const FloatPoint& point2, float radius)
{
    if (auto initial = initialMoveToPoint())
        m_data = PathSegment(PathDataArc { *initial, point1, point2, radius });
    else
        ensureProtectedImpl()->add(PathArcTo { point1, point2, radius });
}

inline void Path::addArc(const FloatPoint& point, float radius, float startAngle, float endAngle, RotationDirection direction)
{
    // Workaround for <rdar://problem/5189233> CGPathAddArc hangs or crashes when passed inf as start or end angle,
    // as well as http://bugs.webkit.org/show_bug.cgi?id=16449, since cairo_arc() functions hang or crash when
    // passed inf as radius or start/end angle.
    if (!std::isfinite(radius) || !std::isfinite(startAngle) || !std::isfinite(endAngle))
        return;

    if (isEmpty())
        m_data = PathSegment(PathArc { point, radius, startAngle, endAngle, direction });
    else
        ensureProtectedImpl()->add(PathArc { point, radius, startAngle, endAngle, direction });
}

inline void Path::addEllipse(const FloatPoint& point, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, RotationDirection direction)
{
    if (isEmpty())
        m_data = PathSegment(PathEllipse { point, radiusX, radiusY, rotation, startAngle, endAngle, direction });
    else
        ensureProtectedImpl()->add(PathEllipse { point, radiusX, radiusY, rotation, startAngle, endAngle, direction });
}

inline void Path::addEllipseInRect(const FloatRect& rect)
{
    if (isEmpty())
        m_data = PathSegment(PathEllipseInRect { rect });
    else
        ensureProtectedImpl()->add(PathEllipseInRect { rect });
}

inline void Path::addRect(const FloatRect& rect)
{
    if (isEmpty())
        m_data = PathSegment(PathRect { rect });
    else
        ensureProtectedImpl()->add(PathRect { rect });
}

inline void Path::closeSubpath()
{
    if (std::holds_alternative<std::monostate>(m_data))
        return;
    if (const auto* segment = std::get_if<PathSegment>(&m_data)) {
        const auto* data = &segment->data();
        if (auto* arc = std::get_if<PathArc>(data)) {
            m_data = PathSegment(PathClosedArc { *arc });
            return;
        }
        if (std::holds_alternative<PathCloseSubpath>(*data))
            return;
    }
    auto impl = ensureProtectedImpl();
    if (impl->isClosed())
        return;
    impl->add(PathCloseSubpath { });
}

inline FloatPoint Path::currentPoint() const
{
    if (std::holds_alternative<std::monostate>(m_data))
        return { 0, 0 };
    if (const auto* segment = std::get_if<PathSegment>(&m_data)) {
        FloatPoint lastMoveToPoint;
        return segment->calculateEndPoint({ }, lastMoveToPoint);
    }
    return asProtectedImpl()->currentPoint();
}

inline std::optional<FloatPoint> Path::initialMoveToPoint() const
{
    if (std::holds_alternative<std::monostate>(m_data))
        return FloatPoint { 0, 0 };
    if (const auto* segment = std::get_if<PathSegment>(&m_data)) {
        if (const auto* moveTo = std::get_if<PathMoveTo>(&segment->data()))
            return moveTo->point;
    }
    return std::nullopt;
}

} // namespace WebCore
