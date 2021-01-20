/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
#include "ImageQualityController.h"
#include "LengthFunctions.h"
#include "RenderStyle.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/PointerComparison.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

inline DataRef<NinePieceImage::Data>& NinePieceImage::defaultData()
{
    static NeverDestroyed<DataRef<Data>> data { Data::create() };
    return data.get();
}

inline DataRef<NinePieceImage::Data>& NinePieceImage::defaultMaskData()
{
    static NeverDestroyed<DataRef<Data>> maskData { Data::create() };
    auto& data = maskData.get().access();
    data.imageSlices = LengthBox(0);
    data.fill = true;
    data.borderSlices = LengthBox();
    return maskData.get();
}

NinePieceImage::NinePieceImage(Type imageType)
    : m_data(imageType == Type::Normal ? defaultData() : defaultMaskData())
{
}

NinePieceImage::NinePieceImage(RefPtr<StyleImage>&& image, LengthBox imageSlices, bool fill, LengthBox borderSlices, LengthBox outset, NinePieceImageRule horizontalRule, NinePieceImageRule verticalRule)
    : m_data(Data::create(WTFMove(image), imageSlices, fill, borderSlices, outset, horizontalRule, verticalRule))
{
}

LayoutUnit NinePieceImage::computeSlice(Length length, LayoutUnit width, LayoutUnit slice, LayoutUnit extent)
{
    if (length.isRelative())
        return LayoutUnit(length.value() * width);
    if (length.isAuto())
        return slice;
    return valueForLength(length, extent);
}

LayoutBoxExtent NinePieceImage::computeSlices(const LayoutSize& size, const LengthBox& lengths, int scaleFactor)
{
    return {
        std::min(size.height(), valueForLength(lengths.top(), size.height())) * scaleFactor,
        std::min(size.width(), valueForLength(lengths.right(), size.width()))  * scaleFactor,
        std::min(size.height(), valueForLength(lengths.bottom(), size.height())) * scaleFactor,
        std::min(size.width(), valueForLength(lengths.left(), size.width()))  * scaleFactor
    };
}

LayoutBoxExtent NinePieceImage::computeSlices(const LayoutSize& size, const LengthBox& lengths, const FloatBoxExtent& widths, const LayoutBoxExtent& slices)
{
    return {
        computeSlice(lengths.top(), LayoutUnit(widths.top()), slices.top(), size.height()),
        computeSlice(lengths.right(), LayoutUnit(widths.right()), slices.right(), size.width()),
        computeSlice(lengths.bottom(), LayoutUnit(widths.bottom()), slices.bottom(), size.height()),
        computeSlice(lengths.left(), LayoutUnit(widths.left()), slices.left(), size.width())
    };
}

void NinePieceImage::scaleSlicesIfNeeded(const LayoutSize& size, LayoutBoxExtent& slices, float deviceScaleFactor)
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

