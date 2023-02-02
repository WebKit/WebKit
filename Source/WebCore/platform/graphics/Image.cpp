/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004-2022 Apple Inc.  All rights reserved.
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
#include "DeprecatedGlobalSettings.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "Length.h"
#include "MIMETypeRegistry.h"
#include "SVGImage.h"
#include "SharedBuffer.h"
#include <math.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/TextStream.h>

#if USE(CG)
#include "PDFDocumentImage.h"
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace WebCore {

Image::Image(ImageObserver* observer)
    : m_imageObserver(observer)
{
}

Image::~Image() = default;

Image& Image::nullImage()
{
    ASSERT(isMainThread());
    static Image& nullImage = BitmapImage::create().leakRef();
    return nullImage;
}

RefPtr<Image> Image::create(ImageObserver& observer)
{
    // SVGImage and PDFDocumentImage are not safe to use off the main thread.
    // Workers can use BitmapImage directly.
    ASSERT(isMainThread());

    auto mimeType = observer.mimeType();
    if (mimeType == "image/svg+xml"_s)
        return SVGImage::create(observer);

    auto url = observer.sourceUrl();
    if (isPDFResource(mimeType, url) || isPostScriptResource(mimeType, url)) {
#if USE(CG) && !USE(WEBKIT_IMAGE_DECODERS)
        if (!DeprecatedGlobalSettings::arePDFImagesEnabled())
            return nullptr;
        return PDFDocumentImage::create(&observer);
#else
        return nullptr;
#endif
    }

    return BitmapImage::create(&observer);
}

bool Image::supportsType(const String& type)
{
    return MIMETypeRegistry::isSupportedImageMIMEType(type);
} 

bool Image::isPDFResource(const String& mimeType, const URL& url)
{
    if (mimeType.isEmpty())
        return url.path().endsWithIgnoringASCIICase(".pdf"_s);
    return MIMETypeRegistry::isPDFMIMEType(mimeType);
}

bool Image::isPostScriptResource(const String& mimeType, const URL& url)
{
    if (mimeType.isEmpty())
        return url.path().endsWithIgnoringASCIICase(".ps"_s);
    return MIMETypeRegistry::isPostScriptMIMEType(mimeType);
}


EncodedDataStatus Image::setData(RefPtr<FragmentedSharedBuffer>&& data, bool allDataReceived)
{
    m_encodedImageData = WTFMove(data);

    // Don't do anything; it is an empty image.
    if (!m_encodedImageData.get() || !m_encodedImageData->size())
        return EncodedDataStatus::Complete;

    return dataChanged(allDataReceived);
}

URL Image::sourceURL() const
{
    return imageObserver() ? imageObserver()->sourceUrl() : URL();
}

String Image::mimeType() const
{
    return imageObserver() ? imageObserver()->mimeType() : emptyString();
}

long long Image::expectedContentLength() const
{
    return imageObserver() ? imageObserver()->expectedContentLength() : 0;
}

void Image::fillWithSolidColor(GraphicsContext& ctxt, const FloatRect& dstRect, const Color& color, CompositeOperator op)
{
    if (!color.isVisible())
        return;
    
    CompositeOperator previousOperator = ctxt.compositeOperation();
    ctxt.setCompositeOperation(color.isOpaque() && op == CompositeOperator::SourceOver ? CompositeOperator::Copy : op);
    ctxt.fillRect(dstRect, color);
    ctxt.setCompositeOperation(previousOperator);
}

void Image::drawPattern(GraphicsContext& ctxt, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform,  const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    auto tileImage = preTransformedNativeImageForCurrentFrame(options.orientation() == ImageOrientation::Orientation::FromImage);
    if (!tileImage)
        return;

    ctxt.drawPattern(*tileImage, destRect, tileRect, patternTransform, phase, spacing, options);

    if (imageObserver())
        imageObserver()->didDraw(*this);
}

ImageDrawResult Image::drawTiled(GraphicsContext& ctxt, const FloatRect& destRect, const FloatPoint& srcPoint, const FloatSize& scaledTileSize, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    Color color = singlePixelSolidColor();
    if (color.isValid()) {
        fillWithSolidColor(ctxt, destRect, color, options.compositeOperator());
        return ImageDrawResult::DidDraw;
    }

    ASSERT(!isBitmapImage() || notSolidColor());

    FloatSize intrinsicTileSize = size();
    if (hasRelativeWidth())
        intrinsicTileSize.setWidth(scaledTileSize.width());
    if (hasRelativeHeight())
        intrinsicTileSize.setHeight(scaledTileSize.height());

    FloatSize scale(scaledTileSize / intrinsicTileSize);

    FloatRect oneTileRect;
    FloatSize actualTileSize = scaledTileSize + spacing;
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
        return draw(ctxt, destRect, visibleSrcRect, options);
    }

#if PLATFORM(IOS_FAMILY)
    // FIXME: We should re-test this and remove this iOS behavior difference if possible.
    // When using accelerated drawing on iOS, it's faster to stretch an image than to tile it.
    if (ctxt.renderingMode() == RenderingMode::Accelerated) {
        if (size().width() == 1 && intersection(oneTileRect, destRect).height() == destRect.height()) {
            FloatRect visibleSrcRect;
            visibleSrcRect.setX(0);
            visibleSrcRect.setY((destRect.y() - oneTileRect.y()) / scale.height());
            visibleSrcRect.setWidth(1);
            visibleSrcRect.setHeight(destRect.height() / scale.height());
            return draw(ctxt, destRect, visibleSrcRect, { options, BlendMode::Normal });
        }
        if (size().height() == 1 && intersection(oneTileRect, destRect).width() == destRect.width()) {
            FloatRect visibleSrcRect;
            visibleSrcRect.setX((destRect.x() - oneTileRect.x()) / scale.width());
            visibleSrcRect.setY(0);
            visibleSrcRect.setWidth(destRect.width() / scale.width());
            visibleSrcRect.setHeight(1);
            return draw(ctxt, destRect, visibleSrcRect, { options, BlendMode::Normal });
        }
    }
#endif

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

                result = draw(ctxt, toRect, fromRect, { options, BlendMode::Normal });
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
    drawPattern(ctxt, destRect, tileRect, patternTransform, oneTileRect.location(), spacing, options);
    startAnimation();
    return ImageDrawResult::DidDraw;
}

