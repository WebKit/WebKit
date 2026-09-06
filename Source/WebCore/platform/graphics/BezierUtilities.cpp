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
#include "BezierUtilities.h"

#include "FloatPoint.h"
#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "RectEdges.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>

namespace WebCore {

namespace {

// Parameters within this of an interval boundary are snapped to it
constexpr double parameterEpsilon = 1e-7;
constexpr double nearZeroEpsilon = 1e-12;
struct BezierIntersection {
    float positionOnCurve { 0 };
    float positionOnLine { 0 };
    FloatPoint point;
};

// Solves cubic equation where roots are the parameters t at which a cubic Bézier curve crosses a line
Vector<double, 3> solveCubicForParametersInUnitInterval(double cubicCoefficient, double quadraticCoefficient, double linearCoefficient, double constantCoefficient)
{
    Vector<double, 3> parametersInUnitInterval;
    auto addRoot = [&](double root) {
        if (root < -parameterEpsilon || root > 1.0 + parameterEpsilon)
            return;
        parametersInUnitInterval.append(std::clamp(root, 0.0, 1.0));
    };

    // Nearly-zero cubic term, solve for quadratic
    if (std::abs(cubicCoefficient) < nearZeroEpsilon) {
        if (std::abs(quadraticCoefficient) < nearZeroEpsilon) {
            // Linear linearCoefficient·t + constantCoefficient = 0.
            if (std::abs(linearCoefficient) >= nearZeroEpsilon)
                addRoot(-constantCoefficient / linearCoefficient);
            return parametersInUnitInterval;
        }
        double discriminant = linearCoefficient * linearCoefficient - 4 * quadraticCoefficient * constantCoefficient;
        if (discriminant < 0)
            return parametersInUnitInterval;
        double sqrtDiscriminant = std::sqrt(discriminant);
        addRoot((-linearCoefficient + sqrtDiscriminant) / (2 * quadraticCoefficient));
        addRoot((-linearCoefficient - sqrtDiscriminant) / (2 * quadraticCoefficient));
        return parametersInUnitInterval;
    }

    // Make the cubic monic, depress it to y^3 + depressedLinear·y + depressedConstant = 0, and solve via Cardano / Viète.
    double monicQuadratic = quadraticCoefficient / cubicCoefficient;
    double monicLinear = linearCoefficient / cubicCoefficient;
    double monicConstant = constantCoefficient / cubicCoefficient;

    double shift = monicQuadratic / 3.0;
    double depressedLinear = monicLinear - monicQuadratic * monicQuadratic / 3.0;
    double depressedConstant = 2.0 * monicQuadratic * monicQuadratic * monicQuadratic / 27.0 - monicQuadratic * monicLinear / 3.0 + monicConstant;

    double discriminant = depressedConstant * depressedConstant / 4.0 + depressedLinear * depressedLinear * depressedLinear / 27.0;

    if (discriminant > 0) {
        // One real root (Cardano).
        double sqrtDiscriminant = std::sqrt(discriminant);
        double firstCubeRoot = std::cbrt(-depressedConstant / 2.0 + sqrtDiscriminant);
        double secondCubeRoot = std::cbrt(-depressedConstant / 2.0 - sqrtDiscriminant);
        addRoot(firstCubeRoot + secondCubeRoot - shift);
        return parametersInUnitInterval;
    }

    if (std::abs(discriminant) <= nearZeroEpsilon) {
        // Repeated roots.
        double cubeRoot = std::cbrt(-depressedConstant / 2.0);
        addRoot(2.0 * cubeRoot - shift);
        addRoot(-cubeRoot - shift);
        return parametersInUnitInterval;
    }

    // Three distinct real roots — trigonometric (Viète) form.
    double magnitude = 2.0 * std::sqrt(-depressedLinear / 3.0);
    double cosineArgument = std::clamp(3.0 * depressedConstant / (depressedLinear * magnitude), -1.0, 1.0);
    double angle = std::acos(cosineArgument);
    constexpr double twoPi = 2.0 * std::numbers::pi;
    for (int rootIndex = 0; rootIndex < 3; ++rootIndex)
        addRoot(magnitude * std::cos((angle - twoPi * rootIndex) / 3.0) - shift);
    return parametersInUnitInterval;
}

Vector<BezierIntersection> intersectBezierAndLine(const BezierSegment& curve, const FloatPoint& lineStart, const FloatPoint& lineEnd)
{
    double directionX = lineEnd.x() - lineStart.x();
    double directionY = lineEnd.y() - lineStart.y();

    // Control points' signed distances = Bernstein coefficients of the distance-from-line cubic in t (scale doesn't affect roots).
    double distanceStart = signedDistanceToLine(curve.start, lineStart, lineEnd);
    double distanceControl1 = signedDistanceToLine(curve.controlPoint1, lineStart, lineEnd);
    double distanceControl2 = signedDistanceToLine(curve.controlPoint2, lineStart, lineEnd);
    double distanceEnd = signedDistanceToLine(curve.end, lineStart, lineEnd);

    // Convert the Bernstein cubic to power form
    double cubicCoefficient = -distanceStart + 3 * distanceControl1 - 3 * distanceControl2 + distanceEnd;
    double quadraticCoefficient = 3 * distanceStart - 6 * distanceControl1 + 3 * distanceControl2;
    double linearCoefficient = -3 * distanceStart + 3 * distanceControl1;
    double constantCoefficient = distanceStart;

    auto parameters = solveCubicForParametersInUnitInterval(cubicCoefficient, quadraticCoefficient, linearCoefficient, constantCoefficient);

    double lineLengthSquared = directionX * directionX + directionY * directionY;

    Vector<BezierIntersection> intersections;
    for (double parameter : parameters) {
        auto point = pointOnBezierAtParameter(curve, parameter);

        // Parameter of the crossing along the (finite) line segment.
        double positionOnLine = lineLengthSquared > 0
            ? ((point.x() - lineStart.x()) * directionX + (point.y() - lineStart.y()) * directionY) / lineLengthSquared
            : 0.0;

        // Keep only crossings that fall within the segment as well as the curve.
        if (positionOnLine < -parameterEpsilon || positionOnLine > 1.0 + parameterEpsilon)
            continue;

        intersections.append({ float(parameter), float(std::clamp(positionOnLine, 0.0, 1.0)), point });
    }

    std::sort(intersections.begin(), intersections.end(), [](const BezierIntersection& lhs, const BezierIntersection& rhs) {
        return lhs.positionOnCurve < rhs.positionOnCurve;
    });
    return intersections;
}

// De Casteljau's algorithm
std::pair<BezierSegment, BezierSegment> splitBezier(const BezierSegment& curve, float parameter)
{
    auto startToControl1 = linearInterpolation(curve.start, curve.controlPoint1, parameter);
    auto control1ToControl2 = linearInterpolation(curve.controlPoint1, curve.controlPoint2, parameter);
    auto control2ToEnd = linearInterpolation(curve.controlPoint2, curve.end, parameter);
    auto leftControl2 = linearInterpolation(startToControl1, control1ToControl2, parameter);
    auto rightControl1 = linearInterpolation(control1ToControl2, control2ToEnd, parameter);
    auto splitPoint = linearInterpolation(leftControl2, rightControl1, parameter);

    BezierSegment before { curve.start, startToControl1, leftControl2, splitPoint };
    BezierSegment after { splitPoint, rightControl1, control2ToEnd, curve.end };
    return { before, after };
}

Vector<BezierSegment> trimBezierToLine(const BezierSegment& curve, const FloatPoint& lineStart, const FloatPoint& lineEnd, bool keepLeftOfLine)
{
    auto crossings = intersectBezierAndLine(curve, lineStart, lineEnd);

    // Between successive crossings the pieces alternate sides of the line, so keep those whose midpoint is on the requested side.
    Vector<BezierSegment> kept;
    auto remaining = curve;
    float consumedParameter = 0.0f;
    for (const auto& crossing : crossings) {
        // Re-map the crossing parameter into the shrinking `remaining` curve's own [0, 1] range.
        float localParameter = (crossing.positionOnCurve - consumedParameter) / (1.0f - consumedParameter);
        localParameter = std::clamp(localParameter, 0.0f, 1.0f);
        auto [piece, rest] = splitBezier(remaining, localParameter);

        bool pieceOnLeft = signedDistanceToLine(pointOnBezierAtParameter(piece, 0.5), lineStart, lineEnd) > 0;
        if (pieceOnLeft == keepLeftOfLine)
            kept.append(piece);

        remaining = rest;
        consumedParameter = crossing.positionOnCurve;
    }
    bool tailOnLeft = signedDistanceToLine(pointOnBezierAtParameter(remaining, 0.5), lineStart, lineEnd) > 0;
    if (tailOnLeft == keepLeftOfLine)
        kept.append(remaining);

    return kept;
}

// Inclusive overlap test, zero-area boxes (e.g. an axis-aligned line) and touching edges count as overlapping.
static bool boxesTouchOrOverlap(const FloatRect& first, const FloatRect& second)
{
    return first.maxX() >= second.x() && first.x() <= second.maxX()
        && first.maxY() >= second.y() && first.y() <= second.maxY();
}

static FloatRect controlPointBounds(const BezierSegment& curve)
{
    FloatRect bounds { curve.start, FloatSize { } };
    bounds.extend(curve.controlPoint1);
    bounds.extend(curve.controlPoint2);
    bounds.extend(curve.end);
    return bounds;
}

// Boxes below this are treated as a point when locating a crossing, and the subdivision
// depth is capped in case a pair of curves overlaps rather than crossing cleanly.
constexpr float crossingTolerance = 1.0f / 256;
constexpr unsigned maximumCrossingDepth = 40;

static FloatRect monotonicBounds(const BezierSegment& curve)
{
    FloatRect bounds { curve.start, FloatSize { } };
    bounds.extend(curve.end);
    return bounds;
}

static FloatRect monotonicBounds(const Vector<BezierSegment>& curves)
{
    FloatRect bounds { curves.first().start, FloatSize { } };
    bounds.extend(curves.last().end);
    return bounds;
}

static bool boxesCanReach(FloatRect first, const FloatRect& second)
{
    first.inflate(crossingTolerance);
    return boxesTouchOrOverlap(first, second);
}

struct CurveParameterPair {
    float onFirst { 0 };
    float onSecond { 0 };
};

static void appendCurveCrossings(const BezierSegment& first, float firstStart, float firstEnd, const BezierSegment& second, float secondStart, float secondEnd, unsigned depth, Vector<CurveParameterPair>& crossings)
{
    auto firstBounds = monotonicBounds(first);
    auto secondBounds = monotonicBounds(second);
    if (!boxesTouchOrOverlap(firstBounds, secondBounds))
        return;

    float firstExtent = std::max(firstBounds.width(), firstBounds.height());
    float secondExtent = std::max(secondBounds.width(), secondBounds.height());

    float firstMiddle = (firstStart + firstEnd) / 2;
    float secondMiddle = (secondStart + secondEnd) / 2;

    if (depth >= maximumCrossingDepth || (firstExtent <= crossingTolerance && secondExtent <= crossingTolerance)) {
        crossings.append({ firstMiddle, secondMiddle });
        return;
    }

    if (firstExtent >= secondExtent) {
        auto [before, after] = splitBezier(first, 0.5f);
        appendCurveCrossings(before, firstStart, firstMiddle, second, secondStart, secondEnd, depth + 1, crossings);
        appendCurveCrossings(after, firstMiddle, firstEnd, second, secondStart, secondEnd, depth + 1, crossings);
        return;
    }

    auto [before, after] = splitBezier(second, 0.5f);
    appendCurveCrossings(first, firstStart, firstEnd, before, secondStart, secondMiddle, depth + 1, crossings);
    appendCurveCrossings(first, firstStart, firstEnd, after, secondMiddle, secondEnd, depth + 1, crossings);
}

static Vector<BezierSegment> curvesUpToIntersection(const Vector<BezierSegment>& curves, size_t index, float parameter)
{
    Vector<BezierSegment> kept;
    for (size_t position = 0; position < index; ++position)
        kept.append(curves[position]);

    if (parameter > parameterEpsilon)
        kept.append(splitBezier(curves[index], parameter).first);

    return kept;
}

static Vector<BezierSegment> curvesFromIntersection(const Vector<BezierSegment>& curves, size_t index, float parameter)
{
    Vector<BezierSegment> kept;
    if (parameter < 1.0f - parameterEpsilon)
        kept.append(splitBezier(curves[index], parameter).second);

    for (size_t position = index + 1; position < curves.size(); ++position)
        kept.append(curves[position]);

    return kept;
}

} // namespace

// `parameter` is the Bézier curve parameter t in [0, 1] (the variable of the Bernstein basis
// below, and of solveCubicForParametersInUnitInterval): t = 0 is curve.start, t = 1 is curve.end.
FloatPoint pointOnBezierAtParameter(const BezierSegment& curve, double parameter)
{
    double oneMinusParameter = 1.0 - parameter;
    double weightStart = oneMinusParameter * oneMinusParameter * oneMinusParameter;
    double weightControl1 = 3.0 * oneMinusParameter * oneMinusParameter * parameter;
    double weightControl2 = 3.0 * oneMinusParameter * parameter * parameter;
    double weightEnd = parameter * parameter * parameter;
    return {
        static_cast<float>(curve.start.x() * weightStart + curve.controlPoint1.x() * weightControl1 + curve.controlPoint2.x() * weightControl2 + curve.end.x() * weightEnd),
        static_cast<float>(curve.start.y() * weightStart + curve.controlPoint1.y() * weightControl1 + curve.controlPoint2.y() * weightControl2 + curve.end.y() * weightEnd),
    };
}

static void appendFlattenedBezier(Vector<FloatPoint>& points, const BezierSegment& curve, float tolerance, unsigned depth)
{
    // Deep enough that a curve spanning the whole viewport still flattens well within `tolerance`.
    constexpr unsigned maximumDepth = 16;

    auto chord = curve.end - curve.start;
    auto chordLength = chord.diagonalLength();
    auto distanceFromChord = [&](const FloatPoint& point) {
        auto startToPoint = point - curve.start;
        if (!chordLength)
            return startToPoint.diagonalLength();
        return std::abs(chord.width() * startToPoint.height() - chord.height() * startToPoint.width()) / chordLength;
    };

    bool isFlatEnough = distanceFromChord(curve.controlPoint1) <= tolerance && distanceFromChord(curve.controlPoint2) <= tolerance;
    if (isFlatEnough || depth >= maximumDepth) {
        points.append(curve.end);
        return;
    }

    auto [firstHalf, secondHalf] = splitBezier(curve, 0.5f);
    appendFlattenedBezier(points, firstHalf, tolerance, depth + 1);
    appendFlattenedBezier(points, secondHalf, tolerance, depth + 1);
}

void appendFlattenedBezier(Vector<FloatPoint>& points, const BezierSegment& curve, float tolerance)
{
    appendFlattenedBezier(points, curve, tolerance, 0);
}

Vector<BezierSegment> trimBezierToRect(const BezierSegment& curve, const FloatRect& rect)
{
    auto controlBounds = controlPointBounds(curve);

    // Inside -> no trim; disjoint -> empty. Inclusive test keeps zero-area (axis-aligned line) bounds.
    if (rect.contains(controlBounds))
        return { curve };
    if (!boxesTouchOrOverlap(controlBounds, rect))
        return { };

    FloatPoint center = rect.center();
    auto edges = rect.edges();

    Vector<BezierSegment> curves;
    curves.append(curve);
    for (auto side : allBoxSides) {
        const auto& edge = edges.at(side);
        bool keepCenterSide = signedDistanceToLine(center, edge.first, edge.second) > 0;
        Vector<BezierSegment> clipped;
        for (const auto& piece : curves) {
            for (const auto& keptCurve : trimBezierToLine(piece, edge.first, edge.second, keepCenterSide))
                clipped.append(keptCurve);
        }
        curves = clipped;
    }
    return curves;
}

std::optional<BezierCurvesIntersection> findMonotonicBezierCurvesIntersection(const Vector<BezierSegment>& first, const Vector<BezierSegment>& second)
{
    if (first.isEmpty() || second.isEmpty())
        return { };

    if (!boxesCanReach(monotonicBounds(first), monotonicBounds(second)))
        return { };

    std::optional<BezierCurvesIntersection> found;
    for (size_t firstIndex = 0; firstIndex < first.size(); ++firstIndex) {
        for (size_t secondIndex = 0; secondIndex < second.size(); ++secondIndex) {
            const auto& firstCurve = first[firstIndex];
            const auto& secondCurve = second[secondIndex];

            if (!boxesCanReach(monotonicBounds(firstCurve), monotonicBounds(secondCurve)))
                continue;

            Vector<CurveParameterPair> candidates;
            appendCurveCrossings(firstCurve, 0, 1, secondCurve, 0, 1, 0, candidates);

            for (auto candidate : candidates) {
                bool isLater = !found || firstIndex > found->indexOnFirst || (firstIndex == found->indexOnFirst && candidate.onFirst > found->parameterOnFirst);
                if (!isLater)
                    continue;

                found = BezierCurvesIntersection {
                    firstIndex, candidate.onFirst,
                    secondIndex, candidate.onSecond,
                    (firstIndex + candidate.onFirst) / static_cast<float>(first.size()),
                    (secondIndex + candidate.onSecond) / static_cast<float>(second.size()),
                };
            }
        }
    }

    return found;
}

void trimMonotonicBezierCurvesAtIntersection(Vector<BezierSegment>& first, Vector<BezierSegment>& second, const BezierCurvesIntersection& intersection)
{
    first = curvesUpToIntersection(first, intersection.indexOnFirst, intersection.parameterOnFirst);
    second = curvesFromIntersection(second, intersection.indexOnSecond, intersection.parameterOnSecond);
}

unsigned numberOfCrossingsWithSegment(const BezierSegment& curve, const FloatPoint& segmentStart, const FloatPoint& segmentEnd)
{
    return intersectBezierAndLine(curve, segmentStart, segmentEnd).size();
}

} // namespace WebCore
