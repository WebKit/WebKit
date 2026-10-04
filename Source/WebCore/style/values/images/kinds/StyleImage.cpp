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
#include "ImagePaintingOptions.h"
#include "NinePieceGeometry.h"
#include "RenderElement.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Style {

static ConcreteObjectSize concreteObjectSizeToDrawAt(const WebCore::Image& image, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize)
{
    if (!image.drawsSVGImage())
        return ConcreteObjectSize::fixed(image.size());
    auto zoom = renderer.style().usedZoom();
    return ConcreteObjectSize::fixed(concreteObjectSize.size() * concreteObjectSize.zoom() / zoom, zoom);
}

static NaturalDimensions tileNaturalDimensions(const WebCore::Image& image)
{
    if (image.drawsSVGImage())
        return NaturalDimensions::none();
    auto size = image.size();
    return {
        .width = image.hasRelativeWidth() ? std::nullopt : std::optional { size.width() },
        .height = image.hasRelativeHeight() ? std::nullopt : std::optional { size.height() },
        .aspectRatio = std::nullopt,
    };
}

static ImageDrawResult drawImageAsPattern(GraphicsContext& context, WebCore::Image& image, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, const WebCore::ImageDrawingExtras* extras)
{
    image.drawPattern(context, concreteObjectSize, destination, tile, patternTransform, phase, spacing, options, extras);
    image.startAnimation();
    return ImageDrawResult::DidDraw;
}

ImageDrawResult Image::drawIntoDestinationImpl(GraphicsContext& context, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, const ScopedLambda<DestinationPaint>& paint)
{
    GraphicsContextStateSaver stateSaver(context);
    context.setCompositeOperation(options.compositeOperator(), options.blendMode());
    if (options.interpolationQuality() != InterpolationQuality::Default)
        context.setImageInterpolationQuality(options.interpolationQuality());

    context.clip(destination);
    context.translate(destination.location());

    // An empty source would make this scale non-finite, which poisons the CTM.
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

    // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
    if (oneTileRect.contains(destRect) && options.drawLuminanceMask() == DrawLuminanceMask::No) {
        FloatRect visibleSrcRect;
        visibleSrcRect.setX((destRect.x() - oneTileRect.x()) / scale.width());
        visibleSrcRect.setY((destRect.y() - oneTileRect.y()) / scale.height());
        visibleSrcRect.setWidth(destRect.width() / scale.width());
        visibleSrcRect.setHeight(destRect.height() / scale.height());
        return draw(ctxt, concreteObjectSize, destRect, visibleSrcRect);
    }

    // When using accelerated drawing, it's faster to stretch an image than to tile it.
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

    // Patterned images and gradients can use lots of memory for caching when the
    // tile size is large (<rdar://problem/4691859>, <rdar://problem/6239505>).
    // Memory consumption depends on the transformed tile size which can get
    // larger than the original tile if user zooms in enough.
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
        // https://drafts.csswg.org/css-backgrounds/#border-image-process
        float scaledSourceWidth = srcRect.width() * tileScale.width();
        int numItems = std::max<int>(roundf(dstRect.width() / scaledSourceWidth), 1);
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
        // https://drafts.csswg.org/css-backgrounds/#border-image-process
        float scaledSourceHeight = srcRect.height() * tileScale.height();
        int numItems = std::max<int>(roundf(dstRect.height() / scaledSourceHeight), 1);
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

    // We want to construct the phase such that the pattern is centered (when stretch is not
    // set for a particular rule).
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
    auto updateResult = [&](auto pieceResult) {
        if (pieceResult == ImageDrawResult::DidRequestDecoding || result == ImageDrawResult::DidRequestDecoding)
            result = ImageDrawResult::DidRequestDecoding;
        else if (pieceResult == ImageDrawResult::DidDraw)
            result = ImageDrawResult::DidDraw;
    };

    for (auto piece : allImagePieces) {
        if (geometry.shouldSkipPiece(piece))
            continue;

        if (isCornerPiece(piece)) {
            updateResult(draw(context, concreteObjectSize, geometry.destinationRects[piece], geometry.sourceRects[piece]));
            continue;
        }

        auto hRule = isHorizontalPiece(piece)
            ? static_cast<WebCore::Image::TileRule>(geometry.horizontalRule)
            : WebCore::Image::StretchTile;

        auto vRule = isVerticalPiece(piece)
            ? static_cast<WebCore::Image::TileRule>(geometry.verticalRule)
            : WebCore::Image::StretchTile;

        if (hRule == WebCore::Image::StretchTile && vRule == WebCore::Image::StretchTile) {
            updateResult(draw(context, concreteObjectSize, geometry.destinationRects[piece], geometry.sourceRects[piece]));
            continue;
        }

        updateResult(drawTiledUsingImpl(context, drawPattern, concreteObjectSize, geometry.destinationRects[piece], geometry.sourceRects[piece], geometry.tileScales[piece], hRule, vRule));
    }

    return result;
}

