/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
#include "ImageData.h"

namespace WebCore {

ImageBufferBackend::ImageBufferBackend(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace colorSpace)
    : m_logicalSize(logicalSize)
    , m_backendSize(backendSize)
    , m_resolutionScale(resolutionScale)
    , m_colorSpace(colorSpace)
{
}

IntSize ImageBufferBackend::calculateBackendSize(const FloatSize& size, float resolutionScale)
{
    FloatSize scaledSize = { ceilf(resolutionScale * size.width()), ceilf(resolutionScale * size.height()) };
    if (scaledSize.isEmpty() || !scaledSize.isExpressibleAsIntSize())
        return { };

    IntSize backendSize = IntSize(scaledSize);

    Checked<unsigned, RecordOverflow> bytesPerRow = 4 * Checked<unsigned, RecordOverflow>(backendSize.width());
    if (bytesPerRow.hasOverflowed())
        return { };

    CheckedSize numBytes = Checked<unsigned, RecordOverflow>(backendSize.height()) * bytesPerRow;
    if (numBytes.hasOverflowed())
        return { };

    return backendSize;
}

NativeImagePtr ImageBufferBackend::sinkIntoNativeImage()
{
    return copyNativeImage(DontCopyBackingStore);
}

RefPtr<Image> ImageBufferBackend::sinkIntoImage(PreserveResolution preserveResolution)
{
    return copyImage(DontCopyBackingStore, preserveResolution);
}

void ImageBufferBackend::drawConsuming(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    draw(destContext, destRect, srcRect, options);
}

void ImageBufferBackend::convertToLuminanceMask()
{
    auto imageData = getImageData(AlphaPremultiplication::Unpremultiplied, logicalRect());
    if (!imageData)
        return;

    auto* srcPixelArray = imageData->data();
    unsigned pixelArrayLength = srcPixelArray->length();
    for (unsigned pixelOffset = 0; pixelOffset < pixelArrayLength; pixelOffset += 4) {
        uint8_t a = srcPixelArray->item(pixelOffset + 3);
        if (!a)
            continue;
        uint8_t r = srcPixelArray->item(pixelOffset);
        uint8_t g = srcPixelArray->item(pixelOffset + 1);
        uint8_t b = srcPixelArray->item(pixelOffset + 2);

        double luma = (r * 0.2125 + g * 0.7154 + b * 0.0721) * ((double)a / 255.0);
        srcPixelArray->set(pixelOffset + 3, luma);
    }

    putImageData(AlphaPremultiplication::Unpremultiplied, *imageData, logicalRect(), IntPoint::zero(), AlphaPremultiplication::Premultiplied);
}

Vector<uint8_t> ImageBufferBackend::toBGRAData(void* data) const
{
    Vector<uint8_t> result(4 * m_logicalSize.area().unsafeGet());
    size_t destBytesPerRow = m_logicalSize.width() * 4;
    size_t srcBytesPerRow = bytesPerRow();

    uint8_t* srcRows = reinterpret_cast<uint8_t*>(data);

    copyImagePixels(
        AlphaPremultiplication::Premultiplied, backendColorFormat(), srcBytesPerRow, srcRows,
        AlphaPremultiplication::Unpremultiplied, ColorFormat::BGRA, destBytesPerRow, result.data(), m_logicalSize);

    return result;
}

static inline void copyPremultipliedToPremultiplied(ColorFormat srcColorFormat, const uint8_t* srcPixel, ColorFormat destColorFormat, uint8_t* destPixel)
{
    uint8_t alpha = srcPixel[3];
    if (!alpha) {
        reinterpret_cast<uint32_t*>(destPixel)[0] = 0;
        return;
    }

    if (srcColorFormat == destColorFormat) {
        reinterpret_cast<uint32_t*>(destPixel)[0] = reinterpret_cast<const uint32_t*>(srcPixel)[0];
        return;
    }

    // Swap pixel channels BGRA <-> RGBA.
    destPixel[0] = srcPixel[2];
    destPixel[1] = srcPixel[1];
    destPixel[2] = srcPixel[0];
    destPixel[3] = srcPixel[3];
}

static inline void copyPremultipliedToUnpremultiplied(ColorFormat srcColorFormat, const uint8_t* srcPixel, ColorFormat destColorFormat, uint8_t* destPixel)
{
    uint8_t alpha = srcPixel[3];
    if (!alpha || alpha == 255) {
        copyPremultipliedToPremultiplied(srcColorFormat, srcPixel, destColorFormat, destPixel);
        return;
    }

    if (srcColorFormat == destColorFormat) {
        destPixel[0] = (srcPixel[0] * 255) / alpha;
        destPixel[1] = (srcPixel[1] * 255) / alpha;
        destPixel[2] = (srcPixel[2] * 255) / alpha;
        destPixel[3] = alpha;
        return;
    }

    // Swap pixel channels BGRA <-> RGBA.
    destPixel[0] = (srcPixel[2] * 255) / alpha;
    destPixel[1] = (srcPixel[1] * 255) / alpha;
    destPixel[2] = (srcPixel[0] * 255) / alpha;
    destPixel[3] = alpha;
}

static inline void copyUnpremultipliedToPremultiplied(ColorFormat srcColorFormat, const uint8_t* srcPixel, ColorFormat destColorFormat, uint8_t* destPixel)
{
    uint8_t alpha = srcPixel[3];
    if (!alpha || alpha == 255) {
        copyPremultipliedToPremultiplied(srcColorFormat, srcPixel, destColorFormat, destPixel);
        return;
    }

    if (srcColorFormat == destColorFormat) {
        destPixel[0] = (srcPixel[0] * alpha + 254) / 255;
        destPixel[1] = (srcPixel[1] * alpha + 254) / 255;
        destPixel[2] = (srcPixel[2] * alpha + 254) / 255;
        destPixel[3] = alpha;
        return;
    }

    // Swap pixel channels BGRA <-> RGBA.
    destPixel[0] = (srcPixel[2] * alpha + 254) / 255;
    destPixel[1] = (srcPixel[1] * alpha + 254) / 255;
    destPixel[2] = (srcPixel[0] * alpha + 254) / 255;
    destPixel[3] = alpha;
}

static inline void copyUnpremultipliedToUnpremultiplied(ColorFormat srcColorFormat, const uint8_t* srcPixel, ColorFormat destColorFormat, uint8_t* destPixel)
{
    if (srcColorFormat == destColorFormat) {
        reinterpret_cast<uint32_t*>(destPixel)[0] = reinterpret_cast<const uint32_t*>(srcPixel)[0];
        return;
    }

    // Swap pixel channels BGRA <-> RGBA.
    destPixel[0] = srcPixel[2];
    destPixel[1] = srcPixel[1];
    destPixel[2] = srcPixel[0];
    destPixel[3] = srcPixel[3];
}

template<void (*copyFunctor)(ColorFormat, const uint8_t*, ColorFormat, uint8_t*)>
static inline void copyImagePixelsUnaccelerated(
    ColorFormat srcColorFormat, unsigned srcBytesPerRow, uint8_t* srcRows,
    ColorFormat destColorFormat, unsigned destBytesPerRow, uint8_t* destRows, const IntSize& size)
{
    size_t bytesPerRow = size.width() * 4;
    for (int y = 0; y < size.height(); ++y) {
        for (size_t x = 0; x < bytesPerRow; x += 4)
            copyFunctor(srcColorFormat, &srcRows[x], destColorFormat, &destRows[x]);
        srcRows += srcBytesPerRow;
        destRows += destBytesPerRow;
    }
}

void ImageBufferBackend::copyImagePixels(
    AlphaPremultiplication srcAlphaFormat, ColorFormat srcColorFormat, unsigned srcBytesPerRow, uint8_t* srcRows,
    AlphaPremultiplication destAlphaFormat, ColorFormat destColorFormat, unsigned destBytesPerRow, uint8_t* destRows, const IntSize& size) const
{
    if (srcAlphaFormat == destAlphaFormat) {
        if (srcAlphaFormat == AlphaPremultiplication::Premultiplied)
            copyImagePixelsUnaccelerated<copyPremultipliedToPremultiplied>(srcColorFormat, srcBytesPerRow, srcRows, destColorFormat, destBytesPerRow, destRows, size);
        else
            copyImagePixelsUnaccelerated<copyUnpremultipliedToUnpremultiplied>(srcColorFormat, srcBytesPerRow, srcRows, destColorFormat, destBytesPerRow, destRows, size);
        return;
    }

    if (destAlphaFormat == AlphaPremultiplication::Unpremultiplied) {
        copyImagePixelsUnaccelerated<copyPremultipliedToUnpremultiplied>(srcColorFormat, srcBytesPerRow, srcRows, destColorFormat, destBytesPerRow, destRows, size);
        return;
    }

    copyImagePixelsUnaccelerated<copyUnpremultipliedToPremultiplied>(srcColorFormat, srcBytesPerRow, srcRows, destColorFormat, destBytesPerRow, destRows, size);
}

RefPtr<ImageData> ImageBufferBackend::getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect, void* data) const
{
    IntRect srcRectScaled = toBackendCoordinates(srcRect);

    auto imageData = ImageData::create(srcRectScaled.size());
    if (!imageData || !imageData->data())
        return nullptr;

    IntRect srcRectClipped =  intersection(backendRect(), srcRectScaled);
    IntRect destRect = { IntPoint::zero(), srcRectClipped.size() };

    if (srcRectScaled.x() < 0)
        destRect.setX(-srcRectScaled.x());

    if (srcRectScaled.y() < 0)
        destRect.setY(-srcRectScaled.y());

    if (destRect.size() != srcRectScaled.size())
        imageData->data()->zeroFill();

    unsigned destBytesPerRow = 4 * srcRectScaled.width();
    uint8_t* destRows = imageData->data()->data() + destRect.y() * destBytesPerRow + destRect.x() * 4;

    unsigned srcBytesPerRow = bytesPerRow();
    uint8_t* srcRows = reinterpret_cast<uint8_t*>(data) + srcRectClipped.y() * srcBytesPerRow + srcRectClipped.x() * 4;

    copyImagePixels(
        AlphaPremultiplication::Premultiplied, backendColorFormat(), srcBytesPerRow, srcRows,
        outputFormat, ColorFormat::RGBA, destBytesPerRow, destRows, destRect.size());

    return imageData;
}

