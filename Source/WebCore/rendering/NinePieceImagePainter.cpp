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
#include "RenderStyleConstants.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleImage.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <wtf/Vector.h>

namespace WebCore {

// Used for array indexing, so not an enum class.
enum ImagePiece {
    MinPiece = 0,
    TopLeftPiece = MinPiece,
    LeftPiece,
    BottomLeftPiece,
    TopRightPiece,
    RightPiece,
    BottomRightPiece,
    TopPiece,
    BottomPiece,
    MiddlePiece,
    MaxPiece
};

static ImagePiece& operator++(ImagePiece& piece)
{
    piece = static_cast<ImagePiece>(std::to_underlying(piece) + 1);
    return piece;
}

static bool NODELETE isCornerPiece(ImagePiece piece)
{
    return piece == TopLeftPiece || piece == TopRightPiece || piece == BottomLeftPiece || piece == BottomRightPiece;
}

static bool NODELETE isHorizontalPiece(ImagePiece piece)
{
    return piece == TopPiece || piece == BottomPiece || piece == MiddlePiece;
}

static bool NODELETE isVerticalPiece(ImagePiece piece)
{
    return piece == LeftPiece || piece == RightPiece || piece == MiddlePiece;
}

template<typename WidthValue>
static LayoutUnit computeSlice(const WidthValue& length, LayoutUnit width, LayoutUnit slice, LayoutUnit extent, const Style::ZoomFactor&)
{
    return WTF::switchOn(length,
        [&](const typename WidthValue::LengthPercentage& value) {
            return Style::evaluate<LayoutUnit>(value, extent, Style::ZoomNeeded { });
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

static bool NODELETE isEmptyPieceRect(ImagePiece piece, const Vector<FloatRect, MaxPiece>& destinationRects, const Vector<FloatRect, MaxPiece>& sourceRects)
{
    return destinationRects[piece].isEmpty() || sourceRects[piece].isEmpty();
}

static Vector<FloatRect, MaxPiece> computeNineRects(const FloatRect& outer, const LayoutBoxExtent& slices, float deviceScaleFactor)
{
    FloatRect inner = outer;
    inner.move(slices.left(), slices.top());
    inner.contract(slices.left() + slices.right(), slices.top() + slices.bottom());
    ASSERT(outer.contains(inner));

    Vector<FloatRect, MaxPiece> rects(MaxPiece);

    auto outerX = LayoutUnit(outer.x());
    auto outerY = LayoutUnit(outer.y());
    auto innerX = LayoutUnit(inner.x());
    auto innerY = LayoutUnit(inner.y());
    auto innerMaxX = LayoutUnit(inner.maxX());
    auto innerMaxY = LayoutUnit(inner.maxY());
    auto innerHeight = LayoutUnit(inner.height());
    auto innerWidth = LayoutUnit(inner.width());

    rects[TopLeftPiece]     = snapRectToDevicePixels(outerX,    outerY,     slices.left(),  slices.top(),    deviceScaleFactor);
    rects[BottomLeftPiece]  = snapRectToDevicePixels(outerX,    innerMaxY,  slices.left(),  slices.bottom(), deviceScaleFactor);
    rects[LeftPiece]        = snapRectToDevicePixels(outerX,    innerY,     slices.left(),  innerHeight,     deviceScaleFactor);

    rects[TopRightPiece]    = snapRectToDevicePixels(innerMaxX, outerY,     slices.right(), slices.top(),    deviceScaleFactor);
    rects[BottomRightPiece] = snapRectToDevicePixels(innerMaxX, innerMaxY,  slices.right(), slices.bottom(), deviceScaleFactor);
    rects[RightPiece]       = snapRectToDevicePixels(innerMaxX, innerY,     slices.right(), innerHeight,     deviceScaleFactor);

    rects[TopPiece]         = snapRectToDevicePixels(innerX,    outerY,     innerWidth,     slices.top(),    deviceScaleFactor);
    rects[BottomPiece]      = snapRectToDevicePixels(innerX,    innerMaxY,  innerWidth,     slices.bottom(), deviceScaleFactor);

    rects[MiddlePiece]      = snapRectToDevicePixels(innerX,    innerY,     innerWidth,     innerHeight,     deviceScaleFactor);

    return rects;
}

static FloatSize computeSideTileScale(ImagePiece piece, const Vector<FloatRect, MaxPiece>& destinationRects, const Vector<FloatRect, MaxPiece>& sourceRects)
{
    ASSERT(!isCornerPiece(piece) && piece != MiddlePiece);
    if (isEmptyPieceRect(piece, destinationRects, sourceRects))
        return FloatSize(1, 1);

    float scale;
    if (isHorizontalPiece(piece))
        scale = destinationRects[piece].height() / sourceRects[piece].height();
    else
        scale = destinationRects[piece].width() / sourceRects[piece].width();

    return FloatSize(scale, scale);
}

static FloatSize computeMiddleTileScale(const Vector<FloatSize, MaxPiece>& scales, const Vector<FloatRect, MaxPiece>& destinationRects, const Vector<FloatRect, MaxPiece>& sourceRects, NinePieceImageRule hRule, NinePieceImageRule vRule)
{
    FloatSize scale(1, 1);
    if (isEmptyPieceRect(MiddlePiece, destinationRects, sourceRects))
        return scale;

    // Unlike the side pieces, the middle piece can have "stretch" specified in one axis but not the other.
    // In fact the side pieces don't even use the scale factor unless they have a rule other than "stretch".
    if (hRule == NinePieceImageRule::Stretch)
        scale.setWidth(destinationRects[MiddlePiece].width() / sourceRects[MiddlePiece].width());
    else if (!isEmptyPieceRect(TopPiece, destinationRects, sourceRects))
        scale.setWidth(scales[TopPiece].width());
    else if (!isEmptyPieceRect(BottomPiece, destinationRects, sourceRects))
        scale.setWidth(scales[BottomPiece].width());

    if (vRule == NinePieceImageRule::Stretch)
        scale.setHeight(destinationRects[MiddlePiece].height() / sourceRects[MiddlePiece].height());
    else if (!isEmptyPieceRect(LeftPiece, destinationRects, sourceRects))
        scale.setHeight(scales[LeftPiece].height());
    else if (!isEmptyPieceRect(RightPiece, destinationRects, sourceRects))
        scale.setHeight(scales[RightPiece].height());

    return scale;
}

static Vector<FloatSize, MaxPiece> computeTileScales(const Vector<FloatRect, MaxPiece>& destinationRects, const Vector<FloatRect, MaxPiece>& sourceRects, NinePieceImageRule hRule, NinePieceImageRule vRule)
{
    Vector<FloatSize, MaxPiece> scales(MaxPiece, FloatSize(1, 1));

    scales[TopPiece]    = computeSideTileScale(TopPiece,    destinationRects, sourceRects);
    scales[RightPiece]  = computeSideTileScale(RightPiece,  destinationRects, sourceRects);
    scales[BottomPiece] = computeSideTileScale(BottomPiece, destinationRects, sourceRects);
    scales[LeftPiece]   = computeSideTileScale(LeftPiece,   destinationRects, sourceRects);

    scales[MiddlePiece] = computeMiddleTileScale(scales, destinationRects, sourceRects, hRule, vRule);

    return scales;
}

template<typename T>
static void paintNinePieceImage(const T& ninePieceImage, GraphicsContext& graphicsContext, const RenderElement* renderer, const RenderStyle& style, const LayoutRect& destination, const LayoutSize& source, float deviceScaleFactor, ImagePaintingOptions options)
{
    auto styleImage = ninePieceImage.source().tryStyleImage();
    ASSERT(styleImage);
    ASSERT(styleImage->isLoaded(renderer));

    auto sourceSlices      = computeSlices(source, ninePieceImage.slice(), styleImage->imageScaleFactor());
    auto destinationSlices = computeSlices(destination.size(), ninePieceImage.width(), Style::evaluate<LayoutBoxExtent>(style.usedBorderWidths().to<Style::LineWidthBox>(), Style::ZoomNeeded { }), sourceSlices, style.usedZoomForLength());

    scaleSlicesIfNeeded(destination.size(), destinationSlices, deviceScaleFactor);

    auto destinationRects  = computeNineRects(destination, destinationSlices, deviceScaleFactor);
    auto sourceRects       = computeNineRects(FloatRect(FloatPoint(), source), sourceSlices, deviceScaleFactor);

    auto tileScales = computeTileScales(destinationRects, sourceRects, ninePieceImage.repeat().horizontalRule(), ninePieceImage.repeat().verticalRule());

    RefPtr image = styleImage->image(renderer, source, graphicsContext);
    if (!image)
        return;

    InterpolationQualityMaintainer interpolationMaintainer(graphicsContext, ImageQualityController::interpolationQualityFromStyle(style));

    for (ImagePiece piece = MinPiece; piece < MaxPiece; ++piece) {
        if ((piece == MiddlePiece && !ninePieceImage.slice().fill) || isEmptyPieceRect(piece, destinationRects, sourceRects))
            continue;

        if (isCornerPiece(piece)) {
            graphicsContext.drawImage(*image, destinationRects[piece], sourceRects[piece], options);
            continue;
        }

        auto hRule = isHorizontalPiece(piece)
            ? static_cast<Image::TileRule>(ninePieceImage.repeat().horizontalRule())
            : Image::StretchTile;

        auto vRule = isVerticalPiece(piece)
            ? static_cast<Image::TileRule>(ninePieceImage.repeat().verticalRule())
            : Image::StretchTile;

        graphicsContext.drawTiledImage(*image, destinationRects[piece], sourceRects[piece], tileScales[piece], hRule, vRule, options);
    }
}

// MARK: - Painter entry point

void NinePieceImagePainter::paint(const Style::BorderImage& ninePieceImage, GraphicsContext& graphicsContext, const RenderElement* renderer, const RenderStyle& style, const LayoutRect& destination, const LayoutSize& source, float deviceScaleFactor, ImagePaintingOptions options)
{
    return paintNinePieceImage(ninePieceImage, graphicsContext, renderer, style, destination, source, deviceScaleFactor, options);
}

void NinePieceImagePainter::paint(const Style::MaskBorder& ninePieceImage, GraphicsContext& graphicsContext, const RenderElement* renderer, const RenderStyle& style, const LayoutRect& destination, const LayoutSize& source, float deviceScaleFactor, ImagePaintingOptions options)
{
    return paintNinePieceImage(ninePieceImage, graphicsContext, renderer, style, destination, source, deviceScaleFactor, options);
}

} // namespace WebCore
