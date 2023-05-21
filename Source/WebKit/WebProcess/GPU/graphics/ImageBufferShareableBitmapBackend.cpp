/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageBufferShareableBitmapBackend.h"

#include "ShareableBitmap.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/NullGraphicsContext.h>
#include <WebCore/PixelBuffer.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferShareableBitmapBackend);

IntSize ImageBufferShareableBitmapBackend::calculateSafeBackendSize(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    if (backendSize.isEmpty())
        return { };

    CheckedUint32 numBytes = ShareableBitmapConfiguration::calculateSizeInBytes(backendSize, parameters.colorSpace);
    if (numBytes.hasOverflowed())
        return { };

    return backendSize;
}

unsigned ImageBufferShareableBitmapBackend::calculateBytesPerRow(const Parameters& parameters, const IntSize& backendSize)
{
    ASSERT(!backendSize.isEmpty());
    return ShareableBitmapConfiguration::calculateBytesPerRow(backendSize, parameters.colorSpace);
}

size_t ImageBufferShareableBitmapBackend::calculateMemoryCost(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    return ImageBufferBackend::calculateMemoryCost(backendSize, calculateBytesPerRow(parameters, backendSize));
}

std::unique_ptr<ImageBufferShareableBitmapBackend> ImageBufferShareableBitmapBackend::create(const Parameters& parameters, const WebCore::ImageBufferCreationContext&)
{
    ASSERT(parameters.pixelFormat == PixelFormat::BGRA8 || parameters.pixelFormat == PixelFormat::BGRX8);

    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    auto bitmap = ShareableBitmap::create({ backendSize, parameters.colorSpace });
    if (!bitmap)
        return nullptr;

    return makeUnique<ImageBufferShareableBitmapBackend>(parameters, bitmap.releaseNonNull());
}

std::unique_ptr<ImageBufferShareableBitmapBackend> ImageBufferShareableBitmapBackend::create(const Parameters& parameters, ImageBufferBackendHandle handle)
{
    if (!std::holds_alternative<ShareableBitmap::Handle>(handle)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto bitmap = ShareableBitmap::create(WTFMove(std::get<ShareableBitmap::Handle>(handle)));
    if (!bitmap)
        return nullptr;


    return makeUnique<ImageBufferShareableBitmapBackend>(parameters, bitmap.releaseNonNull());
}

ImageBufferShareableBitmapBackend::ImageBufferShareableBitmapBackend(const Parameters& parameters, Ref<ShareableBitmap>&& bitmap)
    : ImageBufferBackend(parameters)
    , m_bitmap(WTFMove(bitmap))
{
}

ImageBufferBackendHandle ImageBufferShareableBitmapBackend::createBackendHandle(SharedMemory::Protection protection) const
{
    if (auto handle = m_bitmap->createHandle(protection))
        return ImageBufferBackendHandle(WTFMove(*handle));
    return { };
}

std::unique_ptr<GraphicsContext> ImageBufferShareableBitmapBackend::createContext()
{
    return m_bitmap->createGraphicsContext();
}

IntSize ImageBufferShareableBitmapBackend::backendSize() const
{
    return m_bitmap->size();
}

unsigned ImageBufferShareableBitmapBackend::bytesPerRow() const
{
    return m_bitmap->bytesPerRow();
}

#if USE(CAIRO)
RefPtr<cairo_surface_t> ImageBufferShareableBitmapBackend::createCairoSurface()
{
    return m_bitmap->createPersistentCairoSurface();
}
#endif

RefPtr<NativeImage> ImageBufferShareableBitmapBackend::copyNativeImage(BackingStoreCopy copyBehavior)
{
    return NativeImage::create(m_bitmap->createPlatformImage(copyBehavior));
}

void ImageBufferShareableBitmapBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    ImageBufferBackend::getPixelBuffer(srcRect, m_bitmap->data(), destination);
}

void ImageBufferShareableBitmapBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat)
{
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, m_bitmap->data());
}

} // namespace WebKit
