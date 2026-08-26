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

#include "DoublePoint.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "RectEdges.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>

namespace WebCore {

namespace {

// Parameters within this of an interval boundary are snapped to it
constexpr double parameterEpsilon = 1e-7;
constexpr double nearZeroEpsilon = 1e-12;

// Curve/curve tolerances: geometric ones are fractions of a device pixel, parameter ones unitless.
// Two edges no further apart than this over a stretch are the same edge.
constexpr double coincidenceDistanceTolerance = 0.01;
// Below this the run is a tangency rather than a shared stretch.
constexpr double coincidenceSpanMinimum = 0.05;
constexpr unsigned maximumBisectionIterations = 24;
// Candidates this close in both parameters are the same crossing.
constexpr double crossingWeldTolerance = 1e-4;
// So are candidates whose points land this close.
constexpr double crossingWeldDistance = 0.05;
// Cross product of the unit tangents below this counts as parallel, i.e. a tangency.
constexpr double parallelTangentTolerance = 1e-3;
// A refined crossing is real only if the two points end up this close.
constexpr double crossingAcceptanceDistance = 0.1;
// Subdivision stops when a piece's control points fit in a box this size
constexpr double subdivisionSizeLimit = crossingWeldDistance;

constexpr unsigned maximumSubdivisionDepth = 40;
constexpr unsigned maximumCandidateCount = 96;

// Samples along a curve are spaced by size on screen, within these bounds on the count.
constexpr double sampleSpacing = 1.0;
constexpr unsigned minimumSampleCount = 16;
constexpr unsigned maximumSampleCount = 48;

constexpr unsigned maximumNewtonIterations = 16;
constexpr unsigned maximumTangentialProjections = 24;
constexpr unsigned refinementWalkAllowance = 8;

// Returns the position on the curve at `parameter` in double. `parameter` is the Bézier curve parameter t in [0, 1]
DoublePoint precisePointAtParameter(const BezierSegment& curve, double parameter)
{
    double oneMinusParameter = 1.0 - parameter;
    double weightStart = oneMinusParameter * oneMinusParameter * oneMinusParameter;
    double weightControl1 = 3.0 * oneMinusParameter * oneMinusParameter * parameter;
    double weightControl2 = 3.0 * oneMinusParameter * parameter * parameter;
    double weightEnd = parameter * parameter * parameter;
    return {
        curve.start.x() * weightStart + curve.controlPoint1.x() * weightControl1 + curve.controlPoint2.x() * weightControl2 + curve.end.x() * weightEnd,
        curve.start.y() * weightStart + curve.controlPoint1.y() * weightControl1 + curve.controlPoint2.y() * weightControl2 + curve.end.y() * weightEnd,
    };
}

// Returns the direction of travel at `parameter`, magnitude included in double. The derivative of the Bernstein form, so it weights the legs between control points rather than the points.
DoubleSize preciseTangentAtParameter(const BezierSegment& curve, double parameter)
{
    double oneMinusParameter = 1.0 - parameter;
    double weightFirstLeg = 3.0 * oneMinusParameter * oneMinusParameter;
    double weightMiddleLeg = 6.0 * oneMinusParameter * parameter;
    double weightLastLeg = 3.0 * parameter * parameter;
    return {
        weightFirstLeg * (curve.controlPoint1.x() - curve.start.x()) + weightMiddleLeg * (curve.controlPoint2.x() - curve.controlPoint1.x()) + weightLastLeg * (curve.end.x() - curve.controlPoint2.x()),
        weightFirstLeg * (curve.controlPoint1.y() - curve.start.y()) + weightMiddleLeg * (curve.controlPoint2.y() - curve.controlPoint1.y()) + weightLastLeg * (curve.end.y() - curve.controlPoint2.y()),
    };
}

FloatRect controlPointBounds(const BezierSegment& curve)
{
    FloatRect bounds { curve.start, FloatSize { } };
    bounds.extend(curve.controlPoint1);
    bounds.extend(curve.controlPoint2);
    bounds.extend(curve.end);
    return bounds;
}

// How many samples to walk along this curve
unsigned sampleCountForCurve(const BezierSegment& curve)
{
    auto bounds = controlPointBounds(curve);
    double extent = std::max(bounds.width(), bounds.height());
    if (!std::isfinite(extent))
        return minimumSampleCount;
    double wanted = std::clamp(std::ceil(extent / sampleSpacing), static_cast<double>(minimumSampleCount), static_cast<double>(maximumSampleCount));
    return static_cast<unsigned>(wanted);
}

// Halving a window of this radius down to parameterEpsilon takes this many steps
unsigned refinementBudget(double windowRadius)
{
    if (!(windowRadius > parameterEpsilon))
        return refinementWalkAllowance;
    return static_cast<unsigned>(std::ceil(std::log2(windowRadius / parameterEpsilon))) + refinementWalkAllowance;
}

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

Vector<BezierIntersection, 3> intersectBezierAndLineInternal(const BezierSegment& curve, const FloatPoint& lineStart, const FloatPoint& lineEnd)
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

