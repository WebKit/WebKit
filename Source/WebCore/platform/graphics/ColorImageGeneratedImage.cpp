/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "ColorImageGeneratedImage.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImageBuffer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ColorImageGeneratedImage::ColorImageGeneratedImage(const Color& color, const FloatSize& size)
    : m_color(color)
{
    setContainerSize(size);
}

ImageDrawResult ColorImageGeneratedImage::draw(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect&, ImagePaintingOptions options)
{
    Image::fillWithSolidColor(context, destinationRect, m_color, options.compositeOperator());
    return ImageDrawResult::DidDraw;
}

void ColorImageGeneratedImage::drawPattern(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect& sourceRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    if (spacing.isZero()) {
        Image::fillWithSolidColor(context, destinationRect, m_color, options.compositeOperator());
        return;
    }

    auto imageBuffer = context.createAlignedImageBuffer(size());
    if (!imageBuffer)
        return;

    imageBuffer->context().fillRect(FloatRect { { }, size() }, m_color);
    context.drawPattern(*imageBuffer, destinationRect, sourceRect, patternTransform, phase, spacing, options);
}

void ColorImageGeneratedImage::dump(TextStream& ts) const
{
    GeneratedImage::dump(ts);
    ts.dumpProperty("color"_s, m_color);
}

} // namespace WebCore
