/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "BorderShape.h"

#include "AffineTransform.h"
#include "BorderData.h"
#include "CornerShapeUtilities.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "HitTestLocation.h"
#include "LayoutRect.h"
#include "LayoutRoundedRect.h"
#include "Path.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <algorithm>
#include <cmath>

namespace WebCore {

static void zeroRadiiForOpenEdges(LayoutRoundedRectRadii& radii, RectEdges<bool> closedEdges)
{
    if (!closedEdges.top()) {
        radii.setTopLeft({ });
        radii.setTopRight({ });
    }
    if (!closedEdges.right()) {
        radii.setTopRight({ });
        radii.setBottomRight({ });
    }
    if (!closedEdges.bottom()) {
        radii.setBottomRight({ });
        radii.setBottomLeft({ });
    }
    if (!closedEdges.left()) {
        radii.setBottomLeft({ });
        radii.setTopLeft({ });
    }
}

static RectEdges<LayoutUnit> applyClosedEdges(const RectEdges<LayoutUnit>& widths, RectEdges<bool> closedEdges)
{
    return {
        LayoutUnit(closedEdges.top() ? widths.top() : 0_lu),
        LayoutUnit(closedEdges.right() ? widths.right() : 0_lu),
        LayoutUnit(closedEdges.bottom() ? widths.bottom() : 0_lu),
        LayoutUnit(closedEdges.left() ? widths.left() : 0_lu),
    };
}

static RectCorners<float> cornerCurvaturesFromStyle(const Style::ComputedStyle& style)
{
    auto& border = style.border();
    return {
        static_cast<float>(border.topLeftCornerShape().superellipse->value),
        static_cast<float>(border.topRightCornerShape().superellipse->value),
        static_cast<float>(border.bottomLeftCornerShape().superellipse->value),
        static_cast<float>(border.bottomRightCornerShape().superellipse->value)
    };
}

static void buildCornerInputs(const FloatRoundedRect&, const RectCorners<float>&,
    double leftWidth, double topWidth, double rightWidth, double bottomWidth, RectCorners<CornerInput>&);

static float constrainedRadiiScale(const LayoutRect& borderRect, const LayoutRoundedRectRadii& radii, const RectCorners<float>& cornerCurvatures)
{
    auto adjacent = calcBorderRadiiConstraintScaleFor(borderRect, radii);

    RectCorners<CornerInput> cornerRects;
    buildCornerInputs(FloatRoundedRect { LayoutRoundedRect { borderRect, radii } }, cornerCurvatures, 0, 0, 0, 0, cornerRects);
    auto opposite = static_cast<float>(oppositeCornerScaleFactor(cornerRects));

    return std::min(adjacent, opposite);
}

BorderShape BorderShape::shapeForBorderRect(const Style::ComputedStyle& style, const LayoutRect& borderRect, RectEdges<bool> closedEdges)
{
    auto zoom = style.usedZoomForLength();
    auto deviceScaleFactor = style.deviceScaleFactor();
    auto borderWidths = RectEdges<LayoutUnit>::map(style.usedBorderWidths(), [&](auto width) {
        return Style::evaluate<LayoutUnit>(width, zoom, deviceScaleFactor);
    });
    return shapeForBorderRect(style, borderRect, borderWidths, closedEdges);
}

BorderShape BorderShape::shapeForBorderRect(const Style::ComputedStyle& style, const LayoutRect& borderRect, const RectEdges<LayoutUnit>& overrideBorderWidths, RectEdges<bool> closedEdges)
{
    auto usedBorderWidths = applyClosedEdges(overrideBorderWidths, closedEdges);

    if (style.border().hasBorderRadius()) {
        auto radii = Style::evaluate<LayoutRoundedRectRadii>(style.borderRadii(), borderRect.size(), style.usedZoomForLength());
        auto cornerCurvatures = cornerCurvaturesFromStyle(style);
        radii.scale(constrainedRadiiScale(borderRect, radii, cornerCurvatures));
        zeroRadiiForOpenEdges(radii, closedEdges);

        if (!radii.areRenderableInRect(borderRect))
            radii.makeRenderableInRect(borderRect);

        return BorderShape { borderRect, usedBorderWidths, radii, cornerCurvatures };
    }

    return BorderShape { borderRect, usedBorderWidths };
}

BorderShape BorderShape::shapeForOffsetRect(const LayoutRect& borderRect, const LayoutRoundedRectRadii& borderRadii, const RectCorners<float>& cornerCurvatures, const LayoutRect& offsetRect)
{
    auto radii = borderRadii;
    radii.expand(borderRect.y() - offsetRect.y(), offsetRect.maxY() - borderRect.maxY(), borderRect.x() - offsetRect.x(), offsetRect.maxX() - borderRect.maxX());
    if (!radii.areRenderableInRect(offsetRect))
        radii.makeRenderableInRect(offsetRect);

    auto shape = BorderShape { offsetRect, { }, radii, cornerCurvatures };
    shape.m_offsetReferenceRect = LayoutRoundedRect { borderRect, borderRadii };
    return shape;
}

BorderShape BorderShape::shapeForOffsetRect(const Style::ComputedStyle& style, const LayoutRect& borderRect, const LayoutRect& offsetRect, const RectEdges<LayoutUnit>& edgeWidths, RectEdges<bool> closedEdges)
{
    auto usedEdgeWidths = applyClosedEdges(edgeWidths, closedEdges);

    if (style.border().hasBorderRadius()) {
        auto radii = Style::evaluate<LayoutRoundedRectRadii>(style.borderRadii(), borderRect.size(), style.usedZoomForLength());
        // Copy the unmodified border-box radii for the offset reference, before we expand them for the offset rect.
        auto referenceRadii = radii;

        auto leftDelta = borderRect.x() - offsetRect.x();
        auto topDelta = borderRect.y() - offsetRect.y();
        auto rightDelta = offsetRect.maxX() - borderRect.maxX();
        auto bottomDelta = offsetRect.maxY() - borderRect.maxY();

        radii.expand(topDelta, bottomDelta, leftDelta, rightDelta);
        zeroRadiiForOpenEdges(radii, closedEdges);

        if (!radii.areRenderableInRect(offsetRect))
            radii.makeRenderableInRect(offsetRect);

        auto shape = BorderShape { offsetRect, usedEdgeWidths, radii, cornerCurvaturesFromStyle(style) };

        // Always record the border-box shape for offset shapes: corners with no radius to expand or inset (bevel,
        // notch, square) are rebuilt by offsetting this reference curve to the moved rect
        referenceRadii.scale(calcBorderRadiiConstraintScaleFor(borderRect, referenceRadii));
        zeroRadiiForOpenEdges(referenceRadii, closedEdges);
        if (!referenceRadii.areRenderableInRect(borderRect))
            referenceRadii.makeRenderableInRect(borderRect);
        shape.m_offsetReferenceRect = LayoutRoundedRect { borderRect, referenceRadii };

        return shape;
    }

    return BorderShape { offsetRect, usedEdgeWidths };
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths)
    : m_borderRect(borderRect)
    , m_innerEdgeRect(computeInnerEdgeRoundedRect(m_borderRect, borderWidths))
    , m_borderWidths(borderWidths)
{
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const LayoutRoundedRectRadii& radii)
    : m_borderRect(borderRect, radii)
    , m_innerEdgeRect(computeInnerEdgeRoundedRect(m_borderRect, borderWidths))
    , m_borderWidths(borderWidths)
{
    // The caller should have adjusted the radii already.
    ASSERT(m_borderRect.isRenderable());
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const LayoutRoundedRectRadii& radii, const RectCorners<float>& cornerCurvatures)
    : m_borderRect(borderRect, radii)
    , m_innerEdgeRect(computeInnerEdgeRoundedRect(m_borderRect, borderWidths))
    , m_borderWidths(borderWidths)
    , m_cornerCurvatures(cornerCurvatures)
{
    // The caller should have adjusted the radii already.
    ASSERT(m_borderRect.isRenderable());
}

BorderShape BorderShape::shapeWithBorderWidths(const RectEdges<LayoutUnit>& borderWidths) const
{
    auto shape = BorderShape(m_borderRect.rect(), borderWidths, m_borderRect.radii(), m_cornerCurvatures);
    shape.m_offsetReferenceRect = m_offsetReferenceRect;
    return shape;
}

LayoutRoundedRect BorderShape::shapedRectForOuterShape() const
{
    return m_borderRect;
}

LayoutRoundedRect BorderShape::shapedRectForInnerShape() const
{
    return m_innerEdgeRect;
}

FloatRoundedRect BorderShape::snappedShapedRectForOuterShape(float deviceScaleFactor) const
{
    return m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

FloatRoundedRect BorderShape::snappedShapedRectForInnerShape(float deviceScaleFactor) const
{
    return m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

FloatRect BorderShape::snappedOuterRect(float deviceScaleFactor) const
{
    return snapRectToDevicePixels(m_borderRect.rect(), deviceScaleFactor);
}

FloatRect BorderShape::snappedInnerRect(float deviceScaleFactor) const
{
    return snapRectToDevicePixels(innerEdgeRect(), deviceScaleFactor);
}

bool BorderShape::innerShapeContains(const LayoutRect& rect) const
{
    return m_innerEdgeRect.contains(rect);
}

bool BorderShape::outerShapeContains(const LayoutRect& rect) const
{
    return m_borderRect.contains(rect);
}

bool BorderShape::shapeIntersectsHitTestLocation(const HitTestLocation& hitTestLocation, float deviceScaleFactor) const
{
    // FIXME: This needs to handle area hit-testing
    if (!hasNonRoundCornerShape())
        return hitTestLocation.intersects(m_borderRect);

    if (!hitTestLocation.intersects(snappedOuterRect(deviceScaleFactor)))
        return false;

    // Non-zero winding, so overlapping concave corners read as covered rather than punching a hole.
    return pathForOuterShape(deviceScaleFactor).contains(hitTestLocation.point());
}

bool BorderShape::allCornersClippedOut(const LayoutRect& rect) const
{
    if (!hasNonZeroRadii())
        return true;

    auto borderRect = m_borderRect.rect();
    if (rect.contains(borderRect))
        return false;

    auto radii = m_borderRect.radii();

    LayoutRect topLeftRect(borderRect.location(), radii.topLeft());
    if (rect.intersects(topLeftRect))
        return false;

    LayoutRect topRightRect(borderRect.location(), radii.topRight());
    topRightRect.setX(borderRect.maxX() - topRightRect.width());
    if (rect.intersects(topRightRect))
        return false;

    LayoutRect bottomLeftRect(borderRect.location(), radii.bottomLeft());
    bottomLeftRect.setY(borderRect.maxY() - bottomLeftRect.height());
    if (rect.intersects(bottomLeftRect))
        return false;

    LayoutRect bottomRightRect(borderRect.location(), radii.bottomRight());
    bottomRightRect.setX(borderRect.maxX() - bottomRightRect.width());
    bottomRightRect.setY(borderRect.maxY() - bottomRightRect.height());
    if (rect.intersects(bottomRightRect))
        return false;

    return true;
}

bool BorderShape::outerShapeIsRectangular() const
{
    return !m_borderRect.hasNonZeroRadii();
}

bool BorderShape::innerShapeIsRectangular() const
{
    return !m_innerEdgeRect.hasNonZeroRadii();
}

void BorderShape::move(LayoutSize offset)
{
    m_borderRect.move(offset);
    m_innerEdgeRect.move(offset);
    if (m_offsetReferenceRect)
        m_offsetReferenceRect->move(offset);
}

void BorderShape::inflate(LayoutUnit amount)
{
    m_borderRect.inflateWithRadii(amount);
    m_innerEdgeRect = computeInnerEdgeRoundedRect(m_borderRect, m_borderWidths);
}

static void addRoundedRectToPath(const FloatRoundedRect& roundedRect, Path& path)
{
    if (roundedRect.hasNonZeroRadii())
        path.addRoundedRect(roundedRect);
    else
        path.addRect(roundedRect.rect());
}

static void buildCornerInputs(const FloatRoundedRect& outerSnapped, const RectCorners<float>& cornerCurvatures,
    double leftWidth, double topWidth, double rightWidth, double bottomWidth, RectCorners<CornerInput>& cornerRects)
{
    auto tr = outerSnapped.topRightCorner();
    auto br = outerSnapped.bottomRightCorner();
    auto bl = outerSnapped.bottomLeftCorner();
    auto tl = outerSnapped.topLeftCorner();

    cornerRects.topRight() = { tr.x(), tr.y(), tr.width(), tr.height(), cornerCurvatures.topRight(), topWidth, rightWidth, BoxCorner::TopRight };
    cornerRects.bottomRight() = { br.x(), br.y(), br.width(), br.height(), cornerCurvatures.bottomRight(), rightWidth, bottomWidth, BoxCorner::BottomRight };
    cornerRects.bottomLeft() = { bl.x(), bl.y(), bl.width(), bl.height(), cornerCurvatures.bottomLeft(), bottomWidth, leftWidth, BoxCorner::BottomLeft };
    cornerRects.topLeft() = { tl.x(), tl.y(), tl.width(), tl.height(), cornerCurvatures.topLeft(), leftWidth, topWidth, BoxCorner::TopLeft };
}

// Bevel, notch, and square have no radius to expand or inset (the way round/superellipse corners are offset), so they're rebuilt by offsetting the border-box corner to the moved rect.
static void rebuildOffsetCornersFromReference(RectCorners<CornerInput>& contourCorners, const std::optional<FloatRoundedRect>& borderBoxReference, const RectCorners<float>& cornerCurvatures, const FloatRect& contourRect)
{
    if (!borderBoxReference)
        return;

    auto needsRebuild = [](float curvature) {
        return curvature == 0.0f || std::isinf(curvature);
    };
    bool hasCornerNeedingRebuild = needsRebuild(cornerCurvatures.topLeft()) || needsRebuild(cornerCurvatures.topRight())
        || needsRebuild(cornerCurvatures.bottomLeft()) || needsRebuild(cornerCurvatures.bottomRight());
    if (!hasCornerNeedingRebuild)
        return;

    auto snappedReference = *borderBoxReference;
    auto snappedReferenceRect = snappedReference.rect();

    double leftOffset = snappedReferenceRect.x() - contourRect.x();
    double topOffset = snappedReferenceRect.y() - contourRect.y();
    double rightOffset = contourRect.maxX() - snappedReferenceRect.maxX();
    double bottomOffset = contourRect.maxY() - snappedReferenceRect.maxY();

    RectCorners<CornerInput> rebuiltCorners;
    buildCornerInputs(snappedReference, cornerCurvatures, -leftOffset, -topOffset, -rightOffset, -bottomOffset, rebuiltCorners);

    if (needsRebuild(cornerCurvatures.topLeft()))
        contourCorners.topLeft() = rebuiltCorners.topLeft();
    if (needsRebuild(cornerCurvatures.topRight()))
        contourCorners.topRight() = rebuiltCorners.topRight();
    if (needsRebuild(cornerCurvatures.bottomLeft()))
        contourCorners.bottomLeft() = rebuiltCorners.bottomLeft();
    if (needsRebuild(cornerCurvatures.bottomRight()))
        contourCorners.bottomRight() = rebuiltCorners.bottomRight();
}

bool BorderShape::hasNonRoundCornerShape() const
{
    const auto& radii = m_borderRect.radii();
    return (m_cornerCurvatures.topLeft() != 1.0f && !radii.topLeft().isEmpty())
        || (m_cornerCurvatures.topRight() != 1.0f && !radii.topRight().isEmpty())
        || (m_cornerCurvatures.bottomLeft() != 1.0f && !radii.bottomLeft().isEmpty())
        || (m_cornerCurvatures.bottomRight() != 1.0f && !radii.bottomRight().isEmpty());
}

Path BorderShape::pathForOuterRoundedRect(const FloatRoundedRect& outerSnapped) const
{
    Path path;
    addRoundedRectToPath(outerSnapped, path);
    return path;
}

Path BorderShape::pathForInnerRoundedRect(const FloatRoundedRect& innerSnapped) const
{
    ASSERT(innerSnapped.isRenderable());
    Path path;
    addRoundedRectToPath(innerSnapped, path);
    return path;
}

std::optional<FloatRoundedRect> BorderShape::snappedOffsetReferenceRect(float deviceScaleFactor) const
{
    if (!m_offsetReferenceRect)
        return std::nullopt;
    return m_offsetReferenceRect->pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

static bool cornersHaveConvexSuperellipse(const RectCorners<float>& cornerCurvatures)
{
    auto isConvexSuperellipse = [](float curvature) {
        return std::isfinite(curvature) && curvature > 1.0f;
    };
    return isConvexSuperellipse(cornerCurvatures.topLeft()) || isConvexSuperellipse(cornerCurvatures.topRight())
        || isConvexSuperellipse(cornerCurvatures.bottomLeft()) || isConvexSuperellipse(cornerCurvatures.bottomRight());
}

// "Aligned-to-curve offset" generates the moved contour by offsetting the border-box corner curve at
// constant distance (see addAlignedToCurveOffsetContour), not by scaling radii. Only needed for corners
// whose shape can't be reproduced by scaling: finite superellipses with curvature < 1 and != 0.
static bool cornersUseAlignedToCurveOffset(const RectCorners<float>& cornerCurvatures)
{
    auto usesAlignedOffset = [](float curvature) {
        return std::isfinite(curvature) && curvature < 1.0f && curvature != 0.0f;
    };
    return usesAlignedOffset(cornerCurvatures.topLeft()) || usesAlignedOffset(cornerCurvatures.topRight())
        || usesAlignedOffset(cornerCurvatures.bottomLeft()) || usesAlignedOffset(cornerCurvatures.bottomRight());
}

// Offset the border-box reference curve to targetRect at constant thickness: outset corners miter out to the edges along their tangent, inset corners trim to the rect.
static void addAlignedToCurveOffsetContour(Path& path, const FloatRoundedRect& referenceSnapped, const RectCorners<float>& cornerCurvatures, const FloatRect& targetRect)
{
    auto referenceRect = referenceSnapped.rect();
    double leftOffset = referenceRect.x() - targetRect.x();
    double topOffset = referenceRect.y() - targetRect.y();
    double rightOffset = targetRect.maxX() - referenceRect.maxX();
    double bottomOffset = targetRect.maxY() - referenceRect.maxY();

    RectCorners<CornerInput> referenceCorners;
    buildCornerInputs(referenceSnapped, cornerCurvatures, -leftOffset, -topOffset, -rightOffset, -bottomOffset, referenceCorners);

    auto outsetMiter = referenceRect.contains(targetRect) ? OutsetMiter::No : OutsetMiter::Yes;
    borderContourPath(path, referenceCorners, &targetRect, outsetMiter);
}

static void buildOuterCornerInputs(const FloatRoundedRect& outerSnapped, const RectCorners<float>& cornerCurvatures, const std::optional<FloatRoundedRect>& offsetReference, RectCorners<CornerInput>& cornerRects)
{
    buildCornerInputs(outerSnapped, cornerCurvatures, 0, 0, 0, 0, cornerRects);
    rebuildOffsetCornersFromReference(cornerRects, offsetReference, cornerCurvatures, outerSnapped.rect());
}

static void addOuterCornerShapeToPath(Path& path, const FloatRoundedRect& outerSnapped, const RectCorners<float>& cornerCurvatures, const std::optional<FloatRoundedRect>& offsetReferenceRect)
{
    RectCorners<CornerInput> cornerRects;
    buildOuterCornerInputs(outerSnapped, cornerCurvatures, offsetReferenceRect, cornerRects);
    borderContourPath(path, cornerRects);
}

static FloatPoint outerVertexForCorner(const FloatRect& rect, BoxCorner corner)
{
    switch (corner) {
    case BoxCorner::TopLeft:     return rect.minXMinYCorner();
    case BoxCorner::TopRight:    return rect.maxXMinYCorner();
    case BoxCorner::BottomLeft:  return rect.minXMaxYCorner();
    case BoxCorner::BottomRight: return rect.maxXMaxYCorner();
    }
    return { };
}

static Vector<FloatPoint> densifyContour(const Vector<FloatPoint>& points, float maximumSpacing)
{
    Vector<FloatPoint> dense;
    for (size_t index = 0; index + 1 < points.size(); ++index) {
        auto start = points[index];
        auto span = points[index + 1] - start;
        dense.append(start);

        auto interiorCount = static_cast<unsigned>(std::floor(span.diagonalLength() / maximumSpacing));
        for (unsigned step = 1; step <= interiorCount; ++step)
            dense.append(start + span.scaled(float(step) / (interiorCount + 1)));
    }
    if (!points.isEmpty())
        dense.append(points.last());
    return dense;
}

Vector<FloatPoint> BorderShape::outerShapeAsPolygon(float deviceScaleFactor, unsigned stepsPerHalf) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    RectCorners<CornerInput> cornerRects;
    buildOuterCornerInputs(outerSnapped, m_cornerCurvatures, snappedOffsetReferenceRect(deviceScaleFactor), cornerRects);

    Vector<FloatPoint> contour;
    for (auto key : { BoxCorner::TopRight, BoxCorner::BottomRight, BoxCorner::BottomLeft, BoxCorner::TopLeft })
        contour.appendVector(sampleCornerShape(cornerRects[key], stepsPerHalf));

    if (!contour.isEmpty())
        contour.append(contour.first());
    return contour;
}

std::optional<Path> BorderShape::pathForShapedRect(const FloatRoundedRect& roundedRect, const RectCorners<float>& cornerCurvatures)
{
    auto& radii = roundedRect.radii();
    auto isRound = [](float curvature, const FloatSize& radius) {
        return curvature == 1.0f || radius.isEmpty();
    };
    if (isRound(cornerCurvatures.topLeft(), radii.topLeft())
        && isRound(cornerCurvatures.topRight(), radii.topRight())
        && isRound(cornerCurvatures.bottomLeft(), radii.bottomLeft())
        && isRound(cornerCurvatures.bottomRight(), radii.bottomRight()))
        return std::nullopt;

    RectCorners<CornerInput> cornerRects;
    buildCornerInputs(roundedRect, cornerCurvatures, 0, 0, 0, 0, cornerRects);

    Path path;
    borderContourPath(path, cornerRects);
    return path;
}

Region BorderShape::approximateAsRegion(float deviceScaleFactor, unsigned stepLength) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (!hasNonRoundCornerShape())
        return WebCore::approximateAsRegion(outerSnapped, stepLength);

    RectCorners<CornerInput> cornerRects;
    buildOuterCornerInputs(outerSnapped, m_cornerCurvatures, snappedOffsetReferenceRect(deviceScaleFactor), cornerRects);

    auto rect = outerSnapped.rect();
    Region region;
    region.unite(enclosingIntRect(rect));

    for (auto key : { BoxCorner::TopLeft, BoxCorner::TopRight, BoxCorner::BottomLeft, BoxCorner::BottomRight }) {
        const auto& input = cornerRects[key];

        auto extent = std::max(0.0, std::min(input.width, input.height));
        auto step = std::max(1u, stepLength);
        auto count = std::clamp(static_cast<unsigned>((extent + step / 2.0) / step), 2u, 20u);

        bool straightSided = !input.curvature || std::isinf(input.curvature);
        auto samples = sampleCornerShape(input, straightSided ? 1 : count);
        if (samples.size() < 2)
            continue;
        if (straightSided)
            samples = densifyContour(samples, std::max(1.0, extent / count));

        auto vertex = outerVertexForCorner(rect, key);
        for (auto& point : samples) {
            FloatRect cornerRect { vertex, FloatSize { } };
            cornerRect.extend(point);
            region.subtract(roundedIntRect(cornerRect));
        }
    }

    return region;
}

Path BorderShape::pathForOuterCornerShape(const FloatRoundedRect& outerSnapped, const std::optional<FloatRoundedRect>& snappedOffsetReference) const
{
    Path path;
    bool useAlignedToCurve = snappedOffsetReference
        && (cornersUseAlignedToCurveOffset(m_cornerCurvatures)
            || cornersHaveConvexSuperellipse(m_cornerCurvatures));
    if (useAlignedToCurve) {
        addAlignedToCurveOffsetContour(path, *snappedOffsetReference, m_cornerCurvatures, outerSnapped.rect());
        if (!path.isEmpty())
            return path;
    }
    addOuterCornerShapeToPath(path, outerSnapped, m_cornerCurvatures, snappedOffsetReference);
    if (!path.isEmpty())
        return path;
    return pathForOuterRoundedRect(outerSnapped);
}

static void addInnerCornerShapeToPath(Path& path, const FloatRoundedRect& outerSnapped, const FloatRoundedRect& innerSnapped, const RectCorners<float>& cornerCurvatures, const std::optional<FloatRoundedRect>& offsetReferenceRect)
{
    auto outerRect = outerSnapped.rect();
    auto innerRect = innerSnapped.rect();

    double leftWidth = innerRect.x() - outerRect.x();
    double topWidth = innerRect.y() - outerRect.y();
    double rightWidth = outerRect.maxX() - innerRect.maxX();
    double bottomWidth = outerRect.maxY() - innerRect.maxY();

    RectCorners<CornerInput> cornerRects;
    buildCornerInputs(outerSnapped, cornerCurvatures, leftWidth, topWidth, rightWidth, bottomWidth, cornerRects);
    rebuildOffsetCornersFromReference(cornerRects, offsetReferenceRect, cornerCurvatures, innerRect);
    borderContourPath(path, cornerRects, &innerRect);
}

Path BorderShape::pathForInnerCornerShape(const FloatRoundedRect& outerSnapped, const FloatRoundedRect& innerSnapped, const std::optional<FloatRoundedRect>& snappedOffsetReference) const
{
    Path path;
    bool useAlignedToCurve = snappedOffsetReference
        && (cornersUseAlignedToCurveOffset(m_cornerCurvatures)
            || cornersHaveConvexSuperellipse(m_cornerCurvatures));
    if (useAlignedToCurve) {
        addAlignedToCurveOffsetContour(path, *snappedOffsetReference, m_cornerCurvatures, innerSnapped.rect());
        if (!path.isEmpty())
            return path;
    }
    addInnerCornerShapeToPath(path, outerSnapped, innerSnapped, m_cornerCurvatures, snappedOffsetReference);
    if (path.isEmpty())
        return pathForInnerRoundedRect(innerSnapped);
    return path;
}

Path BorderShape::pathForOuterShape(float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape())
        return pathForOuterCornerShape(outerSnapped, snappedOffsetReferenceRect(deviceScaleFactor));
    return pathForOuterRoundedRect(outerSnapped);
}

Path BorderShape::pathForInnerShape(float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto innerSnapped = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape())
        return pathForInnerCornerShape(outerSnapped, innerSnapped, snappedOffsetReferenceRect(deviceScaleFactor));
    return pathForInnerRoundedRect(innerSnapped);
}

