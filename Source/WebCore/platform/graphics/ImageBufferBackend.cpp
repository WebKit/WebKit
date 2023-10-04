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

#include "GraphicsContext.h"
#include "Image.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"

namespace WebCore {

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
    return createNativeImageReference();
}

void ImageBufferBackend::convertToLuminanceMask()
{
    IntRect sourceRect { { }, size() };
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, colorSpace() };
    auto pixelBuffer = ImageBufferAllocator().createPixelBuffer(format, sourceRect.size());
    if (!pixelBuffer)
        return;
    getPixelBuffer(sourceRect, *pixelBuffer);

    unsigned pixelArrayLength = pixelBuffer->sizeInBytes();
    for (unsigned pixelOffset = 0; pixelOffset < pixelArrayLength; pixelOffset += 4) {
        uint8_t a = pixelBuffer->item(pixelOffset + 3);
        if (!a)
            continue;
        uint8_t r = pixelBuffer->item(pixelOffset);
        uint8_t g = pixelBuffer->item(pixelOffset + 1);
        uint8_t b = pixelBuffer->item(pixelOffset + 2);

        double luma = (r * 0.2125 + g * 0.7154 + b * 0.0721) * ((double)a / 255.0);
        pixelBuffer->set(pixelOffset + 3, luma);
    }

    putPixelBuffer(*pixelBuffer, sourceRect, IntPoint::zero(), AlphaPremultiplication::Premultiplied);
}

void ImageBufferBackend::getPixelBuffer(const IntRect& sourceRect, void* sourceData, PixelBuffer& destinationPixelBuffer)
{
    IntRect backendRect { { }, size() };
    auto sourceRectClipped = intersection(backendRect, sourceRect);
    IntRect destinationRect { IntPoint::zero(), sourceRectClipped.size() };

    if (sourceRect.x() < 0)
        destinationRect.setX(-sourceRect.x());

    if (sourceRect.y() < 0)
        destinationRect.setY(-sourceRect.y());

    if (destinationRect.size() != sourceRect.size())
        destinationPixelBuffer.zeroFill();

    unsigned sourceBytesPerRow = bytesPerRow();
    ConstPixelBufferConversionView source {
        { AlphaPremultiplication::Premultiplied, pixelFormat(), colorSpace() },
        sourceBytesPerRow,
        static_cast<uint8_t*>(sourceData) + sourceRectClipped.y() * sourceBytesPerRow + sourceRectClipped.x() * 4
    };
    unsigned destinationBytesPerRow = static_cast<unsigned>(4u * sourceRect.width());
    PixelBufferConversionView destination {
        destinationPixelBuffer.format(),
        destinationBytesPerRow,
        destinationPixelBuffer.bytes() + destinationRect.y() * destinationBytesPerRow + destinationRect.x() * 4
    };

    convertImagePixels(source, destination, destinationRect.size());
}

void ImageBufferBackend::putPixelBuffer(const PixelBuffer& sourcePixelBuffer, const IntRect& sourceRect, const IntPoint& destinationPoint, AlphaPremultiplication destinationAlphaFormat, void* destinationData)
{
    IntRect backendRect { { }, size() };
    auto sourceRectClipped = intersection({ IntPoint::zero(), sourcePixelBuffer.size() }, sourceRect);
    auto destinationRect = sourceRectClipped;
    destinationRect.moveBy(destinationPoint);

    if (sourceRect.x() < 0)
        destinationRect.setX(destinationRect.x() - sourceRect.x());

    if (sourceRect.y() < 0)
        destinationRect.setY(destinationRect.y() - sourceRect.y());

    destinationRect.intersect(backendRect);
    sourceRectClipped.setSize(destinationRect.size());

    unsigned sourceBytesPerRow = static_cast<unsigned>(4u * sourcePixelBuffer.size().width());
    ConstPixelBufferConversionView source {
        sourcePixelBuffer.format(),
        sourceBytesPerRow,
        sourcePixelBuffer.bytes() + sourceRectClipped.y() * sourceBytesPerRow + sourceRectClipped.x() * 4
    };
    unsigned destinationBytesPerRow = bytesPerRow();
    PixelBufferConversionView destination {
        { destinationAlphaFormat, pixelFormat(), colorSpace() },
        destinationBytesPerRow,
        static_cast<uint8_t*>(destinationData) + destinationRect.y() * destinationBytesPerRow + destinationRect.x() * 4
    };

    convertImagePixels(source, destination, destinationRect.size());
}

AffineTransform ImageBufferBackend::calculateBaseTransform(const Parameters& parameters, bool originAtBottomLeftCorner)
{
    AffineTransform baseTransform;

    if (originAtBottomLeftCorner) {
        baseTransform.scale(1, -1);
        baseTransform.translate(0, -parameters.backendSize.height());
    }

    baseTransform.scale(parameters.resolutionScale);

    return baseTransform;
}

TextStream& operator<<(TextStream& ts, VolatilityState state)
{
    switch (state) {
    case VolatilityState::NonVolatile: ts << "non-volatile"; break;
    case VolatilityState::Volatile: ts << "volatile"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const ImageBufferBackend& imageBufferBackend)
{
    ts << imageBufferBackend.debugDescription();
    return ts;
}

} // namespace WebCore
