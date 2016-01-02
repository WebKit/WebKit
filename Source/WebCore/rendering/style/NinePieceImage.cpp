/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2013, 2015 Apple Inc. All rights reserved.
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
#include "NinePieceImage.h"

#include "GraphicsContext.h"
#include "LengthFunctions.h"
#include "RenderStyle.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/PointerComparison.h>

namespace WebCore {

static DataRef<NinePieceImageData>& defaultData()
{
    static NeverDestroyed<DataRef<NinePieceImageData>> data(NinePieceImageData::create());
    return data.get();
}

NinePieceImage::NinePieceImage()
    : m_data(defaultData())
{
}

NinePieceImage::NinePieceImage(PassRefPtr<StyleImage> image, LengthBox imageSlices, bool fill, LengthBox borderSlices, LengthBox outset, ENinePieceImageRule horizontalRule, ENinePieceImageRule verticalRule)
    : m_data(NinePieceImageData::create())
{
    m_data.access()->image = image;
    m_data.access()->imageSlices = WTFMove(imageSlices);
    m_data.access()->borderSlices = WTFMove(borderSlices);
    m_data.access()->outset = WTFMove(outset);
    m_data.access()->fill = fill;
    m_data.access()->horizontalRule = horizontalRule;
    m_data.access()->verticalRule = verticalRule;
}

LayoutUnit NinePieceImage::computeSlice(Length length, LayoutUnit width, LayoutUnit slice, LayoutUnit extent)
{
    if (length.isRelative())
        return length.value() * width;
    if (length.isAuto())
        return slice;
    return valueForLength(length, extent);
}

LayoutBoxExtent NinePieceImage::computeSlices(const LayoutSize& size, const LengthBox& lengths, int scaleFactor)
{
    LayoutUnit top    = std::min<LayoutUnit>(size.height(), valueForLength(lengths.top(),    size.height())) * scaleFactor;
    LayoutUnit right  = std::min<LayoutUnit>(size.width(),  valueForLength(lengths.right(),  size.width()))  * scaleFactor;
    LayoutUnit bottom = std::min<LayoutUnit>(size.height(), valueForLength(lengths.bottom(), size.height())) * scaleFactor;
    LayoutUnit left   = std::min<LayoutUnit>(size.width(),  valueForLength(lengths.left(),   size.width()))  * scaleFactor;
    return LayoutBoxExtent(top, right, bottom, left);
}

LayoutBoxExtent NinePieceImage::computeSlices(const LayoutSize& size, const LengthBox& lengths, const FloatBoxExtent& widths, const LayoutBoxExtent& slices)
{
    LayoutUnit top    = computeSlice(lengths.top(),    widths.top(),    slices.top(),    size.height());
    LayoutUnit right  = computeSlice(lengths.right(),  widths.right(),  slices.right(),  size.width());
    LayoutUnit bottom = computeSlice(lengths.bottom(), widths.bottom(), slices.bottom(), size.height());
    LayoutUnit left   = computeSlice(lengths.left(),   widths.left(),   slices.left(),   size.width());
    return LayoutBoxExtent(top, right, bottom, left);
}

void NinePieceImage::scaleSlicesIfNeeded(const LayoutSize& size, LayoutBoxExtent& slices, float deviceScaleFactor)
{
    LayoutUnit width  = std::max<LayoutUnit>(1 / deviceScaleFactor, slices.left() + slices.right());
    LayoutUnit height = std::max<LayoutUnit>(1 / deviceScaleFactor, slices.top() + slices.bottom());

    float sliceScaleFactor = std::min((float)size.width() / width, (float)size.height() / height);

    if (sliceScaleFactor >= 1)
        return;

    // All slices are reduced by multiplying them by sliceScaleFactor.
    slices.top()    *= sliceScaleFactor;
    slices.right()  *= sliceScaleFactor;
    slices.bottom() *= sliceScaleFactor;
    slices.left()   *= sliceScaleFactor;
}

bool NinePieceImage::isEmptyPieceRect(ImagePiece piece, const LayoutBoxExtent& slices)
{
    if (piece == MiddlePiece)
        return false;

    PhysicalBoxSide horizontalSide = imagePieceHorizontalSide(piece);
    PhysicalBoxSide verticalSide = imagePieceVerticalSide(piece);
    return !((horizontalSide == NilSide || slices.at(horizontalSide)) && (verticalSide == NilSide || slices.at(verticalSide)));
}

bool NinePieceImage::isEmptyPieceRect(ImagePiece piece, const Vector<FloatRect>& destinationRects, const Vector<FloatRect>& sourceRects)
{
    return destinationRects[piece].isEmpty() || sourceRects[piece].isEmpty();
}

Vector<FloatRect> NinePieceImage::computeNineRects(const FloatRect& outer, const LayoutBoxExtent& slices, float deviceScaleFactor)
{
    FloatRect inner = outer;
    inner.move(slices.left(), slices.top());
    inner.contract(slices.left() + slices.right(), slices.top() + slices.bottom());
    ASSERT(outer.contains(inner));

    Vector<FloatRect> rects(MaxPiece);

    rects[TopLeftPiece]     = snapRectToDevicePixels(outer.x(),    outer.y(),    slices.left(),  slices.top(),    deviceScaleFactor);
    rects[BottomLeftPiece]  = snapRectToDevicePixels(outer.x(),    inner.maxY(), slices.left(),  slices.bottom(), deviceScaleFactor);
    rects[LeftPiece]        = snapRectToDevicePixels(outer.x(),    inner.y(),    slices.left(),  inner.height(),  deviceScaleFactor);

    rects[TopRightPiece]    = snapRectToDevicePixels(inner.maxX(), outer.y(),    slices.right(), slices.top(),    deviceScaleFactor);
    rects[BottomRightPiece] = snapRectToDevicePixels(inner.maxX(), inner.maxY(), slices.right(), slices.bottom(), deviceScaleFactor);
    rects[RightPiece]       = snapRectToDevicePixels(inner.maxX(), inner.y(),    slices.right(), inner.height(),  deviceScaleFactor);

    rects[TopPiece]         = snapRectToDevicePixels(inner.x(),    outer.y(),    inner.width(),  slices.top(),    deviceScaleFactor);
    rects[BottomPiece]      = snapRectToDevicePixels(inner.x(),    inner.maxY(), inner.width(),  slices.bottom(), deviceScaleFactor);

    rects[MiddlePiece]      = snapRectToDevicePixels(inner.x(),    inner.y(),    inner.width(),  inner.height(),  deviceScaleFactor);
    return rects;
}

FloatSize NinePieceImage::computeSideTileScale(ImagePiece piece, const Vector<FloatRect>& destinationRects, const Vector<FloatRect>& sourceRects)
{
    ASSERT(!isCornerPiece(piece) && !isMiddlePiece(piece));
    if (isEmptyPieceRect(piece, destinationRects, sourceRects))
        return FloatSize(1, 1);

    float scale;
    if (isHorizontalPiece(piece))
        scale = destinationRects[piece].height() / sourceRects[piece].height();
    else
        scale = destinationRects[piece].width() / sourceRects[piece].width();

    return FloatSize(scale, scale);
}

FloatSize NinePieceImage::computeMiddleTileScale(const Vector<FloatSize>& scales, const Vector<FloatRect>& destinationRects, const Vector<FloatRect>& sourceRects, ENinePieceImageRule hRule, ENinePieceImageRule vRule)
{
    FloatSize scale(1, 1);
    if (isEmptyPieceRect(MiddlePiece, destinationRects, sourceRects))
        return scale;

    // Unlike the side pieces, the middle piece can have "stretch" specified in one axis but not the other.
    // In fact the side pieces don't even use the scale factor unless they have a rule other than "stretch".
    if (hRule == StretchImageRule)
        scale.setWidth(destinationRects[MiddlePiece].width() / sourceRects[MiddlePiece].width());
    else if (!isEmptyPieceRect(TopPiece, destinationRects, sourceRects))
        scale.setWidth(scales[TopPiece].width());
    else if (!isEmptyPieceRect(BottomPiece, destinationRects, sourceRects))
        scale.setWidth(scales[BottomPiece].width());

    if (vRule == StretchImageRule)
        scale.setHeight(destinationRects[MiddlePiece].height() / sourceRects[MiddlePiece].height());
    else if (!isEmptyPieceRect(LeftPiece, destinationRects, sourceRects))
        scale.setHeight(scales[LeftPiece].height());
    else if (!isEmptyPieceRect(RightPiece, destinationRects, sourceRects))
        scale.setHeight(scales[RightPiece].height());

    return scale;
}

Vector<FloatSize> NinePieceImage::computeTileScales(const Vector<FloatRect>& destinationRects, const Vector<FloatRect>& sourceRects, ENinePieceImageRule hRule, ENinePieceImageRule vRule)
{
    Vector<FloatSize> scales(MaxPiece, FloatSize(1, 1));

    scales[TopPiece]    = computeSideTileScale(TopPiece,    destinationRects, sourceRects);
    scales[RightPiece]  = computeSideTileScale(RightPiece,  destinationRects, sourceRects);
    scales[BottomPiece] = computeSideTileScale(BottomPiece, destinationRects, sourceRects);
    scales[LeftPiece]   = computeSideTileScale(LeftPiece,   destinationRects, sourceRects);

    scales[MiddlePiece] = computeMiddleTileScale(scales, destinationRects, sourceRects, hRule, vRule);
    return scales;
}

void NinePieceImage::paint(GraphicsContext& graphicsContext, RenderElement* renderer, const RenderStyle& style, const LayoutRect& destination, const LayoutSize& source, float deviceScaleFactor, CompositeOperator op) const
{
    StyleImage* styleImage = image();
    ASSERT(styleImage && styleImage->isLoaded());

    LayoutBoxExtent sourceSlices = computeSlices(source, imageSlices(), styleImage->imageScaleFactor());
    LayoutBoxExtent destinationSlices = computeSlices(destination.size(), borderSlices(), style.borderWidth(), sourceSlices);

    scaleSlicesIfNeeded(destination.size(), destinationSlices, deviceScaleFactor);

    Vector<FloatRect> destinationRects = computeNineRects(destination, destinationSlices, deviceScaleFactor);
    Vector<FloatRect> sourceRects = computeNineRects(FloatRect(FloatPoint(), source), sourceSlices, deviceScaleFactor);
    Vector<FloatSize> tileScales = computeTileScales(destinationRects, sourceRects, horizontalRule(), verticalRule());

    RefPtr<Image> image = styleImage->image(renderer, source);
    if (!image)
        return;

    for (ImagePiece piece = MinPiece; piece < MaxPiece; ++piece) {
        if ((piece == MiddlePiece && !fill()) || isEmptyPieceRect(piece, destinationRects, sourceRects))
            continue;

        if (isCornerPiece(piece)) {
            graphicsContext.drawImage(*image, destinationRects[piece], sourceRects[piece], op);
            continue;
        }

        Image::TileRule hRule = isHorizontalPiece(piece) ? static_cast<Image::TileRule>(horizontalRule()) : Image::StretchTile;
        Image::TileRule vRule = isVerticalPiece(piece) ? static_cast<Image::TileRule>(verticalRule()) : Image::StretchTile;
        graphicsContext.drawTiledImage(*image, destinationRects[piece], sourceRects[piece], tileScales[piece], hRule, vRule, op);
    }
}

NinePieceImageData::NinePieceImageData()
    : fill(false)
    , horizontalRule(StretchImageRule)
    , verticalRule(StretchImageRule)
    , image(nullptr)
    , imageSlices(Length(100, Percent), Length(100, Percent), Length(100, Percent), Length(100, Percent))
    , borderSlices(Length(1, Relative), Length(1, Relative), Length(1, Relative), Length(1, Relative))
    , outset(0)
{
}

inline NinePieceImageData::NinePieceImageData(const NinePieceImageData& other)
    : RefCounted<NinePieceImageData>()
    , fill(other.fill)
    , horizontalRule(other.horizontalRule)
    , verticalRule(other.verticalRule)
    , image(other.image)
    , imageSlices(other.imageSlices)
    , borderSlices(other.borderSlices)
    , outset(other.outset)
{
}

Ref<NinePieceImageData> NinePieceImageData::copy() const
{
    return adoptRef(*new NinePieceImageData(*this));
}

bool NinePieceImageData::operator==(const NinePieceImageData& other) const
{
    return arePointingToEqualData(image, other.image)
        && imageSlices == other.imageSlices
        && fill == other.fill
        && borderSlices == other.borderSlices
        && outset == other.outset
        && horizontalRule == other.horizontalRule
        && verticalRule == other.verticalRule;
}

}
