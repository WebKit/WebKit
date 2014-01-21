/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CrossfadeGeneratedImage.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"

namespace WebCore {

CrossfadeGeneratedImage::CrossfadeGeneratedImage(Image* fromImage, Image* toImage, float percentage, IntSize crossfadeSize, const IntSize& size)
    : m_fromImage(fromImage)
    , m_toImage(toImage)
    , m_percentage(percentage)
    , m_crossfadeSize(crossfadeSize)
{
    setContainerSize(size);
}

static void drawCrossfadeSubimage(GraphicsContext* context, Image* image, CompositeOperator operation, float opacity, IntSize targetSize)
{
    IntSize imageSize = image->size();

    // SVGImage resets the opacity when painting, so we have to use transparency layers to accurately paint one at a given opacity.
    bool useTransparencyLayer = image->isSVGImage();

    GraphicsContextStateSaver stateSaver(*context);

    context->setCompositeOperation(operation);

    if (useTransparencyLayer)
        context->beginTransparencyLayer(opacity);
    else
        context->setAlpha(opacity);

    if (targetSize != imageSize)
        context->scale(FloatSize(static_cast<float>(targetSize.width()) / imageSize.width(),
            static_cast<float>(targetSize.height()) / imageSize.height()));
    context->drawImage(image, ColorSpaceDeviceRGB, IntPoint());

    if (useTransparencyLayer)
        context->endTransparencyLayer();
}

void CrossfadeGeneratedImage::drawCrossfade(GraphicsContext* context)
{
    // Draw nothing if either of the images hasn't loaded yet.
    if (m_fromImage == Image::nullImage() || m_toImage == Image::nullImage())
        return;

    GraphicsContextStateSaver stateSaver(*context);

    context->clip(IntRect(IntPoint(), m_crossfadeSize));
    context->beginTransparencyLayer(1);

    drawCrossfadeSubimage(context, m_fromImage, CompositeSourceOver, 1 - m_percentage, m_crossfadeSize);
    drawCrossfadeSubimage(context, m_toImage, CompositePlusLighter, m_percentage, m_crossfadeSize);

    context->endTransparencyLayer();
}

void CrossfadeGeneratedImage::draw(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace, CompositeOperator compositeOp, BlendMode blendMode, ImageOrientationDescription)
{
    GraphicsContextStateSaver stateSaver(*context);
    context->setCompositeOperation(compositeOp, blendMode);
    context->clip(dstRect);
    context->translate(dstRect.x(), dstRect.y());
    if (dstRect.size() != srcRect.size())
        context->scale(FloatSize(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height()));
    context->translate(-srcRect.x(), -srcRect.y());
    
    drawCrossfade(context);
}

void CrossfadeGeneratedImage::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator compositeOp, const FloatRect& dstRect, BlendMode blendMode)
{
    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(size(), 1, ColorSpaceDeviceRGB, context->isAcceleratedContext() ? Accelerated : Unaccelerated);
    if (!imageBuffer)
        return;

    // Fill with the cross-faded image.
    GraphicsContext* graphicsContext = imageBuffer->context();
    drawCrossfade(graphicsContext);

    // Tile the image buffer into the context.
    imageBuffer->drawPattern(context, srcRect, patternTransform, phase, styleColorSpace, compositeOp, dstRect, blendMode);
}

}
