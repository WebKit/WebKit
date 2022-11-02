/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DisplayFillLayerImageGeometry.h"

#include "DisplayBox.h"
#include "FillLayer.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LengthFunctions.h"
#include "RenderStyle.h"

namespace WebCore {
namespace Display {

static inline LayoutUnit resolveWidthForRatio(LayoutUnit height, const LayoutSize& intrinsicRatio)
{
    return height * intrinsicRatio.width() / intrinsicRatio.height();
}

static inline LayoutUnit resolveHeightForRatio(LayoutUnit width, const LayoutSize& intrinsicRatio)
{
    return width * intrinsicRatio.height() / intrinsicRatio.width();
}

static inline LayoutSize resolveAgainstIntrinsicWidthOrHeightAndRatio(LayoutSize size, LayoutSize intrinsicRatio, LayoutUnit useWidth, LayoutUnit useHeight)
{
    if (intrinsicRatio.isEmpty()) {
        if (useWidth)
            return LayoutSize(useWidth, size.height());

        return LayoutSize(size.width(), useHeight);
    }

    if (useWidth)
        return LayoutSize(useWidth, resolveHeightForRatio(useWidth, intrinsicRatio));

    return LayoutSize(resolveWidthForRatio(useHeight, intrinsicRatio), useHeight);
}

static inline LayoutSize resolveAgainstIntrinsicRatio(LayoutSize size, const LayoutSize& intrinsicRatio)
{
    // Two possible solutions: (size.width(), solutionHeight) or (solutionWidth, size.height())
    // "... must be assumed to be the largest dimensions..." = easiest answer: the rect with the largest surface area.

    LayoutUnit solutionWidth = resolveWidthForRatio(size.height(), intrinsicRatio);
    LayoutUnit solutionHeight = resolveHeightForRatio(size.width(), intrinsicRatio);
    if (solutionWidth <= size.width()) {
        if (solutionHeight <= size.height()) {
            // If both solutions fit, choose the one covering the larger area.
            LayoutUnit areaOne = solutionWidth * size.height();
            LayoutUnit areaTwo = size.width() * solutionHeight;
            if (areaOne < areaTwo)
                return LayoutSize(size.width(), solutionHeight);

            return LayoutSize(solutionWidth, size.height());
        }

        // Only the first solution fits.
        return LayoutSize(solutionWidth, size.height());
    }

    // Only the second solution fits, assert that.
    ASSERT(solutionHeight <= size.height());
    return LayoutSize(size.width(), solutionHeight);
}

static LayoutSize calculateImageIntrinsicDimensions(StyleImage* image, LayoutSize positioningAreaSize)
{
    // A generated image without a fixed size, will always return the container size as intrinsic size.
    if (!image->imageHasNaturalDimensions())
        return LayoutSize(positioningAreaSize.width(), positioningAreaSize.height());

    // FIXME: Call computeIntrinsicDimensions().
    auto imageSize = image->imageSize(nullptr, 1);
    auto intrinsicRatio = imageSize;
    Length intrinsicWidth = Length(intrinsicRatio.width(), LengthType::Fixed);
    Length intrinsicHeight = Length(intrinsicRatio.height(), LengthType::Fixed);

    ASSERT(!intrinsicWidth.isPercentOrCalculated());
    ASSERT(!intrinsicHeight.isPercentOrCalculated());

    LayoutSize resolvedSize(intrinsicWidth.value(), intrinsicHeight.value());
    LayoutSize minimumSize(resolvedSize.width() > 0 ? 1 : 0, resolvedSize.height() > 0 ? 1 : 0);

    // FIXME: Respect ScaleByEffectiveZoom.
    resolvedSize.clampToMinimumSize(minimumSize);

    if (!resolvedSize.isEmpty())
        return resolvedSize;

    // If the image has one of either an intrinsic width or an intrinsic height:
    // * and an intrinsic aspect ratio, then the missing dimension is calculated from the given dimension and the ratio.
    // * and no intrinsic aspect ratio, then the missing dimension is assumed to be the size of the rectangle that
    //   establishes the coordinate system for the 'background-position' property.
    if (resolvedSize.width() > 0 || resolvedSize.height() > 0)
        return resolveAgainstIntrinsicWidthOrHeightAndRatio(positioningAreaSize, LayoutSize(intrinsicRatio), resolvedSize.width(), resolvedSize.height());

    // If the image has no intrinsic dimensions and has an intrinsic ratio the dimensions must be assumed to be the
    // largest dimensions at that ratio such that neither dimension exceeds the dimensions of the rectangle that
    // establishes the coordinate system for the 'background-position' property.
    if (!intrinsicRatio.isEmpty())
        return resolveAgainstIntrinsicRatio(positioningAreaSize, LayoutSize(intrinsicRatio));

    // If the image has no intrinsic ratio either, then the dimensions must be assumed to be the rectangle that
    // establishes the coordinate system for the 'background-position' property.
    return positioningAreaSize;
}

static LayoutSize calculateFillTileSize(const FillLayer& fillLayer, LayoutSize positioningAreaSize, float pixelSnappingFactor)
{
    StyleImage* image = fillLayer.image();
    FillSizeType type = fillLayer.size().type;
    auto devicePixelSize = LayoutUnit { 1.0 / pixelSnappingFactor };

    LayoutSize imageIntrinsicSize;
    if (image) {
        imageIntrinsicSize = calculateImageIntrinsicDimensions(image, positioningAreaSize);
        imageIntrinsicSize.scale(1 / image->imageScaleFactor(), 1 / image->imageScaleFactor());
    } else
        imageIntrinsicSize = positioningAreaSize;

    switch (type) {
    case FillSizeType::Size: {
        LayoutSize tileSize = positioningAreaSize;

        Length layerWidth = fillLayer.size().size.width;
        Length layerHeight = fillLayer.size().size.height;

        if (layerWidth.isFixed())
            tileSize.setWidth(layerWidth.value());
        else if (layerWidth.isPercentOrCalculated()) {
            auto resolvedWidth = valueForLength(layerWidth, positioningAreaSize.width());
            // Non-zero resolved value should always produce some content.
            tileSize.setWidth(!resolvedWidth ? resolvedWidth : std::max(devicePixelSize, resolvedWidth));
        }
        
        if (layerHeight.isFixed())
            tileSize.setHeight(layerHeight.value());
        else if (layerHeight.isPercentOrCalculated()) {
            auto resolvedHeight = valueForLength(layerHeight, positioningAreaSize.height());
            // Non-zero resolved value should always produce some content.
            tileSize.setHeight(!resolvedHeight ? resolvedHeight : std::max(devicePixelSize, resolvedHeight));
        }

        // If one of the values is auto we have to use the appropriate
        // scale to maintain our aspect ratio.
        bool hasNaturalAspectRatio = image && image->imageHasNaturalDimensions();
        if (layerWidth.isAuto() && !layerHeight.isAuto()) {
            if (hasNaturalAspectRatio && imageIntrinsicSize.height())
                tileSize.setWidth(imageIntrinsicSize.width() * tileSize.height() / imageIntrinsicSize.height());
        } else if (!layerWidth.isAuto() && layerHeight.isAuto()) {
            if (hasNaturalAspectRatio && imageIntrinsicSize.width())
                tileSize.setHeight(imageIntrinsicSize.height() * tileSize.width() / imageIntrinsicSize.width());
        } else if (layerWidth.isAuto() && layerHeight.isAuto()) {
            // If both width and height are auto, use the image's intrinsic size.
            tileSize = imageIntrinsicSize;
        }

        tileSize.clampNegativeToZero();
        return tileSize;
    }
    case FillSizeType::None: {
        // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
        if (!imageIntrinsicSize.isEmpty())
            return imageIntrinsicSize;

        // If the image has neither an intrinsic width nor an intrinsic height, its size is determined as for ‘contain’.
        type = FillSizeType::Contain;
    }
    FALLTHROUGH;
    case FillSizeType::Contain:
    case FillSizeType::Cover: {
        // Scale computation needs higher precision than what LayoutUnit can offer.
        FloatSize localImageIntrinsicSize = imageIntrinsicSize;
        FloatSize localPositioningAreaSize = positioningAreaSize;

        float horizontalScaleFactor = localImageIntrinsicSize.width() ? (localPositioningAreaSize.width() / localImageIntrinsicSize.width()) : 1;
        float verticalScaleFactor = localImageIntrinsicSize.height() ? (localPositioningAreaSize.height() / localImageIntrinsicSize.height()) : 1;
        float scaleFactor = type == FillSizeType::Contain ? std::min(horizontalScaleFactor, verticalScaleFactor) : std::max(horizontalScaleFactor, verticalScaleFactor);
        
        if (localImageIntrinsicSize.isEmpty())
            return { };
        
        return LayoutSize(localImageIntrinsicSize.scaled(scaleFactor).expandedTo({ devicePixelSize, devicePixelSize }));
    }
    }

    ASSERT_NOT_REACHED();
    return { };
}

static inline LayoutUnit getSpace(LayoutUnit areaSize, LayoutUnit tileSize)
{
    int numberOfTiles = areaSize / tileSize;
    LayoutUnit space = -1;

    if (numberOfTiles > 1)
        space = (areaSize - numberOfTiles * tileSize) / (numberOfTiles - 1);

    return space;
}

static LayoutUnit resolveEdgeRelativeLength(const Length& length, Edge edge, LayoutUnit availableSpace, const LayoutSize& areaSize, const LayoutSize& tileSize)
{
    LayoutUnit result = minimumValueForLength(length, availableSpace);

    if (edge == Edge::Right)
        return areaSize.width() - tileSize.width() - result;
    
    if (edge == Edge::Bottom)
        return areaSize.height() - tileSize.height() - result;

    return result;
}

static FillLayerImageGeometry pixelSnappedFillLayerImageGeometry(LayoutRect& destinationRect, LayoutSize& tileSize, LayoutSize& phase, LayoutSize& space, FillAttachment attachment, float pixelSnappingFactor)
{
    return FillLayerImageGeometry {
        snapRectToDevicePixels(destinationRect, pixelSnappingFactor),
        snapRectToDevicePixels({ destinationRect.location(), tileSize }, pixelSnappingFactor).size(),
        snapRectToDevicePixels({ destinationRect.location(), phase }, pixelSnappingFactor).size(),
        snapRectToDevicePixels({ { }, space }, pixelSnappingFactor).size(),
        attachment
    };
}

static FillLayerImageGeometry geometryForLayer(const FillLayer& fillLayer, LayoutRect borderBoxRect, const Layout::BoxGeometry& geometry, float pixelSnappingFactor)
{
    LayoutUnit left;
    LayoutUnit top;
    LayoutSize positioningAreaSize;

    auto destinationRect = borderBoxRect;

    switch (fillLayer.attachment()) {
    case FillAttachment::ScrollBackground:
    case FillAttachment::LocalBackground: {
        LayoutUnit right;
        LayoutUnit bottom;
        if (fillLayer.origin() != FillBox::Border) {
            left = geometry.borderStart();
            right = geometry.borderEnd();
            top = geometry.borderBefore();
            bottom = geometry.borderAfter();
            if (fillLayer.origin() == FillBox::Content) {
                left += geometry.paddingStart().value_or(0);
                right += geometry.paddingEnd().value_or(0);
                top += geometry.paddingBefore().value_or(0);
                bottom += geometry.paddingAfter().value_or(0);
            }
        }

        // FIXME: Handle the root element sizing.
        positioningAreaSize = borderBoxRect.size() - LayoutSize(left + right, top + bottom);
        break;
    }
    case FillAttachment::FixedBackground: {
        // FIXME: Handle fixed backgrounds.
        positioningAreaSize = borderBoxRect.size();
        break;
    }
    }

    LayoutSize tileSize = calculateFillTileSize(fillLayer, positioningAreaSize, pixelSnappingFactor);
    
    FillRepeat backgroundRepeatX = fillLayer.repeat().x;
    FillRepeat backgroundRepeatY = fillLayer.repeat().y;
    LayoutUnit availableWidth = positioningAreaSize.width() - tileSize.width();
    LayoutUnit availableHeight = positioningAreaSize.height() - tileSize.height();

    LayoutSize spaceSize;
    LayoutSize phase;
    LayoutSize noRepeat;
    LayoutUnit computedXPosition = resolveEdgeRelativeLength(fillLayer.xPosition(), fillLayer.backgroundXOrigin(), availableWidth, positioningAreaSize, tileSize);
    if (backgroundRepeatX == FillRepeat::Round && positioningAreaSize.width() > 0 && tileSize.width() > 0) {
        int numTiles = std::max(1, roundToInt(positioningAreaSize.width() / tileSize.width()));
        if (fillLayer.size().size.height.isAuto() && backgroundRepeatY != FillRepeat::Round)
            tileSize.setHeight(tileSize.height() * positioningAreaSize.width() / (numTiles * tileSize.width()));

        tileSize.setWidth(positioningAreaSize.width() / numTiles);
        phase.setWidth(tileSize.width() ? tileSize.width() - fmodf((computedXPosition + left), tileSize.width()) : 0);
    }

    LayoutUnit computedYPosition = resolveEdgeRelativeLength(fillLayer.yPosition(), fillLayer.backgroundYOrigin(), availableHeight, positioningAreaSize, tileSize);
    if (backgroundRepeatY == FillRepeat::Round && positioningAreaSize.height() > 0 && tileSize.height() > 0) {
        int numTiles = std::max(1, roundToInt(positioningAreaSize.height() / tileSize.height()));
        if (fillLayer.size().size.width.isAuto() && backgroundRepeatX != FillRepeat::Round)
            tileSize.setWidth(tileSize.width() * positioningAreaSize.height() / (numTiles * tileSize.height()));

        tileSize.setHeight(positioningAreaSize.height() / numTiles);
        phase.setHeight(tileSize.height() ? tileSize.height() - fmodf((computedYPosition + top), tileSize.height()) : 0);
    }

    if (backgroundRepeatX == FillRepeat::Repeat) {
        phase.setWidth(tileSize.width() ? tileSize.width() - fmodf(computedXPosition + left, tileSize.width()) : 0);
        spaceSize.setWidth(0);
    } else if (backgroundRepeatX == FillRepeat::Space && tileSize.width() > 0) {
        LayoutUnit space = getSpace(positioningAreaSize.width(), tileSize.width());
        if (space >= 0) {
            LayoutUnit actualWidth = tileSize.width() + space;
            computedXPosition = minimumValueForLength(Length(), availableWidth);
            spaceSize.setWidth(space);
            spaceSize.setHeight(0);
            phase.setWidth(actualWidth ? actualWidth - fmodf((computedXPosition + left), actualWidth) : 0);
        } else
            backgroundRepeatX = FillRepeat::NoRepeat;
    }

    if (backgroundRepeatX == FillRepeat::NoRepeat) {
        LayoutUnit xOffset = left + computedXPosition;
        if (xOffset > 0)
            destinationRect.move(xOffset, 0_lu);
        xOffset = std::min<LayoutUnit>(xOffset, 0);
        phase.setWidth(-xOffset);
        destinationRect.setWidth(tileSize.width() + xOffset);
        spaceSize.setWidth(0);
    }

    if (backgroundRepeatY == FillRepeat::Repeat) {
        phase.setHeight(tileSize.height() ? tileSize.height() - fmodf(computedYPosition + top, tileSize.height()) : 0);
        spaceSize.setHeight(0);
    } else if (backgroundRepeatY == FillRepeat::Space && tileSize.height() > 0) {
        LayoutUnit space = getSpace(positioningAreaSize.height(), tileSize.height());

        if (space >= 0) {
            LayoutUnit actualHeight = tileSize.height() + space;
            computedYPosition = minimumValueForLength(Length(), availableHeight);
            spaceSize.setHeight(space);
            phase.setHeight(actualHeight ? actualHeight - fmodf((computedYPosition + top), actualHeight) : 0);
        } else
            backgroundRepeatY = FillRepeat::NoRepeat;
    }
    if (backgroundRepeatY == FillRepeat::NoRepeat) {
        LayoutUnit yOffset = top + computedYPosition;
        if (yOffset > 0)
            destinationRect.move(0_lu, yOffset);
        yOffset = std::min<LayoutUnit>(yOffset, 0);
        phase.setHeight(-yOffset);
        destinationRect.setHeight(tileSize.height() + yOffset);
        spaceSize.setHeight(0);
    }

    if (fillLayer.attachment() == FillAttachment::FixedBackground) {
        LayoutPoint attachmentPoint = borderBoxRect.location();
        phase.expand(std::max<LayoutUnit>(attachmentPoint.x() - destinationRect.x(), 0), std::max<LayoutUnit>(attachmentPoint.y() - destinationRect.y(), 0));
    }

    destinationRect.intersect(borderBoxRect);
    return pixelSnappedFillLayerImageGeometry(destinationRect, tileSize, phase, spaceSize, fillLayer.attachment(), pixelSnappingFactor);
}

Vector<FillLayerImageGeometry, 1> calculateFillLayerImageGeometry(const RenderStyle& renderStyle, const Layout::BoxGeometry& boxGeometry, LayoutSize offsetFromRoot, float pixelSnappingFactor)
{
    // FIXME: Need to map logical to physical rects.
    auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(boxGeometry) };
    borderBoxRect.move(offsetFromRoot);

    Vector<FillLayerImageGeometry, 1> backgroundGeometry;

    for (auto fillLayer = &renderStyle.backgroundLayers(); fillLayer; fillLayer = fillLayer->next())
        backgroundGeometry.append(geometryForLayer(*fillLayer, borderBoxRect, boxGeometry, pixelSnappingFactor));

    return backgroundGeometry;
}

} // namespace Display
} // namespace WebCore

