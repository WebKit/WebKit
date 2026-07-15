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
#include "FloatSize.h"
#include "GeometryUtilities.h"
#include "Path.h"

#include <algorithm>
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

static Corner adjustCornerForInset(const Corner& original, double startInset, double endInset)
{
    if (isEmpty(original) || (startInset == 0.0 && endInset == 0.0))
        return original;

    if (isBevel(original)) {
        auto [cutStart, cutEnd] = buildBevelCorners(original, startInset, endInset);
        return { cutStart, original.outer, cutEnd, original.center, cornerRadii(original.center, original.outer), original.curvature, original.orientation };
    }

    if (isScoop(original))
        return buildScoopCorners(original, startInset, endInset);

    if (isRound(original))
        return buildRoundCorners(original, startInset, endInset);

    double strokeA = 0, strokeB = 0;
    if (isNotch(original)) {
        strokeA = -1;
        strokeB = 1;
    } else if (isSquare(original)) {
        strokeB = 1;
    }
    // TODO: implement inset for squircle (§3.9.4.2 hull direction).

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

static void addEllipticalArc(Path& path, const Corner& corner)
{
    float radiusX = corner.radii.width();
    float radiusY = corner.radii.height();

    // Border ate the whole radius: the inner corner collapses to a sharp vertex
    if (radiusX < limit || radiusY < limit) {
        path.addLineTo(corner.outer);
        path.addLineTo(corner.end);
        return;
    }

    float startAngle = eccentricAngle(corner.start, corner.center, radiusX, radiusY);
    float endAngle = eccentricAngle(corner.end, corner.center, radiusX, radiusY);

    float delta = std::remainder(endAngle - startAngle, 2.0f * std::numbers::pi_v<float>);
    auto direction = delta >= 0.0f ? RotationDirection::Clockwise : RotationDirection::Counterclockwise;

    path.addEllipse(corner.center, radiusX, radiusY, 0.0f, startAngle, startAngle + delta, direction);
}

static void addCurvedCorner(Path& path, const Corner& corner)
{
    if (isBevel(corner)) {
        path.addLineTo(corner.start);
        path.addLineTo(corner.end);
        return;
    }
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
    if (isRound(corner)) {
        path.addLineTo(corner.start);
        addEllipticalArc(path, corner);
        return;
    }
    if (isSquare(corner)) {
        path.addLineTo(corner.start);
        path.addLineTo(corner.outer);
        path.addLineTo(corner.end);
        return;
    }

    // TODO: squircle and the general superellipse curve
    path.addLineTo(corner.start);
    path.addLineTo(corner.outer);
    path.addLineTo(corner.end);
}

static void buildCorners(RectCorners<Corner>& corners, const RectCorners<CornerInput>& cornerRects)
{
    for (auto key : { BoxCorner::TopLeft, BoxCorner::TopRight, BoxCorner::BottomLeft, BoxCorner::BottomRight }) {
        auto& input = cornerRects[key];
        auto original = makeCorner(input);
        auto corner = adjustCornerForInset(original, input.startInset, input.endInset);
        corners[key] = corner;
    }
}

} // namespace

// https://drafts.csswg.org/css-borders-4/#contour-path
void borderContourPath(Path& path, const RectCorners<CornerInput>& cornerRects)
{
    RectCorners<Corner> corners;
    buildCorners(corners, cornerRects);
    path.moveTo(corners.topRight().start);
    for (auto key : { BoxCorner::TopRight, BoxCorner::BottomRight, BoxCorner::BottomLeft, BoxCorner::TopLeft })
        addCurvedCorner(path, corners[key]);
    path.closeSubpath();
}

// https://drafts.csswg.org/css-borders-4/#corner-shape-constrain-radii
double oppositeCornerScaleFactor(const RectCorners<CornerInput>&)
{
    // TODO: implement opposite-corner scale factor computation.
    return 1.0;
}

} // namespace WebCore