ImageDrawResult Image::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, bool isForFirstLine) const
{
    if (isPending())
        return ImageDrawResult::DidNothing;

    RefPtr image = this->image(&renderer, flooredIntSize(destination.size()), context, isForFirstLine);
    if (!image || image->isNull())
        return ImageDrawResult::DidNothing;

    auto imageSource = source;
    if (!image->drawsSVGImage()) {
        auto imageSize = image->size(options.orientation());
        auto box = concreteObjectSize.size() * concreteObjectSize.zoom();
        if (box.isEmpty())
            imageSource = { { }, imageSize };
        else {
            auto mapX = [&](auto x) { return narrowPrecisionToFloat(static_cast<double>(x) * imageSize.width() / box.width()); };
            auto mapY = [&](auto y) { return narrowPrecisionToFloat(static_cast<double>(y) * imageSize.height() / box.height()); };
            auto minX = mapX(source.x());
            auto minY = mapY(source.y());
            imageSource = { minX, minY, mapX(source.maxX()) - minX, mapY(source.maxY()) - minY };
        }
    }

    auto imageConcreteObjectSize = concreteObjectSizeToDrawAt(*image, renderer, concreteObjectSize);
    auto extras = drawingExtrasForRenderer(renderer);
    return context.drawImage(*image, imageConcreteObjectSize, destination, imageSource, options, &extras);
}

ImageDrawResult Image::drawTiled(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions options, bool isForFirstLine) const
{
    RefPtr image = this->image(&renderer, tileSize, context, isForFirstLine);
    if (!image || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    if (auto color = image->singlePixelSolidColor()) {
        WebCore::Image::fillWithSolidColor(context, destination, *color, options.compositeOperator());
        return ImageDrawResult::DidDraw;
    }
    ASSERT_IMPLIES(image->isBitmapImage(), !image->hasSolidColor());

    auto extras = drawingExtrasForRenderer(renderer);
    return drawTiledUsing(context, tileNaturalDimensions(*image), [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        return context.drawImage(*image, tileConcreteObjectSize, destination, source, options, &extras);
    }, [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        return drawImageAsPattern(context, *image, tileConcreteObjectSize, destination, tile, patternTransform, phase, spacing, options, &extras);
    }, concreteObjectSizeToDrawAt(*image, renderer, concreteObjectSize), destination, phase, tileSize, spacing, options);
}

ImageDrawResult Image::drawNinePiece(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const NinePieceGeometry& geometry, ImagePaintingOptions options) const
{
    RefPtr image = this->image(&renderer, concreteObjectSize.size() * concreteObjectSize.zoom(), context);
    if (!image || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    // FIXME: With 'border-image-repeat: space', partial tiles should be discarded so the gaps around the tiles are
    // empty. Similarly, the whole region should be left empty if no tile fits.
    //
    // The previous implementation got this wrong for a single pixel image, instead filling the whole region with
    // its color. To match that behavior, we convert NinePieceImageRule::Space to NinePieceImageRule::Repeat when
    // dealing with a single pixel image.
    //
    // https://bugs.webkit.org/show_bug.cgi?id=326231

    auto singlePixelSolidColor = image->singlePixelSolidColor();
    auto pieceGeometry = geometry;
    if (singlePixelSolidColor) {
        if (pieceGeometry.horizontalRule == NinePieceImageRule::Space)
            pieceGeometry.horizontalRule = NinePieceImageRule::Repeat;
        if (pieceGeometry.verticalRule == NinePieceImageRule::Space)
            pieceGeometry.verticalRule = NinePieceImageRule::Repeat;
    }

    auto extras = drawingExtrasForRenderer(renderer);
    return drawNinePieceUsing(context, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        return context.drawImage(*image, pieceConcreteObjectSize, destination, source, options, &extras);
    }, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        if (singlePixelSolidColor) {
            WebCore::Image::fillWithSolidColor(context, destination, *singlePixelSolidColor, options.compositeOperator());
            return ImageDrawResult::DidDraw;
        }
        return drawImageAsPattern(context, *image, pieceConcreteObjectSize, destination, tile, patternTransform, phase, spacing, { options.compositeOperator(), options.interpolationQuality() }, &extras);
    }, concreteObjectSizeToDrawAt(*image, renderer, concreteObjectSize), pieceGeometry);
}

} // namespace Style
} // namespace WebCore
