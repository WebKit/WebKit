/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CornerShapeUtilities.h"

#include "FloatPoint.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include <WebCore/BezierUtilities.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>

namespace WebCore {

namespace {

// Lengths below this are treated as zero (degenerate edge/radius)
static constexpr float limit = 1e-6f;

struct Corner {
    FloatPoint start;
    FloatPoint outer;
    FloatPoint end;
    FloatPoint center;
    FloatSize radii { };
    double curvature { 1.0 };
    BoxCorner orientation;
};

static FloatSize cornerRadii(const FloatPoint& center, const FloatPoint& outer)
{
    return { std::abs(center.x() - outer.x()), std::abs(center.y() - outer.y()) };
}

static std::pair<float, float> inwardDirection(BoxCorner orientation)
{
    float directionX = (orientation == BoxCorner::TopLeft || orientation == BoxCorner::BottomLeft) ? 1.0f : -1.0f; // left corners point right toward center
    float directionY = (orientation == BoxCorner::TopLeft || orientation == BoxCorner::TopRight) ? 1.0f : -1.0f; // top corners point down toward center
    return { directionX, directionY };
}

static bool startsOnVerticalEdge(BoxCorner orientation)
{
    return orientation == BoxCorner::TopLeft || orientation == BoxCorner::BottomRight;
}

static std::pair<float, float> edgeBorders(BoxCorner orientation, double startInset, double endInset)
{
    bool startOnVerticalEdge = startsOnVerticalEdge(orientation);
    float verticalEdgeBorder = float(startOnVerticalEdge ? startInset : endInset);
    float horizontalEdgeBorder = float(startOnVerticalEdge ? endInset : startInset);
    return { verticalEdgeBorder, horizontalEdgeBorder };
}

static bool isConcave(const Corner& corner) { return corner.curvature < 0.0; }
static bool isRound(const Corner& corner) { return corner.curvature == 1.0; }
static bool isBevel(const Corner& corner) { return corner.curvature == 0.0; }
static bool isNotch(const Corner& corner) { return std::isinf(corner.curvature) && corner.curvature < 0.0; }
static bool isSquare(const Corner& corner) { return std::isinf(corner.curvature) && corner.curvature > 0.0; }
// A shallow concave corner (scoop and anything between scoop and bevel) is drawn from its endpoint tangents
// rather than from the superellipse exponent.
static bool isShallowConcave(const Corner& corner) { return corner.curvature < 0.0 && corner.curvature >= -1.0; }
static bool isEmpty(const Corner& corner)
{
    return (corner.outer - corner.start).diagonalLength() < limit
        || (corner.end - corner.outer).diagonalLength() < limit;
}

static Corner inverseCorner(const Corner& corner)
{
    return { corner.start, corner.center, corner.end, corner.outer, corner.radii, -corner.curvature, corner.orientation };
}

static Corner makeCorner(const CornerInput& input)
{
    std::array<FloatPoint, 4> vertices = { {
        { float(input.x), float(input.y) },
        { float(input.x + input.width), float(input.y) },
        { float(input.x + input.width), float(input.y + input.height) },
        { float(input.x), float(input.y + input.height) },
    } };

    int outer;
    switch (input.orientation) {
    case BoxCorner::TopLeft:     outer = 0; break;
    case BoxCorner::TopRight:    outer = 1; break;
    case BoxCorner::BottomLeft:  outer = 3; break;
    case BoxCorner::BottomRight: outer = 2; break;
    }
    return {
        vertices[(outer + 3) % 4], // start
        vertices[outer], // outer
        vertices[(outer + 1) % 4], // end
        vertices[(outer + 2) % 4], // center
        cornerRadii(vertices[(outer + 2) % 4], vertices[outer]),
        input.curvature,
        input.orientation,
    };
}

struct ResolvedCorner {
    Corner adjusted;
    FloatPoint miterStart;
    FloatPoint miterEnd;
    std::optional<FloatPoint> tangentVertex;
    std::optional<FloatPoint> collapsePoint;
};

static std::optional<FloatPoint> findMiterArmCrossing(const FloatPoint& startArmOuter, const FloatPoint& startArmInner, const FloatPoint& endArmOuter, const FloatPoint& endArmInner)
{
    if (auto crossing = findSegmentLineIntersection(startArmOuter, startArmInner, endArmOuter, endArmInner))
        return crossing;
    return findSegmentLineIntersection(endArmOuter, endArmInner, startArmOuter, startArmInner);
}

// Defined in https://drafts.csswg.org/css-borders-4/#corner-shape-interpolation
static double normalizedSuperellipseHalfCorner(double superellipseParameter)
{
    if (std::isinf(superellipseParameter))
        return superellipseParameter < 0.0 ? 0.0 : 1.0;

    double magnitude = std::abs(superellipseParameter);

    static thread_local double lastMagnitude = std::numeric_limits<double>::quiet_NaN();
    static thread_local double lastConvexHalfCorner = 0.0;
    if (magnitude != lastMagnitude) {
        double exponent = std::pow(0.5, magnitude);
        lastConvexHalfCorner = std::pow(0.5, exponent);
        lastMagnitude = magnitude;
    }

    return superellipseParameter < 0.0 ? 1.0 - lastConvexHalfCorner : lastConvexHalfCorner;
}

// Moves the corner's curve endpoints to where the offset contour needs them, and works out where the
// contour leaves the corner for the target rect's edges.
static ResolvedCorner resolveCorner(const Corner& original, double startInset, double endInset)
{
    auto degenerateAt = [&](FloatPoint point) -> ResolvedCorner {
        return { { point, point, point, point, { }, original.curvature, original.orientation }, point, point, std::nullopt, std::nullopt };
    };

    auto vectorTowardStart = original.start - original.outer;
    auto vectorTowardEnd = original.end - original.outer;
    // The curve's start point sits `endRadius` along one edge of the corner box and its end point `startRadius` along the other.
    double endRadius = vectorTowardStart.diagonalLength();
    double startRadius = vectorTowardEnd.diagonalLength();

    if (isEmpty(original)) {
        auto [directionX, directionY] = inwardDirection(original.orientation);
        auto [verticalEdgeBorder, horizontalEdgeBorder] = edgeBorders(original.orientation, startInset, endInset);
        return degenerateAt(original.outer + FloatSize(directionX * verticalEdgeBorder, directionY * horizontalEdgeBorder));
    }

    auto unitVectorTowardStart = vectorTowardStart.normalized();
    auto unitVectorTowardEnd = vectorTowardEnd.normalized();
    auto targetCornerOuter = original.outer + unitVectorTowardStart.scaled(float(endInset)) + unitVectorTowardEnd.scaled(float(startInset));

    // Where the endpoint offset directions sit between the corner's two edges: 0 runs along the edge the
    // endpoint lies on, 1 runs perpendicular to it, and 0.5 is the diagonal between the two.
    double halfCornerX = normalizedSuperellipseHalfCorner(original.curvature);
    double controlPointX = std::clamp((1.0 / (std::numbers::sqrt2 - 1.0)) * halfCornerX - 1.0 / std::numbers::sqrt2, 0.0, 1.0);

    double insetDifference = endInset - startInset;
    if (original.curvature <= 0.0 && (insetDifference <= -startRadius || insetDifference >= endRadius))
        return degenerateAt(targetCornerOuter);

    double startControlPointX = controlPointX;
    double endControlPointX = controlPointX;
    if (insetDifference) {
        // Bevel corner as the common tangent to two circles centred on corner-start and corner-end with radii the signed insets
        // negative sign: outward, away from box, positive sign: inward, towards box center
        double root = std::sqrt(std::max(0.0, startRadius * startRadius + endRadius * endRadius - insetDifference * insetDifference));
        double bevelNormalVectorX = endRadius * insetDifference + startRadius * root;
        double bevelNormalVectorY = -startRadius * insetDifference + endRadius * root;
        double bevelDenominator = startRadius * bevelNormalVectorY + endRadius * bevelNormalVectorX;
        double bevelControlPointX = bevelDenominator != 0.0 ? (startRadius * bevelNormalVectorY) / bevelDenominator : 0.5;

        // The two endpoints get separate directions, placed symmetrically either side of controlPointX. How far
        // apart they land comes from the bevel tangent, which sits at the midpoint unless the two insets differ,
        // so equal insets give both endpoints the same direction.
        startControlPointX = original.curvature < 0.0
            ? bevelControlPointX * (2.0 * controlPointX)
            : 1.0 - (1.0 - bevelControlPointX) * (2.0 * (1.0 - controlPointX));
        endControlPointX = 2.0 * controlPointX - startControlPointX;
    }

    // Per-axis normal vectors: the corner radii weight each component, so elliptical corners offset correctly.
    auto unmappedStartNormal = FloatSize(float((1.0 - startControlPointX) * startRadius), float(startControlPointX * endRadius)).normalized();
    auto unmappedEndNormal = FloatSize(float(endControlPointX * startRadius), float((1.0 - endControlPointX) * endRadius)).normalized();

    auto startNormal = unitVectorTowardStart.scaled(unmappedStartNormal.width()) + unitVectorTowardEnd.scaled(unmappedStartNormal.height());
    auto endNormal = unitVectorTowardStart.scaled(unmappedEndNormal.width()) + unitVectorTowardEnd.scaled(unmappedEndNormal.height());
    auto startTangent = startNormal.perpendicular().scaled(-1);
    auto endTangent = endNormal.perpendicular();

    auto adjustedStart = original.start + startNormal.scaled(float(startInset));
    auto adjustedEnd = original.end + endNormal.scaled(float(endInset));

    auto miterStart = adjustedStart;
    auto miterEnd = adjustedEnd;

    if (startInset < 0.0) {
        miterStart = findLineIntersection(adjustedStart, startTangent, targetCornerOuter, unitVectorTowardStart).value_or(adjustedStart);
        if (original.curvature >= 0.0)
            adjustedStart = miterStart;
    }
    if (endInset < 0.0) {
        miterEnd = findLineIntersection(adjustedEnd, endTangent, targetCornerOuter, unitVectorTowardEnd).value_or(adjustedEnd);
        if (original.curvature >= 0.0)
            adjustedEnd = miterEnd;
    }

    float adjustedWidth = dotProduct(adjustedStart - adjustedEnd, unitVectorTowardStart);
    auto adjustedOuter = adjustedStart - unitVectorTowardStart.scaled(adjustedWidth);
    auto adjustedCenter = adjustedEnd + unitVectorTowardStart.scaled(adjustedWidth);

    Corner adjusted {
        adjustedStart,
        adjustedOuter,
        adjustedEnd,
        adjustedCenter,
        cornerRadii(adjustedCenter, adjustedOuter),
        original.curvature,
        original.orientation,
    };

    ResolvedCorner resolved;
    resolved.adjusted = adjusted;
    resolved.miterStart = miterStart;
    resolved.miterEnd = miterEnd;

    if (isShallowConcave(adjusted))
        resolved.tangentVertex = findLineIntersection(adjustedStart, startTangent, adjustedEnd, endTangent);

    if (isConcave(original) && startInset < 0.0 && endInset < 0.0 && (-endInset >= startRadius || -startInset >= endRadius))
        resolved.collapsePoint = findMiterArmCrossing(miterStart, adjustedStart, miterEnd, adjustedEnd);

    return resolved;
}

static bool extendPathForSharpCorner(Path& path, const Corner& corner)
{
    if (corner.radii.width() >= limit && corner.radii.height() >= limit)
        return false;
    path.addLineTo(corner.outer);
    path.addLineTo(corner.end);
    return true;
}

static void addEllipticalArc(Path& path, const Corner& corner)
{
    if (extendPathForSharpCorner(path, corner))
        return;

    float radiusX = corner.radii.width();
    float radiusY = corner.radii.height();

    float startAngle = eccentricAngle(corner.start, corner.center, radiusX, radiusY);
    float endAngle = eccentricAngle(corner.end, corner.center, radiusX, radiusY);

    float delta = std::remainder(endAngle - startAngle, 2.0f * std::numbers::pi_v<float>);
    auto direction = delta >= 0.0f ? RotationDirection::Clockwise : RotationDirection::Counterclockwise;

    path.addEllipse(corner.center, radiusX, radiusY, 0.0f, startAngle, startAngle + delta, direction);
}

struct SuperellipseBezierHandles {
    double handleA;
    double handleB;
    double halfCorner;
};
static SuperellipseBezierHandles superellipseBezierHandles(double parameter)
{
    static thread_local double lastParameter = std::numeric_limits<double>::quiet_NaN();
    static thread_local SuperellipseBezierHandles lastResult { };
    if (parameter == lastParameter)
        return lastResult;

    static constexpr std::array<double, 7> fitCoefficients {
        1.2430920942724248, 2.010479023614843, 0.32922901179443753,
        0.2823023142212073, 1.3473704261055421, 2.9149468637949814, 0.9106507102917086
    };
    double slope = fitCoefficients[0] + (fitCoefficients[6] - fitCoefficients[0]) * 0.5 * (1.0 + std::tanh(fitCoefficients[5] * (parameter - fitCoefficients[1])));
    double base = 1.0 / (1.0 + std::exp(slope * fitCoefficients[1]));
    double logistic = 1.0 / (1.0 + std::exp(slope * (fitCoefficients[1] - parameter)));
    double handleA = (logistic - base) / (1.0 - base);
    double handleB = fitCoefficients[2] * std::exp(-fitCoefficients[3] * std::pow(parameter, fitCoefficients[4]));
    double halfCorner = normalizedSuperellipseHalfCorner(parameter);

    lastParameter = parameter;
    lastResult = { handleA, handleB, halfCorner };
    return lastResult;
}

static FloatPoint mapPointToCorner(const Corner& corner, FloatSize normalizedPoint)
{
    auto curveCenter = corner.center;
    auto centerToEnd = corner.end - curveCenter;
    auto centerToStart = corner.start - curveCenter;
    return curveCenter + centerToEnd * normalizedPoint.width() + centerToStart * normalizedPoint.height();
}

static FloatSize transpose(FloatSize size) { return FloatSize(size.height(), size.width()); }

static std::array<BezierSegment, 2> superellipseCornerBeziers(const Corner& cornerInput)
{
    auto corner = isConcave(cornerInput) ? inverseCorner(cornerInput) : cornerInput;
    auto [handleA, handleB, halfCorner] = superellipseBezierHandles(corner.curvature);
    FloatSize firstControl { float(handleA), 1.0f };
    FloatSize secondControl { float(halfCorner - handleB), float(halfCorner + handleB) };
    FloatSize midpointNormalized { float(halfCorner), float(halfCorner) };
    auto midpoint = mapPointToCorner(corner, midpointNormalized);
    BezierSegment firstHalf { corner.start, mapPointToCorner(corner, firstControl), mapPointToCorner(corner, secondControl), midpoint };
    BezierSegment secondHalf { midpoint, mapPointToCorner(corner, transpose(secondControl)), mapPointToCorner(corner, transpose(firstControl)), corner.end };
    return { firstHalf, secondHalf };
}

static BezierSegment shallowConcaveCornerBezier(const Corner& corner, const FloatPoint& tangentVertex)
{
    constexpr float circleHandle = 0.5522847498307933f; // 4 / 3 * (sqrt(2) - 1): cubic approximation of a quarter circle
    Corner mappingFrame = corner;
    mappingFrame.center = tangentVertex;
    return {
        corner.start,
        mapPointToCorner(mappingFrame, { 0.0f, 1.0f - circleHandle }),
        mapPointToCorner(mappingFrame, { 1.0f - circleHandle, 0.0f }),
        corner.end,
    };
}

using CornerCurves = Vector<BezierSegment, 2>;

static BezierSegment straightBezierSegment(const FloatPoint& start, const FloatPoint& end)
{
    auto delta = end - start;
    return { start, start + delta * (1.0f / 3.0f), start + delta * (2.0f / 3.0f), end };
}

static CornerCurves cornerCurveBeziers(const ResolvedCorner& corner)
{
    if (isNotch(corner.adjusted) || isSquare(corner.adjusted)) {
        auto turningPoint = isNotch(corner.adjusted) ? corner.adjusted.center : corner.adjusted.outer;
        return { straightBezierSegment(corner.adjusted.start, turningPoint), straightBezierSegment(turningPoint, corner.adjusted.end) };
    }

    if (isShallowConcave(corner.adjusted) && corner.tangentVertex)
        return { shallowConcaveCornerBezier(corner.adjusted, *corner.tangentVertex) };

    CornerCurves curves;
    for (const auto& curve : superellipseCornerBeziers(corner.adjusted))
        curves.append(curve);
    return curves;
}

static void addCurvedCorner(Path& path, const ResolvedCorner& corner)
{
    const auto& adjusted = corner.adjusted;

    if (isNotch(adjusted)) {
        path.addLineTo(adjusted.center);
        path.addLineTo(adjusted.end);
        return;
    }

    if (isShallowConcave(adjusted) && corner.tangentVertex) {
        auto curve = shallowConcaveCornerBezier(adjusted, *corner.tangentVertex);
        path.addBezierCurveTo(curve.controlPoint1, curve.controlPoint2, curve.end);
        return;
    }

    if (isConcave(adjusted)) {
        auto inverted = corner;
        inverted.adjusted = inverseCorner(adjusted);
        addCurvedCorner(path, inverted);
        return;
    }

    if (isSquare(adjusted)) {
        path.addLineTo(adjusted.outer);
        path.addLineTo(adjusted.end);
        return;
    }
    if (isBevel(adjusted)) {
        path.addLineTo(adjusted.end);
        return;
    }
    if (isRound(adjusted)) {
        addEllipticalArc(path, adjusted);
        return;
    }

    // General superellipse: squircle (s=2) and superellipse(n)
    if (extendPathForSharpCorner(path, adjusted))
        return;

    for (const auto& curve : cornerCurveBeziers(corner))
        path.addBezierCurveTo(curve.controlPoint1, curve.controlPoint2, curve.end);
}

static FloatPoint sharpInnerCornerPoint(const FloatRect& innerRect, BoxCorner orientation)
{
    switch (orientation) {
    case BoxCorner::TopRight:    return innerRect.maxXMinYCorner();
    case BoxCorner::BottomRight: return innerRect.maxXMaxYCorner();
    case BoxCorner::BottomLeft:  return innerRect.minXMaxYCorner();
    case BoxCorner::TopLeft:     return innerRect.minXMinYCorner();
    }
    return { };
}

static Vector<BezierSegment> curvesTrimmedToRect(const CornerCurves& curves, const FloatRect& innerRect)
{
    Vector<BezierSegment> clippedCurves;
    for (const auto& bezier : curves) {
        for (const auto& curve : trimBezierToRect(bezier, innerRect))
            clippedCurves.append(curve);
    }
    return clippedCurves;
}

struct PreparedCorner {
    ResolvedCorner resolved;
    bool insetToTargetRect { false };
    bool isElided { false };
    // The whole corner curve, describing the area this corner removes from the target rect.
    CornerCurves fullCurves;
    // The part of that curve the contour draws: trimmed to the target rect, then to the neighboring corners.
    Vector<BezierSegment> curves;
};

// A corner's curve, together with the two edges that meet at its vertex, encloses the area that the corner
// removes from the target rect, so a point lies inside that area when the segment joining it to the vertex
// crosses the curve an even number of times.
static bool cornerRemovesPoint(const PreparedCorner& corner, const FloatPoint& point)
{
    const auto& adjusted = corner.resolved.adjusted;

    if (isSquare(adjusted))
        return false;

    FloatRect cornerBox { adjusted.outer, FloatSize { } };
    cornerBox.extend(adjusted.center);
    if (!cornerBox.contains(point))
        return false;

    unsigned crossings = 0;
    for (const auto& curve : corner.fullCurves)
        crossings += numberOfCrossingsWithSegment(curve, point, adjusted.outer);
    return !(crossings % 2);
}

static bool contourEnclosesArea(const Vector<PreparedCorner, 4>& corners, const FloatRect& targetRect)
{
    bool anyCornerIsConcave = false;
    for (const auto& corner : corners) {
        if (!corner.insetToTargetRect)
            return true;
        anyCornerIsConcave |= isConcave(corner.resolved.adjusted);
    }

    if (!anyCornerIsConcave)
        return true;

    auto isRemovedByAnyCorner = [&](const FloatPoint& point, const PreparedCorner* pointLiesOn) {
        for (const auto& corner : corners) {
            if (&corner != pointLiesOn && cornerRemovesPoint(corner, point))
                return true;
        }
        return false;
    };

    for (const auto& corner : corners) {
        if (corner.isElided)
            continue;

        if (corner.curves.isEmpty()) {
            if (!isRemovedByAnyCorner(sharpInnerCornerPoint(targetRect, corner.resolved.adjusted.orientation), nullptr))
                return true;
            continue;
        }

        for (const auto& curve : corner.curves) {
            if (!isRemovedByAnyCorner(pointOnBezierAtParameter(curve, 0.5), &corner))
                return true;
        }
    }

    return false;
}

static void addInsetCornerCurves(Path& path, const Vector<BezierSegment>& curves, const FloatPoint& sharpCornerPoint, bool& started)
{
    auto lineOrMoveTo = [&](FloatPoint point) {
        if (!started) {
            path.moveTo(point);
            started = true;
        } else
            path.addLineTo(point);
    };

    if (curves.isEmpty()) {
        lineOrMoveTo(sharpCornerPoint);
        return;
    }

    for (const auto& curve : curves) {
        lineOrMoveTo(curve.start);
        path.addBezierCurveTo(curve.controlPoint1, curve.controlPoint2, curve.end);
    }
}

} // namespace

// https://drafts.csswg.org/css-borders-4/#contour-path
ContourResult borderContourPath(Path& path, const RectCorners<CornerInput>& cornerRects, const FloatRect* targetRect, ContourStart contourStart)
{
    bool started = false;

    // offset-path's <coord-box> wants the contour to begin on the top edge instead of at the first corner.
    if (contourStart == ContourStart::TopEdge) {
        path.moveTo(makeCorner(cornerRects[BoxCorner::TopLeft]).end);
        started = true;
    }

    auto lineOrMoveTo = [&](FloatPoint point) {
        if (!started) {
            path.moveTo(point);
            started = true;
        } else
            path.addLineTo(point);
    };

    static constexpr std::array<BoxCorner, 4> contourOrder { BoxCorner::TopRight, BoxCorner::BottomRight, BoxCorner::BottomLeft, BoxCorner::TopLeft };

    Vector<PreparedCorner, 4> corners;
    for (auto key : contourOrder) {
        const auto& input = cornerRects[key];

        auto resolved = resolveCorner(makeCorner(input), input.startInset, input.endInset);
        bool insetToTargetRect = targetRect && input.startInset >= 0.0 && input.endInset >= 0.0
            && !isEmpty(resolved.adjusted);

        CornerCurves fullCurves;
        Vector<BezierSegment> curves;
        if (insetToTargetRect) {
            fullCurves = cornerCurveBeziers(resolved);
            curves = curvesTrimmedToRect(fullCurves, *targetRect);
        }

        corners.append(PreparedCorner { WTF::move(resolved), insetToTargetRect, false, WTF::move(fullCurves), WTF::move(curves) });
    }

    // Concave corners can intersect; trim them at the cusp. A corner's inset curve can also intersect with non-adjacent edges.
    // Trim non-adjacent pairs come first
    for (size_t gap = corners.size() / 2; gap; --gap) {
        for (size_t index = 0; index < corners.size(); ++index) {
            auto& first = corners[index];
            auto& second = corners[(index + gap) % corners.size()];

            if (!first.insetToTargetRect || !second.insetToTargetRect || first.isElided || second.isElided)
                continue;

            auto intersection = findMonotonicBezierCurvesIntersection(first.curves, second.curves);
            if (!intersection)
                continue;

            // Corners sharing an edge are visited once, in contour order, so their ordering
            // is already the right way round. A non-adjacent pair gets visited both ways, and only
            // one of those describes the tail of the first running into the head of the second.
            if (gap > 1 && !intersection->isTailToHead())
                continue;

            trimMonotonicBezierCurvesAtIntersection(first.curves, second.curves, *intersection);

            for (size_t between = 1; between < gap; ++between)
                corners[(index + between) % corners.size()].isElided = true;
        }
    }

    if (targetRect && !contourEnclosesArea(corners, *targetRect))
        return ContourResult::Empty;

    for (auto& prepared : corners) {
        if (prepared.isElided)
            continue;

        const auto& corner = prepared.resolved;
        const auto& adjusted = corner.adjusted;

        if (prepared.insetToTargetRect) {
            addInsetCornerCurves(path, prepared.curves, sharpInnerCornerPoint(*targetRect, adjusted.orientation), started);
            continue;
        }

        lineOrMoveTo(corner.miterStart);
        if (corner.collapsePoint) {
            path.addLineTo(*corner.collapsePoint);
            path.addLineTo(corner.miterEnd);
            continue;
        }

        if ((adjusted.start - corner.miterStart).diagonalLength() >= limit)
            path.addLineTo(adjusted.start);

        addCurvedCorner(path, corner);
        path.addLineTo(corner.miterEnd);
    }
    path.closeSubpath();
    return ContourResult::Contour;
}

static Vector<FloatPoint> normalizedInnerCornerHull(double curvature)
{
    if (curvature >= 0.0)
        return { { 1, 1 }, { 1, 0 }, { 0, 1 } };

    auto tangentIntercept = float(2.0 * normalizedSuperellipseHalfCorner(curvature));
    return { { 1, 1 }, { 1, 0 }, { tangentIntercept, 0 }, { 0, tangentIntercept }, { 0, 1 } };
}

static Vector<FloatPoint> cornerHullPolygon(const CornerInput& input)
{
    auto corner = makeCorner(input);
    auto normalizedHull = normalizedInnerCornerHull(input.curvature);
    return WTF::map(normalizedHull, [&](auto& point) {
        return mapPointToCorner(corner, FloatSize(point.x(), point.y()));
    });
}

static std::pair<double, double> offsetExtentAlongAxis(const Vector<FloatPoint>& hull, FloatSize axis)
{
    auto origin = hull.first();
    double low = 0.0;
    double high = 0.0;
    for (auto& point : hull) {
        double projection = (point.x() - origin.x()) * axis.width() + (point.y() - origin.y()) * axis.height();
        low = std::min(low, projection);
        high = std::max(high, projection);
    }
    return { low, high };
}

static double largestScaleWithoutIntersection(const Vector<FloatPoint>& first, const Vector<FloatPoint>& second)
{
    auto appendAxesOf = [](const Vector<FloatPoint>& hull, Vector<FloatSize>& axes) {
        for (size_t index = 0; index < hull.size(); ++index) {
            auto edgeStart = hull[index];
            auto edgeEnd = hull[(index + 1) % hull.size()];
            FloatSize axis { -(edgeEnd.y() - edgeStart.y()), edgeEnd.x() - edgeStart.x() };
            // A notch collapses two vertices onto the centre, leaving an edge with no normal.
            if (!axis.isZero())
                axes.append(axis);
        }
    };

    Vector<FloatSize> axes;
    appendAxesOf(first, axes);
    appendAxesOf(second, axes);

    auto projectPoint = [](const FloatPoint& point, FloatSize axis) {
        return point.x() * axis.width() + point.y() * axis.height();
    };

    double largest = 0.0;
    for (auto axis : axes) {
        auto [firstLow, firstHigh] = offsetExtentAlongAxis(first, axis);
        auto [secondLow, secondHigh] = offsetExtentAlongAxis(second, axis);
        double originGap = projectPoint(first.first(), axis) - projectPoint(second.first(), axis);

        auto consider = [&](double gap, double closing) {
            if (closing <= 0.0)
                return gap >= 0.0;
            if (gap >= 0.0)
                largest = std::max(largest, gap / closing);
            return false;
        };

        if (consider(originGap, secondHigh - firstLow) || consider(-originGap, firstHigh - secondLow))
            return 1.0;
    }

    return std::min(1.0, largest);
}

// https://drafts.csswg.org/css-borders-4/#corner-shape-constrain-radii
double oppositeCornerScaleFactor(const RectCorners<CornerInput>& cornerRects)
{
    auto pairScale = [](const CornerInput& first, const CornerInput& second) -> double {
        if (first.curvature >= 0.0 && second.curvature >= 0.0)
            return 1.0;

        if (!first.width || !first.height || !second.width || !second.height)
            return 1.0;

        return largestScaleWithoutIntersection(cornerHullPolygon(first), cornerHullPolygon(second));
    };

    return std::min({ 1.0,
        pairScale(cornerRects.topLeft(), cornerRects.bottomRight()),
        pairScale(cornerRects.topRight(), cornerRects.bottomLeft()) });
}

} // namespace WebCore