void BorderShape::addOuterShapeToPath(Path& path, float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape()) {
        path.addPath(pathForOuterCornerShape(outerSnapped, snappedOffsetReferenceRect(deviceScaleFactor)), AffineTransform());
        return;
    }
    addRoundedRectToPath(outerSnapped, path);
}

void BorderShape::addInnerShapeToPath(Path& path, float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto innerSnapped = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape()) {
        path.addPath(pathForInnerCornerShape(outerSnapped, innerSnapped, snappedOffsetReferenceRect(deviceScaleFactor)), AffineTransform());
        return;
    }
    ASSERT(innerSnapped.isRenderable());
    addRoundedRectToPath(innerSnapped, path);
}

Path BorderShape::pathForBorderArea(float deviceScaleFactor) const
{
    if (hasNonRoundCornerShape()) {
        Path path;
        addOuterShapeToPath(path, deviceScaleFactor);
        addInnerShapeToPath(path, deviceScaleFactor);
        return path;
    }

    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto innerSnapped = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);

    ASSERT(innerSnapped.isRenderable());

    Path path;
    addRoundedRectToPath(outerSnapped, path);
    addRoundedRectToPath(innerSnapped, path);
    return path;
}

void BorderShape::clipToOuterShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape()) {
        context.clipPath(pathForOuterCornerShape(outerSnapped, snappedOffsetReferenceRect(deviceScaleFactor)));
        return;
    }

    if (outerSnapped.hasNonZeroRadii())
        context.clipRoundedRect(outerSnapped);
    else
        context.clip(outerSnapped.rect());
}