void ImageBufferBackend::putImageData(AlphaPremultiplication inputFormat, const ImageData& imageData, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat, void* data)
{
    IntRect srcRectScaled = toBackendCoordinates(srcRect);
    IntPoint destPointScaled = toBackendCoordinates(destPoint);

    IntRect srcRectClipped = intersection({ IntPoint::zero(), imageData.size() }, srcRectScaled);
    IntRect destRect = srcRectClipped;
    destRect.moveBy(destPointScaled);

    if (srcRectScaled.x() < 0)
        destRect.setX(destRect.x() - srcRectScaled.x());

    if (srcRectScaled.y() < 0)
        destRect.setY(destRect.y() - srcRectScaled.y());

    destRect.intersect(backendRect());
    srcRectClipped.setSize(destRect.size());

    unsigned destBytesPerRow = bytesPerRow();
    uint8_t* destRows = reinterpret_cast<uint8_t*>(data) + destRect.y() * destBytesPerRow + destRect.x() * 4;

    unsigned srcBytesPerRow = 4 * imageData.size().width();
    uint8_t* srcRows = imageData.data()->data() + srcRectClipped.y() * srcBytesPerRow + srcRectClipped.x() * 4;

    copyImagePixels(
        inputFormat, ColorFormat::RGBA, srcBytesPerRow, srcRows,
        destFormat, backendColorFormat(), destBytesPerRow, destRows, destRect.size());
}

} // namespace WebCore
