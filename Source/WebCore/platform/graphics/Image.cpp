/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Inc.  All rights reserved.
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
#include "Image.h"

#include "AffineTransform.h"
#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "Length.h"
#include "MIMETypeRegistry.h"
#include "SharedBuffer.h"
#include "TextStream.h"
#include <math.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>

#if USE(CG)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace WebCore {

Image::Image(ImageObserver* observer)
    : m_imageObserver(observer)
{
}

Image::~Image()
{
}

Image* Image::nullImage()
{
    ASSERT(isMainThread());
    static Image& nullImage = BitmapImage::create().leakRef();
    return &nullImage;
}

bool Image::supportsType(const String& type)
{
    return MIMETypeRegistry::isSupportedImageResourceMIMEType(type); 
} 

bool Image::setData(RefPtr<SharedBuffer>&& data, bool allDataReceived)
{
    m_encodedImageData = WTFMove(data);
    if (!m_encodedImageData.get())
        return true;

    int length = m_encodedImageData->size();
    if (!length)
        return true;
    
    return dataChanged(allDataReceived);
}

void Image::fillWithSolidColor(GraphicsContext& ctxt, const FloatRect& dstRect, const Color& color, CompositeOperator op)
{
    if (!color.alpha())
        return;
    
    CompositeOperator previousOperator = ctxt.compositeOperation();
    ctxt.setCompositeOperation(!color.hasAlpha() && op == CompositeSourceOver ? CompositeCopy : op);
    ctxt.fillRect(dstRect, color);
    ctxt.setCompositeOperation(previousOperator);
}

void Image::drawTiled(GraphicsContext& ctxt, const FloatRect& destRect, const FloatPoint& srcPoint, const FloatSize& scaledTileSize, const FloatSize& spacing, CompositeOperator op, BlendMode blendMode)
{    
    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, destRect, solidColor(), op);
        return;
    }

    ASSERT(!isBitmapImage() || notSolidColor());

#if PLATFORM(IOS)
    FloatSize intrinsicTileSize = originalSize();
#else
    FloatSize intrinsicTileSize = size();
#endif
    if (hasRelativeWidth())
        intrinsicTileSize.setWidth(scaledTileSize.width());
    if (hasRelativeHeight())
        intrinsicTileSize.setHeight(scaledTileSize.height());

    FloatSize scale(scaledTileSize.width() / intrinsicTileSize.width(),
                    scaledTileSize.height() / intrinsicTileSize.height());

    FloatRect oneTileRect;
    FloatSize actualTileSize(scaledTileSize.width() + spacing.width(), scaledTileSize.height() + spacing.height());
    oneTileRect.setX(destRect.x() + fmodf(fmodf(-srcPoint.x(), actualTileSize.width()) - actualTileSize.width(), actualTileSize.width()));
    oneTileRect.setY(destRect.y() + fmodf(fmodf(-srcPoint.y(), actualTileSize.height()) - actualTileSize.height(), actualTileSize.height()));
    oneTileRect.setSize(scaledTileSize);
    
    // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
    if (oneTileRect.contains(destRect) && !ctxt.drawLuminanceMask()) {
        FloatRect visibleSrcRect;
        visibleSrcRect.setX((destRect.x() - oneTileRect.x()) / scale.width());
        visibleSrcRect.setY((destRect.y() - oneTileRect.y()) / scale.height());
        visibleSrcRect.setWidth(destRect.width() / scale.width());
        visibleSrcRect.setHeight(destRect.height() / scale.height());
        draw(ctxt, destRect, visibleSrcRect, op, blendMode, ImageOrientationDescription());
        return;
    }

#if PLATFORM(IOS)
    // When using accelerated drawing on iOS, it's faster to stretch an image than to tile it.
    if (ctxt.isAcceleratedContext()) {
        if (size().width() == 1 && intersection(oneTileRect, destRect).height() == destRect.height()) {
            FloatRect visibleSrcRect;
            visibleSrcRect.setX(0);
            visibleSrcRect.setY((destRect.y() - oneTileRect.y()) / scale.height());
            visibleSrcRect.setWidth(1);
            visibleSrcRect.setHeight(destRect.height() / scale.height());
            draw(ctxt, destRect, visibleSrcRect, op, BlendModeNormal, ImageOrientationDescription());
            return;
        }
        if (size().height() == 1 && intersection(oneTileRect, destRect).width() == destRect.width()) {
            FloatRect visibleSrcRect;
            visibleSrcRect.setX((destRect.x() - oneTileRect.x()) / scale.width());
            visibleSrcRect.setY(0);
            visibleSrcRect.setWidth(destRect.width() / scale.width());
            visibleSrcRect.setHeight(1);
            draw(ctxt, destRect, visibleSrcRect, op, BlendModeNormal, ImageOrientationDescription());
            return;
        }
    }
#endif

    // Patterned images and gradients can use lots of memory for caching when the
    // tile size is large (<rdar://problem/4691859>, <rdar://problem/6239505>).
    // Memory consumption depends on the transformed tile size which can get
    // larger than the original tile if user zooms in enough.
