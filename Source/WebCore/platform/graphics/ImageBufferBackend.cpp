/*
 * Copyright (C) 2020-2021 Apple Inc.  All rights reserved.
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
#include "ImageBufferBackend.h"

#include "Image.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"

namespace WebCore {

IntSize ImageBufferBackend::calculateBackendSize(const Parameters& parameters)
{
    FloatSize scaledSize = { ceilf(parameters.resolutionScale * parameters.logicalSize.width()), ceilf(parameters.resolutionScale * parameters.logicalSize.height()) };
    if (scaledSize.isEmpty() || !scaledSize.isExpressibleAsIntSize())
        return { };

    return IntSize(scaledSize);
}

size_t ImageBufferBackend::calculateMemoryCost(const IntSize& backendSize, unsigned bytesPerRow)
{
    ASSERT(!backendSize.isEmpty());
    return CheckedUint32(backendSize.height()) * bytesPerRow;
}

ImageBufferBackend::ImageBufferBackend(const Parameters& parameters)
    : m_parameters(parameters)
{
}

ImageBufferBackend::~ImageBufferBackend() = default;

RefPtr<NativeImage> ImageBufferBackend::sinkIntoNativeImage()
{
    return copyNativeImage(DontCopyBackingStore);
}

RefPtr<Image> ImageBufferBackend::sinkIntoImage(PreserveResolution preserveResolution)
{
    return copyImage(DontCopyBackingStore, preserveResolution);
}

void ImageBufferBackend::drawConsuming(GraphicsContext& destinationContext, const FloatRect& destinationRect, const FloatRect& sourceRect, const ImagePaintingOptions& options)
{
    draw(destinationContext, destinationRect, sourceRect, options);
}

void ImageBufferBackend::convertToLuminanceMask()
{
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, colorSpace() };
    auto pixelBuffer = getPixelBuffer(format, logicalRect());
    if (!pixelBuffer)
        return;

    auto& pixelArray = pixelBuffer->data();
    unsigned pixelArrayLength = pixelArray.length();
    for (unsigned pixelOffset = 0; pixelOffset < pixelArrayLength; pixelOffset += 4) {
        uint8_t a = pixelArray.item(pixelOffset + 3);
        if (!a)
            continue;
        uint8_t r = pixelArray.item(pixelOffset);
        uint8_t g = pixelArray.item(pixelOffset + 1);
        uint8_t b = pixelArray.item(pixelOffset + 2);

        double luma = (r * 0.2125 + g * 0.7154 + b * 0.0721) * ((double)a / 255.0);
        pixelArray.set(pixelOffset + 3, luma);
    }

    putPixelBuffer(*pixelBuffer, logicalRect(), IntPoint::zero(), AlphaPremultiplication::Premultiplied);
}

std::optional<PixelBuffer> ImageBufferBackend::getPixelBuffer(const PixelBufferFormat& destinationFormat, const IntRect& sourceRect, void* data) const
{
    ASSERT(PixelBuffer::supportedPixelFormat(destinationFormat.pixelFormat));

    auto sourceRectScaled = toBackendCoordinates(sourceRect);

    auto pixelBuffer = PixelBuffer::tryCreate(destinationFormat, sourceRectScaled.size());
    if (!pixelBuffer)
        return std::nullopt;

    auto sourceRectClipped = intersection(backendRect(), sourceRectScaled);
    IntRect destinationRect { IntPoint::zero(), sourceRectClipped.size() };

    if (sourceRectScaled.x() < 0)
        destinationRect.setX(-sourceRectScaled.x());

    if (sourceRectScaled.y() < 0)
        destinationRect.setY(-sourceRectScaled.y());

    if (destinationRect.size() != sourceRectScaled.size())
        pixelBuffer->data().zeroFill();

    ConstPixelBufferConversionView source {
        { AlphaPremultiplication::Premultiplied, pixelFormat(), colorSpace() },
        bytesPerRow(),
        static_cast<uint8_t*>(data) + sourceRectClipped.y() * source.bytesPerRow + sourceRectClipped.x() * 4
    };
    
    PixelBufferConversionView destination {
        destinationFormat,
        static_cast<unsigned>(4 * sourceRectScaled.width()),
        pixelBuffer->data().data() + destinationRect.y() * destination.bytesPerRow + destinationRect.x() * 4
    };

    convertImagePixels(source, destination, destinationRect.size());

    return pixelBuffer;
}

void ImageBufferBackend::putPixelBuffer(const PixelBuffer& sourcePixelBuffer, const IntRect& sourceRect, const IntPoint& destinationPoint, AlphaPremultiplication destinationAlphaFormat, void* data)
{
    auto sourceRectScaled = toBackendCoordinates(sourceRect);
    auto destinationPointScaled = toBackendCoordinates(destinationPoint);

    auto sourceRectClipped = intersection({ IntPoint::zero(), sourcePixelBuffer.size() }, sourceRectScaled);
    auto destinationRect = sourceRectClipped;
    destinationRect.moveBy(destinationPointScaled);

    if (sourceRectScaled.x() < 0)
        destinationRect.setX(destinationRect.x() - sourceRectScaled.x());

    if (sourceRectScaled.y() < 0)
        destinationRect.setY(destinationRect.y() - sourceRectScaled.y());

    destinationRect.intersect(backendRect());
    sourceRectClipped.setSize(destinationRect.size());

    ConstPixelBufferConversionView source {
        sourcePixelBuffer.format(),
        static_cast<unsigned>(4 * sourcePixelBuffer.size().width()),
        sourcePixelBuffer.data().data() + sourceRectClipped.y() * source.bytesPerRow + sourceRectClipped.x() * 4
    };

    PixelBufferConversionView destination {
        { destinationAlphaFormat, pixelFormat(), colorSpace() },
        bytesPerRow(),
        static_cast<uint8_t*>(data) + destinationRect.y() * destination.bytesPerRow + destinationRect.x() * 4
    };

    convertImagePixels(source, destination, destinationRect.size());
}

} // namespace WebCore
