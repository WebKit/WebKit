/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "ImageBufferSkiaSurfaceBackend.h"

#if USE(SKIA)
#include "IntRect.h"
#include "PixelBuffer.h"
#include "SkiaSpanExtras.h"

namespace WebCore {

IntSize ImageBufferSkiaSurfaceBackend::calculateSafeBackendSize(const Parameters& parameters)
{
    IntSize backendSize = parameters.backendSize;
    if (backendSize.isEmpty())
        return backendSize;

    auto bytesPerRow = 4 * CheckedUint32(backendSize.width());
    if (bytesPerRow.hasOverflowed())
        return { };

    CheckedSize numBytes = CheckedUint32(backendSize.height()) * bytesPerRow;
    if (numBytes.hasOverflowed())
        return { };

    return backendSize;
}

unsigned ImageBufferSkiaSurfaceBackend::calculateBytesPerRow(const IntSize& backendSize)
{
    ASSERT(!backendSize.isEmpty());
    return CheckedUint32(backendSize.width()) * 4;
}

size_t ImageBufferSkiaSurfaceBackend::calculateMemoryCost(const Parameters& parameters)
{
    return ImageBufferBackend::calculateMemoryCost(parameters.backendSize, calculateBytesPerRow(parameters.backendSize));
}

ImageBufferSkiaSurfaceBackend::ImageBufferSkiaSurfaceBackend(const Parameters& parameters, sk_sp<SkSurface>&& surface, RenderingMode renderingMode)
    : ImageBufferSkiaBackend(parameters)
    , m_surface(WTF::move(surface))
    , m_context(*m_surface->getCanvas(), renderingMode, parameters.purpose)
{
    m_context.applyDeviceScaleFactor(parameters.resolutionScale);
}

ImageBufferSkiaSurfaceBackend::~ImageBufferSkiaSurfaceBackend() = default;

unsigned ImageBufferSkiaSurfaceBackend::bytesPerRow() const
{
    return m_surface->imageInfo().minRowBytes64();
}

bool ImageBufferSkiaSurfaceBackend::canMapBackingStore() const
{
    return true;
}

void ImageBufferSkiaSurfaceBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    const IntRect backendRect { { }, size() };
    const auto sourceRectClipped = intersection(backendRect, srcRect);
    IntRect destinationRect { IntPoint::zero(), sourceRectClipped.size() };

    if (srcRect.x() < 0)
        destinationRect.setX(destinationRect.x() - srcRect.x());
    if (srcRect.y() < 0)
        destinationRect.setY(destinationRect.y() - srcRect.y());

    if (destination.size() != sourceRectClipped.size())
        destination.zeroFill();

    const auto destinationColorType = (destination.format().pixelFormat == PixelFormat::RGBA8)
        ? SkColorType::kRGBA_8888_SkColorType : SkColorType::kBGRA_8888_SkColorType;

    const auto destinationAlphaType = (destination.format().alphaFormat == AlphaPremultiplication::Premultiplied)
        ? SkAlphaType::kPremul_SkAlphaType : SkAlphaType::kUnpremul_SkAlphaType;

    auto destinationInfo = SkImageInfo::Make(destination.size().width(), destination.size().height(),
        destinationColorType, destinationAlphaType, destination.format().colorSpace.platformColorSpace());
    SkPixmap pixmap(destinationInfo, destination.bytes().data(), destination.size().width() * 4);

    SkPixmap dstPixmap;
    if (!pixmap.extractSubset(&dstPixmap, destinationRect)) [[unlikely]]
        return;

    m_surface->readPixels(dstPixmap, sourceRectClipped.x(), sourceRectClipped.y());
}

void ImageBufferSkiaSurfaceBackend::putPixelBuffer(const PixelBufferSourceView& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    ASSERT(IntRect({ 0, 0 }, pixelBuffer.size()).contains(srcRect));
    ASSERT(pixelBuffer.format().pixelFormat == PixelFormat::RGBA8 || pixelBuffer.format().pixelFormat == PixelFormat::BGRA8);
    ASSERT(pixelBuffer.format().alphaFormat == AlphaPremultiplication::Premultiplied || pixelBuffer.format().alphaFormat == AlphaPremultiplication::Unpremultiplied);

    const auto colorType = (pixelBuffer.format().pixelFormat == PixelFormat::RGBA8)
        ? SkColorType::kRGBA_8888_SkColorType : SkColorType::kBGRA_8888_SkColorType;

    const auto alphaType = (pixelBuffer.format().alphaFormat == AlphaPremultiplication::Premultiplied)
        ? SkAlphaType::kPremul_SkAlphaType : SkAlphaType::kUnpremul_SkAlphaType;

    const IntRect backendRect { { }, size() };
    auto sourceRectClipped = intersection({ IntPoint::zero(), pixelBuffer.size() }, srcRect);
    auto destinationRect = sourceRectClipped;
    destinationRect.moveBy(destPoint);

    if (srcRect.x() < 0)
        destinationRect.setX(destinationRect.x() - srcRect.x());
    if (srcRect.y() < 0)
        destinationRect.setY(destinationRect.y() - srcRect.y());

    destinationRect.intersect(backendRect);
    sourceRectClipped.setSize(destinationRect.size());

    auto pixelBufferInfo = SkImageInfo::Make(pixelBuffer.size().width(), pixelBuffer.size().height(),
        colorType, alphaType, pixelBuffer.format().colorSpace.platformColorSpace());
    SkPixmap pixmap(pixelBufferInfo, pixelBuffer.bytes().data(), pixelBuffer.size().width() * 4);

    SkPixmap srcPixmap;
    if (!pixmap.extractSubset(&srcPixmap, sourceRectClipped)) [[unlikely]]
        return;

    const auto destAlphaType = (destFormat == AlphaPremultiplication::Premultiplied)
        ? SkAlphaType::kPremul_SkAlphaType : SkAlphaType::kUnpremul_SkAlphaType;

    // If all the pixels in the source rectangle are opaque, it does not matter which kind
    // of alpha is involved: the destination pixels will be replaced by the source ones.
    if (m_surface->imageInfo().alphaType() == destAlphaType || srcPixmap.computeIsOpaque()) {
        m_surface->writePixels(srcPixmap, destinationRect.x(), destinationRect.y());
        return;
    }

    // Fall back to converting, but only the part covered by sourceRectClipped/srcPixmap.
    auto data = SkData::MakeUninitialized(srcPixmap.computeByteSize());
    ImageBufferBackend::putPixelBuffer(pixelBuffer, sourceRectClipped, IntPoint::zero(), destFormat, mutableSpan(*data));
    auto convertedSrcInfo = SkImageInfo::Make(srcPixmap.dimensions(), SkColorType::kBGRA_8888_SkColorType,
        SkAlphaType::kPremul_SkAlphaType, colorSpace().platformColorSpace());
    SkPixmap convertedSrcPixmap(convertedSrcInfo, data->writable_data(), convertedSrcInfo.minRowBytes64());
    m_surface->writePixels(convertedSrcPixmap, destinationRect.x(), destinationRect.y());
}

} // namespace WebCore

#endif // USE(SKIA)