// FIXME: Merge with the other drawTiled eventually, since we need a combination of both for some things.
ImageDrawResult Image::drawTiled(GraphicsContext& ctxt, const FloatRect& dstRect, const FloatRect& srcRect, const FloatSize& tileScaleFactor, TileRule hRule, TileRule vRule, const ImagePaintingOptions& options)
{    
    Color color = singlePixelSolidColor();
    if (color.isValid()) {
        fillWithSolidColor(ctxt, dstRect, color, options.compositeOperator());
        return ImageDrawResult::DidDraw;
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
            return ImageDrawResult::DidNothing;
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
            return ImageDrawResult::DidNothing;
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
    drawPattern(ctxt, dstRect, srcRect, patternTransform, patternPhase, spacing, options);
    startAnimation();
    return ImageDrawResult::DidDraw;
}

void Image::computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    intrinsicRatio = size();
    intrinsicWidth = Length(intrinsicRatio.width(), LengthType::Fixed);
    intrinsicHeight = Length(intrinsicRatio.height(), LengthType::Fixed);
}

void Image::startAnimationAsynchronously()
{
    if (!m_animationStartTimer)
        m_animationStartTimer = makeUnique<Timer>(*this, &Image::startAnimation);
    if (m_animationStartTimer->isActive())
        return;
    m_animationStartTimer->startOneShot(0_s);
}

DestinationColorSpace Image::colorSpace()
{
    return DestinationColorSpace::SRGB();
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
    else if (image.isSVGImageForContainer())
        ts << "svg image for container";
    else if (image.isPDFDocumentImage())
        ts << "pdf image";

    image.dump(ts);
    return ts;
}

#if !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WIN)

void BitmapImage::invalidatePlatformData()
{
}

Ref<Image> Image::loadPlatformResource(const char* resource)
{
    WTFLogAlways("WARNING: trying to load platform resource '%s'", resource);
    return BitmapImage::create();
}

#endif // !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WIN)
}
