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

#include "BorderData.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "LayoutRoundedRect.h"
#include "Path.h"
#include "RenderStyle+GettersInlines.h"

namespace WebCore {

BorderShape BorderShape::shapeForBorderRect(const RenderStyle& style, const LayoutRect& borderRect, RectEdges<bool> closedEdges)
{
    auto borderWidths = RectEdges<LayoutUnit>::map(style.usedBorderWidths(), [&](auto width) {
        return Style::evaluate<LayoutUnit>(width, Style::ZoomNeeded { });
    });
    return shapeForBorderRect(style, borderRect, borderWidths, closedEdges);
}

BorderShape BorderShape::shapeForBorderRect(const RenderStyle& style, const LayoutRect& borderRect, const RectEdges<LayoutUnit>& overrideBorderWidths, RectEdges<bool> closedEdges)
{
    // top, right, bottom, left.
    auto usedBorderWidths = RectEdges<LayoutUnit> {
        LayoutUnit(closedEdges.top() ? overrideBorderWidths.top() : 0_lu),
        LayoutUnit(closedEdges.right() ? overrideBorderWidths.right() : 0_lu),
        LayoutUnit(closedEdges.bottom() ? overrideBorderWidths.bottom() : 0_lu),
        LayoutUnit(closedEdges.left() ? overrideBorderWidths.left() : 0_lu),
    };

    if (style.border().hasBorderRadius()) {
        auto radii = Style::evaluate<LayoutRoundedRectRadii>(style.borderRadii(), borderRect.size(), style.usedZoomForLength());
        radii.scale(calcBorderRadiiConstraintScaleFor(borderRect, radii));

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

        if (!radii.areRenderableInRect(borderRect))
            radii.makeRenderableInRect(borderRect);

        return BorderShape { borderRect, usedBorderWidths, radii };
    }

    return BorderShape { borderRect, usedBorderWidths };
}

BorderShape BorderShape::shapeForOutsetRect(const RenderStyle& style, const LayoutRect& borderRect, const LayoutRect& outlineBoxRect, const RectEdges<LayoutUnit>& outlineWidths, RectEdges<bool> closedEdges)
{
    // top, right, bottom, left.
    auto usedOutlineWidths = RectEdges<LayoutUnit> {
        LayoutUnit(closedEdges.top() ? outlineWidths.top() : 0_lu),
        LayoutUnit(closedEdges.right() ? outlineWidths.right() : 0_lu),
        LayoutUnit(closedEdges.bottom() ? outlineWidths.bottom() : 0_lu),
        LayoutUnit(closedEdges.left() ? outlineWidths.left() : 0_lu),
    };

    if (style.border().hasBorderRadius()) {
        auto radii = Style::evaluate<LayoutRoundedRectRadii>(style.borderRadii(), borderRect.size(), style.usedZoomForLength());

        auto leftOutset = std::max(borderRect.x() - outlineBoxRect.x(), 0_lu);
        auto topOutset = std::max(borderRect.y() - outlineBoxRect.y(), 0_lu);
        auto rightOutset = std::max(outlineBoxRect.maxX() - borderRect.maxX(), 0_lu);
        auto bottomOutset = std::max(outlineBoxRect.maxY() - borderRect.maxY(), 0_lu);

        radii.expand(topOutset, bottomOutset, leftOutset, rightOutset);

        // FIXME: Share
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

        if (!radii.areRenderableInRect(outlineBoxRect))
            radii.makeRenderableInRect(outlineBoxRect);

        return BorderShape { outlineBoxRect, usedOutlineWidths, radii };
    }

    return BorderShape { outlineBoxRect, usedOutlineWidths };
}

BorderShape BorderShape::shapeForInsetRect(const RenderStyle& style, const LayoutRect& borderRect, const LayoutRect& insetRect)
{
    if (style.border().hasBorderRadius()) {
        auto radii = Style::evaluate<LayoutRoundedRectRadii>(style.borderRadii(), borderRect.size(), style.usedZoomForLength());

        auto leftInset = std::max(insetRect.x() - borderRect.x(), 0_lu);
        auto topInset = std::max(insetRect.y()- borderRect.y(), 0_lu);
        auto rightInset = std::max(borderRect.maxX() - insetRect.maxX(), 0_lu);
        auto bottomInset = std::max(borderRect.maxY() - insetRect.maxY(), 0_lu);

        auto insetWidths = RectEdges<LayoutUnit> { topInset, rightInset, bottomInset, leftInset };
        auto roundedRect = LayoutRoundedRect { borderRect, radii };

        auto insetRoundedRect = computeInnerEdgeRoundedRect(roundedRect, insetWidths);

        return BorderShape { insetRect, { }, insetRoundedRect.radii() };
    }

    return BorderShape { insetRect, { } };
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

BorderShape BorderShape::shapeWithBorderWidths(const RectEdges<LayoutUnit>& borderWidths) const
{
    return BorderShape(m_borderRect.rect(), borderWidths, m_borderRect.radii());
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
    if (!isRounded())
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
    return !m_borderRect.isRounded();
}

bool BorderShape::innerShapeIsRectangular() const
{
    return !m_innerEdgeRect.isRounded();
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
    if (roundedRect.isRounded())
        path.addRoundedRect(roundedRect);
    else
        path.addRect(roundedRect.rect());
}

Path BorderShape::pathForOuterShape(float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    Path path;
    addRoundedRectToPath(pixelSnappedRect, path);
    return path;
}

Path BorderShape::pathForInnerShape(float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());

    Path path;
    addRoundedRectToPath(pixelSnappedRect, path);
    return path;
}

void BorderShape::addOuterShapeToPath(Path& path, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    addRoundedRectToPath(pixelSnappedRect, path);
}

void BorderShape::addInnerShapeToPath(Path& path, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    addRoundedRectToPath(pixelSnappedRect, path);
}

Path BorderShape::pathForBorderArea(float deviceScaleFactor) const
{
    auto pixelSnappedOuterRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto pixelSnappedInnerRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);

