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
    CheckedSize numBytes = Checked<unsigned, RecordOverflow>(backendSize.height()) * bytesPerRow;
    return numBytes.unsafeGet();
}

ImageBufferBackend::ImageBufferBackend(const Parameters& parameters)
    : m_parameters(parameters)
{
}

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
    auto pixelBuffer = getPixelBuffer(AlphaPremultiplication::Unpremultiplied, logicalRect());
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

    putPixelBuffer(AlphaPremultiplication::Unpremultiplied, *pixelBuffer, logicalRect(), IntPoint::zero(), AlphaPremultiplication::Premultiplied);
}

Vector<uint8_t> ImageBufferBackend::toBGRAData(void* data) const
{
    Vector<uint8_t> result(4 * logicalSize().area().unsafeGet());
    size_t destinationBytesPerRow = logicalSize().width() * 4;
    size_t sourceBytesPerRow = bytesPerRow();

    uint8_t* sourceRows = reinterpret_cast<uint8_t*>(data);

    copyImagePixels(
        AlphaPremultiplication::Premultiplied, pixelFormat(), sourceBytesPerRow, sourceRows,
        AlphaPremultiplication::Unpremultiplied, PixelFormat::BGRA8, destinationBytesPerRow, result.data(), logicalSize());

    return result;
}

static inline void copyPremultipliedToPremultiplied(PixelFormat sourcePixelFormat, const uint8_t* sourcePixel, PixelFormat destinationPixelFormat, uint8_t* destinationPixel)
{
    uint8_t alpha = sourcePixel[3];
    if (!alpha) {
        reinterpret_cast<uint32_t*>(destinationPixel)[0] = 0;
        return;
    }

    if (sourcePixelFormat == destinationPixelFormat) {
        reinterpret_cast<uint32_t*>(destinationPixel)[0] = reinterpret_cast<const uint32_t*>(sourcePixel)[0];
        return;
    }

    // Swap pixel channels BGRA <-> RGBA.
    destinationPixel[0] = sourcePixel[2];
    destinationPixel[1] = sourcePixel[1];
    destinationPixel[2] = sourcePixel[0];
    destinationPixel[3] = sourcePixel[3];
}

static inline void copyPremultipliedToUnpremultiplied(PixelFormat sourcePixelFormat, const uint8_t* sourcePixel, PixelFormat destinationPixelFormat, uint8_t* destinationPixel)
{
    uint8_t alpha = sourcePixel[3];
    if (!alpha || alpha == 255) {
        copyPremultipliedToPremultiplied(sourcePixelFormat, sourcePixel, destinationPixelFormat, destinationPixel);
        return;
    }

    if (sourcePixelFormat == destinationPixelFormat) {
        destinationPixel[0] = (sourcePixel[0] * 255) / alpha;
        destinationPixel[1] = (sourcePixel[1] * 255) / alpha;
        destinationPixel[2] = (sourcePixel[2] * 255) / alpha;
        destinationPixel[3] = alpha;
        return;
    }

    // Swap pixel channels BGRA <-> RGBA.
    destinationPixel[0] = (sourcePixel[2] * 255) / alpha;
    destinationPixel[1] = (sourcePixel[1] * 255) / alpha;
    destinationPixel[2] = (sourcePixel[0] * 255) / alpha;
    destinationPixel[3] = alpha;
}

static inline void copyUnpremultipliedToPremultiplied(PixelFormat sourcePixelFormat, const uint8_t* sourcePixel, PixelFormat destinationPixelFormat, uint8_t* destinationPixel)
{
    uint8_t alpha = sourcePixel[3];
    if (!alpha || alpha == 255) {
        copyPremultipliedToPremultiplied(sourcePixelFormat, sourcePixel, destinationPixelFormat, destinationPixel);
        return;
    }

    if (sourcePixelFormat == destinationPixelFormat) {
        destinationPixel[0] = (sourcePixel[0] * alpha + 254) / 255;
        destinationPixel[1] = (sourcePixel[1] * alpha + 254) / 255;
        destinationPixel[2] = (sourcePixel[2] * alpha + 254) / 255;
        destinationPixel[3] = alpha;
        return;
    }

    // Swap pixel channels BGRA <-> RGBA.
    destinationPixel[0] = (sourcePixel[2] * alpha + 254) / 255;
    destinationPixel[1] = (sourcePixel[1] * alpha + 254) / 255;
    destinationPixel[2] = (sourcePixel[0] * alpha + 254) / 255;
    destinationPixel[3] = alpha;
}

static inline void copyUnpremultipliedToUnpremultiplied(PixelFormat sourcePixelFormat, const uint8_t* sourcePixel, PixelFormat destinationPixelFormat, uint8_t* destinationPixel)
{
    if (sourcePixelFormat == destinationPixelFormat) {
        reinterpret_cast<uint32_t*>(destinationPixel)[0] = reinterpret_cast<const uint32_t*>(sourcePixel)[0];
        return;
    }

    // Swap pixel channels BGRA <-> RGBA.
    destinationPixel[0] = sourcePixel[2];
    destinationPixel[1] = sourcePixel[1];
    destinationPixel[2] = sourcePixel[0];
    destinationPixel[3] = sourcePixel[3];
}

template<void (*copyFunctor)(PixelFormat, const uint8_t*, PixelFormat, uint8_t*)>
static inline void copyImagePixelsUnaccelerated(
    PixelFormat sourcePixelFormat, unsigned sourceBytesPerRow, uint8_t* sourceRows,
    PixelFormat destinationPixelFormat, unsigned destinationBytesPerRow, uint8_t* destinationRows, const IntSize& size)
{
    size_t bytesPerRow = size.width() * 4;
    for (int y = 0; y < size.height(); ++y) {
        for (size_t x = 0; x < bytesPerRow; x += 4)
            copyFunctor(sourcePixelFormat, &sourceRows[x], destinationPixelFormat, &destinationRows[x]);
        sourceRows += sourceBytesPerRow;
        destinationRows += destinationBytesPerRow;
    }
}

