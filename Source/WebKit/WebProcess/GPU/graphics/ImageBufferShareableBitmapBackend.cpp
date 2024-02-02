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

#include <WebCore/GraphicsContext.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(COCOA)
#include <WebCore/GraphicsContextCG.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferShareableBitmapBackend);

IntSize ImageBufferShareableBitmapBackend::calculateSafeBackendSize(const Parameters& parameters)
{
    IntSize backendSize = parameters.backendSize;
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
    return ImageBufferBackend::calculateMemoryCost(parameters.backendSize, calculateBytesPerRow(parameters, parameters.backendSize));
}

std::unique_ptr<ImageBufferShareableBitmapBackend> ImageBufferShareableBitmapBackend::create(const Parameters& parameters, const ImageBufferCreationContext& creationContext)
{
    ASSERT(parameters.pixelFormat == PixelFormat::BGRA8 || parameters.pixelFormat == PixelFormat::BGRX8);

    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    auto bitmap = ShareableBitmap::create({ backendSize, parameters.colorSpace });
    if (!bitmap)
        return nullptr;
    if (creationContext.resourceOwner)
        bitmap->setOwnershipOfMemory(creationContext.resourceOwner);
    auto context = bitmap->createGraphicsContext();
    if (!context)
        return nullptr;

    return makeUnique<ImageBufferShareableBitmapBackend>(parameters, bitmap.releaseNonNull(), WTFMove(context));
}

std::unique_ptr<ImageBufferShareableBitmapBackend> ImageBufferShareableBitmapBackend::create(const Parameters& parameters, ShareableBitmap::Handle handle)
{
    auto bitmap = ShareableBitmap::create(WTFMove(handle));
    if (!bitmap)
        return nullptr;

    auto context = bitmap->createGraphicsContext();
    if (!context)
        return nullptr;

    return makeUnique<ImageBufferShareableBitmapBackend>(parameters, bitmap.releaseNonNull(), WTFMove(context));
}

ImageBufferShareableBitmapBackend::ImageBufferShareableBitmapBackend(const Parameters& parameters, Ref<ShareableBitmap>&& bitmap, std::unique_ptr<GraphicsContext>&& context)
    : ImageBufferShareableBitmapBackendBase(parameters)
    , m_bitmap(WTFMove(bitmap))
    , m_context(WTFMove(context))
{
    // ShareableBitmap ensures that the coordinate space in the context that we're adopting
    // has a top-left origin, so we don't ever need to flip here, so we don't call setupContext().
    // However, ShareableBitmap does not have a notion of scale, so we must apply the device
    // scale factor to the context ourselves.
    m_context->applyDeviceScaleFactor(resolutionScale());
}

std::optional<ImageBufferBackendHandle> ImageBufferShareableBitmapBackend::createBackendHandle(SharedMemory::Protection protection) const
{
    if (auto handle = m_bitmap->createHandle(protection))
        return ImageBufferBackendHandle(WTFMove(*handle));
    return { };
}

void ImageBufferShareableBitmapBackend::transferToNewContext(const ImageBufferCreationContext& creationContext)
{
    if (creationContext.resourceOwner)
        m_bitmap->setOwnershipOfMemory(creationContext.resourceOwner);
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

RefPtr<NativeImage> ImageBufferShareableBitmapBackend::copyNativeImage()
{
    return NativeImage::create(m_bitmap->createPlatformImage(CopyBackingStore));
}

RefPtr<NativeImage> ImageBufferShareableBitmapBackend::createNativeImageReference()
{
    return NativeImage::create(m_bitmap->createPlatformImage(DontCopyBackingStore));
}

void ImageBufferShareableBitmapBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    ImageBufferBackend::getPixelBuffer(srcRect, m_bitmap->data(), destination);
}

void ImageBufferShareableBitmapBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, m_bitmap->data());
}

String ImageBufferShareableBitmapBackend::debugDescription() const
{
    TextStream stream;
    stream << "ImageBufferShareableBitmapBackend " << this;
    return stream.release();
}

} // namespace WebKit