    ASSERT(pixelSnappedInnerRect.isRenderable());

    Path path;
    addRoundedRectToPath(pixelSnappedOuterRect, path);
    addRoundedRectToPath(pixelSnappedInnerRect, path);
    return path;
}

void BorderShape::clipToOuterShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isRounded())
        context.clipRoundedRect(pixelSnappedRect);
    else
        context.clip(pixelSnappedRect.rect());
}

void BorderShape::clipToInnerShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    if (pixelSnappedRect.isRounded())
        context.clipRoundedRect(pixelSnappedRect);
    else
        context.clip(pixelSnappedRect.rect());
}

void BorderShape::clipOutOuterShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isEmpty())
        return;

    if (pixelSnappedRect.isRounded())
        context.clipOutRoundedRect(pixelSnappedRect);
    else
        context.clipOut(pixelSnappedRect.rect());
}

void BorderShape::clipOutInnerShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isEmpty())
        return;

    if (pixelSnappedRect.isRounded())
        context.clipOutRoundedRect(pixelSnappedRect);
    else
        context.clipOut(pixelSnappedRect.rect());
}

void BorderShape::fillOuterShape(GraphicsContext& context, const Color& color, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isRounded())
        context.fillRoundedRect(pixelSnappedRect, color);
    else
        context.fillRect(pixelSnappedRect.rect(), color);
}

void BorderShape::fillInnerShape(GraphicsContext& context, const Color& color, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    if (pixelSnappedRect.isRounded())
        context.fillRoundedRect(pixelSnappedRect, color);
    else
        context.fillRect(pixelSnappedRect.rect(), color);
}

void BorderShape::fillRectWithInnerHoleShape(GraphicsContext& context, const LayoutRect& outerRect, const Color& color, float deviceScaleFactor) const
{
    auto pixelSnappedOuterRect = snapRectToDevicePixels(outerRect, deviceScaleFactor);
    auto innerSnappedRoundedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(innerSnappedRoundedRect.isRenderable());
    context.fillRectWithRoundedHole(pixelSnappedOuterRect, innerSnappedRoundedRect, color);
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
    if (borderRoundedRect.isRounded()) {
        auto innerRadii = borderRoundedRect.radii();
        innerRadii.shrink(borderWidths.top(), borderWidths.bottom(), borderWidths.left(), borderWidths.right());
        innerEdgeRect.setRadii(innerRadii);

        if (!innerEdgeRect.isRenderable())
            innerEdgeRect.adjustRadii();
    }

    return innerEdgeRect;
}

} // namespace WebCore