bool NinePieceImage::isEmptyPieceRect(ImagePiece piece, const LayoutBoxExtent& slices)
{
    if (piece == MiddlePiece)
        return false;

    auto horizontalSide = imagePieceHorizontalSide(piece);
    auto verticalSide = imagePieceVerticalSide(piece);
    return !((!horizontalSide || slices.at(*horizontalSide)) && (!verticalSide || slices.at(*verticalSide)));
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

    rects[TopLeftPiece] = snapRectToDevicePixels(LayoutUnit(outer.x()), LayoutUnit(outer.y()), slices.left(), slices.top(), deviceScaleFactor);
    rects[BottomLeftPiece] = snapRectToDevicePixels(LayoutUnit(outer.x()), LayoutUnit(inner.maxY()), slices.left(), slices.bottom(), deviceScaleFactor);
    rects[LeftPiece] = snapRectToDevicePixels(LayoutUnit(outer.x()), LayoutUnit(inner.y()), slices.left(), LayoutUnit(inner.height()), deviceScaleFactor);

    rects[TopRightPiece] = snapRectToDevicePixels(LayoutUnit(inner.maxX()), LayoutUnit(outer.y()), slices.right(), slices.top(), deviceScaleFactor);
    rects[BottomRightPiece] = snapRectToDevicePixels(LayoutUnit(inner.maxX()), LayoutUnit(inner.maxY()), slices.right(), slices.bottom(), deviceScaleFactor);
    rects[RightPiece] = snapRectToDevicePixels(LayoutUnit(inner.maxX()), LayoutUnit(inner.y()), slices.right(), LayoutUnit(inner.height()), deviceScaleFactor);

    rects[TopPiece] = snapRectToDevicePixels(LayoutUnit(inner.x()), LayoutUnit(outer.y()), LayoutUnit(inner.width()), slices.top(), deviceScaleFactor);
    rects[BottomPiece] = snapRectToDevicePixels(LayoutUnit(inner.x()), LayoutUnit(inner.maxY()), LayoutUnit(inner.width()), slices.bottom(), deviceScaleFactor);

    rects[MiddlePiece] = snapRectToDevicePixels(LayoutUnit(inner.x()), LayoutUnit(inner.y()), LayoutUnit(inner.width()), LayoutUnit(inner.height()), deviceScaleFactor);
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

FloatSize NinePieceImage::computeMiddleTileScale(const Vector<FloatSize>& scales, const Vector<FloatRect>& destinationRects, const Vector<FloatRect>& sourceRects, NinePieceImageRule hRule, NinePieceImageRule vRule)
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

Vector<FloatSize> NinePieceImage::computeTileScales(const Vector<FloatRect>& destinationRects, const Vector<FloatRect>& sourceRects, NinePieceImageRule hRule, NinePieceImageRule vRule)
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
    ASSERT(styleImage);
    ASSERT(styleImage->isLoaded());

    LayoutBoxExtent sourceSlices = computeSlices(source, imageSlices(), styleImage->imageScaleFactor());
    LayoutBoxExtent destinationSlices = computeSlices(destination.size(), borderSlices(), style.borderWidth(), sourceSlices);

    scaleSlicesIfNeeded(destination.size(), destinationSlices, deviceScaleFactor);

    Vector<FloatRect> destinationRects = computeNineRects(destination, destinationSlices, deviceScaleFactor);
    Vector<FloatRect> sourceRects = computeNineRects(FloatRect(FloatPoint(), source), sourceSlices, deviceScaleFactor);
    Vector<FloatSize> tileScales = computeTileScales(destinationRects, sourceRects, horizontalRule(), verticalRule());

    RefPtr<Image> image = styleImage->image(renderer, source);
    if (!image)
        return;

    InterpolationQualityMaintainer interpolationMaintainer(graphicsContext, ImageQualityController::interpolationQualityFromStyle(style));
    for (ImagePiece piece = MinPiece; piece < MaxPiece; ++piece) {
        if ((piece == MiddlePiece && !fill()) || isEmptyPieceRect(piece, destinationRects, sourceRects))
            continue;

        if (isCornerPiece(piece)) {
            graphicsContext.drawImage(*image, destinationRects[piece], sourceRects[piece], { op, ImageOrientation::FromImage });
            continue;
        }

        Image::TileRule hRule = isHorizontalPiece(piece) ? static_cast<Image::TileRule>(horizontalRule()) : Image::StretchTile;
        Image::TileRule vRule = isVerticalPiece(piece) ? static_cast<Image::TileRule>(verticalRule()) : Image::StretchTile;
        graphicsContext.drawTiledImage(*image, destinationRects[piece], sourceRects[piece], tileScales[piece], hRule, vRule, { op, ImageOrientation::FromImage });
    }
}

inline NinePieceImage::Data::Data() = default;

inline NinePieceImage::Data::Data(RefPtr<StyleImage>&& image, LengthBox imageSlices, bool fill, LengthBox borderSlices, LengthBox outset, NinePieceImageRule horizontalRule, NinePieceImageRule verticalRule)
    : fill(fill)
    , horizontalRule(horizontalRule)
    , verticalRule(verticalRule)
    , image(WTFMove(image))
    , imageSlices(imageSlices)
    , borderSlices(borderSlices)
    , outset(outset)
{
}

inline NinePieceImage::Data::Data(const Data& other)
    : RefCounted<Data>()
    , fill(other.fill)
    , horizontalRule(other.horizontalRule)
    , verticalRule(other.verticalRule)
    , image(other.image)
    , imageSlices(other.imageSlices)
    , borderSlices(other.borderSlices)
    , outset(other.outset)
{
}

inline Ref<NinePieceImage::Data> NinePieceImage::Data::create()
{
    return adoptRef(*new Data);
}

inline Ref<NinePieceImage::Data> NinePieceImage::Data::create(RefPtr<StyleImage>&& image, LengthBox imageSlices, bool fill, LengthBox borderSlices, LengthBox outset, NinePieceImageRule horizontalRule, NinePieceImageRule verticalRule)
{
    return adoptRef(*new Data(WTFMove(image), imageSlices, fill, borderSlices, outset, horizontalRule, verticalRule));
}

Ref<NinePieceImage::Data> NinePieceImage::Data::copy() const
{
    return adoptRef(*new Data(*this));
}

bool NinePieceImage::Data::operator==(const Data& other) const
{
    return arePointingToEqualData(image, other.image)
        && imageSlices == other.imageSlices
        && fill == other.fill
        && borderSlices == other.borderSlices
        && outset == other.outset
        && horizontalRule == other.horizontalRule
        && verticalRule == other.verticalRule;
}

TextStream& operator<<(TextStream& ts, const NinePieceImage& image)
{
    ts << "style-image " << image.image() << " slices " << image.imageSlices();
    return ts;
}

}