void ImageBufferBackend::copyImagePixels(
    AlphaPremultiplication sourceAlphaFormat, PixelFormat sourcePixelFormat, unsigned sourceBytesPerRow, uint8_t* sourceRows,
    AlphaPremultiplication destinationAlphaFormat, PixelFormat destinationPixelFormat, unsigned destinationBytesPerRow, uint8_t* destinationRows, const IntSize& size) const
{
    // We don't currently support getting or putting pixel data with deep color buffers.
    ASSERT(sourcePixelFormat == PixelFormat::RGBA8 || sourcePixelFormat == PixelFormat::BGRA8);
    ASSERT(destinationPixelFormat == PixelFormat::RGBA8 || destinationPixelFormat == PixelFormat::BGRA8);

    if (sourceAlphaFormat == destinationAlphaFormat) {
        if (sourceAlphaFormat == AlphaPremultiplication::Premultiplied)
            copyImagePixelsUnaccelerated<copyPremultipliedToPremultiplied>(sourcePixelFormat, sourceBytesPerRow, sourceRows, destinationPixelFormat, destinationBytesPerRow, destinationRows, size);
        else
            copyImagePixelsUnaccelerated<copyUnpremultipliedToUnpremultiplied>(sourcePixelFormat, sourceBytesPerRow, sourceRows, destinationPixelFormat, destinationBytesPerRow, destinationRows, size);
        return;
    }

    if (destinationAlphaFormat == AlphaPremultiplication::Unpremultiplied) {
        copyImagePixelsUnaccelerated<copyPremultipliedToUnpremultiplied>(sourcePixelFormat, sourceBytesPerRow, sourceRows, destinationPixelFormat, destinationBytesPerRow, destinationRows, size);
        return;
    }

    copyImagePixelsUnaccelerated<copyUnpremultipliedToPremultiplied>(sourcePixelFormat, sourceBytesPerRow, sourceRows, destinationPixelFormat, destinationBytesPerRow, destinationRows, size);
}

Optional<PixelBuffer> ImageBufferBackend::getPixelBuffer(AlphaPremultiplication destinationAlphaFormat, const IntRect& sourceRect, void* data) const
{
    auto sourceRectScaled = toBackendCoordinates(sourceRect);

    auto pixelBuffer = PixelBuffer::tryCreate(DestinationColorSpace::SRGB, PixelFormat::RGBA8, sourceRectScaled.size());
    if (!pixelBuffer)
        return WTF::nullopt;

    IntRect sourceRectClipped = intersection(backendRect(), sourceRectScaled);
    IntRect destinationRect = { IntPoint::zero(), sourceRectClipped.size() };

    if (sourceRectScaled.x() < 0)
        destinationRect.setX(-sourceRectScaled.x());

    if (sourceRectScaled.y() < 0)
        destinationRect.setY(-sourceRectScaled.y());

    if (destinationRect.size() != sourceRectScaled.size())
        pixelBuffer->data().zeroFill();

    unsigned destinationBytesPerRow = 4 * sourceRectScaled.width();
    uint8_t* destinationRows = pixelBuffer->data().data() + destinationRect.y() * destinationBytesPerRow + destinationRect.x() * 4;

    unsigned sourceBytesPerRow = bytesPerRow();
    uint8_t* sourceRows = reinterpret_cast<uint8_t*>(data) + sourceRectClipped.y() * sourceBytesPerRow + sourceRectClipped.x() * 4;

    copyImagePixels(
        AlphaPremultiplication::Premultiplied, pixelFormat(), sourceBytesPerRow, sourceRows,
        destinationAlphaFormat, PixelFormat::RGBA8, destinationBytesPerRow, destinationRows, destinationRect.size());

    return pixelBuffer;
}

void ImageBufferBackend::putPixelBuffer(AlphaPremultiplication sourceAlphaFormat, const PixelBuffer& pixelBuffer, const IntRect& sourceRect, const IntPoint& destinationPoint, AlphaPremultiplication destinationAlphaFormat, void* data)
{
    auto sourceRectScaled = toBackendCoordinates(sourceRect);
    auto destinationPointScaled = toBackendCoordinates(destinationPoint);

    IntRect sourceRectClipped = intersection({ IntPoint::zero(), pixelBuffer.size() }, sourceRectScaled);
    IntRect destinationRect = sourceRectClipped;
    destinationRect.moveBy(destinationPointScaled);

    if (sourceRectScaled.x() < 0)
        destinationRect.setX(destinationRect.x() - sourceRectScaled.x());

    if (sourceRectScaled.y() < 0)
        destinationRect.setY(destinationRect.y() - sourceRectScaled.y());

    destinationRect.intersect(backendRect());
    sourceRectClipped.setSize(destinationRect.size());

    unsigned destinationBytesPerRow = bytesPerRow();
    uint8_t* destinationRows = reinterpret_cast<uint8_t*>(data) + destinationRect.y() * destinationBytesPerRow + destinationRect.x() * 4;

    unsigned sourceBytesPerRow = 4 * pixelBuffer.size().width();
    uint8_t* sourceRows = pixelBuffer.data().data() + sourceRectClipped.y() * sourceBytesPerRow + sourceRectClipped.x() * 4;

    copyImagePixels(
        sourceAlphaFormat, PixelFormat::RGBA8, sourceBytesPerRow, sourceRows,
        destinationAlphaFormat, pixelFormat(), destinationBytesPerRow, destinationRows, destinationRect.size());
}

} // namespace WebCore
