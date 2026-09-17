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
#include "CrossfadeGeneratedImage.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

CrossfadeGeneratedImage::CrossfadeGeneratedImage(Image& fromImage, Image& toImage, float percentage, const FloatSize& crossfadeSize, std::unique_ptr<ImageDrawingExtras> fromExtras, std::unique_ptr<ImageDrawingExtras> toExtras)
    : GeneratedImage(crossfadeSize)
    , m_fromImage(fromImage)
    , m_toImage(toImage)
    , m_fromExtras(WTF::move(fromExtras))
    , m_toExtras(WTF::move(toExtras))
    , m_percentage(percentage)
{
}

static void drawCrossfadeSubimage(GraphicsContext& context, Image& image, CompositeOperator operation, float opacity, const FloatSize& targetSize, const ImageDrawingExtras* extras)
{
    auto concreteSize = ObjectSizeNegotiation::defaultSizingAlgorithm(image.naturalDimensions(),
        ObjectSizeNegotiation::SpecifiedSize::none(),
        { .defaultObjectSize = targetSize });
    FloatSize imageSize = concreteSize.size();

    // A zero-sized image would produce a non-finite scale below, poisoning the CTM.
    if (imageSize.isEmpty())
        return;

    // SVGImage resets the opacity when painting, so we have to use transparency layers to accurately paint one at a given opacity.
    bool useTransparencyLayer = image.drawsSVGImage();

    GraphicsContextStateSaver stateSaver(context);

    ImagePaintingOptions options;

    if (useTransparencyLayer) {
        context.setCompositeOperation(operation);
        context.beginTransparencyLayer(opacity);
    } else {
        context.setAlpha(opacity);
        options = { operation };
    }

    if (targetSize != imageSize)
        context.scale(targetSize / imageSize);

    context.drawImage(image, concreteSize, FloatRect { { }, imageSize }, FloatRect { { }, imageSize }, options, extras);

    if (useTransparencyLayer)
        context.endTransparencyLayer();
}

void CrossfadeGeneratedImage::drawCrossfade(GraphicsContext& context)
{
    // Draw nothing if either of the images hasn't loaded yet.
    if (m_fromImage.ptr() == &Image::nullImage() || m_toImage.ptr() == &Image::nullImage())
        return;

    GraphicsContextStateSaver stateSaver(context);

    context.clip(FloatRect(FloatPoint(), constructedSize()));
    context.beginTransparencyLayer(1);

    drawCrossfadeSubimage(context, m_fromImage.get(), CompositeOperator::SourceOver, 1 - m_percentage, constructedSize(), m_fromExtras.get());
    drawCrossfadeSubimage(context, m_toImage.get(), CompositeOperator::PlusLighter, m_percentage, constructedSize(), m_toExtras.get());

    context.endTransparencyLayer();
}

ImageDrawResult CrossfadeGeneratedImage::draw(GraphicsContext& context, ConcreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect, ImagePaintingOptions options, const ImageDrawingExtras* )
{
    GraphicsContextStateSaver stateSaver(context);
    context.setCompositeOperation(options.compositeOperator(), options.blendMode());
    if (options.interpolationQuality() != InterpolationQuality::Default)
        context.setImageInterpolationQuality(options.interpolationQuality());
    context.clip(dstRect);
    context.translate(dstRect.location());
    if (dstRect.size() != srcRect.size() && !srcRect.isEmpty())
        context.scale(dstRect.size() / srcRect.size());
    context.translate(-srcRect.location());
    
    drawCrossfade(context);
    return ImageDrawResult::DidDraw;
}

void CrossfadeGeneratedImage::drawPattern(GraphicsContext& context, ConcreteObjectSize concreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, const ImageDrawingExtras* )
{
    auto imageBuffer = context.createImageBuffer(concreteObjectSize.size());
    if (!imageBuffer)
        return;

    // Fill with the cross-faded image.
    GraphicsContext& graphicsContext = imageBuffer->context();
    drawCrossfade(graphicsContext);

    // Tile the image buffer into the context.
    context.drawPattern(*imageBuffer, dstRect, srcRect, patternTransform, phase, spacing, options);
}

void CrossfadeGeneratedImage::dump(TextStream& ts) const
{
    GeneratedImage::dump(ts);
    ts.dumpProperty("from-image"_s, m_fromImage.get());
    ts.dumpProperty("to-image"_s, m_toImage.get());
    ts.dumpProperty("percentage"_s, m_percentage);
}

}
