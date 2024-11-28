/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "LengthFunctions.h"
#include "Path.h"
#include "RenderStyleInlines.h"
#include "RoundedRect.h"

namespace WebCore {

static RoundedRect::Radii calcRadiiFor(const BorderData::Radii& radii, const LayoutSize& size)
{
    return {
        sizeForLengthSize(radii.topLeft, size),
        sizeForLengthSize(radii.topRight, size),
        sizeForLengthSize(radii.bottomLeft, size),
        sizeForLengthSize(radii.bottomRight, size)
    };
}

BorderShape BorderShape::shapeForBorderRect(const RenderStyle& style, const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    auto borderWidths = RectEdges<LayoutUnit> {
        LayoutUnit(style.borderTopWidth()),
        LayoutUnit(style.borderRightWidth()),
        LayoutUnit(style.borderBottomWidth()),
        LayoutUnit(style.borderLeftWidth()),
    };
    return shapeForBorderRect(style, borderRect, borderWidths, includeLogicalLeftEdge, includeLogicalRightEdge);
}

BorderShape BorderShape::shapeForBorderRect(const RenderStyle& style, const LayoutRect& borderRect, const RectEdges<LayoutUnit>& overrideBorderWidths, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    bool isHorizontal = style.writingMode().isHorizontal();

    // top, right, bottom, left.
    auto usedBorderWidths = RectEdges<LayoutUnit> {
        LayoutUnit((isHorizontal || includeLogicalLeftEdge) ? overrideBorderWidths.top() : 0_lu),
        LayoutUnit((!isHorizontal || includeLogicalRightEdge) ? overrideBorderWidths.right() : 0_lu),
        LayoutUnit((isHorizontal || includeLogicalRightEdge) ? overrideBorderWidths.bottom() : 0_lu),
        LayoutUnit((!isHorizontal || includeLogicalLeftEdge) ? overrideBorderWidths.left() : 0_lu),
    };

    if (style.hasBorderRadius()) {
        auto radii = calcRadiiFor(style.borderRadii(), borderRect.size());
        radii.scale(calcBorderRadiiConstraintScaleFor(borderRect, radii));

        if (!includeLogicalLeftEdge) {
            radii.setTopLeft({ });

            if (isHorizontal)
                radii.setBottomLeft({ });
            else
                radii.setTopRight({ });
        }

        if (!includeLogicalRightEdge) {
            radii.setBottomRight({ });

            if (isHorizontal)
                radii.setTopRight({ });
            else
                radii.setBottomLeft({ });
        }

        if (!radii.areRenderableInRect(borderRect))
            radii.makeRenderableInRect(borderRect);

        return BorderShape { borderRect, usedBorderWidths, radii };
    }

    return BorderShape { borderRect, usedBorderWidths };
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths)
    : m_borderRect(borderRect)
    , m_borderWidths(borderWidths)
{
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const RoundedRectRadii& radii)
    : m_borderRect(borderRect, radii)
    , m_borderWidths(borderWidths)
{
    // The caller should have adjusted the radii already.
    ASSERT(m_borderRect.isRenderable());
}

RoundedRect BorderShape::deprecatedRoundedRect() const
{
    return m_borderRect;
}

RoundedRect BorderShape::deprecatedInnerRoundedRect() const
{
    return innerEdgeRoundedRect();
}

FloatRoundedRect BorderShape::deprecatedPixelSnappedRoundedRect(float deviceScaleFactor) const
{
    return m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

FloatRoundedRect BorderShape::deprecatedPixelSnappedInnerRoundedRect(float deviceScaleFactor) const
{
    return innerEdgeRoundedRect().pixelSnappedRoundedRectForPainting(deviceScaleFactor);
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
    return innerEdgeRoundedRect().contains(rect);
}

bool BorderShape::outerShapeContains(const LayoutRect& rect) const
{
    return m_borderRect.contains(rect);
}

void BorderShape::move(LayoutSize offset)
{
    m_borderRect.move(offset);
}

void BorderShape::inflate(LayoutUnit amount)
{
    m_borderRect.inflateWithRadii(amount);
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
    auto pixelSnappedRect = innerEdgeRoundedRect().pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());

    Path path;
    addRoundedRectToPath(pixelSnappedRect, path);
    return path;
}

Path BorderShape::pathForBorderArea(float deviceScaleFactor) const
{
    auto pixelSnappedOuterRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto pixelSnappedInnerRect = innerEdgeRoundedRect().pixelSnappedRoundedRectForPainting(deviceScaleFactor);

    ASSERT(pixelSnappedInnerRect.isRenderable());

    Path path;
    addRoundedRectToPath(pixelSnappedOuterRect, path);
    addRoundedRectToPath(pixelSnappedInnerRect, path);
    return path;
}

void BorderShape::clipToOuterShape(GraphicsContext& context, float deviceScaleFactor)
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isRounded())
        context.clipRoundedRect(pixelSnappedRect);
    else
        context.clip(pixelSnappedRect.rect());
}

void BorderShape::clipToInnerShape(GraphicsContext& context, float deviceScaleFactor)
{
    auto pixelSnappedRect = innerEdgeRoundedRect().pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    if (pixelSnappedRect.isRounded())
        context.clipRoundedRect(pixelSnappedRect);
    else
        context.clip(pixelSnappedRect.rect());
}

void BorderShape::clipOutOuterShape(GraphicsContext& context, float deviceScaleFactor)
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isEmpty())
        return;

    if (pixelSnappedRect.isRounded())
        context.clipOutRoundedRect(pixelSnappedRect);
    else
        context.clipOut(pixelSnappedRect.rect());
}

void BorderShape::clipOutInnerShape(GraphicsContext& context, float deviceScaleFactor)
{
    auto pixelSnappedRect = innerEdgeRoundedRect().pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isEmpty())
        return;

    if (pixelSnappedRect.isRounded())
        context.clipOutRoundedRect(pixelSnappedRect);
    else
        context.clipOut(pixelSnappedRect.rect());
}

void BorderShape::fillOuterShape(GraphicsContext& context, const Color& color, float deviceScaleFactor)
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isRounded())
        context.fillRoundedRect(pixelSnappedRect, color);
    else
        context.fillRect(pixelSnappedRect.rect(), color);
}

void BorderShape::fillInnerShape(GraphicsContext& context, const Color& color, float deviceScaleFactor)
{
    auto pixelSnappedRect = innerEdgeRoundedRect().pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    if (pixelSnappedRect.isRounded())
        context.fillRoundedRect(pixelSnappedRect, color);
    else
        context.fillRect(pixelSnappedRect.rect(), color);
}

RoundedRect BorderShape::innerEdgeRoundedRect() const
{
    auto roundedRect = RoundedRect { innerEdgeRect() };
    if (m_borderRect.isRounded()) {
        auto innerRadii = m_borderRect.radii();
        innerRadii.shrink(m_borderWidths.top(), m_borderWidths.bottom(), m_borderWidths.left(), m_borderWidths.right());
        roundedRect.setRadii(innerRadii);
    }

    if (!roundedRect.isRenderable())
        roundedRect.adjustRadii();

    return roundedRect;
}

LayoutRect BorderShape::innerEdgeRect() const
{
    auto& borderRect = m_borderRect.rect();
    auto width = std::max(0_lu, borderRect.width() - m_borderWidths.left() - m_borderWidths.right());
    auto height = std::max(0_lu, borderRect.height() - m_borderWidths.top() - m_borderWidths.bottom());
    return {
        borderRect.x() + m_borderWidths.left(),
        borderRect.y() + m_borderWidths.top(),
        width,
        height
    };
}

} // namespace WebCore
