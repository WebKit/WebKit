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

#include <cmath>
#include <utility>

namespace WebCore {

namespace {

struct Corner {
    FloatPoint start;
    FloatPoint outer;
    FloatPoint end;
    FloatPoint center;
    double curvature { 1.0 };
};

static bool isBevel(const Corner& corner) { return corner.curvature == 0.0; }
static bool isNotch(const Corner& corner) { return std::isinf(corner.curvature) && corner.curvature < 0.0; }
static bool isEmpty(const Corner& corner)
{
    return (corner.outer - corner.start).diagonalLength() < 1e-6f
        || (corner.end - corner.outer).diagonalLength() < 1e-6f;
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
        input.curvature
    };
}
static std::pair<FloatPoint, FloatPoint> bevelAxisAlignedCorners(const Corner& original, double startInset, double endInset)
{
    auto inwardNormal = (original.end - original.start).perpendicular().normalized();
    auto outerToCenter = original.center - original.outer;
    if (inwardNormal.width() * outerToCenter.width() + inwardNormal.height() * outerToCenter.height() < 0)
        inwardNormal = inwardNormal.scaled(-1);
    auto innerStart = original.start + inwardNormal * float(startInset);
    auto innerEnd = original.end + inwardNormal * float(endInset);

    auto extendStart = (original.center - original.start).directionScaledBy(float(startInset));
    auto extendEnd = (original.center - original.end).directionScaledBy(float(endInset));
    auto clipStart = original.start + extendStart;
    auto clipEnd = original.end + extendEnd;
    auto clipOuter = original.outer + extendStart + extendEnd;

    auto axisAlignedCornerStart = findIntersection(innerStart, innerEnd, clipStart, clipOuter).value_or(innerStart);
    auto axisAlignedCornerEnd = findIntersection(innerEnd, innerStart, clipEnd, clipOuter).value_or(innerEnd);

    return { axisAlignedCornerStart, axisAlignedCornerEnd };
}

static Corner adjustCornerForInset(const Corner& original, double startInset, double endInset)
{
    if (isEmpty(original) || (startInset == 0.0 && endInset == 0.0))
        return original;

    if (isBevel(original)) {
        auto [cutStart, cutEnd] = bevelAxisAlignedCorners(original, startInset, endInset);
        return { cutStart, original.outer, cutEnd, original.center, original.curvature };
    }
    double strokeA = 0, strokeB = 0;
    if (isNotch(original)) {
        strokeA = -1;
        strokeB = 1;
    }
    // TODO: implement inset for bevel, scoop, round, squircle, square (§3.9.4.2 hull direction).

    auto offset1 = (original.outer - original.start).directionScaledBy(float(startInset * strokeA));
    auto offset2 = (original.end - original.outer).directionScaledBy(float(startInset * strokeB));
    auto offset3 = (original.center - original.end).directionScaledBy(float(endInset * strokeB));
    auto offset4 = (original.start - original.center).directionScaledBy(float(endInset * strokeA));

    return {
        original.start + offset1 + offset2,
        original.outer + offset2 + offset3,
        original.end + offset3 + offset4,
        original.center + offset4 + offset1,
        original.curvature,
    };
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
    // TODO: round, squircle, square, and the general superellipse curve.
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