void BorderShape::clipToInnerShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto innerSnapped = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape()) {
        context.clipPath(pathForInnerCornerShape(outerSnapped, innerSnapped, snappedOffsetReferenceRect(deviceScaleFactor)));
        return;
    }

    ASSERT(innerSnapped.isRenderable());
    if (innerSnapped.hasNonZeroRadii())
        context.clipRoundedRect(innerSnapped);
    else
        context.clip(innerSnapped.rect());
}

void BorderShape::clipOutOuterShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (outerSnapped.isEmpty())
        return;

    if (hasNonRoundCornerShape()) {
        context.clipOut(pathForOuterCornerShape(outerSnapped, snappedOffsetReferenceRect(deviceScaleFactor)));
        return;
    }

    if (outerSnapped.hasNonZeroRadii())
        context.clipOutRoundedRect(outerSnapped);
    else
        context.clipOut(outerSnapped.rect());
}

void BorderShape::clipOutInnerShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto innerSnapped = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (innerSnapped.isEmpty())
        return;

    if (hasNonRoundCornerShape()) {
        context.clipOut(pathForInnerCornerShape(outerSnapped, innerSnapped, snappedOffsetReferenceRect(deviceScaleFactor)));
        return;
    }

    if (innerSnapped.hasNonZeroRadii())
        context.clipOutRoundedRect(innerSnapped);
    else
        context.clipOut(innerSnapped.rect());
}

