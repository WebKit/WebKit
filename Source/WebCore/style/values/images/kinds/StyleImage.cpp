/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleImage.h"

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImagePaintingOptions.h"
#include "ImageSizingContext.h"
#include "NinePieceGeometry.h"
#include "StyleCachedImage.h"

namespace WebCore {
namespace Style {

ConcreteObjectSize negotiate(const Image& image, const RenderElement& renderer, const ImageSizingContext& context)
{
    return context.resolve(image.naturalDimensions(renderer, context));
}

ImageDrawResult Image::drawTiled(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions options, bool isForFirstLine) const
{
    if (tileSize.isEmpty())
        return ImageDrawResult::DidNothing;

    return drawTiledUsing(context, NaturalDimensions::none(), [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        return draw(context, renderer, tileConcreteObjectSize, destination, source, options, isForFirstLine);
    }, [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        return drawAsPattern(context, renderer, tileConcreteObjectSize, destination, tile, patternTransform, phase, spacing, options, isForFirstLine);
    }, concreteObjectSize, destination, phase, tileSize, spacing, options);
}

ImageDrawResult Image::drawNinePiece(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const NinePieceGeometry& geometry, ImagePaintingOptions options) const
{
    return drawNinePieceUsing(context, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        return draw(context, renderer, pieceConcreteObjectSize, destination, source, options, false);
    }, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        return drawAsPattern(context, renderer, pieceConcreteObjectSize, destination, tile, patternTransform, phase, spacing, options, false);
    }, concreteObjectSize, geometry);
}

ImageDrawResult Image::drawIntoDestinationImpl(GraphicsContext& context, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, const ScopedLambda<DestinationPaint>& paint)
{
    GraphicsContextStateSaver stateSaver(context);
    context.setCompositeOperation(options.compositeOperator(), options.blendMode());
    if (options.interpolationQuality() != InterpolationQuality::Default)
        context.setImageInterpolationQuality(options.interpolationQuality());

    context.clip(destination);
    context.translate(destination.location());

    if (destination.size() != source.size() && !source.isEmpty())
        context.scale(destination.size() / source.size());

    context.translate(-source.location());

    return paint(context);
}

