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

// The summed per-endpoint offsets are the start and end normal vectors whose perpendiculars give the outset miter
// tangents. The radii are carried through for the concave self-intersection test.
struct CornerAdjustment {
    Corner corner;
    FloatSize startNormal;
    FloatSize endNormal;
    double startRadius { 0 };
    double endRadius { 0 };
    FloatPoint targetCornerOuter;
    FloatSize unitVectorTowardStart;
    FloatSize unitVectorTowardEnd;
};

// Defined in https://drafts.csswg.org/css-borders-4/#corner-shape-interpolation
static double normalizedSuperellipseHalfCorner(double superellipseParameter)
{
    if (std::isinf(superellipseParameter))
        return superellipseParameter < 0.0 ? 0.0 : 1.0;
    double exponent = std::pow(0.5, std::abs(superellipseParameter));
    double convexHalfCorner = std::pow(0.5, exponent);
    return superellipseParameter < 0.0 ? 1.0 - convexHalfCorner : convexHalfCorner;
}

// Offsets both curve endpoints along the corner's two edge vectors, scaling the offset normal per-axis by the two corner radii
static CornerAdjustment computeCornerAdjustment(const Corner& original, double startInset, double endInset)
{
    auto vectorTowardStart = original.start - original.outer;
    auto vectorTowardEnd = original.end - original.outer;
    double endRadius = vectorTowardStart.diagonalLength();
    double startRadius = vectorTowardEnd.diagonalLength();
    if (startRadius < limit || endRadius < limit) {
        CornerAdjustment degenerate;
        degenerate.corner = original;
        degenerate.startRadius = startRadius;
        degenerate.endRadius = endRadius;
        degenerate.targetCornerOuter = original.outer;
        return degenerate;
    }

    auto unitVectorTowardStart = vectorTowardStart.normalized();
    auto unitVectorTowardEnd = vectorTowardEnd.normalized();
    auto targetCornerOuter = original.outer + unitVectorTowardStart * float(endInset) + unitVectorTowardEnd * float(startInset);

    double clampedHalfCornerX = normalizedSuperellipseHalfCorner(std::clamp(original.curvature, -1.0, 1.0));
    // Where the endpoint offset directions sit between the corner's two edges: 0 runs along the edge the
    // endpoint lies on, 1 runs perpendicular to it, and 0.5 is the diagonal between the two, weighted by
    // the radii
    double controlPointX = (1.0 / (std::numbers::sqrt2 - 1.0)) * clampedHalfCornerX - 1.0 / std::numbers::sqrt2;

    // Bevel corner as the common tangent to two circles centred on corner-start and corner-end with radii the signed insets
    // negative sign: outward, away from box, postive sign: inward, towards box center
    double insetDiff = std::clamp(endInset - startInset, -startRadius, endRadius);
    double root = std::sqrt(std::max(0.0, startRadius * startRadius + endRadius * endRadius - insetDiff * insetDiff));
    double bevelNormalVectorX = endRadius * insetDiff + startRadius * root;
    double bevelNormalVectorY = -startRadius * insetDiff + endRadius * root;
    double bevelDenominator = startRadius * bevelNormalVectorY + endRadius * bevelNormalVectorX;
    double bevelControlPointX = bevelDenominator != 0.0 ? (startRadius * bevelNormalVectorY) / bevelDenominator : 0.5;

    // The two endpoints get separate directions, placed symmetrically either side of controlPointX. How far
    // apart they land comes from the bevel tangent, which sits at the midpoint unless the two insets differ,
    // so equal insets give both endpoints the same direction.
    double startControlPointX = original.curvature < 0.0
        ? bevelControlPointX * (2.0 * controlPointX)
        : 1.0 - (1.0 - bevelControlPointX) * (2.0 * (1.0 - controlPointX));
    double endControlPointX = 2.0 * controlPointX - startControlPointX;

    // Per-axis normal vectors: the corner radii weight each component, so elliptical corners offset correctly.
    auto startNormal = FloatSize(float((1.0 - startControlPointX) * startRadius), float(startControlPointX * endRadius)).normalized();
    auto endNormal = FloatSize(float(endControlPointX * startRadius), float((1.0 - endControlPointX) * endRadius)).normalized();

    auto startOffsetTowardStart = vectorTowardStart.directionScaledBy(float(startNormal.width() * startInset));
    auto startOffsetTowardEnd = vectorTowardEnd.directionScaledBy(float(startNormal.height() * startInset));
    auto endOffsetTowardStart = vectorTowardStart.directionScaledBy(float(endNormal.width() * endInset));
    auto endOffsetTowardEnd = vectorTowardEnd.directionScaledBy(float(endNormal.height() * endInset));

    auto adjustedStart = original.start + startOffsetTowardStart + startOffsetTowardEnd;
    auto adjustedOuter = original.outer + endOffsetTowardStart + startOffsetTowardEnd;
    auto adjustedEnd = original.end + endOffsetTowardStart + endOffsetTowardEnd;
    auto adjustedCenter = original.center + startOffsetTowardStart + endOffsetTowardEnd;

    Corner adjusted {
        adjustedStart,
        adjustedOuter,
        adjustedEnd,
        adjustedCenter,
        cornerRadii(adjustedCenter, adjustedOuter),
        original.curvature,
        original.orientation,
    };

    return {
        .corner = adjusted,
        .startNormal = startOffsetTowardStart + startOffsetTowardEnd,
        .endNormal = endOffsetTowardStart + endOffsetTowardEnd,
        .startRadius = startRadius,
        .endRadius = endRadius,
        .targetCornerOuter = targetCornerOuter,
        .unitVectorTowardStart = unitVectorTowardStart,
        .unitVectorTowardEnd = unitVectorTowardEnd,
    };
}