void BorderShape::fillOuterShape(GraphicsContext& context, const Color& color, float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape()) {
        context.setFillColor(color);
        context.fillPath(pathForOuterCornerShape(outerSnapped, snappedOffsetReferenceRect(deviceScaleFactor)));
        return;
    }

    if (outerSnapped.hasNonZeroRadii())
        context.fillRoundedRect(outerSnapped, color);
    else
        context.fillRect(outerSnapped.rect(), color);
}

void BorderShape::fillInnerShape(GraphicsContext& context, const Color& color, float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto innerSnapped = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape()) {
        context.setFillColor(color);
        context.fillPath(pathForInnerCornerShape(outerSnapped, innerSnapped, snappedOffsetReferenceRect(deviceScaleFactor)));
        return;
    }

    ASSERT(innerSnapped.isRenderable());
    if (innerSnapped.hasNonZeroRadii())
        context.fillRoundedRect(innerSnapped, color);
    else
        context.fillRect(innerSnapped.rect(), color);
}

void BorderShape::fillRectWithInnerHoleShape(GraphicsContext& context, const LayoutRect& outerRect, const Color& color, float deviceScaleFactor) const
{
    auto outerSnapped = snapRectToDevicePixels(outerRect, deviceScaleFactor);

    if (hasNonRoundCornerShape()) {
        Path path;
        path.addRect(outerSnapped);
        addInnerShapeToPath(path, deviceScaleFactor);
        context.setFillRule(WindRule::EvenOdd);
        context.setFillColor(color);
        context.fillPath(path);
        return;
    }

    auto innerSnapped = snappedInnerEdgeRectForPainting(deviceScaleFactor);
    ASSERT(innerSnapped.isRenderable());
    context.fillRectWithRoundedHole(outerSnapped, innerSnapped, color);
}