ImageDrawResult Image::drawTiledUsingImpl(GraphicsContext& ctxt, NaturalDimensions naturalDimensions, const ScopedLambda<TiledDraw>& draw, const ScopedLambda<TiledDrawPattern>& drawPattern, ConcreteObjectSize concreteObjectSize, const FloatRect& destRect, const FloatPoint& srcPoint, const FloatSize& scaledTileSize, const FloatSize& spacing, ImagePaintingOptions options)
{
    FloatSize intrinsicTileSize = concreteObjectSize.size();
    if (!naturalDimensions.width)
        intrinsicTileSize.setWidth(scaledTileSize.width());
    if (!naturalDimensions.height)
        intrinsicTileSize.setHeight(scaledTileSize.height());

    FloatSize scale(scaledTileSize / intrinsicTileSize);

    FloatRect oneTileRect;
    FloatSize actualTileSize = scaledTileSize + spacing;
    oneTileRect.setX(destRect.x() + fmodf(fmodf(-srcPoint.x(), actualTileSize.width()) - actualTileSize.width(), actualTileSize.width()));
    oneTileRect.setY(destRect.y() + fmodf(fmodf(-srcPoint.y(), actualTileSize.height()) - actualTileSize.height(), actualTileSize.height()));
    oneTileRect.setSize(scaledTileSize);

    if (oneTileRect.contains(destRect) && options.drawLuminanceMask() == DrawLuminanceMask::No) {
        FloatRect visibleSrcRect;
        visibleSrcRect.setX((destRect.x() - oneTileRect.x()) / scale.width());
        visibleSrcRect.setY((destRect.y() - oneTileRect.y()) / scale.height());
        visibleSrcRect.setWidth(destRect.width() / scale.width());
        visibleSrcRect.setHeight(destRect.height() / scale.height());
        return draw(ctxt, concreteObjectSize, destRect, visibleSrcRect);
    }

    if (ctxt.renderingMode() == RenderingMode::Accelerated) {
        if (concreteObjectSize.size().width() == 1 && intersection(oneTileRect, destRect).height() == destRect.height()) {
            FloatRect visibleSrcRect;
            visibleSrcRect.setX(0);
            visibleSrcRect.setY((destRect.y() - oneTileRect.y()) / scale.height());
            visibleSrcRect.setWidth(1);
            visibleSrcRect.setHeight(destRect.height() / scale.height());
            return draw(ctxt, concreteObjectSize, destRect, visibleSrcRect);
        }
        if (concreteObjectSize.size().height() == 1 && intersection(oneTileRect, destRect).width() == destRect.width()) {
            FloatRect visibleSrcRect;
            visibleSrcRect.setX((destRect.x() - oneTileRect.x()) / scale.width());
            visibleSrcRect.setY(0);
            visibleSrcRect.setWidth(destRect.width() / scale.width());
            visibleSrcRect.setHeight(1);
            return draw(ctxt, concreteObjectSize, destRect, visibleSrcRect);
        }
    }

#if PLATFORM(IOS_FAMILY)
    const float maxPatternTilePixels = 512 * 512;
#else
    const float maxPatternTilePixels = 2048 * 2048;
#endif
    FloatRect transformedTileSize = ctxt.getCTM().mapRect(FloatRect(FloatPoint(), scaledTileSize));
    float transformedTileSizePixels = transformedTileSize.width() * transformedTileSize.height();
    FloatRect currentTileRect = oneTileRect;
    if (transformedTileSizePixels > maxPatternTilePixels) {
        GraphicsContextStateSaver stateSaver(ctxt);
        ctxt.clip(destRect);

        currentTileRect.shiftYEdgeTo(destRect.y());
        float toY = currentTileRect.y();
        ImageDrawResult result = ImageDrawResult::DidNothing;
        while (toY < destRect.maxY()) {
            currentTileRect.shiftXEdgeTo(destRect.x());
            float toX = currentTileRect.x();
            while (toX < destRect.maxX()) {
                FloatRect toRect(toX, toY, currentTileRect.width(), currentTileRect.height());
                FloatRect fromRect(toFloatPoint(currentTileRect.location() - oneTileRect.location()), currentTileRect.size());
                fromRect.scale(1 / scale.width(), 1 / scale.height());

                result = draw(ctxt, concreteObjectSize, toRect, fromRect);
                if (result == ImageDrawResult::DidRequestDecoding)
                    return result;
                toX += currentTileRect.width();
                currentTileRect.shiftXEdgeTo(oneTileRect.x());
            }
            toY += currentTileRect.height();
            currentTileRect.shiftYEdgeTo(oneTileRect.y());
        }
        return result;
    }

    AffineTransform patternTransform = AffineTransform().scaleNonUniform(scale.width(), scale.height());
    FloatRect tileRect(FloatPoint(), intrinsicTileSize);
    return drawPattern(ctxt, concreteObjectSize, destRect, tileRect, patternTransform, oneTileRect.location(), spacing);
}

ImageDrawResult Image::drawTiledUsingImpl(GraphicsContext& ctxt, const ScopedLambda<TiledDrawPattern>& drawPattern, ConcreteObjectSize concreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect, const FloatSize& tileScaleFactor, WebCore::Image::TileRule hRule, WebCore::Image::TileRule vRule)
{
    FloatSize tileScale = tileScaleFactor;
    FloatSize spacing;

    bool centerOnGapHorizonally = false;
    bool centerOnGapVertically = false;
    switch (hRule) {
    case WebCore::Image::RoundTile: {
        float scaledSourceWidth = srcRect.width() * tileScale.width();
        int numItems = std::max<int>(floorf(dstRect.width() / scaledSourceWidth), 1);
        tileScale.setWidth(dstRect.width() / (srcRect.width() * numItems));
        break;
    }
    case WebCore::Image::SpaceTile: {
        float scaledSourceWidth = srcRect.width() * tileScale.width();
        int numItems = floorf(dstRect.width() / scaledSourceWidth);
        if (!numItems)
            return ImageDrawResult::DidNothing;
        spacing.setWidth((dstRect.width() - scaledSourceWidth * numItems) / (numItems + 1));
        centerOnGapHorizonally = !(numItems & 1);
        break;
    }
    case WebCore::Image::StretchTile:
    case WebCore::Image::RepeatTile:
        break;
    }

    switch (vRule) {
    case WebCore::Image::RoundTile: {
        float scaledSourceHeight = srcRect.height() * tileScale.height();
        int numItems = std::max<int>(floorf(dstRect.height() / scaledSourceHeight), 1);
        tileScale.setHeight(dstRect.height() / (srcRect.height() * numItems));
        break;
    }
    case WebCore::Image::SpaceTile: {
        float scaledSourceHeight = srcRect.height() * tileScale.height();
        int numItems = floorf(dstRect.height() / scaledSourceHeight);
        if (!numItems)
            return ImageDrawResult::DidNothing;
        spacing.setHeight((dstRect.height() - scaledSourceHeight * numItems) / (numItems + 1));
        centerOnGapVertically = !(numItems & 1);
        break;
    }
    case WebCore::Image::StretchTile:
    case WebCore::Image::RepeatTile:
        break;
    }

    AffineTransform patternTransform = AffineTransform().scaleNonUniform(tileScale.width(), tileScale.height());

    float hPhase = tileScale.width() * srcRect.x();
    float vPhase = tileScale.height() * srcRect.y();
    float scaledTileWidth = tileScale.width() * srcRect.width();
    float scaledTileHeight = tileScale.height() * srcRect.height();

    if (centerOnGapHorizonally)
        hPhase -= spacing.width();
    else if (hRule == WebCore::Image::RepeatTile || hRule == WebCore::Image::SpaceTile)
        hPhase -= (dstRect.width() - scaledTileWidth) / 2;

    if (centerOnGapVertically)
        vPhase -= spacing.height();
    else if (vRule == WebCore::Image::RepeatTile || vRule == WebCore::Image::SpaceTile)
        vPhase -= (dstRect.height() - scaledTileHeight) / 2;

    FloatPoint patternPhase(dstRect.x() - hPhase, dstRect.y() - vPhase);
    return drawPattern(ctxt, concreteObjectSize, dstRect, srcRect, patternTransform, patternPhase, spacing);
}

