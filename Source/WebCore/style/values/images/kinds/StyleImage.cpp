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
#include "ImageDrawingOptions.h"
#include "ImageSizingContext.h"
#include "NativeImage.h"
#include "NinePieceGeometry.h"
#include "RenderElement.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Style {

static std::optional<SourceImage> convertToLuminanceMask(GraphicsContext& context, SourceImage&& image)
{
    if (RefPtr imageBuffer = image.imageBufferIfExists()) {
        imageBuffer->convertToLuminanceMask();
        return WTF::move(image);
    }

    RefPtr nativeImage = image.nativeImageIfExists();
    if (!nativeImage)
        return std::nullopt;

    RefPtr buffer = context.createImageBuffer(nativeImage->size());
    if (!buffer)
        return std::nullopt;

    buffer->context().drawNativeImage(*nativeImage, FloatRect { { }, nativeImage->size() });
    buffer->convertToLuminanceMask();
    return SourceImage { buffer.releaseNonNull() };
}

static void drawPatternTile(GraphicsContext& context, WebCore::Image& image, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options, const WebCore::ImageDrawingExtras* extras)
{
    if (tile.isEmpty())
        return;

    if (options.headroom() == Headroom::FromImage && image.hasHDRContentForTesting()) {
        WebCore::Image::fillWithSolidColor(context, destination, WebCore::Color::gold, options.compositeOperator());
        return;
    }

    auto patternTile = image.currentPatternTile(context, concreteObjectSize, options, extras);
    if (!patternTile)
        return;

    auto scaledTile = tile;
    scaledTile.scale(patternTile->scale.width(), patternTile->scale.height());
    auto scaledPatternTransform = patternTransform;
    scaledPatternTransform.scale(1 / patternTile->scale.width(), 1 / patternTile->scale.height());

    auto tileImage = WTF::move(patternTile->image);
    if (options.drawLuminanceMask() == DrawLuminanceMask::Yes) {
        auto mask = convertToLuminanceMask(context, WTF::move(tileImage));
        if (!mask)
            return;
        tileImage = WTF::move(*mask);
    }

    context.drawPattern(tileImage, destination, scaledTile, scaledPatternTransform, phase, spacing, options);
}

static ImageDrawResult drawImageAsPattern(GraphicsContext& context, WebCore::Image& image, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options, const WebCore::ImageDrawingExtras* extras)
{
    drawPatternTile(context, image, concreteObjectSize, destination, tile, patternTransform, phase, spacing, options, extras);
    image.startAnimation();
    return ImageDrawResult::DidDraw;
}

ImageDrawResult Image::drawIntoDestinationImpl(GraphicsContext& context, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions options, const ScopedLambda<DestinationPaint>& paint)
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

ImageDrawResult Image::drawTiledUsingImpl(GraphicsContext& ctxt, NaturalDimensions naturalDimensions, const ScopedLambda<TiledDraw>& draw, const ScopedLambda<TiledDrawPattern>& drawPattern, ConcreteObjectSize concreteObjectSize, const FloatRect& destRect, const FloatPoint& srcPoint, const FloatSize& scaledTileSize, const FloatSize& spacing, ImageDrawingOptions options)
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

