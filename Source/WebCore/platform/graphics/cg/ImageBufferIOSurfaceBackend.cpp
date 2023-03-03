/*
 * Copyright (C) 2020-2023 Apple Inc.  All rights reserved.
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
#include "ImageBufferIOSurfaceBackend.h"

#if HAVE(IOSURFACE)

#include "GraphicsClient.h"
#include "GraphicsContextCG.h"
#include "IOSurface.h"
#include "IOSurfacePool.h"
#include "IntRect.h"
#include "PixelBuffer.h"
#include <CoreGraphics/CoreGraphics.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferIOSurfaceBackend);

IntSize ImageBufferIOSurfaceBackend::calculateSafeBackendSize(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    if (backendSize.isEmpty())
        return { };

    IntSize maxSize = IOSurface::maximumSize();
    if (backendSize.width() > maxSize.width() || backendSize.height() > maxSize.height())
        return { };

    return backendSize;
}

unsigned ImageBufferIOSurfaceBackend::calculateBytesPerRow(const IntSize& backendSize)
{
    unsigned bytesPerRow = ImageBufferCGBackend::calculateBytesPerRow(backendSize);
    size_t alignmentMask = IOSurface::bytesPerRowAlignment() - 1;
    return (bytesPerRow + alignmentMask) & ~alignmentMask;
}

size_t ImageBufferIOSurfaceBackend::calculateMemoryCost(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    return ImageBufferBackend::calculateMemoryCost(backendSize, calculateBytesPerRow(backendSize));
}

size_t ImageBufferIOSurfaceBackend::calculateExternalMemoryCost(const Parameters& parameters)
{
    return calculateMemoryCost(parameters);
}

std::unique_ptr<ImageBufferIOSurfaceBackend> ImageBufferIOSurfaceBackend::create(const Parameters& parameters, const ImageBufferCreationContext& creationContext)
{
    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    auto surface = IOSurface::create(creationContext.surfacePool, backendSize, parameters.colorSpace, IOSurface::formatForPixelFormat(parameters.pixelFormat));
    if (!surface)
        return nullptr;

    auto displayID = creationContext.graphicsClient ? creationContext.graphicsClient->displayID() : 0;
    RetainPtr<CGContextRef> cgContext = surface->ensurePlatformContext(displayID);
    if (!cgContext)
        return nullptr;

    CGContextClearRect(cgContext.get(), FloatRect(FloatPoint::zero(), backendSize));

    return makeUnique<ImageBufferIOSurfaceBackend>(parameters, WTFMove(surface), displayID, creationContext.surfacePool);
}

ImageBufferIOSurfaceBackend::ImageBufferIOSurfaceBackend(const Parameters& parameters, std::unique_ptr<IOSurface>&& surface, PlatformDisplayID displayID, IOSurfacePool* ioSurfacePool)
    : ImageBufferCGBackend(parameters)
    , m_surface(WTFMove(surface))
    , m_displayID(displayID)
    , m_ioSurfacePool(ioSurfacePool)
{
    ASSERT(m_surface);
}

ImageBufferIOSurfaceBackend::~ImageBufferIOSurfaceBackend()
{
    ensureNativeImagesHaveCopiedBackingStore();
    IOSurface::moveToPool(WTFMove(m_surface), m_ioSurfacePool.get());
}

GraphicsContext& ImageBufferIOSurfaceBackend::context()
{
    if (!m_context) {
        m_context = makeUnique<GraphicsContextCG>(m_surface->ensurePlatformContext(m_displayID));
        applyBaseTransform(*m_context);
    }
    return *m_context;
}

void ImageBufferIOSurfaceBackend::flushContext()
{
    CGContextFlush(context().platformContext());
}

IntSize ImageBufferIOSurfaceBackend::backendSize() const
{
    return m_surface->size();
}

unsigned ImageBufferIOSurfaceBackend::bytesPerRow() const
{
    return m_surface->bytesPerRow();
}

void ImageBufferIOSurfaceBackend::transferToNewContext(const ImageBufferCreationContext& creationContext)
{
    m_ioSurfacePool = creationContext.surfacePool;
}

void ImageBufferIOSurfaceBackend::invalidateCachedNativeImage()
{
    // Force QuartzCore to invalidate its cached CGImageRef for this IOSurface.
    // This is necessary in cases where we know (a priori) that the IOSurface has been
    // modified, but QuartzCore may have a cached CGImageRef that does not reflect the
    // current state of the IOSurface.
    // See https://webkit.org/b/157966 and https://webkit.org/b/228682 for more context.
    context().fillRect({ });
}


RefPtr<NativeImage> ImageBufferIOSurfaceBackend::copyNativeImage(BackingStoreCopy)
{
    m_mayHaveOutstandingBackingStoreReferences = true;
    return NativeImage::create(m_surface->createImage());
}

RefPtr<NativeImage> ImageBufferIOSurfaceBackend::copyNativeImageForDrawing(GraphicsContext& destination)
{
    if (destination.hasPlatformContext() && CGContextGetType(destination.platformContext()) == kCGContextTypeBitmap) {
        // The destination backend is not deferred, so we can return a reference.
        // The destination backend needs to read the actual pixels. Returning non-refence will
        // copy the pixels and but still cache the image to the context. This means we must
        // return the reference or cleanup later if we return the non-reference.
        return NativeImage::create(adoptCF(CGIOSurfaceContextCreateImageReference(m_surface->ensurePlatformContext())));
    }
    // Other backends are deferred (iosurface, display list) or potentially deferred. Must copy for drawing.
    return ImageBufferIOSurfaceBackend::copyNativeImage(CopyBackingStore);

}

RefPtr<NativeImage> ImageBufferIOSurfaceBackend::sinkIntoNativeImage()
{
    return NativeImage::create(IOSurface::sinkIntoImage(WTFMove(m_surface)));
}

RefPtr<PixelBuffer> ImageBufferIOSurfaceBackend::getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect, const ImageBufferAllocator& allocator)
{
    const_cast<ImageBufferIOSurfaceBackend*>(this)->prepareForExternalRead();
    IOSurface::Locker lock(*m_surface);
    return ImageBufferBackend::getPixelBuffer(outputFormat, srcRect, lock.surfaceBaseAddress(), allocator);
}

void ImageBufferIOSurfaceBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    prepareForExternalWrite();
    IOSurface::Locker lock(*m_surface, IOSurface::Locker::AccessMode::ReadWrite);
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, lock.surfaceBaseAddress());
}

IOSurface* ImageBufferIOSurfaceBackend::surface()
{
    prepareForExternalWrite(); // This is conservative. At the time of writing this is not used.
    return m_surface.get();
}

bool ImageBufferIOSurfaceBackend::isInUse() const
{
    return m_surface->isInUse();
}

void ImageBufferIOSurfaceBackend::releaseGraphicsContext()
{
    m_context = nullptr;
    m_surface->releasePlatformContext();
}

bool ImageBufferIOSurfaceBackend::setVolatile()
{
    if (m_surface->isInUse())
        return false;

    setVolatilityState(VolatilityState::Volatile);
    m_surface->setVolatile(true);
    return true;
}

SetNonVolatileResult ImageBufferIOSurfaceBackend::setNonVolatile()
{
    setVolatilityState(VolatilityState::NonVolatile);
    return m_surface->setVolatile(false);
}

VolatilityState ImageBufferIOSurfaceBackend::volatilityState() const
{
    return m_volatilityState;
}

void ImageBufferIOSurfaceBackend::setVolatilityState(VolatilityState volatilityState)
{
    m_volatilityState = volatilityState;
}

void ImageBufferIOSurfaceBackend::ensureNativeImagesHaveCopiedBackingStore()
{
    // FIXME: This will be removed. This was needed when putImageData was not properly accounting
    // for outstanding reads.
    if (!m_mayHaveOutstandingBackingStoreReferences)
        return;
    prepareForExternalWrite();
}

void ImageBufferIOSurfaceBackend::prepareForExternalRead()
{
    // Ensure that there are no pending draws to this surface. This is ensured by flushing the context
    // through which the draws may have come.
    flushContext();
}

void ImageBufferIOSurfaceBackend::prepareForExternalWrite()
{
    // Ensure that there are no future draws from the surface that would use the surface context image cache.
    if (m_mayHaveOutstandingBackingStoreReferences) {
        invalidateCachedNativeImage();
        m_mayHaveOutstandingBackingStoreReferences = false;
    }

    // Ensure that there are no pending draws to this surface. This is ensured by flushing the context
    // through which the draws may have come.
    // Ensure that there are no pending draws from this surface. This is ensured by drawing the invalidation marker before
    // flushing the the context. The invalidation marker forces the draws from this surface to complete before
    // the invalidation marker completes.
    flushContext();
}

} // namespace WebCore

#endif // HAVE(IOSURFACE)