ImageDrawResult Image::drawNinePieceUsingImpl(GraphicsContext& context, const ScopedLambda<TiledDraw>& draw, const ScopedLambda<TiledDrawPattern>& drawPattern, ConcreteObjectSize concreteObjectSize, const NinePieceGeometry& geometry)
{
    auto result = ImageDrawResult::DidNothing;
    auto record = [&](ImageDrawResult pieceResult) {
        if (pieceResult == ImageDrawResult::DidRequestDecoding || result == ImageDrawResult::DidRequestDecoding)
            result = ImageDrawResult::DidRequestDecoding;
        else if (pieceResult == ImageDrawResult::DidDraw)
            result = ImageDrawResult::DidDraw;
    };

    for (auto piece : allImagePieces) {
        if (geometry.shouldSkipPiece(piece))
            continue;

        if (isCornerPiece(piece)) {
            record(draw(context, concreteObjectSize, geometry.destinationRects[piece], geometry.sourceRects[piece]));
            continue;
        }

        auto horizontalRule = isHorizontalPiece(piece)
            ? static_cast<WebCore::Image::TileRule>(geometry.horizontalRule)
            : WebCore::Image::StretchTile;

        auto verticalRule = isVerticalPiece(piece)
            ? static_cast<WebCore::Image::TileRule>(geometry.verticalRule)
            : WebCore::Image::StretchTile;

        if (horizontalRule == WebCore::Image::StretchTile && verticalRule == WebCore::Image::StretchTile) {
            record(draw(context, concreteObjectSize, geometry.destinationRects[piece], geometry.sourceRects[piece]));
            continue;
        }

        record(drawTiledUsingImpl(context, drawPattern, concreteObjectSize, geometry.destinationRects[piece], geometry.sourceRects[piece], geometry.tileScales[piece], horizontalRule, verticalRule));
    }

    return result;
}

Vector<Ref<const CachedImage>, 1> Image::cachedImages() const
{
    Vector<Ref<const CachedImage>, 1> result;
    if (RefPtr image = cachedImage())
        result.append(image.releaseNonNull());
    return result;
}

bool Image::hasHDRContent() const
{
    RefPtr styleCachedImage = cachedImage();
    return styleCachedImage && protect(styleCachedImage->resource())->hasHDRContent();
}

RefPtr<NativeImage> Image::nativeImage(const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const ColorSpace& colorSpace) const
{
    auto size = concreteObjectSize.size();
    RefPtr imageBuffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, colorSpace, PixelFormat::BGRA8);
    if (!imageBuffer)
        return nullptr;

    auto rect = FloatRect { { }, size };
    draw(imageBuffer->context(), renderer, concreteObjectSize, rect, rect);
    return ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
}

ImageDrawResult Image::drawResolved(GraphicsContext& context, const RenderElement& renderer, WebCore::Image& image, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options) const
{
    auto extras = drawingExtrasForRenderer(renderer);
    return context.drawImage(image, concreteObjectSize, destination, source, options, &extras);
}

} // namespace Style
} // namespace WebCore