ImageDrawResult Image::drawTiledUsingImpl(GraphicsContext& ctxt, const ScopedLambda<TiledDrawPattern>& drawPattern, ConcreteObjectSize concreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect, const FloatSize& tileScaleFactor, NinePieceImageRule horizontalRule, NinePieceImageRule verticalRule)
{
    FloatSize tileScale = tileScaleFactor;
    FloatSize spacing;

    bool centerOnGapHorizonally = false;
    bool centerOnGapVertically = false;
    switch (horizontalRule) {
    case NinePieceImageRule::Round: {
        // https://drafts.csswg.org/css-backgrounds/#border-image-process
        float scaledSourceWidth = srcRect.width() * tileScale.width();
        int numItems = std::max<int>(roundf(dstRect.width() / scaledSourceWidth), 1);
        tileScale.setWidth(dstRect.width() / (srcRect.width() * numItems));
        break;
    }
    case NinePieceImageRule::Space: {
        float scaledSourceWidth = srcRect.width() * tileScale.width();
        int numItems = floorf(dstRect.width() / scaledSourceWidth);
        if (!numItems)
            return ImageDrawResult::DidNothing;
        spacing.setWidth((dstRect.width() - scaledSourceWidth * numItems) / (numItems + 1));
        centerOnGapHorizonally = !(numItems & 1);
        break;
    }
    case NinePieceImageRule::Stretch:
    case NinePieceImageRule::Repeat:
        break;
    }

    switch (verticalRule) {
    case NinePieceImageRule::Round: {
        // https://drafts.csswg.org/css-backgrounds/#border-image-process
        float scaledSourceHeight = srcRect.height() * tileScale.height();
        int numItems = std::max<int>(roundf(dstRect.height() / scaledSourceHeight), 1);
        tileScale.setHeight(dstRect.height() / (srcRect.height() * numItems));
        break;
    }
    case NinePieceImageRule::Space: {
        float scaledSourceHeight = srcRect.height() * tileScale.height();
        int numItems = floorf(dstRect.height() / scaledSourceHeight);
        if (!numItems)
            return ImageDrawResult::DidNothing;
        spacing.setHeight((dstRect.height() - scaledSourceHeight * numItems) / (numItems + 1));
        centerOnGapVertically = !(numItems & 1);
        break;
    }
    case NinePieceImageRule::Stretch:
    case NinePieceImageRule::Repeat:
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
    else if (horizontalRule == NinePieceImageRule::Repeat || horizontalRule == NinePieceImageRule::Space)
        hPhase -= (dstRect.width() - scaledTileWidth) / 2;

    if (centerOnGapVertically)
        vPhase -= spacing.height();
    else if (verticalRule == NinePieceImageRule::Repeat || verticalRule == NinePieceImageRule::Space)
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

        auto horizontalRule = isHorizontalPiece(piece)
            ? geometry.horizontalRule
            : NinePieceImageRule::Stretch;

        auto verticalRule = isVerticalPiece(piece)
            ? geometry.verticalRule
            : NinePieceImageRule::Stretch;

        if (horizontalRule == NinePieceImageRule::Stretch && verticalRule == NinePieceImageRule::Stretch) {
            updateResult(draw(context, concreteObjectSize, geometry.destinationRects[piece], geometry.sourceRects[piece]));
            continue;
        }

        updateResult(drawTiledUsingImpl(context, drawPattern, concreteObjectSize, geometry.destinationRects[piece], geometry.sourceRects[piece], geometry.tileScales[piece], horizontalRule, verticalRule));
    }

    return result;
}

static FloatRect scaleRect(const FloatRect& rect, const FloatSize& from, const FloatSize& to)
{
    if (from.isEmpty())
        return { { }, to };

    auto mapX = [&](float x) { return narrowPrecisionToFloat(static_cast<double>(x) * to.width() / from.width()); };
    auto mapY = [&](float y) { return narrowPrecisionToFloat(static_cast<double>(y) * to.height() / from.height()); };
    auto minX = mapX(rect.x());
    auto minY = mapY(rect.y());
    return { minX, minY, mapX(rect.maxX()) - minX, mapY(rect.maxY()) - minY };
}

FloatRect Image::mapSourceToSize(const FloatRect& rect, ConcreteObjectSize concreteObjectSize, const FloatSize& size)
{
    return scaleRect(rect, concreteObjectSize.size() * concreteObjectSize.zoom(), size);
}

FloatRect Image::mapSizeToSource(const FloatRect& rect, const FloatSize& size, ConcreteObjectSize concreteObjectSize)
{
    return scaleRect(rect, size, concreteObjectSize.size() * concreteObjectSize.zoom());
}

FloatRect Image::mapChangedRectToSource(const RenderElement&, ConcreteObjectSize concreteObjectSize, const FloatRect&) const
{
    return { { }, concreteObjectSize.size() * concreteObjectSize.zoom() };
}

