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
#include "LayoutRect.h"
#include "LayoutRoundedRect.h"
#include "Path.h"
#include "StyleComputedStyle+GettersInlines.h"

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

        auto leftDelta = borderRect.x() - offsetRect.x();
        auto topDelta = borderRect.y() - offsetRect.y();
        auto rightDelta = offsetRect.maxX() - borderRect.maxX();
        auto bottomDelta = offsetRect.maxY() - borderRect.maxY();

        radii.expand(topDelta, bottomDelta, leftDelta, rightDelta);
        zeroRadiiForOpenEdges(radii, closedEdges);

        if (!radii.areRenderableInRect(offsetRect))
            radii.makeRenderableInRect(offsetRect);

        return BorderShape { offsetRect, usedEdgeWidths, radii, cornerCurvaturesFromStyle(style) };
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
    return BorderShape(m_borderRect.rect(), borderWidths, m_borderRect.radii(), m_cornerCurvatures);
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

static void addOuterCornerShapeToPath(Path& path, const FloatRoundedRect& outerSnapped, const RectCorners<float>& cornerCurvatures)
{
    RectCorners<CornerInput> cornerRects;
    buildScaledCornerInputs(outerSnapped, cornerCurvatures, 0, 0, 0, 0, cornerRects);
    borderContourPath(path, cornerRects);
}

Path BorderShape::pathForOuterCornerShape(const FloatRoundedRect& outerSnapped) const
{
    Path path;
    addOuterCornerShapeToPath(path, outerSnapped, m_cornerCurvatures);
    if (!path.isEmpty())
        return path;
    return pathForOuterRoundedRect(outerSnapped);
}

static void addInnerCornerShapeToPath(Path& path, const FloatRoundedRect& outerSnapped, const FloatRoundedRect& innerSnapped, const RectCorners<float>& cornerCurvatures)
{
    auto outerRect = outerSnapped.rect();
    auto innerRect = innerSnapped.rect();

    double leftWidth = innerRect.x() - outerRect.x();
    double topWidth = innerRect.y() - outerRect.y();
    double rightWidth = outerRect.maxX() - innerRect.maxX();
    double bottomWidth = outerRect.maxY() - innerRect.maxY();

    RectCorners<CornerInput> cornerRects;
    buildScaledCornerInputs(outerSnapped, cornerCurvatures, leftWidth, topWidth, rightWidth, bottomWidth, cornerRects);
    borderContourPath(path, cornerRects);
}

Path BorderShape::pathForInnerCornerShape(const FloatRoundedRect& outerSnapped, const FloatRoundedRect& innerSnapped) const
{
    Path path;
    addInnerCornerShapeToPath(path, outerSnapped, innerSnapped, m_cornerCurvatures);
    if (path.isEmpty())
        return pathForInnerRoundedRect(innerSnapped);
    return path;
}

Path BorderShape::pathForOuterShape(float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape())
        return pathForOuterCornerShape(outerSnapped);
    return pathForOuterRoundedRect(outerSnapped);
}

Path BorderShape::pathForInnerShape(float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto innerSnapped = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape())
        return pathForInnerCornerShape(outerSnapped, innerSnapped);
    return pathForInnerRoundedRect(innerSnapped);
}

void BorderShape::addOuterShapeToPath(Path& path, float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape()) {
        Path cornerPath;
        addOuterCornerShapeToPath(cornerPath, outerSnapped, m_cornerCurvatures);
        if (!cornerPath.isEmpty()) {
            path.addPath(cornerPath, AffineTransform());
            return;
        }
    }
    addRoundedRectToPath(outerSnapped, path);
}

void BorderShape::addInnerShapeToPath(Path& path, float deviceScaleFactor) const
{
    auto outerSnapped = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto innerSnapped = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShape()) {
        Path cornerPath;
        addInnerCornerShapeToPath(cornerPath, outerSnapped, innerSnapped, m_cornerCurvatures);
        if (!cornerPath.isEmpty()) {
            path.addPath(cornerPath, AffineTransform());
            return;
        }
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
        context.clipPath(pathForOuterCornerShape(outerSnapped));
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
        context.clipPath(pathForInnerCornerShape(outerSnapped, innerSnapped));
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
        context.clipOut(pathForOuterCornerShape(outerSnapped));
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
        context.clipOut(pathForInnerCornerShape(outerSnapped, innerSnapped));
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
        context.fillPath(pathForOuterCornerShape(outerSnapped));
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
        context.fillPath(pathForInnerCornerShape(outerSnapped, innerSnapped));
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
        context.setFillColor(color);
        context.fillPath(path);
        return;
    }

    auto innerSnapped = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(innerSnapped.isRenderable());
    context.fillRectWithRoundedHole(outerSnapped, innerSnapped, color);
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