// Moves the corner's four vertices to where the inset contour needs them. A corner with no radius has no
// curve to offset, so it collapses to a single point at the inset rect's corner. A corner with no inset is
// returned unchanged.
static CornerAdjustment resolveCornerAdjustment(const Corner& original, double startInset, double endInset)
{
    auto unchanged = [](const Corner& corner) -> CornerAdjustment {
        CornerAdjustment adjustment;
        adjustment.corner = corner;
        adjustment.targetCornerOuter = corner.outer;
        return adjustment;
    };

    if (isEmpty(original)) {
        if (startInset == 0.0 && endInset == 0.0)
            return unchanged(original);
        // Choose a corner position such that the two edges meet on the inset rect rather than on the border
        // box: the vertical edge's inset moves it horizontally, the horizontal edge's inset vertically.
        auto [directionX, directionY] = inwardDirection(original.orientation);
        auto [verticalEdgeBorder, horizontalEdgeBorder] = edgeBorders(original.orientation, startInset, endInset);
        auto targetCorner = original.outer + FloatSize(directionX * verticalEdgeBorder, directionY * horizontalEdgeBorder);
        return unchanged({ targetCorner, targetCorner, targetCorner, targetCorner, { }, original.curvature, original.orientation });
    }

    if (startInset == 0.0 && endInset == 0.0)
        return unchanged(original);

    if (std::isfinite(original.curvature))
        return computeCornerAdjustment(original, startInset, endInset);

    double strokeA = isNotch(original) ? -1.0 : 0.0;
    double strokeB = 1.0;

    auto offset1 = (original.outer - original.start).directionScaledBy(float(startInset * strokeA));
    auto offset2 = (original.end - original.outer).directionScaledBy(float(startInset * strokeB));
    auto offset3 = (original.center - original.end).directionScaledBy(float(endInset * strokeB));
    auto offset4 = (original.start - original.center).directionScaledBy(float(endInset * strokeA));

    auto adjustedOuter = original.outer + offset2 + offset3;
    auto adjustedCenter = original.center + offset4 + offset1;

    // Notch and square keep the offsets above, but take their miter vectors from the shared construction.
    auto adjustment = computeCornerAdjustment(original, startInset, endInset);
    adjustment.corner = {
        original.start + offset1 + offset2,
        adjustedOuter,
        original.end + offset3 + offset4,
        adjustedCenter,
        cornerRadii(adjustedCenter, adjustedOuter),
        original.curvature,
        original.orientation,
    };
    return adjustment;
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
    return { handleA, handleB, halfCorner };
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

// Control point of the quadratic that approximates the corner, giving the curve's tangent at its endpoints
static FloatPoint quadraticControlPoint(const Corner& corner)
{
    auto drawn = isConcave(corner) ? inverseCorner(corner) : corner;
    if (drawn.curvature >= 1.0)
        return drawn.outer;
    auto handles = superellipseBezierHandles(drawn.curvature);
    float normalizedControl = 2.0f * float(handles.halfCorner) - 0.5f;
    return mapPointToCorner(drawn, FloatSize(normalizedControl, normalizedControl));
}

static void addCurvedCorner(Path& path, const Corner& corner)
{
    if (isNotch(corner)) {
        path.addLineTo(corner.start);
        path.addLineTo(corner.center);
        path.addLineTo(corner.end);
        return;
    }

    if (isConcave(corner)) {
        addCurvedCorner(path, inverseCorner(corner));
        return;
    }

    path.addLineTo(corner.start);

    if (isBevel(corner)) {
        path.addLineTo(corner.end);
        return;
    }
    if (isRound(corner)) {
        addEllipticalArc(path, corner);
        return;
    }
    if (isSquare(corner)) {
        path.addLineTo(corner.outer);
        path.addLineTo(corner.end);
        return;
    }

    // General superellipse: squircle (s=2) and superellipse(n)
    if (extendPathForSharpCorner(path, corner))
        return;
    for (const auto& curve : superellipseCornerBeziers(corner))
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

// rect ∩ corner-carve
static void addTrimmedSuperellipseCorner(Path& path, const Corner& corner, const FloatRect& innerRect, bool& started)
{
    auto lineOrMoveTo = [&](FloatPoint point) {
        if (!started) {
            path.moveTo(point);
            started = true;
        } else
            path.addLineTo(point);
    };

    Vector<BezierSegment> clippedCurves;
    for (const auto& bezier : superellipseCornerBeziers(corner)) {
        for (const auto& curve : trimBezierToRect(bezier, innerRect))
            clippedCurves.append(curve);
    }

    if (clippedCurves.isEmpty()) {
        lineOrMoveTo(sharpInnerCornerPoint(innerRect, corner.orientation));
        return;
    }
    for (const auto& curve : clippedCurves) {
        lineOrMoveTo(curve.start);
        path.addBezierCurveTo(curve.controlPoint1, curve.controlPoint2, curve.end);
    }
}

static void buildCorners(RectCorners<Corner>& corners, const RectCorners<CornerInput>& cornerRects)
{
    for (auto key : { BoxCorner::TopLeft, BoxCorner::TopRight, BoxCorner::BottomLeft, BoxCorner::BottomRight }) {
        auto& input = cornerRects[key];
        auto original = makeCorner(input);
        auto corner = resolveCornerAdjustment(original, input.startInset, input.endInset).corner;
        corners[key] = corner;
    }
}

using ContourEdge = std::pair<FloatPoint, FloatPoint>;

// Outer miters. Convex corners miter along the equivalent-quadratic control point, concave ones
// along the curve tangent. When the outset exceeds a corner radius the two miters cross and the curve collapses.
struct OutsetCornerContour {
    FloatPoint miterStart;
    FloatPoint miterEnd;
    bool collapsed { false };
    FloatPoint collapsePoint;
};

static std::optional<FloatPoint> findMiterArmCrossing(const FloatPoint& startArmOuter, const FloatPoint& startArmInner, const FloatPoint& endArmOuter, const FloatPoint& endArmInner)
{
    if (auto crossing = findSegmentLineIntersection(startArmOuter, startArmInner, endArmOuter, endArmInner))
        return crossing;
    return findSegmentLineIntersection(endArmOuter, endArmInner, startArmOuter, startArmInner);
}

static OutsetCornerContour outsetCornerContour(const Corner& adjusted, const CornerAdjustment& adjustment, double startInset, double endInset)
{
    auto clipOuter = adjustment.targetCornerOuter;
    ContourEdge startLine { clipOuter + adjustment.unitVectorTowardStart, clipOuter };
    ContourEdge endLine { clipOuter + adjustment.unitVectorTowardEnd, clipOuter };

    OutsetCornerContour contour;
    if (isConcave(adjusted)) {
        auto startTangent = adjustment.startNormal.perpendicular().scaled(-1);
        auto endTangent = adjustment.endNormal.perpendicular();
        contour.miterStart = findIntersection(startLine.first, startLine.second, adjusted.start, adjusted.start + startTangent).value_or(adjusted.start);
        contour.miterEnd = findIntersection(endLine.first, endLine.second, adjusted.end, adjusted.end + endTangent).value_or(adjusted.end);

        bool swallowsAnArm = -endInset >= adjustment.startRadius || -startInset >= adjustment.endRadius;
        if (startInset < 0.0 && endInset < 0.0 && swallowsAnArm) {
            if (auto crossing = findMiterArmCrossing(contour.miterStart, adjusted.start, contour.miterEnd, adjusted.end)) {
                contour.collapsed = true;
                contour.collapsePoint = *crossing;
            }
        }
        return contour;
    }

    auto controlPoint = quadraticControlPoint(adjusted);
    contour.miterStart = findIntersection(startLine.first, startLine.second, adjusted.start, controlPoint).value_or(adjusted.start);
    contour.miterEnd = findIntersection(endLine.first, endLine.second, adjusted.end, controlPoint).value_or(adjusted.end);
    return contour;
}

// Flatten the drawn corner curve (start..end) into points, matching addCurvedCorner's geometry.
static void sampleDrawnCorner(const Corner& corner, unsigned stepsPerHalf, Vector<FloatPoint>& out)
{
    if (isConcave(corner)) {
        sampleDrawnCorner(inverseCorner(corner), stepsPerHalf, out);
        return;
    }
    out.append(corner.start);
    if (isBevel(corner)) {
        out.append(corner.end);
        return;
    }
    if (isSquare(corner)) {
        out.append(corner.outer);
        out.append(corner.end);
        return;
    }
    for (auto& segment : superellipseCornerBeziers(corner)) {
        for (unsigned step = 1; step <= stepsPerHalf; ++step)
            out.append(pointOnBezierAtParameter(segment, double(step) / stepsPerHalf));
    }
}

static std::optional<FloatSize> outsetArmDirection(const FloatPoint& from, const FloatPoint& to)
{
    auto direction = to - from;
    if (direction.diagonalLength() < limit)
        return std::nullopt;
    return direction.normalized();
}

// The outset contour is a straight arm, then the curve, then another straight arm. Fitting the curve with its
// end tangents forced to the arm directions makes each junction smooth, instead of the crease that shows when
// the curve and the arm arrive at different angles.
static void addTangentJoinedCorner(Path& path, const Corner& adjusted, const OutsetCornerContour& contour, float deviceScaleFactor, bool& started)
{
    constexpr double flatnessTolerance = 0.25; // device pixels
    double radius = std::max(adjusted.radii.width(), adjusted.radii.height());
    double scale = deviceScaleFactor > 0.0f ? deviceScaleFactor : 1.0;
    double quarterArcChords = std::numbers::pi / 2.0 * std::sqrt(radius * scale / (8.0 * flatnessTolerance));
    unsigned stepsPerHalf = std::max(2u, static_cast<unsigned>(std::clamp(std::ceil(quarterArcChords / 2.0), 4.0, 64.0)));

    Vector<FloatPoint> samples;
    sampleDrawnCorner(adjusted, stepsPerHalf, samples);
    if (samples.size() < 2) {
        addCurvedCorner(path, adjusted);
        return;
    }

    constexpr unsigned bezierSegmentCount = 18;
    addCatmullRomBeziers(path, samples, bezierSegmentCount, started,
        outsetArmDirection(contour.miterStart, samples.first()), outsetArmDirection(samples.last(), contour.miterEnd));
}

// The outset+miter curve for a single corner, flattened from its start miter to its end miter.
static Vector<FloatPoint> cornerOutsetSamples(const CornerInput& input, double curvature, unsigned stepsPerHalf)
{
    CornerInput swapped = input;
    swapped.curvature = curvature;
    auto adjustment = resolveCornerAdjustment(makeCorner(swapped), input.startInset, input.endInset);
    auto contour = outsetCornerContour(adjustment.corner, adjustment, input.startInset, input.endInset);

    Vector<FloatPoint> points;
    points.append(contour.miterStart);
    if (contour.collapsed) {
        // Degenerate concave miter: the curve becomes the crossing of the two arms.
        points.append(contour.collapsePoint);
        points.append(contour.collapsePoint);
    } else
        sampleDrawnCorner(adjustment.corner, stepsPerHalf, points);
    points.append(contour.miterEnd);
    return points;
}

// Cubic Hermite morph of the outset curve over s in (0, 1), anchored on the contour that is constructed at s = 0 and s = 1.
static void addMorphedOutsetCorner(Path& path, const CornerInput& input, float deviceScaleFactor, bool& started)
{
    constexpr double flatnessTolerance = 0.25; // device pixels
    double radius = std::max(input.width, input.height) + std::max(std::abs(input.startInset), std::abs(input.endInset));
    double scale = deviceScaleFactor > 0.0f ? deviceScaleFactor : 1.0;
    double quarterArcChords = std::numbers::pi / 2.0 * std::sqrt(radius * scale / (8.0 * flatnessTolerance));
    unsigned stepsPerHalf = std::max(2u, static_cast<unsigned>(std::clamp(std::ceil(quarterArcChords / 2.0), 4.0, 64.0)));
    unsigned sampleCount = 2 * stepsPerHalf + 4;
    constexpr double epsilon = 0.04; // curvature step for the endpoint-velocity finite differences

    auto anchor = [&](double sampleCurvature) {
        return resampleByArcLength(cornerOutsetSamples(input, sampleCurvature, stepsPerHalf), sampleCount);
    };
    auto velocity = [&](const Vector<FloatPoint>& ahead, const Vector<FloatPoint>& behind, double deltaCurvature) {
        Vector<FloatSize> perPointVelocity(sampleCount);
        for (unsigned index = 0; index < sampleCount; ++index)
            perPointVelocity[index] = (ahead[index] - behind[index]).scaled(1.0f / deltaCurvature);
        return perPointVelocity;
    };

    auto startAnchor = anchor(0.0);
    auto endAnchor = anchor(1.0);
    auto startVelocity = velocity(anchor(epsilon), startAnchor, epsilon); // dPosition/ds at s = 0 (from above)
    auto endVelocity = velocity(anchor(1.0 + epsilon), endAnchor, epsilon); // dPosition/ds at s = +1 (from above)

    auto morphedCorner = hermiteInterpolate(startAnchor, startVelocity, endAnchor, endVelocity, input.curvature);
    if (morphedCorner.size() < 2)
        return;

    constexpr unsigned bezierSegmentCount = 18;
    addCatmullRomBeziers(path, morphedCorner, bezierSegmentCount, started,
        outsetArmDirection(morphedCorner[0], morphedCorner[1]),
        outsetArmDirection(morphedCorner[morphedCorner.size() - 2], morphedCorner[morphedCorner.size() - 1]));
}

} // namespace

// https://drafts.csswg.org/css-borders-4/#contour-path
void borderContourPath(Path& path, const RectCorners<CornerInput>& cornerRects, const FloatRect* targetRect, OutsetMiter outsetMiter, float deviceScaleFactor)
{
    RectCorners<Corner> corners;
    buildCorners(corners, cornerRects);

    bool started = false;
    for (auto key : { BoxCorner::TopRight, BoxCorner::BottomRight, BoxCorner::BottomLeft, BoxCorner::TopLeft }) {
        const auto& corner = corners[key];
        double startInset = cornerRects[key].startInset;
        double endInset = cornerRects[key].endInset;

        auto addOutsetCorner = [&](const OutsetCornerContour& contour) {
            if (!started) {
                path.moveTo(contour.miterStart);
                started = true;
            } else
                path.addLineTo(contour.miterStart);
            if (contour.collapsed)
                path.addLineTo(contour.collapsePoint);
            else
                addTangentJoinedCorner(path, corner, contour, deviceScaleFactor, started);
            path.addLineTo(contour.miterEnd);
        };

        if (outsetMiter == OutsetMiter::Yes) {
            double curvature = cornerRects[key].curvature;
            if (targetRect && curvature > 0.0 && curvature < 1.0) {
                addMorphedOutsetCorner(path, cornerRects[key], deviceScaleFactor, started);
                continue;
            }
            auto adjustment = computeCornerAdjustment(makeCorner(cornerRects[key]), startInset, endInset);
            addOutsetCorner(outsetCornerContour(corner, adjustment, startInset, endInset));
            continue;
        }

        // An outward offset must still reach the offset rect's edges when no target rect was given
        if ((startInset < 0.0 || endInset < 0.0) && std::isfinite(corner.curvature) && !isEmpty(corner)) {
            auto adjustment = computeCornerAdjustment(makeCorner(cornerRects[key]), startInset, endInset);
            addOutsetCorner(outsetCornerContour(corner, adjustment, startInset, endInset));
            continue;
        }
        // Inset: carve the superellipse corner's curve to the target rect.
        if (targetRect && std::isfinite(corner.curvature) && !isEmpty(corner)) {
            addTrimmedSuperellipseCorner(path, corner, *targetRect, started);
            continue;
        }

        if (!started) {
            path.moveTo(corner.start);
            started = true;
        }
        addCurvedCorner(path, corner);
    }
    path.closeSubpath();
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