FloatRoundedRect BorderShape::snappedInnerEdgeRectForPainting(float deviceScaleFactor) const
{
    // Derive the inner rect from the already-snapped outer rect, insetting each side by the width
    // rounded to whole device pixels, so the gap (border/spread thickness) is equal on every side
    // regardless of sub-pixel position.
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto outerRect = outerSnapped.rect();

    auto topWidth = roundToDevicePixel(m_borderWidths.top(), deviceScaleFactor);
    auto rightWidth = roundToDevicePixel(m_borderWidths.right(), deviceScaleFactor);
    auto bottomWidth = roundToDevicePixel(m_borderWidths.bottom(), deviceScaleFactor);
    auto leftWidth = roundToDevicePixel(m_borderWidths.left(), deviceScaleFactor);

    FloatRect innerRect {
        outerRect.x() + leftWidth,
        outerRect.y() + topWidth,
        std::max(0.0f, outerRect.width() - leftWidth - rightWidth),
        std::max(0.0f, outerRect.height() - topWidth - bottomWidth),
    };

    auto innerSnapped = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    innerSnapped.setRect(innerRect);
    if (!innerSnapped.isRenderable())
        innerSnapped.adjustRadii();
    return innerSnapped;
}

LayoutRoundedRect BorderShape::computeInnerEdgeRoundedRect(const LayoutRoundedRect& borderRoundedRect, const RectEdges<LayoutUnit>& borderWidths)
{
    auto borderRect = borderRoundedRect.rect();
    auto width = std::max(0_lu, borderRect.width() - borderWidths.left() - borderWidths.right());
    auto height = std::max(0_lu, borderRect.height() - borderWidths.top() - borderWidths.bottom());
    auto innerRect = LayoutRect {
        borderRect.x() + borderWidths.left(),
        borderRect.y() + borderWidths.top(),
        width,
        height
    };

    auto innerEdgeRect = LayoutRoundedRect { innerRect };
    if (borderRoundedRect.hasNonZeroRadii()) {
        auto innerRadii = borderRoundedRect.radii();
        innerRadii.shrink(borderWidths.top(), borderWidths.bottom(), borderWidths.left(), borderWidths.right());
        innerEdgeRect.setRadii(innerRadii);

        if (!innerEdgeRect.isRenderable())
            innerEdgeRect.adjustRadii();
    }

    return innerEdgeRect;
}

} // namespace WebCore