#if PLATFORM(IOS)
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
        while (toY < destRect.maxY()) {
            currentTileRect.shiftXEdgeTo(destRect.x());
            float toX = currentTileRect.x();
            while (toX < destRect.maxX()) {
                FloatRect toRect(toX, toY, currentTileRect.width(), currentTileRect.height());
                FloatRect fromRect(toFloatPoint(currentTileRect.location() - oneTileRect.location()), currentTileRect.size());
                fromRect.scale(1 / scale.width(), 1 / scale.height());

                draw(ctxt, toRect, fromRect, op, BlendModeNormal, ImageOrientationDescription());
                toX += currentTileRect.width();
                currentTileRect.shiftXEdgeTo(oneTileRect.x());
            }
            toY += currentTileRect.height();
            currentTileRect.shiftYEdgeTo(oneTileRect.y());
        }
        return;
    }

    AffineTransform patternTransform = AffineTransform().scaleNonUniform(scale.width(), scale.height());
    FloatRect tileRect(FloatPoint(), intrinsicTileSize);
    drawPattern(ctxt, tileRect, patternTransform, oneTileRect.location(), spacing, op, destRect, blendMode);

#if PLATFORM(IOS)
    startAnimation(DoNotCatchUp);
#else
    startAnimation();
#endif
}

// FIXME: Merge with the other drawTiled eventually, since we need a combination of both for some things.
void Image::drawTiled(GraphicsContext& ctxt, const FloatRect& dstRect, const FloatRect& srcRect,
    const FloatSize& tileScaleFactor, TileRule hRule, TileRule vRule, CompositeOperator op)
{    
    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, dstRect, solidColor(), op);
        return;
    }
    
    FloatSize tileScale = tileScaleFactor;
    FloatSize spacing;
    
    // FIXME: These rules follow CSS border-image rules, but they should not be down here in Image.
    bool centerOnGapHorizonally = false;
    bool centerOnGapVertically = false;
    switch (hRule) {
    case RoundTile: {
        int numItems = std::max<int>(floorf(dstRect.width() / srcRect.width()), 1);
        tileScale.setWidth(dstRect.width() / (srcRect.width() * numItems));
        break;
    }
    case SpaceTile: {
        int numItems = floorf(dstRect.width() / srcRect.width());
        if (!numItems)
            return;
        spacing.setWidth((dstRect.width() - srcRect.width() * numItems) / (numItems + 1));
        tileScale.setWidth(1);
        centerOnGapHorizonally = !(numItems & 1);
        break;
    }
    case StretchTile:
    case RepeatTile:
        break;
    }

    switch (vRule) {
    case RoundTile: {
        int numItems = std::max<int>(floorf(dstRect.height() / srcRect.height()), 1);
        tileScale.setHeight(dstRect.height() / (srcRect.height() * numItems));
        break;
        }
    case SpaceTile: {
        int numItems = floorf(dstRect.height() / srcRect.height());
        if (!numItems)
            return;
        spacing.setHeight((dstRect.height() - srcRect.height() * numItems) / (numItems + 1));
        tileScale.setHeight(1);
        centerOnGapVertically = !(numItems & 1);
        break;
    }
    case StretchTile:
    case RepeatTile:
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
    else if (hRule == Image::RepeatTile || hRule == Image::SpaceTile)
        hPhase -= (dstRect.width() - scaledTileWidth) / 2;

    if (centerOnGapVertically)
        vPhase -= spacing.height();
    else if (vRule == Image::RepeatTile || vRule == Image::SpaceTile)
        vPhase -= (dstRect.height() - scaledTileHeight) / 2;

    FloatPoint patternPhase(dstRect.x() - hPhase, dstRect.y() - vPhase);
    drawPattern(ctxt, srcRect, patternTransform, patternPhase, spacing, op, dstRect);

#if PLATFORM(IOS)
    startAnimation(DoNotCatchUp);
#else
    startAnimation();
#endif
}

#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
FloatRect Image::adjustSourceRectForDownSampling(const FloatRect& srcRect, const IntSize& scaledSize) const
{
    const FloatSize unscaledSize = size();
    if (unscaledSize == scaledSize)
        return srcRect;

    // Image has been down-sampled.
    float xscale = static_cast<float>(scaledSize.width()) / unscaledSize.width();
    float yscale = static_cast<float>(scaledSize.height()) / unscaledSize.height();
    FloatRect scaledSrcRect = srcRect;
    scaledSrcRect.scale(xscale, yscale);

    return scaledSrcRect;
}
#endif

void Image::computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
#if PLATFORM(IOS)
    intrinsicRatio = originalSize();
#else
    intrinsicRatio = size();
#endif
    intrinsicWidth = Length(intrinsicRatio.width(), Fixed);
    intrinsicHeight = Length(intrinsicRatio.height(), Fixed);
}

void Image::dump(TextStream& ts) const
{
    if (isAnimated())
        ts.dumpProperty("animated", isAnimated());

    if (isNull())
        ts.dumpProperty("is-null-image", true);

    ts.dumpProperty("size", size());
}

TextStream& operator<<(TextStream& ts, const Image& image)
{
    TextStream::GroupScope scope(ts);
    
    if (image.isBitmapImage())
        ts << "bitmap image";
    else if (image.isCrossfadeGeneratedImage())
        ts << "crossfade image";
    else if (image.isNamedImageGeneratedImage())
        ts << "named image";
    else if (image.isGradientImage())
        ts << "gradient image";
    else if (image.isSVGImage())
        ts << "svg image";
    else if (image.isPDFDocumentImage())
        ts << "pdf image";

    image.dump(ts);
    return ts;
}

}
