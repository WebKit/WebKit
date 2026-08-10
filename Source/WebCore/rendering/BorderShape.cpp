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
        radii.scale(calcBorderRadiiConstraintScaleFor(borderRect, radii));
        zeroRadiiForOpenEdges(radii, closedEdges);

        if (!radii.areRenderableInRect(borderRect))
            radii.makeRenderableInRect(borderRect);

        return BorderShape { borderRect, usedBorderWidths, radii, cornerCurvaturesFromStyle(style) };
    }

    return BorderShape { borderRect, usedBorderWidths };
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

LayoutRoundedRect BorderShape::deprecatedRoundedRect() const
{
    return m_borderRect;
}

LayoutRoundedRect BorderShape::deprecatedInnerRoundedRect() const
{
    return m_innerEdgeRect;
}

FloatRoundedRect BorderShape::deprecatedPixelSnappedRoundedRect(float deviceScaleFactor) const
{
    return m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

FloatRoundedRect BorderShape::deprecatedPixelSnappedInnerRoundedRect(float deviceScaleFactor) const
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

static void buildScaledCornerInputs(const FloatRoundedRect& outerSnapped, const RectCorners<float>& cornerCurvatures,
    double leftWidth, double topWidth, double rightWidth, double bottomWidth, RectCorners<CornerInput>& cornerRects)
{
    // Measure unmodified outer corners
    RectCorners<CornerInput> outerRects;
    buildCornerInputs(outerSnapped, cornerCurvatures, 0, 0, 0, 0, outerRects);
    double scale = oppositeCornerScaleFactor(outerRects);

    // Build corners with border insets
    buildCornerInputs(outerSnapped, cornerCurvatures, leftWidth, topWidth, rightWidth, bottomWidth, cornerRects);
    if (scale >= 1.0)
        return;

    for (auto key : { BoxCorner::TopLeft, BoxCorner::TopRight, BoxCorner::BottomLeft, BoxCorner::BottomRight }) {
        CornerInput& corner = cornerRects[key];

        if (!corner.startInset && !corner.endInset)
            continue;

        double scaledWidth = corner.width * scale;
        double scaledHeight = corner.height * scale;

        bool anchorRight = corner.orientation == BoxCorner::TopRight || corner.orientation == BoxCorner::BottomRight;
        bool anchorBottom = corner.orientation == BoxCorner::BottomRight || corner.orientation == BoxCorner::BottomLeft;

        // Push right/down to compensate for shrink
        if (anchorRight)
            corner.x += corner.width - scaledWidth;
        if (anchorBottom)
            corner.y += corner.height - scaledHeight;
        corner.width = scaledWidth;
        corner.height = scaledHeight;
    }
}

enum class UseScaledInputs : bool { No, Yes };

// Bevel, notch, and square have no radius to expand or inset (the way round/superellipse corners are offset), so they're rebuilt by offsetting the border-box corner to the moved rect.
static void rebuildOffsetCornersFromReference(RectCorners<CornerInput>& contourCorners, const std::optional<FloatRoundedRect>& borderBoxReference, const RectCorners<float>& cornerCurvatures, const FloatRect& contourRect, UseScaledInputs useScaledInputs)
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
    if (useScaledInputs == UseScaledInputs::Yes)
        buildScaledCornerInputs(snappedReference, cornerCurvatures, -leftOffset, -topOffset, -rightOffset, -bottomOffset, rebuiltCorners);
    else
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

static void addOuterCornerShapeToPath(Path& path, const FloatRoundedRect& outerSnapped, const RectCorners<float>& cornerCurvatures, const std::optional<FloatRoundedRect>& offsetReferenceRect)
{
    RectCorners<CornerInput> cornerRects;
    buildCornerInputs(outerSnapped, cornerCurvatures, 0, 0, 0, 0, cornerRects);
    rebuildOffsetCornersFromReference(cornerRects, offsetReferenceRect, cornerCurvatures, outerSnapped.rect(), UseScaledInputs::No);
    borderContourPath(path, cornerRects);
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
    buildScaledCornerInputs(outerSnapped, cornerCurvatures, leftWidth, topWidth, rightWidth, bottomWidth, cornerRects);
    rebuildOffsetCornersFromReference(cornerRects, offsetReferenceRect, cornerCurvatures, innerRect, UseScaledInputs::Yes);
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
