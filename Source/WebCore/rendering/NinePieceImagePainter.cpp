/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "NinePieceImagePainter.h"

#include "BoxExtents.h"
#include "GraphicsContext.h"
#include "ImagePaintingOptions.h"
#include "ImageQualityController.h"
#include "LayoutRect.h"
#include "NinePieceGeometry.h"
#include "RenderStyleConstants.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleImage.h"
#include "StyleImageDrawingExtras.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <wtf/Vector.h>

namespace WebCore {

template<typename WidthValue>
static LayoutUnit computeSlice(const WidthValue& length, LayoutUnit width, LayoutUnit slice, LayoutUnit extent, const Style::ZoomFactor& zoom)
{
    return WTF::switchOn(length,
        [&](const typename WidthValue::LengthPercentage& value) {
            return Style::evaluate<LayoutUnit>(value, extent, zoom);
        },
        [&](const typename WidthValue::Number& value) {
            return LayoutUnit { value.value * width };
        },
        [&](const CSS::Keyword::Auto&) {
            return slice;
        }
    );
}

template<typename WidthValues>
static LayoutBoxExtent computeSlices(const LayoutSize& size, const WidthValues& widths, const FloatBoxExtent& borderWidths, const LayoutBoxExtent& slices, const Style::ZoomFactor& zoom)
{
    return {
        computeSlice(widths.values.top(),    LayoutUnit(borderWidths.top()),    slices.top(),    size.height(), zoom),
        computeSlice(widths.values.right(),  LayoutUnit(borderWidths.right()),  slices.right(),  size.width(), zoom),
        computeSlice(widths.values.bottom(), LayoutUnit(borderWidths.bottom()), slices.bottom(), size.height(), zoom),
        computeSlice(widths.values.left(),   LayoutUnit(borderWidths.left()),   slices.left(),   size.width(), zoom),
    };
}

template<typename SliceValues>
static LayoutBoxExtent computeSlices(const LayoutSize& size, const SliceValues& slices, int scaleFactor)
{
    return {
        std::min(size.height(),  Style::evaluate<LayoutUnit>(slices.values.top(),    size.height())) * scaleFactor,
        std::min(size.width(),   Style::evaluate<LayoutUnit>(slices.values.right(),  size.width()))  * scaleFactor,
        std::min(size.height(),  Style::evaluate<LayoutUnit>(slices.values.bottom(), size.height())) * scaleFactor,
        std::min(size.width(),   Style::evaluate<LayoutUnit>(slices.values.left(),   size.width()))  * scaleFactor,
    };
}

static void scaleSlicesIfNeeded(const LayoutSize& size, LayoutBoxExtent& slices, float deviceScaleFactor)
{
    LayoutUnit width  = std::max(LayoutUnit(1 / deviceScaleFactor), slices.left() + slices.right());
    LayoutUnit height = std::max(LayoutUnit(1 / deviceScaleFactor), slices.top() + slices.bottom());

    float sliceScaleFactor = std::min((float)size.width() / width, (float)size.height() / height);

    if (sliceScaleFactor >= 1)
        return;

    // All slices are reduced by multiplying them by sliceScaleFactor.
    slices.top()    *= sliceScaleFactor;
    slices.right()  *= sliceScaleFactor;
    slices.bottom() *= sliceScaleFactor;
    slices.left()   *= sliceScaleFactor;
}

static bool NODELETE isEmptyPieceRect(ImagePiece piece, const NinePieceRects& destinationRects, const NinePieceRects& sourceRects)
{
    return destinationRects[piece].isEmpty() || sourceRects[piece].isEmpty();
}

static NinePieceRects computeNineRects(const FloatRect& outer, const LayoutBoxExtent& slices, float deviceScaleFactor)
{
    using enum ImagePiece;

    FloatRect inner = outer;
    inner.move(slices.left(), slices.top());
    inner.contract(slices.left() + slices.right(), slices.top() + slices.bottom());
    ASSERT(outer.contains(inner));

    NinePieceRects rects;

    auto outerX = LayoutUnit(outer.x());
    auto outerY = LayoutUnit(outer.y());
    auto innerX = LayoutUnit(inner.x());
    auto innerY = LayoutUnit(inner.y());
    auto innerMaxX = LayoutUnit(inner.maxX());
    auto innerMaxY = LayoutUnit(inner.maxY());
    auto innerHeight = LayoutUnit(inner.height());
    auto innerWidth = LayoutUnit(inner.width());

    rects[TopLeft]     = snapRectToDevicePixels(outerX,    outerY,     slices.left(),  slices.top(),    deviceScaleFactor);
    rects[BottomLeft]  = snapRectToDevicePixels(outerX,    innerMaxY,  slices.left(),  slices.bottom(), deviceScaleFactor);
    rects[Left]        = snapRectToDevicePixels(outerX,    innerY,     slices.left(),  innerHeight,     deviceScaleFactor);

    rects[TopRight]    = snapRectToDevicePixels(innerMaxX, outerY,     slices.right(), slices.top(),    deviceScaleFactor);
    rects[BottomRight] = snapRectToDevicePixels(innerMaxX, innerMaxY,  slices.right(), slices.bottom(), deviceScaleFactor);
    rects[Right]       = snapRectToDevicePixels(innerMaxX, innerY,     slices.right(), innerHeight,     deviceScaleFactor);

    rects[Top]         = snapRectToDevicePixels(innerX,    outerY,     innerWidth,     slices.top(),    deviceScaleFactor);
    rects[Bottom]      = snapRectToDevicePixels(innerX,    innerMaxY,  innerWidth,     slices.bottom(), deviceScaleFactor);

    rects[Middle]      = snapRectToDevicePixels(innerX,    innerY,     innerWidth,     innerHeight,     deviceScaleFactor);

    return rects;
}

static FloatSize NODELETE computeSideTileScale(ImagePiece piece, const NinePieceRects& destinationRects, const NinePieceRects& sourceRects)
{
    ASSERT(!isCornerPiece(piece) && piece != ImagePiece::Middle);
    if (isEmptyPieceRect(piece, destinationRects, sourceRects))
        return FloatSize(1, 1);

    float scale;
    if (isHorizontalPiece(piece))
        scale = destinationRects[piece].height() / sourceRects[piece].height();
    else
        scale = destinationRects[piece].width() / sourceRects[piece].width();

    return FloatSize(scale, scale);
}

static FloatSize NODELETE computeMiddleTileScale(const NinePieceScales& scales, const NinePieceRects& destinationRects, const NinePieceRects& sourceRects, NinePieceImageRule hRule, NinePieceImageRule vRule)
{
    using enum ImagePiece;

    FloatSize scale(1, 1);
    if (isEmptyPieceRect(Middle, destinationRects, sourceRects))
        return scale;

    // Unlike the side pieces, the middle piece can have "stretch" specified in one axis but not the other.
    // In fact the side pieces don't even use the scale factor unless they have a rule other than "stretch".
    if (hRule == NinePieceImageRule::Stretch)
        scale.setWidth(destinationRects[Middle].width() / sourceRects[Middle].width());
    else if (!isEmptyPieceRect(Top, destinationRects, sourceRects))
        scale.setWidth(scales[Top].width());
    else if (!isEmptyPieceRect(Bottom, destinationRects, sourceRects))
        scale.setWidth(scales[Bottom].width());

    if (vRule == NinePieceImageRule::Stretch)
        scale.setHeight(destinationRects[Middle].height() / sourceRects[Middle].height());
    else if (!isEmptyPieceRect(Left, destinationRects, sourceRects))
        scale.setHeight(scales[Left].height());
    else if (!isEmptyPieceRect(Right, destinationRects, sourceRects))
        scale.setHeight(scales[Right].height());

    return scale;
}

static NinePieceScales computeTileScales(const NinePieceRects& destinationRects, const NinePieceRects& sourceRects, NinePieceImageRule hRule, NinePieceImageRule vRule)
{
    using enum ImagePiece;

    NinePieceScales scales;
    scales.fill(FloatSize(1, 1));

    scales[Top]    = computeSideTileScale(Top,    destinationRects, sourceRects);
    scales[Right]  = computeSideTileScale(Right,  destinationRects, sourceRects);
    scales[Bottom] = computeSideTileScale(Bottom, destinationRects, sourceRects);
    scales[Left]   = computeSideTileScale(Left,   destinationRects, sourceRects);

    scales[Middle] = computeMiddleTileScale(scales, destinationRects, sourceRects, hRule, vRule);

    return scales;
}

template<typename T>
static void paintNinePieceImage(const T& ninePieceImage, GraphicsContext& graphicsContext, const RenderElement& renderer, const Style::ComputedStyle& style, const LayoutRect& destination, const LayoutSize& source, float deviceScaleFactor, ImagePaintingOptions options)
{
    auto styleImage = ninePieceImage.source().tryStyleImage();
    ASSERT(styleImage);
    ASSERT(styleImage->isLoaded(renderer));

    auto zoom = style.usedZoomForLength();

    auto sourceSlices      = computeSlices(source, ninePieceImage.slice(), styleImage->imageScaleFactor());
    auto destinationSlices = computeSlices(destination.size(), ninePieceImage.width(), Style::evaluate<LayoutBoxExtent>(style.usedBorderWidths().to<Style::LineWidthBox>(), zoom, deviceScaleFactor), sourceSlices, zoom);

    scaleSlicesIfNeeded(destination.size(), destinationSlices, deviceScaleFactor);

    auto destinationRects  = computeNineRects(destination, destinationSlices, deviceScaleFactor);
    auto sourceRects       = computeNineRects(FloatRect(FloatPoint(), source), sourceSlices, deviceScaleFactor);

    auto tileScales = computeTileScales(destinationRects, sourceRects, ninePieceImage.repeat().horizontalRule(), ninePieceImage.repeat().verticalRule());

    auto geometry = NinePieceGeometry {
        .destinationRects = WTF::move(destinationRects),
        .sourceRects = WTF::move(sourceRects),
        .tileScales = WTF::move(tileScales),
        .horizontalRule = ninePieceImage.repeat().horizontalRule(),
        .verticalRule = ninePieceImage.repeat().verticalRule(),
        .fill = ninePieceImage.slice().fill.has_value(),
    };

    InterpolationQualityMaintainer interpolationMaintainer(graphicsContext, ImageQualityController::interpolationQualityFromStyle(style));

    styleImage->drawNinePiece(graphicsContext, renderer, ConcreteObjectSize::fixed(FloatSize(source)), geometry, options);
}

// MARK: - Painter entry point

void NinePieceImagePainter::paint(const Style::BorderImage& ninePieceImage, GraphicsContext& graphicsContext, const RenderElement& renderer, const Style::ComputedStyle& style, const LayoutRect& destination, const LayoutSize& source, float deviceScaleFactor, ImagePaintingOptions options)
{
    return paintNinePieceImage(ninePieceImage, graphicsContext, renderer, style, destination, source, deviceScaleFactor, options);
}

void NinePieceImagePainter::paint(const Style::MaskBorder& ninePieceImage, GraphicsContext& graphicsContext, const RenderElement& renderer, const Style::ComputedStyle& style, const LayoutRect& destination, const LayoutSize& source, float deviceScaleFactor, ImagePaintingOptions options)
{
    return paintNinePieceImage(ninePieceImage, graphicsContext, renderer, style, destination, source, deviceScaleFactor, options);
}

} // namespace WebCore