RefPtr<NativeImage> Image::nativeImage(const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, float resolutionScale, ImageDrawingOptions options) const
{
    auto pixelSize = FloatSize { calculateImageBufferBackendSize(concreteObjectSize.size(), resolutionScale) };
    RefPtr imageBuffer = ImageBuffer::create(pixelSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, ColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return nullptr;

    draw(imageBuffer->context(), renderer, concreteObjectSize, FloatRect { { }, pixelSize }, FloatRect { { }, concreteObjectSize.size() }, options);
    return ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
}

ImageDrawResult Image::drawResolved(GraphicsContext& context, const RenderElement& renderer, WebCore::Image& image, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions options) const
{
    auto extras = drawingExtrasForRenderer(renderer);
    return image.draw(context, concreteObjectSize, destination, source, options, &extras);
}

ImageDrawResult Image::drawResolvedAsPattern(GraphicsContext& context, const RenderElement& renderer, WebCore::Image& image, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options) const
{
    auto extras = drawingExtrasForRenderer(renderer);
    return drawImageAsPattern(context, image, concreteObjectSize, destination, tile, patternTransform, phase, spacing, options, &extras);
}

ImageDrawResult Image::drawResolvedTiled(GraphicsContext& context, const RenderElement& renderer, WebCore::Image& image, NaturalDimensions tileNaturalDimensions, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImageDrawingOptions options) const
{
    if (auto color = image.singlePixelSolidColor()) {
        WebCore::Image::fillWithSolidColor(context, destination, *color, options.compositeOperator());
        return ImageDrawResult::DidDraw;
    }
    ASSERT_IMPLIES(image.isBitmapImage(), !image.hasSolidColor());

    auto extras = drawingExtrasForRenderer(renderer);
    return drawTiledUsing(context, tileNaturalDimensions, [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        return image.draw(context, tileConcreteObjectSize, destination, source, options, &extras);
    }, [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        return drawImageAsPattern(context, image, tileConcreteObjectSize, destination, tile, patternTransform, phase, spacing, options, &extras);
    }, concreteObjectSize, destination, phase, tileSize, spacing, options);
}

ImageDrawResult Image::drawResolvedNinePiece(GraphicsContext& context, const RenderElement& renderer, WebCore::Image& image, ConcreteObjectSize concreteObjectSize, const NinePieceGeometry& geometry, ImageDrawingOptions options) const
{
    // FIXME: With 'border-image-repeat: space', partial tiles should be discarded so the gaps around the tiles are
    // empty. Similarly, the whole region should be left empty if no tile fits.
    //
    // The previous implementation got this wrong for a single pixel image, instead filling the whole region with
    // its color. To match that behavior, we convert NinePieceImageRule::Space to NinePieceImageRule::Repeat when
    // dealing with a single pixel image.
    //
    // https://bugs.webkit.org/show_bug.cgi?id=326231

    auto singlePixelSolidColor = image.singlePixelSolidColor();
    auto pieceGeometry = geometry;
    if (singlePixelSolidColor) {
        if (pieceGeometry.horizontalRule == NinePieceImageRule::Space)
            pieceGeometry.horizontalRule = NinePieceImageRule::Repeat;
        if (pieceGeometry.verticalRule == NinePieceImageRule::Space)
            pieceGeometry.verticalRule = NinePieceImageRule::Repeat;
    }

    auto extras = drawingExtrasForRenderer(renderer);
    return drawNinePieceUsing(context, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        return image.draw(context, pieceConcreteObjectSize, destination, source, options, &extras);
    }, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        if (singlePixelSolidColor) {
            WebCore::Image::fillWithSolidColor(context, destination, *singlePixelSolidColor, options.compositeOperator());
            return ImageDrawResult::DidDraw;
        }
        return drawImageAsPattern(context, image, pieceConcreteObjectSize, destination, tile, patternTransform, phase, spacing, { options.compositeOperator(), options.interpolationQuality(), options.orientation() }, &extras);
    }, concreteObjectSize, pieceGeometry);
}

ImageDrawResult Image::drawTiled(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImageDrawingOptions options, bool isForFirstLine) const
{
    if (!canDrawAtSize(renderer, tileSize) || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    return drawTiledUsing(context, NaturalDimensions::none(), [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        return draw(context, renderer, tileConcreteObjectSize, destination, source, options, isForFirstLine);
    }, [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        return drawAsPattern(context, renderer, tileConcreteObjectSize, destination, tile, patternTransform, phase, spacing, options, isForFirstLine);
    }, ConcreteObjectSize::fixed(tileSize), destination, phase, tileSize, spacing, options);
}

ImageDrawResult Image::drawNinePiece(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const NinePieceGeometry& geometry, ImageDrawingOptions options) const
{
    auto size = concreteObjectSize.size() * concreteObjectSize.zoom();
    if (!canDrawAtSize(renderer, size) || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    return drawNinePieceUsing(context, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        return draw(context, renderer, pieceConcreteObjectSize, destination, source, options, false);
    }, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        return drawAsPattern(context, renderer, pieceConcreteObjectSize, destination, tile, patternTransform, phase, spacing, { options.compositeOperator(), options.interpolationQuality(), options.orientation() }, false);
    }, ConcreteObjectSize::fixed(size), geometry);
}

ConcreteObjectSize Image::negotiate(const RenderElement& renderer, const ImageSizingContext& context) const
{
    return context.resolve(naturalDimensions(renderer, context));
}

} // namespace Style
} // namespace WebCore
