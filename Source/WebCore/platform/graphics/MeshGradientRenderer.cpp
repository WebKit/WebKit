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
#include "MeshGradientRenderer.h"

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "MeshGradient.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"

namespace WebCore {

void MeshGradientRenderer::draw(GraphicsContext& context, const MeshGradient& gradient, const FloatRect& destination)
{
    if (context.paintingDisabled())
        return;

    auto transform = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    float scale = std::max(std::abs(transform.xScale()), std::abs(transform.yScale()));

    auto pixelSize = expandedIntSize(destination.size().scaled(scale));
    if (pixelSize.isEmpty())
        return;

    RefPtr image = gradient.renderedImage(pixelSize, scale);
    if (!image) {
        PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
        auto bufferSize = PixelBuffer::computeBufferSize(format.pixelFormat, pixelSize);
        if (bufferSize.hasOverflowed())
            return;

        Vector<uint8_t> pixels;
        if (!pixels.tryGrow(bufferSize.value()))
            return;

        if (!render(gradient, { format, PixelBuffer::bytesPerPixel(format.pixelFormat) * pixelSize.width(), pixels.mutableSpan() }, pixelSize, scale))
            return;

        auto source = PixelBufferSourceView::create(format, pixelSize, pixels.span());
        if (!source)
            return;

        RefPtr imageBuffer = context.createImageBuffer(pixelSize, 1, ColorSpace::SRGB(), RenderingMode::Unaccelerated);
        if (!imageBuffer)
            return;

        imageBuffer->putPixelBuffer(*source, { { }, pixelSize });

        image = ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
        if (!image)
            return;

        gradient.setRenderedImage(pixelSize, scale, *image);
    }

    context.drawNativeImage(*image, destination, FloatRect { { }, pixelSize });
}

} // namespace WebCore
