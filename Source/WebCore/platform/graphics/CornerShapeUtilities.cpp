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
static bool isScoop(const Corner& corner) { return corner.curvature == -1.0; }
static bool isBevel(const Corner& corner) { return corner.curvature == 0.0; }
static bool isNotch(const Corner& corner) { return std::isinf(corner.curvature) && corner.curvature < 0.0; }
static bool isSquare(const Corner& corner) { return std::isinf(corner.curvature) && corner.curvature > 0.0; }
static bool isEmpty(const Corner& corner)
{
    return (corner.outer - corner.start).diagonalLength() < limit
        || (corner.end - corner.outer).diagonalLength() < limit;
}
static bool isSuperellipse(const Corner& corner)
{
    return !isBevel(corner) && !isRound(corner) && !isScoop(corner) && !isNotch(corner) && !isSquare(corner) && !isEmpty(corner);
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

static std::pair<FloatPoint, FloatPoint> buildBevelCorners(const Corner& original, double startInset, double endInset)
{
    auto strokeDirection = (original.end - original.start).perpendicular().normalized();
    auto outerToCenter = original.center - original.outer;
    // Ensure the stroke direction points inward, toward the center of the corner.
    if (dotProduct(strokeDirection, outerToCenter) < 0)
        strokeDirection = strokeDirection.scaled(-1);
    auto adjustedCornerStart = original.start + strokeDirection * float(startInset);
    auto adjustedCornerEnd = original.end + strokeDirection * float(endInset);

    auto extendStart = (original.center - original.start).directionScaledBy(float(startInset));
    auto extendEnd = (original.center - original.end).directionScaledBy(float(endInset));
    auto clipStart = original.start + extendStart;
    auto clipEnd = original.end + extendEnd;
    auto clipOuter = original.outer + extendStart + extendEnd;

    auto controlPoint = midPoint(adjustedCornerStart, adjustedCornerEnd);
    auto axisAlignedCornerStart = findIntersection(adjustedCornerStart, controlPoint, clipStart, clipOuter).value_or(adjustedCornerStart);
    auto axisAlignedCornerEnd = findIntersection(adjustedCornerEnd, controlPoint, clipEnd, clipOuter).value_or(adjustedCornerEnd);

    return { axisAlignedCornerStart, axisAlignedCornerEnd };
}

static Corner buildScoopCorners(const Corner& original, double startInset, double endInset)
{
    float outerRadiusX = original.radii.width();
    float outerRadiusY = original.radii.height();

    auto [verticalEdgeBorder, horizontalEdgeBorder] = edgeBorders(original.orientation, startInset, endInset);

    float radiusX = outerRadiusX + horizontalEdgeBorder;
    float radiusY = outerRadiusY + verticalEdgeBorder;

    // Endpoints are where the enlarged ellipse (centered on outer) crosses the inset edges
    auto [directionX, directionY] = inwardDirection(original.orientation);

    // x^2 / a^2 + y^2 / b^2 = 1 => y = b * sqrt(1 - x^2 / a^2)
    auto ellipseExtent = [](float radius, float ratio) {
        return radius * std::sqrt(std::max(0.0f, 1.0f - ratio * ratio));
    };

    FloatPoint sideIntersection {
        original.outer.x() + directionX * verticalEdgeBorder,
        original.outer.y() + directionY * ellipseExtent(radiusY, verticalEdgeBorder / radiusX),
    };
    FloatPoint topIntersection {
        original.outer.x() + directionX * ellipseExtent(radiusX, horizontalEdgeBorder / radiusY),
        original.outer.y() + directionY * horizontalEdgeBorder,
    };

    auto innerStart = startsOnVerticalEdge(original.orientation) ? sideIntersection : topIntersection;
    auto innerEnd = startsOnVerticalEdge(original.orientation) ? topIntersection : sideIntersection;

    return { innerStart, original.outer, innerEnd, original.center, FloatSize(radiusX, radiusY), original.curvature, original.orientation };
}

static Corner buildRoundCorners(const Corner& original, double startInset, double endInset)
{
    float outerRadiusX = original.radii.width();
    float outerRadiusY = original.radii.height();

    auto [verticalEdgeBorder, horizontalEdgeBorder] = edgeBorders(original.orientation, startInset, endInset);

    float radiusX = std::max(0.0f, outerRadiusX - verticalEdgeBorder);
    float radiusY = std::max(0.0f, outerRadiusY - horizontalEdgeBorder);

    // Endpoints are axis extremes of the ellipse
    float scaleX = outerRadiusX > limit ? radiusX / outerRadiusX : 0.0f;
    float scaleY = outerRadiusY > limit ? radiusY / outerRadiusY : 0.0f;
    auto scaleToCenter = [&](FloatPoint point) {
        return original.center + FloatSize((point.x() - original.center.x()) * scaleX, (point.y() - original.center.y()) * scaleY);
    };
    auto innerStart = scaleToCenter(original.start);
    auto innerEnd = scaleToCenter(original.end);

    auto [directionX, directionY] = inwardDirection(original.orientation);
    FloatPoint innerOuter {
        original.outer.x() + directionX * verticalEdgeBorder,
        original.outer.y() + directionY * horizontalEdgeBorder,
    };

    return { innerStart, innerOuter, innerEnd, original.center, FloatSize(radiusX, radiusY), original.curvature, original.orientation };
}

enum class ForceHullDirection : bool { No, Yes };

static Corner adjustCornerForInset(const Corner& original, double startInset, double endInset, ForceHullDirection forceHullDirection = ForceHullDirection::No)
{
    if (isEmpty(original) || (startInset == 0.0 && endInset == 0.0))
        return original;

    if (forceHullDirection == ForceHullDirection::No) {
        if (isBevel(original)) {
            auto [cutStart, cutEnd] = buildBevelCorners(original, startInset, endInset);
            return { cutStart, original.outer, cutEnd, original.center, cornerRadii(original.center, original.outer), original.curvature, original.orientation };
        }

        if (isScoop(original))
            return buildScoopCorners(original, startInset, endInset);

        if (isRound(original))
            return buildRoundCorners(original, startInset, endInset);
    }

    double strokeA = 0, strokeB = 0;
    if (isNotch(original)) {
        strokeA = -1;
        strokeB = 1;
    } else if (isSquare(original))
        strokeB = 1;
    else {
        // General: offset each corner vertex inward along the curve's hull direction by the border thickness, keeping the curvature.
        double clampedCurvature = std::clamp(original.curvature, -1.0, 1.0);
        double convexHalfCorner = std::pow(0.5, std::exp2(-clampedCurvature));
        auto hullDirection = FloatSize(float(convexHalfCorner * 2.0 - 0.5), float(1.5 - convexHalfCorner * 2.0)).normalized();
        strokeA = -hullDirection.height();
        strokeB = hullDirection.width();
    }

    auto offset1 = (original.outer - original.start).directionScaledBy(float(startInset * strokeA));
    auto offset2 = (original.end - original.outer).directionScaledBy(float(startInset * strokeB));
    auto offset3 = (original.center - original.end).directionScaledBy(float(endInset * strokeB));
    auto offset4 = (original.start - original.center).directionScaledBy(float(endInset * strokeA));

    auto adjustedStart = original.start + offset1 + offset2;
    auto adjustedOuter = original.outer + offset2 + offset3;
    auto adjustedEnd = original.end + offset3 + offset4;
    auto adjustedCenter = original.center + offset4 + offset1;

    return {
        adjustedStart,
        adjustedOuter,
        adjustedEnd,
        adjustedCenter,
        cornerRadii(adjustedCenter, adjustedOuter),
        original.curvature,
        original.orientation,
    };
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

// Defined in https://drafts.csswg.org/css-borders-4/#corner-shape-interpolation
static double normalizedSuperellipseHalfCorner(double superellipseParameter)
{
    if (std::isinf(superellipseParameter))
        return superellipseParameter < 0.0 ? 0.0 : 1.0;
    double exponent = std::pow(0.5, std::abs(superellipseParameter));
    double convexHalfCorner = std::pow(0.5, exponent);
    return superellipseParameter < 0.0 ? 1.0 - convexHalfCorner : convexHalfCorner;
}

struct SuperellipseBezierHandles {
    double handleA;
    double handleB;
    double halfCorner;
};
static SuperellipseBezierHandles superellipseBezierHandles(double parameter)
{
    static constexpr double fitCoefficients[7] = {
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

static void buildCorners(RectCorners<Corner>& corners, const RectCorners<CornerInput>& cornerRects, ForceHullDirection forceHullDirection = ForceHullDirection::No)
{
    for (auto key : { BoxCorner::TopLeft, BoxCorner::TopRight, BoxCorner::BottomLeft, BoxCorner::BottomRight }) {
        auto& input = cornerRects[key];
        auto original = makeCorner(input);
        auto corner = adjustCornerForInset(original, input.startInset, input.endInset, forceHullDirection);
        corners[key] = corner;
    }
}

using ContourEdge = std::pair<FloatPoint, FloatPoint>;

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

// The outset+miter curve for a single corner, flattened from its start miter to its end miter.
static Vector<FloatPoint> cornerOutsetSamples(const CornerInput& input, double curvature, const ContourEdge& startEdge, const ContourEdge& endEdge, unsigned stepsPerHalf)
{
    CornerInput swapped = input;
    swapped.curvature = curvature;
    auto corner = adjustCornerForInset(makeCorner(swapped), input.startInset, input.endInset, ForceHullDirection::Yes);
    auto controlPoint = quadraticControlPoint(corner);
    auto miterStart = findIntersection(startEdge.first, startEdge.second, corner.start, controlPoint).value_or(corner.start);
    auto miterEnd = findIntersection(endEdge.first, endEdge.second, corner.end, controlPoint).value_or(corner.end);
    Vector<FloatPoint> points;
    points.append(miterStart);
    sampleDrawnCorner(corner, stepsPerHalf, points);
    points.append(miterEnd);
    return points;
}

// Cubic Hermite morph of the outset curve at s = -1, 0, +1 for a corner whose curvature is in (-1, 1).
static void addMorphedOutsetCorner(Path& path, const CornerInput& input, const ContourEdge& startEdge, const ContourEdge& endEdge, float deviceScaleFactor, bool& started)
{
    constexpr double flatnessTolerance = 0.25; // device pixels
    double radius = std::max(input.width, input.height) + std::max(std::abs(input.startInset), std::abs(input.endInset));
    double scale = deviceScaleFactor > 0.0f ? deviceScaleFactor : 1.0;
    double quarterArcChords = std::numbers::pi / 2.0 * std::sqrt(radius * scale / (8.0 * flatnessTolerance));
    unsigned stepsPerHalf = std::max(2u, static_cast<unsigned>(std::clamp(std::ceil(quarterArcChords / 2.0), 4.0, 64.0)));
    unsigned sampleCount = 2 * stepsPerHalf + 4;
    constexpr double epsilon = 0.04; // curvature step for the endpoint-velocity finite differences
    double curvature = input.curvature;

    auto anchor = [&](double sampleCurvature) {
        return resampleByArcLength(cornerOutsetSamples(input, sampleCurvature, startEdge, endEdge, stepsPerHalf), sampleCount);
    };

    auto velocity = [&](const Vector<FloatPoint>& ahead, const Vector<FloatPoint>& behind, double deltaCurvature) {
        Vector<FloatSize> perPointVelocity(sampleCount);
        for (unsigned index = 0; index < sampleCount; ++index)
            perPointVelocity[index] = (ahead[index] - behind[index]).scaled(1.0f / deltaCurvature);
        return perPointVelocity;
    };

    auto middleAnchor = anchor(0.0);
    Vector<FloatPoint> startAnchor, endAnchor;
    Vector<FloatSize> startVelocity, endVelocity;
    // The morph brackets the curvature s into a sub-interval whose endpoints are anchor curves, then
    // remaps s onto [0, 1] as hermiteInterpolate's blend fraction
    double interpolationFraction;
    if (curvature < 0.0) {
        startAnchor = anchor(-1.0);
        endAnchor = middleAnchor;
        startVelocity = velocity(startAnchor, anchor(-1.0 - epsilon), epsilon); // dPosition/ds at s = -1 (from below)
        endVelocity = velocity(anchor(epsilon), anchor(-epsilon), 2.0 * epsilon); // dPosition/ds at s = 0 (central)
        interpolationFraction = curvature + 1.0; // s in [-1, 0] -> [0, 1]
    } else {
        startAnchor = middleAnchor;
        endAnchor = anchor(1.0);
        startVelocity = velocity(anchor(epsilon), anchor(-epsilon), 2.0 * epsilon); // dPosition/ds at s = 0 (central)
        endVelocity = velocity(anchor(1.0 + epsilon), endAnchor, epsilon); // dPosition/ds at s = +1 (from above)
        interpolationFraction = curvature; // s in [0, 1] -> [0, 1]
    }

    auto morphedCorner = hermiteInterpolate(startAnchor, startVelocity, endAnchor, endVelocity, interpolationFraction);

    // Fixed count sized for the worst-case corner (up to ~R = 240) to stay within 0.25px of the true curve
    // TODO: cache the built path then a Schneider fit could use fewer segments on large corners
    constexpr unsigned bezierSegmentCount = 18;
    addCatmullRomBeziers(path, morphedCorner, bezierSegmentCount, started);
}

} // namespace

// https://drafts.csswg.org/css-borders-4/#contour-path
void borderContourPath(Path& path, const RectCorners<CornerInput>& cornerRects, const FloatRect* targetRect, OutsetMiter outsetMiter, float deviceScaleFactor)
{
    RectCorners<Corner> corners;
    buildCorners(corners, cornerRects, outsetMiter == OutsetMiter::Yes ? ForceHullDirection::Yes : ForceHullDirection::No);

    using Edge = std::pair<FloatPoint, FloatPoint>;
    auto edges = targetRect ? targetRect->edges() : RectEdges<Edge> { };

    auto edgesForCorner = [&](BoxCorner key) -> std::pair<Edge, Edge> {
        switch (key) {
        case BoxCorner::TopRight:
            return { edges.top(), edges.right() };
        case BoxCorner::BottomRight:
            return { edges.right(), edges.bottom() };
        case BoxCorner::BottomLeft:
            return { edges.bottom(), edges.left() };
        case BoxCorner::TopLeft:
            return { edges.left(), edges.top() };
        }
        return { edges.top(), edges.right() };
    };

    auto miterToEdge = [](const FloatPoint& endpoint, const FloatPoint& tangentControl, const Edge& edge) {
        return findIntersection(edge.first, edge.second, endpoint, tangentControl).value_or(endpoint);
    };

    bool started = false;
    for (auto key : { BoxCorner::TopRight, BoxCorner::BottomRight, BoxCorner::BottomLeft, BoxCorner::TopLeft }) {
        const auto& corner = corners[key];

        if (outsetMiter == OutsetMiter::Yes) {
            auto [startEdge, endEdge] = edgesForCorner(key);
            double curvature = cornerRects[key].curvature;
            if (targetRect && curvature > -1.0 && curvature < 1.0) {
                addMorphedOutsetCorner(path, cornerRects[key], startEdge, endEdge, deviceScaleFactor, started);
                continue;
            }
            auto controlPoint = quadraticControlPoint(corner);
            auto miterStart = miterToEdge(corner.start, controlPoint, startEdge);
            auto miterEnd = miterToEdge(corner.end, controlPoint, endEdge);
            if (!started) {
                path.moveTo(miterStart);
                started = true;
            } else
                path.addLineTo(miterStart);
            addCurvedCorner(path, corner);
            path.addLineTo(miterEnd);
            continue;
        }

        // Inset: carve the superellipse corner's curve to the target rect.
        if (targetRect && isSuperellipse(corner)) {
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

// https://drafts.csswg.org/css-borders-4/#corner-shape-constrain-radii
double oppositeCornerScaleFactor(const RectCorners<CornerInput>&)
{
    // TODO: implement opposite-corner scale factor computation.
    return 1.0;
}

} // namespace WebCore