    Vector<BezierIntersection, 3> intersections;
    for (double parameter : parameters) {
        auto point = pointOnBezierAtParameter(curve, parameter);

        // Parameter of the crossing along the (finite) line segment.
        double positionOnLine = lineLengthSquared > 0
            ? ((point.x() - lineStart.x()) * directionX + (point.y() - lineStart.y()) * directionY) / lineLengthSquared
            : 0.0;

        // Keep only crossings that fall within the segment as well as the curve.
        if (positionOnLine < -parameterEpsilon || positionOnLine > 1.0 + parameterEpsilon)
            continue;

        auto tangent = preciseTangentAtParameter(curve, parameter);
        double tangentLength = tangent.diagonalLength();
        double lineLength = std::sqrt(lineLengthSquared);
        bool tangential = tangentLength > nearZeroEpsilon && lineLength > nearZeroEpsilon
            && std::abs(tangent.width() * directionY - tangent.height() * directionX) / (tangentLength * lineLength) < parallelTangentTolerance;

        intersections.append({ parameter, std::clamp(positionOnLine, 0.0, 1.0), point, tangential });
    }

    std::sort(intersections.begin(), intersections.end(), [](const BezierIntersection& lhs, const BezierIntersection& rhs) {
        return lhs.parameterOnFirst < rhs.parameterOnFirst;
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
    auto crossings = intersectBezierAndLineInternal(curve, lineStart, lineEnd);

    // Between successive crossings the pieces alternate sides of the line, so keep those whose midpoint is on the requested side.
    Vector<BezierSegment> kept;
    auto remaining = curve;
    float consumedParameter = 0.0f;
    for (const auto& crossing : crossings) {
        // Re-map the crossing parameter into the shrinking `remaining` curve's own [0, 1] range.
        float localParameter = (static_cast<float>(crossing.parameterOnFirst) - consumedParameter) / (1.0f - consumedParameter);
        localParameter = std::clamp(localParameter, 0.0f, 1.0f);
        auto [piece, rest] = splitBezier(remaining, localParameter);

        bool pieceOnLeft = signedDistanceToLine(pointOnBezierAtParameter(piece, 0.5), lineStart, lineEnd) > 0;
        if (pieceOnLeft == keepLeftOfLine)
            kept.append(piece);

        remaining = rest;
        consumedParameter = static_cast<float>(crossing.parameterOnFirst);
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

// Restricts a curve to a parameter window: de Casteljau at the window start, then at the end
// remapped into what remains.
BezierSegment subCurveForWindow(const BezierSegment& curve, double startParameter, double endParameter)
{
    auto afterStart = splitBezier(curve, static_cast<float>(startParameter)).second;
    double remainingSpan = 1.0 - startParameter;
    if (remainingSpan <= nearZeroEpsilon)
        return { curve.end, curve.end, curve.end, curve.end };
    double localEnd = std::clamp((endParameter - startParameter) / remainingSpan, 0.0, 1.0);
    return splitBezier(afterStart, static_cast<float>(localEnd)).first;
}

// Fractions along two segments where their infinite lines meet; false when parallel.
bool chordCrossingFractions(const FloatPoint& firstStart, const FloatPoint& firstEnd, const FloatPoint& secondStart, const FloatPoint& secondEnd, double& fractionOnFirst, double& fractionOnSecond)
{
    double firstDeltaX = firstEnd.x() - firstStart.x();
    double firstDeltaY = firstEnd.y() - firstStart.y();
    double secondDeltaX = secondEnd.x() - secondStart.x();
    double secondDeltaY = secondEnd.y() - secondStart.y();

    double denominator = firstDeltaX * secondDeltaY - firstDeltaY * secondDeltaX;
    if (std::abs(denominator) < nearZeroEpsilon)
        return false;

    double offsetX = secondStart.x() - firstStart.x();
    double offsetY = secondStart.y() - firstStart.y();
    fractionOnFirst = std::clamp((offsetX * secondDeltaY - offsetY * secondDeltaX) / denominator, 0.0, 1.0);
    fractionOnSecond = std::clamp((offsetX * firstDeltaY - offsetY * firstDeltaX) / denominator, 0.0, 1.0);
    return true;
}

double nearestParameterOnBezier(const BezierSegment& curve, const DoublePoint& target, double& distanceOut)
{
    const unsigned sweepSteps = sampleCountForCurve(curve);
    double bestParameter = 0;
    double bestDistance = std::numeric_limits<double>::max();
    for (unsigned step = 0; step <= sweepSteps; ++step) {
        double parameter = static_cast<double>(step) / sweepSteps;
        double distance = (precisePointAtParameter(curve, parameter) - target).diagonalLength();
        if (distance < bestDistance) {
            bestDistance = distance;
            bestParameter = parameter;
        }
    }

    double windowRadius = 1.0 / sweepSteps;
    const unsigned maximumRefinements = refinementBudget(windowRadius);
    for (unsigned refinement = 0; refinement < maximumRefinements; ++refinement) {
        double lowerProbe = std::clamp(bestParameter - windowRadius, 0.0, 1.0);
        double upperProbe = std::clamp(bestParameter + windowRadius, 0.0, 1.0);
        double lowerDistance = (precisePointAtParameter(curve, lowerProbe) - target).diagonalLength();
        double upperDistance = (precisePointAtParameter(curve, upperProbe) - target).diagonalLength();
        if (lowerDistance < bestDistance && lowerDistance <= upperDistance) {
            bestDistance = lowerDistance;
            bestParameter = lowerProbe;
        } else if (upperDistance < bestDistance) {
            bestDistance = upperDistance;
            bestParameter = upperProbe;
        } else
            windowRadius /= 2;
        if (windowRadius < parameterEpsilon)
            break;
    }

    distanceOut = bestDistance;
    return bestParameter;
}

static bool tangentsAreParallel(const DoubleSize& first, const DoubleSize& second)
{
    double firstLength = first.diagonalLength();
    double secondLength = second.diagonalLength();
    if (firstLength <= nearZeroEpsilon || secondLength <= nearZeroEpsilon)
        return true;
    return std::abs(first.width() * second.height() - first.height() * second.width()) / (firstLength * secondLength) < parallelTangentTolerance;
}

// Returns the longest stretch where the two curves lie on top of each other rather than crossing
std::optional<BezierCoincidentSpan> longestCoincidentSpan(const BezierSegment& first, const BezierSegment& second)
{
    const unsigned sampleCount = sampleCountForCurve(first);

    unsigned runStart = 0;
    unsigned runLength = 0;
    unsigned bestRunStart = 0;
    unsigned bestRunLength = 0;
    std::array<double, maximumSampleCount + 1> matchedParameters { };

    for (unsigned sample = 0; sample <= sampleCount; ++sample) {
        double parameterOnFirst = static_cast<double>(sample) / sampleCount;
        double distance = 0;
        matchedParameters[sample] = nearestParameterOnBezier(second, precisePointAtParameter(first, parameterOnFirst), distance);

        bool sharesDirection = tangentsAreParallel(preciseTangentAtParameter(first, parameterOnFirst),
            preciseTangentAtParameter(second, matchedParameters[sample]));

        if (distance <= coincidenceDistanceTolerance && sharesDirection) {
            if (!runLength)
                runStart = sample;
            ++runLength;
            if (runLength > bestRunLength) {
                bestRunLength = runLength;
                bestRunStart = runStart;
            }
        } else
            runLength = 0;
    }

    if (bestRunLength < 2)
        return std::nullopt;

    auto coincidentAt = [&](double parameterOnFirst, double& matchedParameter) {
        double distance = 0;
        matchedParameter = nearestParameterOnBezier(second, precisePointAtParameter(first, parameterOnFirst), distance);
        if (distance > coincidenceDistanceTolerance)
            return false;
        return tangentsAreParallel(preciseTangentAtParameter(first, parameterOnFirst), preciseTangentAtParameter(second, matchedParameter));
    };

    auto refinedBoundary = [&](unsigned insideSample, int outsideSample) {
        double inside = static_cast<double>(insideSample) / sampleCount;
        if (outsideSample < 0 || static_cast<unsigned>(outsideSample) > sampleCount)
            return inside;

        double outside = static_cast<double>(outsideSample) / sampleCount;
        for (unsigned iteration = 0; iteration < maximumBisectionIterations; ++iteration) {
            double middle = (inside + outside) / 2;
            double matchedParameter = 0;
            if (coincidentAt(middle, matchedParameter))
                inside = middle;
            else
                outside = middle;
        }
        return inside;
    };

    unsigned lastSample = bestRunStart + bestRunLength - 1;
    double firstStart = bestRunStart ? refinedBoundary(bestRunStart, static_cast<int>(bestRunStart) - 1) : 0.0;
    double firstEnd = lastSample < sampleCount ? refinedBoundary(lastSample, static_cast<int>(lastSample) + 1) : 1.0;
    if (firstEnd - firstStart < coincidenceSpanMinimum)
        return std::nullopt;

    double secondStart = matchedParameters[bestRunStart];
    double secondEnd = matchedParameters[lastSample];
    coincidentAt(firstStart, secondStart);
    coincidentAt(firstEnd, secondEnd);

    return BezierCoincidentSpan { firstStart, firstEnd, secondStart, secondEnd, secondEnd < secondStart };
}

// Resolves a rough candidate pair of parameters into an exact crossing, or rejects it. Returns the crossing with a
// parameter on each curve, the point where they meet and whether it is a tangency (curves touch without crossing)
std::optional<BezierIntersection> resolveCrossingCandidate(const BezierSegment& first, const BezierSegment& second, double parameterOnFirst, double parameterOnSecond)
{
    for (unsigned iteration = 0; iteration < maximumNewtonIterations; ++iteration) {
        auto firstPoint = precisePointAtParameter(first, parameterOnFirst);
        auto secondPoint = precisePointAtParameter(second, parameterOnSecond);
        auto gap = firstPoint - secondPoint;
        if (gap.diagonalLength() <= nearZeroEpsilon)
            break;

        auto firstTangent = preciseTangentAtParameter(first, parameterOnFirst);
        auto secondTangent = preciseTangentAtParameter(second, parameterOnSecond);

        double determinant = -firstTangent.width() * secondTangent.height() + firstTangent.height() * secondTangent.width();
        if (std::abs(determinant) < nearZeroEpsilon) {
            // Tangential: walk each curve onto the other until the pair stops moving.
            for (unsigned projection = 0; projection < maximumTangentialProjections; ++projection) {
                double distance = 0;
                double nextOnSecond = nearestParameterOnBezier(second, precisePointAtParameter(first, parameterOnFirst), distance);
                double nextOnFirst = nearestParameterOnBezier(first, precisePointAtParameter(second, nextOnSecond), distance);
                bool settled = std::abs(nextOnFirst - parameterOnFirst) < parameterEpsilon
                    && std::abs(nextOnSecond - parameterOnSecond) < parameterEpsilon;
                parameterOnFirst = nextOnFirst;
                parameterOnSecond = nextOnSecond;
                if (settled)
                    break;
            }
            break;
        }

        double deltaFirst = (gap.width() * secondTangent.height() - secondTangent.width() * gap.height()) / determinant;
        double deltaSecond = (-firstTangent.width() * gap.height() + firstTangent.height() * gap.width()) / determinant;

        double nextParameterOnFirst = std::clamp(parameterOnFirst + deltaFirst, 0.0, 1.0);
        double nextParameterOnSecond = std::clamp(parameterOnSecond + deltaSecond, 0.0, 1.0);
        bool converged = std::abs(nextParameterOnFirst - parameterOnFirst) < parameterEpsilon
            && std::abs(nextParameterOnSecond - parameterOnSecond) < parameterEpsilon;
        parameterOnFirst = nextParameterOnFirst;
        parameterOnSecond = nextParameterOnSecond;
        if (converged)
            break;
    }

    auto firstPoint = precisePointAtParameter(first, parameterOnFirst);
    auto secondPoint = precisePointAtParameter(second, parameterOnSecond);
    if ((firstPoint - secondPoint).diagonalLength() > crossingAcceptanceDistance)
        return std::nullopt;

    bool tangential = tangentsAreParallel(preciseTangentAtParameter(first, parameterOnFirst), preciseTangentAtParameter(second, parameterOnSecond));

    FloatPoint meetingPoint {
        static_cast<float>((firstPoint.x() + secondPoint.x()) / 2),
        static_cast<float>((firstPoint.y() + secondPoint.y()) / 2),
    };
    return BezierIntersection { parameterOnFirst, parameterOnSecond, meetingPoint, tangential };
}

void collectCrossingCandidates(const BezierSegment& first, double firstStart, double firstEnd, const BezierSegment& second, double secondStart, double secondEnd, unsigned depth, Vector<BezierIntersection, 9>& candidates)
{
    if (candidates.size() >= maximumCandidateCount)
        return;

    auto firstPiece = subCurveForWindow(first, firstStart, firstEnd);
    auto secondPiece = subCurveForWindow(second, secondStart, secondEnd);
    auto firstBounds = controlPointBounds(firstPiece);
    auto secondBounds = controlPointBounds(secondPiece);
    if (!boxesTouchOrOverlap(firstBounds, secondBounds))
        return;

    double firstSpan = firstEnd - firstStart;
    double secondSpan = secondEnd - secondStart;

    auto boundsAreSmallEnough = [](const FloatRect& bounds) {
        return std::max(bounds.width(), bounds.height()) <= subdivisionSizeLimit;
    };

    if (depth >= maximumSubdivisionDepth || (boundsAreSmallEnough(firstBounds) && boundsAreSmallEnough(secondBounds))) {
        double fractionOnFirst = 0.5;
        double fractionOnSecond = 0.5;
        chordCrossingFractions(firstPiece.start, firstPiece.end, secondPiece.start, secondPiece.end, fractionOnFirst, fractionOnSecond);
        candidates.append({ firstStart + firstSpan * fractionOnFirst, secondStart + secondSpan * fractionOnSecond, { }, false });
        return;
    }

    if (firstSpan >= secondSpan) {
        double middle = (firstStart + firstEnd) / 2;
        collectCrossingCandidates(first, firstStart, middle, second, secondStart, secondEnd, depth + 1, candidates);
        collectCrossingCandidates(first, middle, firstEnd, second, secondStart, secondEnd, depth + 1, candidates);
        return;
    }
    double middle = (secondStart + secondEnd) / 2;
    collectCrossingCandidates(first, firstStart, firstEnd, second, secondStart, middle, depth + 1, candidates);
    collectCrossingCandidates(first, firstStart, firstEnd, second, middle, secondEnd, depth + 1, candidates);
}

} // namespace

// `parameter` is the Bézier curve parameter t in [0, 1] (the variable of the Bernstein basis, and of
// solveCubicForParametersInUnitInterval): t = 0 is curve.start, t = 1 is curve.end.
FloatPoint pointOnBezierAtParameter(const BezierSegment& curve, double parameter)
{
    auto point = precisePointAtParameter(curve, parameter);
    return { static_cast<float>(point.x()), static_cast<float>(point.y()) };
}


// Direction of travel, magnitude included.
FloatSize tangentOnBezierAtParameter(const BezierSegment& curve, double parameter)
{
    auto tangent = preciseTangentAtParameter(curve, parameter);
    return { static_cast<float>(tangent.width()), static_cast<float>(tangent.height()) };
}

Vector<BezierIntersection, 3> intersectBezierAndLine(const BezierSegment& curve, const FloatPoint& lineStart, const FloatPoint& lineEnd)
{
    return intersectBezierAndLineInternal(curve, lineStart, lineEnd);
}

// Crossings come back as a parameter pair each; stretches where the curves run along each other come
// back separately, having no single point to split at.
BezierIntersections intersectBeziers(const BezierSegment& first, const BezierSegment& second)
{
    BezierIntersections result;

    if (!boxesTouchOrOverlap(controlPointBounds(first), controlPointBounds(second)))
        return result;

    if (auto coincidentSpan = longestCoincidentSpan(first, second))
        result.coincidences.append(*coincidentSpan);

    Vector<BezierIntersection, 9> candidates;

    auto seedEndpointPair = [&](double parameterOnFirst, double parameterOnSecond) {
        if ((precisePointAtParameter(first, parameterOnFirst) - precisePointAtParameter(second, parameterOnSecond)).diagonalLength() <= crossingWeldDistance)
            candidates.append({ parameterOnFirst, parameterOnSecond, { }, false });
    };
    seedEndpointPair(0, 0);
    seedEndpointPair(0, 1);
    seedEndpointPair(1, 0);
    seedEndpointPair(1, 1);

    collectCrossingCandidates(first, 0, 1, second, 0, 1, 0, candidates);

    for (const auto& candidate : candidates) {
        auto refined = resolveCrossingCandidate(first, second, candidate.parameterOnFirst, candidate.parameterOnSecond);
        if (!refined)
            continue;

        bool insideCoincidence = false;
        for (const auto& span : result.coincidences) {
            if (refined->parameterOnFirst >= span.firstStart - crossingWeldTolerance && refined->parameterOnFirst <= span.firstEnd + crossingWeldTolerance) {
                insideCoincidence = true;
                break;
            }
        }
        if (insideCoincidence)
            continue;

        bool alreadyFound = false;
        for (auto& existing : result.crossings) {
            bool sameParameters = std::abs(existing.parameterOnFirst - refined->parameterOnFirst) <= crossingWeldTolerance
                && std::abs(existing.parameterOnSecond - refined->parameterOnSecond) <= crossingWeldTolerance;
            bool samePoint = std::hypot(existing.point.x() - refined->point.x(), existing.point.y() - refined->point.y()) <= crossingWeldDistance;
            if (sameParameters || samePoint) {
                // Tangential only if every candidate that landed here says so.
                existing.tangential = existing.tangential && refined->tangential;
                alreadyFound = true;
                break;
            }
        }
        if (alreadyFound)
            continue;

        result.crossings.append(*refined);
        if (result.crossings.size() >= 9)
            break;
    }

    std::sort(result.crossings.begin(), result.crossings.end(), [](const BezierIntersection& lhs, const BezierIntersection& rhs) {
        return lhs.parameterOnFirst < rhs.parameterOnFirst;
    });
    return result;
}

Vector<BezierSegment> splitBezierAtParameters(const BezierSegment& curve, const Vector<double>& parameters)
{
    Vector<double> cuts;
    cuts.reserveInitialCapacity(parameters.size());
    for (double parameter : parameters) {
        if (parameter > parameterEpsilon && parameter < 1.0 - parameterEpsilon)
            cuts.append(parameter);
    }
    std::sort(cuts.begin(), cuts.end());

    Vector<BezierSegment> pieces;
    auto remaining = curve;
    double consumedParameter = 0;
    for (double cut : cuts) {
        if (cut - consumedParameter <= parameterEpsilon)
            continue;
        // Remap the cut into the remainder's own [0, 1] range.
        double localParameter = std::clamp((cut - consumedParameter) / (1.0 - consumedParameter), 0.0, 1.0);
        auto [piece, rest] = splitBezier(remaining, static_cast<float>(localParameter));
        pieces.append(piece);
        remaining = rest;
        consumedParameter = cut;
    }
    pieces.append(remaining);
    return pieces;
}

Vector<BezierSegment> trimBezierToRect(const BezierSegment& curve, const FloatRect& rect)
{
    // The curve stays within its control points' convex hull
    FloatRect controlBounds { curve.start, FloatSize { } };
    controlBounds.extend(curve.controlPoint1);
    controlBounds.extend(curve.controlPoint2);
    controlBounds.extend(curve.end);

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

namespace {

constexpr double loopStitchTolerance = 0.05; // device pixels
constexpr double loopPieceMinimumLength = 1e-4;
// Pieces this close to each other's start leave the same point
constexpr double loopJunctionTolerance = 1e-3;

double doubledSignedArea(const BezierLoop& loop)
{
    double total = 0;
    for (const auto& curve : loop)
        total += curve.start.x() * curve.end.y() - curve.end.x() * curve.start.y();
    return total;
}

BezierSegment reversedSegment(const BezierSegment& curve)
{
    return { curve.end, curve.controlPoint2, curve.controlPoint1, curve.start };
}

BezierLoop reversedLoop(const BezierLoop& loop)
{
    BezierLoop reversed;
    reversed.reserveInitialCapacity(loop.size());
    for (size_t index = loop.size(); index-- > 0;)
        reversed.append(reversedSegment(loop[index]));
    return reversed;
}

bool isDegenerateSegment(const BezierSegment& curve)
{
    auto span = curve.end - curve.start;
    if (span.diagonalLength() > loopPieceMinimumLength)
        return false;
    return (curve.controlPoint1 - curve.start).diagonalLength() <= loopPieceMinimumLength
        && (curve.controlPoint2 - curve.start).diagonalLength() <= loopPieceMinimumLength;
}

// A stretch two loops share rather than cross, as parameters along one segment of one of them.
struct CoincidentRange {
    double start { 0 };
    double end { 0 };
    bool opposedDirection { false };
};

struct LoopPiece {
    BezierSegment curve;
    size_t segmentIndex { 0 };
    // Where the piece sits in its original segment, so it can be matched against that segment's shared stretches.
    double midpointParameter { 0 };
};

// Splits every segment of `loop` at the parameters collected for it, keeping the traversal order.
Vector<LoopPiece> splitLoopAtParameters(const BezierLoop& loop, const Vector<Vector<double>>& parametersPerSegment)
{
    Vector<LoopPiece> pieces;
    for (size_t index = 0; index < loop.size(); ++index) {
        auto parameters = parametersPerSegment[index];
        std::sort(parameters.begin(), parameters.end());

        Vector<double> boundaries;
        for (double parameter : parameters) {
            if (parameter <= parameterEpsilon || parameter >= 1.0 - parameterEpsilon)
                continue;
            if (!boundaries.isEmpty() && parameter - boundaries.last() <= parameterEpsilon)
                continue;
            boundaries.append(parameter);
        }

        auto split = splitBezierAtParameters(loop[index], boundaries);
        ASSERT(split.size() == boundaries.size() + 1);

        double previousParameter = 0;
        for (size_t pieceIndex = 0; pieceIndex < split.size(); ++pieceIndex) {
            double nextParameter = pieceIndex < boundaries.size() ? boundaries[pieceIndex] : 1.0;
            if (!isDegenerateSegment(split[pieceIndex]))
                pieces.append({ split[pieceIndex], index, (previousParameter + nextParameter) / 2 });
            previousParameter = nextParameter;
        }
    }
    return pieces;
}

// Whether a piece lies along a stretch the two loops share, and if so which way the other loop runs there.
std::optional<bool> sharedStretchIsOpposed(const Vector<CoincidentRange>& ranges, double midpointParameter)
{
    for (const auto& range : ranges) {
        if (midpointParameter >= range.start - parameterEpsilon && midpointParameter <= range.end + parameterEpsilon)
            return range.opposedDirection;
    }
    return std::nullopt;
}

Vector<BezierLoop> stitchPiecesIntoLoops(Vector<BezierSegment>&& pieces)
{
    Vector<bool> used(FillWith { }, pieces.size(), false);
    Vector<BezierLoop> loops;

    for (size_t startIndex = 0; startIndex < pieces.size(); ++startIndex) {
        if (used[startIndex])
            continue;

        BezierLoop loop;
        loop.append(pieces[startIndex]);
        used[startIndex] = true;

        while (true) {
            auto tail = loop.last().end;
            if ((tail - loop.first().start).diagonalLength() <= loopStitchTolerance)
                break;

            double nearestDistance = loopStitchTolerance;
            for (size_t index = 0; index < pieces.size(); ++index) {
                if (used[index])
                    continue;
                nearestDistance = std::min<double>(nearestDistance, (pieces[index].start - tail).diagonalLength());
            }

            auto arriving = tangentOnBezierAtParameter(loop.last(), 1);
            std::optional<size_t> nextIndex;
            double widestTurn = -std::numeric_limits<double>::max();
            for (size_t index = 0; index < pieces.size(); ++index) {
                if (used[index])
                    continue;
                double distance = (pieces[index].start - tail).diagonalLength();
                if (distance > nearestDistance + loopJunctionTolerance || distance > loopStitchTolerance)
                    continue;

                auto leaving = tangentOnBezierAtParameter(pieces[index], 0);
                double across = arriving.width() * leaving.height() - arriving.height() * leaving.width();
                double along = arriving.width() * leaving.width() + arriving.height() * leaving.height();
                double turn = std::atan2(across, along);
                if (turn > widestTurn) {
                    widestTurn = turn;
                    nextIndex = index;
                }
            }
            if (!nextIndex)
                break;

            auto next = pieces[*nextIndex];
            auto shift = tail - next.start;
            next.start = tail;
            next.controlPoint1 = next.controlPoint1 + shift;

            loop.append(next);
            used[*nextIndex] = true;
        }

        if (!loop.isEmpty()) {
            auto& last = loop.last();
            auto shift = loop.first().start - last.end;
            if (shift.diagonalLength() <= loopStitchTolerance) {
                last.end = loop.first().start;
                last.controlPoint2 = last.controlPoint2 + shift;
            }
        }

        bool closed = (loop.last().end - loop.first().start).diagonalLength() <= loopStitchTolerance;
        if (closed && loop.size() > 1)
            loops.append(WTF::move(loop));
    }
    return loops;
}

bool loopContainsPoint(const BezierLoop& loop, const FloatPoint& point)
{
    return windingNumberForLoop(loop, point);
}

} // namespace

int windingNumberForLoop(const BezierLoop& loop, const FloatPoint& point)
{
    FloatRect bounds { point, FloatSize { } };
    for (const auto& curve : loop) {
        bounds.extend(curve.start);
        bounds.extend(curve.controlPoint1);
        bounds.extend(curve.controlPoint2);
        bounds.extend(curve.end);
    }
    FloatPoint rayEnd { bounds.maxX() + 1.0f, point.y() };

    int winding = 0;
    for (size_t index = 0; index < loop.size(); ++index) {
        const auto& curve = loop[index];
        for (const auto& crossing : intersectBezierAndLine(curve, point, rayEnd)) {
            if (crossing.parameterOnFirst <= parameterEpsilon)
                continue;
            if (crossing.parameterOnSecond <= 0.0)
                continue;

            auto tangent = tangentOnBezierAtParameter(curve, crossing.parameterOnFirst);
            if (!tangent.height())
                continue;

            if (crossing.parameterOnFirst >= 1.0 - parameterEpsilon) {
                auto leaving = tangentOnBezierAtParameter(loop[(index + 1) % loop.size()], 0);
                if (leaving.height() && (leaving.height() > 0) != (tangent.height() > 0))
                    continue;
            }

            if (tangent.height() > 0)
                ++winding;
            else
                --winding;
        }
    }
    return winding;
}

Vector<BezierLoop> subtractLoopFromLoop(const BezierLoop& region, const BezierLoop& sliver)
{
    if (region.isEmpty())
        return { };
    if (sliver.isEmpty())
        return { region };

    if ((doubledSignedArea(region) > 0) != (doubledSignedArea(sliver) > 0))
        return subtractLoopFromLoop(region, reversedLoop(sliver));

    Vector<Vector<double>> regionParameters(region.size());
    Vector<Vector<double>> sliverParameters(sliver.size());
    Vector<Vector<CoincidentRange>> regionShared(region.size());
    Vector<Vector<CoincidentRange>> sliverShared(sliver.size());

    struct SharedStretch {
        size_t regionIndex { 0 };
        size_t sliverIndex { 0 };
        BezierCoincidentSpan span;
    };
    Vector<SharedStretch> sharedStretches;

    bool haveIntersection = false;
    for (size_t regionIndex = 0; regionIndex < region.size(); ++regionIndex) {
        for (size_t sliverIndex = 0; sliverIndex < sliver.size(); ++sliverIndex) {
            auto intersections = intersectBeziers(region[regionIndex], sliver[sliverIndex]);
            for (const auto& crossing : intersections.crossings) {
                if (crossing.tangential)
                    continue;
                regionParameters[regionIndex].append(crossing.parameterOnFirst);
                sliverParameters[sliverIndex].append(crossing.parameterOnSecond);
                haveIntersection = true;
            }
            for (const auto& shared : intersections.coincidences) {
                sharedStretches.append({ regionIndex, sliverIndex, shared });
                haveIntersection = true;
            }
        }
    }

    auto snappedToCrossing = [](const BezierSegment& curve, double parameter, const Vector<double>& crossings) {
        auto atParameter = pointOnBezierAtParameter(curve, parameter);
        for (double crossing : crossings) {
            if ((pointOnBezierAtParameter(curve, crossing) - atParameter).diagonalLength() <= 2 * coincidenceDistanceTolerance)
                return crossing;
        }
        return parameter;
    };

    for (const auto& stretch : sharedStretches) {
        const auto& regionCurve = region[stretch.regionIndex];
        const auto& sliverCurve = sliver[stretch.sliverIndex];

        double regionStart = snappedToCrossing(regionCurve, stretch.span.firstStart, regionParameters[stretch.regionIndex]);
        double regionEnd = snappedToCrossing(regionCurve, stretch.span.firstEnd, regionParameters[stretch.regionIndex]);
        double sliverStart = snappedToCrossing(sliverCurve, stretch.span.secondStart, sliverParameters[stretch.sliverIndex]);
        double sliverEnd = snappedToCrossing(sliverCurve, stretch.span.secondEnd, sliverParameters[stretch.sliverIndex]);

        regionParameters[stretch.regionIndex].appendVector(Vector<double> { regionStart, regionEnd });
        sliverParameters[stretch.sliverIndex].appendVector(Vector<double> { sliverStart, sliverEnd });
        regionShared[stretch.regionIndex].append({ std::min(regionStart, regionEnd), std::max(regionStart, regionEnd), stretch.span.opposedDirection });
        sliverShared[stretch.sliverIndex].append({ std::min(sliverStart, sliverEnd), std::max(sliverStart, sliverEnd), stretch.span.opposedDirection });
    }

    if (!haveIntersection) {
        if (loopContainsPoint(sliver, region.first().start))
            return { };
        if (!loopContainsPoint(region, sliver.first().start))
            return { region };
        return { region, reversedLoop(sliver) };
    }

    auto regionPieces = splitLoopAtParameters(region, regionParameters);
    auto sliverPieces = splitLoopAtParameters(sliver, sliverParameters);

    Vector<BezierSegment> kept;
    for (const auto& piece : regionPieces) {
        if (auto opposed = sharedStretchIsOpposed(regionShared[piece.segmentIndex], piece.midpointParameter)) {
            if (*opposed)
                kept.append(piece.curve);
            continue;
        }
        if (!loopContainsPoint(sliver, pointOnBezierAtParameter(piece.curve, 0.5)))
            kept.append(piece.curve);
    }
    for (const auto& piece : sliverPieces) {
        if (sharedStretchIsOpposed(sliverShared[piece.segmentIndex], piece.midpointParameter))
            continue;
        if (loopContainsPoint(region, pointOnBezierAtParameter(piece.curve, 0.5)))
            kept.append(reversedSegment(piece.curve));
    }

    return stitchPiecesIntoLoops(WTF::move(kept));
}

} // namespace WebCore
