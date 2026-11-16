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

static PixelBufferFormat renderingFormat(const ColorSpace& destinationColorSpace)
{
    if (!MeshGradient::renderingColorSpace(destinationColorSpace)) {
#if ENABLE(PIXEL_FORMAT_RGBA16F) && ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)
        return { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, ColorSpace::ExtendedSRGB() };
#else
        return { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
#endif
    }

#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (destinationColorSpace.usesExtendedRange())
        return { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, destinationColorSpace };
#endif
    return { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, destinationColorSpace };
}

static PixelFormat imageBufferPixelFormat([[maybe_unused]] const ColorSpace& colorSpace)
{
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (colorSpace.usesExtendedRange())
        return PixelFormat::RGBA16F;
#endif
    return PixelFormat::BGRA8;
}

static const ColorSpace& imageColorSpace(const ColorSpace& contextColorSpace)
{
    if (contextColorSpace.supportsOutput() && contextColorSpace.usesRGBColorModel())
        return contextColorSpace;

#if ENABLE(PIXEL_FORMAT_RGBA16F) && ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)
    return ColorSpace::ExtendedSRGB();
#else
    return ColorSpace::SRGB();
#endif
}

bool MeshGradientRenderer::draw(GraphicsContext& context, const MeshGradient& gradient, const FloatRect& destination)
{
    if (context.paintingDisabled())
        return true;

    auto transform = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    float scale = std::max(std::abs(transform.xScale()), std::abs(transform.yScale()));

    auto pixelSize = expandedIntSize(destination.size().scaled(scale));
    if (pixelSize.isEmpty())
        return true;

    auto& colorSpace = imageColorSpace(context.colorSpace());

    RefPtr image = gradient.renderedImage(pixelSize, scale, colorSpace);
    if (!image) {
        auto format = renderingFormat(colorSpace);
        auto bufferSize = PixelBuffer::computeBufferSize(format.pixelFormat, pixelSize);
        if (bufferSize.hasOverflowed())
            return false;

        Vector<uint8_t> pixels;
        if (!pixels.tryGrow(bufferSize.value()))
            return false;

        if (!render(gradient, { format, PixelBuffer::bytesPerPixel(format.pixelFormat) * pixelSize.width(), pixels.mutableSpan() }, pixelSize, scale))
            return false;

        auto source = PixelBufferSourceView::create(format, pixelSize, pixels.span());
        if (!source)
            return false;

        RefPtr imageBuffer = context.createImageBuffer(pixelSize, 1, colorSpace, RenderingMode::Unaccelerated, std::nullopt, { imageBufferPixelFormat(colorSpace) });
        if (!imageBuffer)
            return false;

        imageBuffer->putPixelBuffer(*source, { { }, pixelSize });

        image = ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
        if (!image)
            return false;

        gradient.setRenderedImage(pixelSize, scale, *image);
    }

    context.drawNativeImage(*image, destination, FloatRect { { }, pixelSize });
    return true;
}

} // namespace WebCore
