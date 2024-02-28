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
#include "ImageBufferSkiaUnacceleratedBackend.h"

#if USE(SKIA)

#include "IntRect.h"
#include "PixelBuffer.h"
#include <skia/core/SkPixmap.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferSkiaUnacceleratedBackend);

std::unique_ptr<ImageBufferSkiaUnacceleratedBackend> ImageBufferSkiaUnacceleratedBackend::create(const Parameters& parameters, const ImageBufferCreationContext&)
{
    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    auto imageInfo = SkImageInfo::MakeN32Premul(backendSize.width(), backendSize.height());
    auto surface = SkSurfaces::Raster(imageInfo, nullptr);
    return std::unique_ptr<ImageBufferSkiaUnacceleratedBackend>(new ImageBufferSkiaUnacceleratedBackend(parameters, WTFMove(surface)));
}

ImageBufferSkiaUnacceleratedBackend::ImageBufferSkiaUnacceleratedBackend(const Parameters& parameters, sk_sp<SkSurface>&& surface)
    : ImageBufferSkiaSurfaceBackend(parameters, WTFMove(surface), RenderingMode::Unaccelerated)
{
}

ImageBufferSkiaUnacceleratedBackend::~ImageBufferSkiaUnacceleratedBackend() = default;

RefPtr<NativeImage> ImageBufferSkiaUnacceleratedBackend::copyNativeImage()
{
    SkPixmap pixmap;
    if (m_surface->peekPixels(&pixmap))
        return NativeImage::create(SkImages::RasterFromPixmapCopy(pixmap));
    return nullptr;
}

RefPtr<NativeImage> ImageBufferSkiaUnacceleratedBackend::createNativeImageReference()
{
    return NativeImage::create(m_surface->makeImageSnapshot());
}

void ImageBufferSkiaUnacceleratedBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    SkPixmap pixmap;
    if (m_surface->peekPixels(&pixmap))
        ImageBufferBackend::getPixelBuffer(srcRect, pixmap.writable_addr(), destination);
}

void ImageBufferSkiaUnacceleratedBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    SkPixmap pixmap;
    if (m_surface->peekPixels(&pixmap))
        ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, pixmap.writable_addr());
}

} // namespace WebCore

#endif // USE(SKIA)
