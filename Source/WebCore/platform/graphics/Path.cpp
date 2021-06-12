/*
 * Copyright (C) 2003, 2006 Apple Inc.  All rights reserved.
 *                     2006 Rob Buis <buis@kde.org>
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

#include "FloatPoint.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "PathTraversalState.h"
#include "RoundedRect.h"
#include <math.h>
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

#if !USE(DIRECT2D)
float Path::length() const
{
    PathTraversalState traversalState(PathTraversalState::Action::TotalLength);

    apply([&traversalState](const PathElement& element) {
        traversalState.processPathElement(element);
    });

    return traversalState.totalLength();
}
#endif

#if !HAVE(CGPATH_GET_NUMBER_OF_ELEMENTS)

size_t Path::elementCountSlowCase() const
{
    size_t numPoints = 0;
    apply([&numPoints](auto&) {
        ++numPoints;
    });
    return numPoints;
}

#endif // !HAVE(CGPATH_GET_NUMBER_OF_ELEMENTS)

PathTraversalState Path::traversalStateAtLength(float length) const
{
    PathTraversalState traversalState(PathTraversalState::Action::VectorAtLength, length);

    apply([&traversalState](const PathElement& element) {
        traversalState.processPathElement(element);
    });

    return traversalState;
}

FloatPoint Path::pointAtLength(float length) const
{
    return traversalStateAtLength(length).current();
}

void Path::addRoundedRect(const FloatRect& rect, const FloatSize& roundingRadii, RoundedRectStrategy strategy)
{
    if (rect.isEmpty())
        return;

    FloatSize radius(roundingRadii);
    FloatSize halfSize = rect.size() / 2;

    // Apply the SVG corner radius constraints, per the rect section of the SVG shapes spec: if
    // one of rx,ry is negative, then the other corner radius value is used. If both values are
    // negative then rx = ry = 0. If rx is greater than half of the width of the rectangle
    // then set rx to half of the width; ry is handled similarly.

    if (radius.width() < 0)
        radius.setWidth((radius.height() < 0) ? 0 : radius.height());

    if (radius.height() < 0)
        radius.setHeight(radius.width());

    if (radius.width() > halfSize.width())
        radius.setWidth(halfSize.width());

    if (radius.height() > halfSize.height())
        radius.setHeight(halfSize.height());

    addRoundedRect(FloatRoundedRect(rect, radius, radius, radius, radius), strategy);
}

void Path::addRoundedRect(const FloatRoundedRect& r, RoundedRectStrategy strategy)
{
    if (r.isEmpty())
        return;

    const FloatRoundedRect::Radii& radii = r.radii();
    const FloatRect& rect = r.rect();

    if (!r.isRenderable()) {
        // If all the radii cannot be accommodated, return a rect.
        addRect(rect);
        return;
    }

    if (strategy == RoundedRectStrategy::PreferNative) {
#if USE(CG) || USE(DIRECT2D)
        platformAddPathForRoundedRect(rect, radii.topLeft(), radii.topRight(), radii.bottomLeft(), radii.bottomRight());
        return;
#endif
    }

    addBeziersForRoundedRect(rect, radii.topLeft(), radii.topRight(), radii.bottomLeft(), radii.bottomRight());
}

void Path::addRoundedRect(const RoundedRect& r)
{
    addRoundedRect(FloatRoundedRect(r));
}

void Path::addBeziersForRoundedRect(const FloatRect& rect, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius)
{
    moveTo(FloatPoint(rect.x() + topLeftRadius.width(), rect.y()));

    addLineTo(FloatPoint(rect.maxX() - topRightRadius.width(), rect.y()));
    if (topRightRadius.width() > 0 || topRightRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.maxX() - topRightRadius.width() * circleControlPoint(), rect.y()),
            FloatPoint(rect.maxX(), rect.y() + topRightRadius.height() * circleControlPoint()),
            FloatPoint(rect.maxX(), rect.y() + topRightRadius.height()));
    addLineTo(FloatPoint(rect.maxX(), rect.maxY() - bottomRightRadius.height()));
    if (bottomRightRadius.width() > 0 || bottomRightRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.maxX(), rect.maxY() - bottomRightRadius.height() * circleControlPoint()),
            FloatPoint(rect.maxX() - bottomRightRadius.width() * circleControlPoint(), rect.maxY()),
            FloatPoint(rect.maxX() - bottomRightRadius.width(), rect.maxY()));
    addLineTo(FloatPoint(rect.x() + bottomLeftRadius.width(), rect.maxY()));
    if (bottomLeftRadius.width() > 0 || bottomLeftRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.x() + bottomLeftRadius.width() * circleControlPoint(), rect.maxY()),
            FloatPoint(rect.x(), rect.maxY() - bottomLeftRadius.height() * circleControlPoint()),
            FloatPoint(rect.x(), rect.maxY() - bottomLeftRadius.height()));
    addLineTo(FloatPoint(rect.x(), rect.y() + topLeftRadius.height()));
    if (topLeftRadius.width() > 0 || topLeftRadius.height() > 0)
        addBezierCurveTo(FloatPoint(rect.x(), rect.y() + topLeftRadius.height() * circleControlPoint()),
            FloatPoint(rect.x() + topLeftRadius.width() * circleControlPoint(), rect.y()),
            FloatPoint(rect.x() + topLeftRadius.width(), rect.y()));

    closeSubpath();
}

void Path::apply(const PathApplierFunction& function) const
{
    if (isNull())
        return;

#if ENABLE(INLINE_PATH_DATA)
    if (hasInlineData<MoveData>()) {
        PathElement element;
        element.type = PathElement::Type::MoveToPoint;
        element.points[0] = WTF::get<MoveData>(m_inlineData).location;
        function(element);
        return;
    }

    if (hasInlineData<LineData>()) {
        auto& line = WTF::get<LineData>(m_inlineData);
        PathElement element;
        element.type = PathElement::Type::MoveToPoint;
        element.points[0] = line.start;
        function(element);
        element.type = PathElement::Type::AddLineToPoint;
        element.points[0] = line.end;
        function(element);
        return;
    }

    if (hasInlineData<BezierCurveData>()) {
        auto& curve = WTF::get<BezierCurveData>(m_inlineData);
        PathElement element;
        element.type = PathElement::Type::MoveToPoint;
        element.points[0] = curve.startPoint;
        function(element);
        element.type = PathElement::Type::AddCurveToPoint;
        element.points[0] = curve.controlPoint1;
        element.points[1] = curve.controlPoint2;
        element.points[2] = curve.endPoint;
        function(element);
        return;
    }

    if (hasInlineData<QuadCurveData>()) {
        auto& curve = WTF::get<QuadCurveData>(m_inlineData);
        PathElement element;
        element.type = PathElement::Type::MoveToPoint;
        element.points[0] = curve.startPoint;
        function(element);
        element.type = PathElement::Type::AddQuadCurveToPoint;
        element.points[0] = curve.controlPoint;
        element.points[1] = curve.endPoint;
        function(element);
        return;
    }
#endif

    applySlowCase(function);
}

bool Path::isEmpty() const
{
    if (isNull())
        return true;

#if ENABLE(INLINE_PATH_DATA)
    if (hasInlineData())
        return false;
#endif

    return isEmptySlowCase();
}

bool Path::hasCurrentPoint() const
{
    return !isEmpty();
}

FloatPoint Path::currentPoint() const
{
    if (isNull())
        return { };

#if ENABLE(INLINE_PATH_DATA)
    if (hasInlineData<MoveData>())
        return inlineData<MoveData>().location;

    if (hasInlineData<LineData>())
        return inlineData<LineData>().end;

    if (hasInlineData<BezierCurveData>())
        return inlineData<BezierCurveData>().endPoint;

    if (hasInlineData<QuadCurveData>())
        return inlineData<QuadCurveData>().endPoint;

    if (hasInlineData<ArcData>()) {
        auto& arc = inlineData<ArcData>();
        if (arc.type == ArcData::Type::ClosedLineAndArc)
            return arc.start;

        return {
            arc.center.x() + arc.radius * std::acos(arc.endAngle),
            arc.center.y() + arc.radius * std::asin(arc.endAngle)
        };
    }
#endif

    return currentPointSlowCase();
}

size_t Path::elementCount() const
{
#if ENABLE(INLINE_PATH_DATA)
    if (hasInlineData<MoveData>())
        return 1;

    if (hasInlineData<LineData>() || hasInlineData<BezierCurveData>() || hasInlineData<QuadCurveData>())
        return 2;
#endif

    return elementCountSlowCase();
}

void Path::addArc(const FloatPoint& point, float radius, float startAngle, float endAngle, bool anticlockwise)
{
    // Workaround for <rdar://problem/5189233> CGPathAddArc hangs or crashes when passed inf as start or end angle,
    // as well as http://bugs.webkit.org/show_bug.cgi?id=16449, since cairo_arc() functions hang or crash when
    // passed inf as radius or start/end angle.
    if (!std::isfinite(radius) || !std::isfinite(startAngle) || !std::isfinite(endAngle))
        return;

#if ENABLE(INLINE_PATH_DATA)
    bool hasMoveData = hasInlineData<MoveData>();
    if (isNull() || hasMoveData) {
        ArcData arc;
        if (hasMoveData) {
            arc.type = ArcData::Type::LineAndArc;
            arc.start = inlineData<MoveData>().location;
        }
        arc.center = point;
        arc.radius = radius;
        arc.startAngle = startAngle;
        arc.endAngle = endAngle;
        // FIXME: Either ArcData::clockwise needs to be renamed to anticlockwise, or the last argument to
        // Path::addArc needs to be renamed to clockwise.
        arc.clockwise = anticlockwise;
        m_inlineData = { WTFMove(arc) };
        return;
    }
#endif

    addArcSlowCase(point, radius, startAngle, endAngle, anticlockwise);
}

void Path::addLineTo(const FloatPoint& point)
{
#if ENABLE(INLINE_PATH_DATA)
    bool hasMoveData = hasInlineData<MoveData>();
    if (isNull() || hasMoveData) {
        LineData line;
        line.start = hasMoveData ? inlineData<MoveData>().location : FloatPoint();
        line.end = point;
        m_inlineData = { WTFMove(line) };
        return;
    }

    if (hasInlineData<ArcData>()) {
        auto& arc = inlineData<ArcData>();
        if (arc.type == ArcData::Type::LineAndArc && arc.start == point) {
            arc.type = ArcData::Type::ClosedLineAndArc;
            return;
        }
    }
#endif

    addLineToSlowCase(point);
}

void Path::addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint)
{
#if ENABLE(INLINE_PATH_DATA)
    if (isNull() || hasInlineData<MoveData>()) {
        QuadCurveData curve;
        curve.startPoint = hasInlineData() ? WTF::get<MoveData>(m_inlineData).location : FloatPoint();
        curve.controlPoint = controlPoint;
        curve.endPoint = endPoint;
        m_inlineData = { WTFMove(curve) };
        return;
    }
#endif

    addQuadCurveToSlowCase(controlPoint, endPoint);
}

void Path::addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint)
{
#if ENABLE(INLINE_PATH_DATA)
    if (isNull() || hasInlineData<MoveData>()) {
        BezierCurveData curve;
        curve.startPoint = hasInlineData() ? WTF::get<MoveData>(m_inlineData).location : FloatPoint();
        curve.controlPoint1 = controlPoint1;
        curve.controlPoint2 = controlPoint2;
        curve.endPoint = endPoint;
        m_inlineData = { WTFMove(curve) };
        return;
    }
#endif

    addBezierCurveToSlowCase(controlPoint1, controlPoint2, endPoint);
}

void Path::moveTo(const FloatPoint& point)
{
#if ENABLE(INLINE_PATH_DATA)
    if (isNull() || hasInlineData<MoveData>()) {
        m_inlineData = MoveData { point };
        return;
    }
#endif

    moveToSlowCase(point);
}

FloatRect Path::boundingRect() const
{
    if (isNull())
        return { };

#if ENABLE(INLINE_PATH_DATA)
    if (auto rect = boundingRectFromInlineData())
        return *rect;
#endif

    return boundingRectSlowCase();
}

FloatRect Path::fastBoundingRect() const
{
    if (isNull())
        return { };

#if ENABLE(INLINE_PATH_DATA)
    if (auto rect = fastBoundingRectFromInlineData())
        return *rect;
#endif

    return fastBoundingRectSlowCase();
}

#if ENABLE(INLINE_PATH_DATA)

std::optional<FloatRect> Path::fastBoundingRectFromInlineData() const
{
    if (hasInlineData<ArcData>()) {
        auto& arc = inlineData<ArcData>();
        auto diameter = 2 * arc.radius;
        FloatRect approximateBounds { arc.center, FloatSize(diameter, diameter) };
        approximateBounds.move(-arc.radius, -arc.radius);
        if (arc.type == ArcData::Type::LineAndArc || arc.type == ArcData::Type::ClosedLineAndArc)
            approximateBounds.extend(arc.start);
        return approximateBounds;
    }

    return boundingRectFromInlineData();
}

static FloatRect computeArcBounds(const FloatPoint& center, float radius, float start, float end, bool clockwise)
{
    if (clockwise)
        std::swap(start, end);

    constexpr float fullCircle = 2 * piFloat;
    if (end - start >= fullCircle) {
        auto diameter = radius * 2;
        return { center.x() - radius, center.y() - radius, diameter, diameter };
    }

    auto normalize = [&] (float radians) {
        double circles = radians / fullCircle;
        return fullCircle * (circles - floor(circles));
    };

    start = normalize(start);
    end = normalize(end);

    auto lengthInRadians = end - start;
    if (start > end)
        lengthInRadians += fullCircle;

    FloatPoint startPoint { center.x() + radius * cos(start), center.y() + radius * sin(start) };
    FloatPoint endPoint { center.x() + radius * cos(end), center.y() + radius * sin(end) };
    FloatRect result;
    result.fitToPoints(startPoint, endPoint);

    auto contains = [&] (float angleToCheck) {
        return (start < angleToCheck && start + lengthInRadians > angleToCheck)
            || (start > angleToCheck && start + lengthInRadians > angleToCheck + fullCircle);
    };

    if (contains(0))
        result.shiftMaxXEdgeTo(center.x() + radius);

    if (contains(piOverTwoFloat))
        result.shiftMaxYEdgeTo(center.y() + radius);

    if (contains(piFloat))
        result.shiftXEdgeTo(center.x() - radius);

    if (contains(3 * piOverTwoFloat))
        result.shiftYEdgeTo(center.y() - radius);

    return result;
}

std::optional<FloatRect> Path::boundingRectFromInlineData() const
{
    if (hasInlineData<ArcData>()) {
        auto& arc = inlineData<ArcData>();
        auto bounds = computeArcBounds(arc.center, arc.radius, arc.startAngle, arc.endAngle, arc.clockwise);
        if (arc.type == ArcData::Type::LineAndArc || arc.type == ArcData::Type::ClosedLineAndArc)
            bounds.extend(arc.start);
        return bounds;
    }

    if (hasInlineData<MoveData>())
        return {{ inlineData<MoveData>().location, FloatSize { } }};

    if (hasInlineData<LineData>()) {
        FloatRect result;
        auto& line = inlineData<LineData>();
        result.fitToPoints(line.start, line.end);
        return result;
    }

    return std::nullopt;
}

#endif

#if !USE(CG) && !USE(DIRECT2D)
Path Path::polygonPathFromPoints(const Vector<FloatPoint>& points)
{
    Path path;
    if (points.size() < 2)
        return path;

    path.moveTo(points[0]);
    for (size_t i = 1; i < points.size(); ++i)
        path.addLineTo(points[i]);

    path.closeSubpath();
    return path;
}
#endif

#ifndef NDEBUG
void Path::dump() const
{
    TextStream stream;
    stream << *this;
    WTFLogAlways("%s", stream.release().utf8().data());
}
#endif

TextStream& operator<<(TextStream& stream, const Path& path)
{
    bool isFirst = true;
    path.apply([&stream, &isFirst](const PathElement& element) {
        if (!isFirst)
            stream << ", ";
        isFirst = false;
        switch (element.type) {
        case PathElement::Type::MoveToPoint: // The points member will contain 1 value.
            stream << "move to " << element.points[0];
            break;
        case PathElement::Type::AddLineToPoint: // The points member will contain 1 value.
            stream << "add line to " << element.points[0];
            break;
        case PathElement::Type::AddQuadCurveToPoint: // The points member will contain 2 values.
            stream << "add quad curve to " << element.points[0] << " " << element.points[1];
            break;
        case PathElement::Type::AddCurveToPoint: // The points member will contain 3 values.
            stream << "add curve to " << element.points[0] << " " << element.points[1] << " " << element.points[2];
            break;
        case PathElement::Type::CloseSubpath: // The points member will contain no values.
            stream << "close subpath";
            break;
        }
    });
    
    return stream;
}

}
