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

RetainPtr<CGColorSpaceRef> ImageBufferIOSurfaceBackend::contextColorSpace(const GraphicsContext& context)
{
    CGContextRef cgContext = context.platformContext();

    if (CGContextGetType(cgContext) == kCGContextTypeIOSurface)
        return CGIOSurfaceContextGetColorSpace(cgContext);
    
    return ImageBufferCGBackend::contextColorSpace(context);
}

std::unique_ptr<ImageBufferIOSurfaceBackend> ImageBufferIOSurfaceBackend::create(const Parameters& parameters, const ImageBufferCreationContext& creationContext)
{
    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    auto surface = IOSurface::create(creationContext.surfacePool, backendSize, parameters.colorSpace, IOSurface::formatForPixelFormat(parameters.pixelFormat));
    if (!surface)
        return nullptr;

    RetainPtr<CGContextRef> cgContext = surface->ensurePlatformContext(creationContext.graphicsClient ? creationContext.graphicsClient->displayID() : 0);
    if (!cgContext)
        return nullptr;

    CGContextClearRect(cgContext.get(), FloatRect(FloatPoint::zero(), backendSize));

    return makeUnique<ImageBufferIOSurfaceBackend>(parameters, WTFMove(surface), creationContext.surfacePool);
}

std::unique_ptr<ImageBufferIOSurfaceBackend> ImageBufferIOSurfaceBackend::create(const Parameters& parameters, const GraphicsContext& context)
{
    if (auto cgColorSpace = context.hasPlatformContext() ? contextColorSpace(context) : nullptr) {
        auto overrideParameters = parameters;
        overrideParameters.colorSpace = DestinationColorSpace { cgColorSpace };

        return ImageBufferIOSurfaceBackend::create(overrideParameters, nullptr);
    }

    return ImageBufferIOSurfaceBackend::create(parameters, nullptr);
}

ImageBufferIOSurfaceBackend::ImageBufferIOSurfaceBackend(const Parameters& parameters, std::unique_ptr<IOSurface>&& surface, IOSurfacePool* ioSurfacePool)
    : ImageBufferCGBackend(parameters)
    , m_surface(WTFMove(surface))
    , m_ioSurfacePool(ioSurfacePool)
{
    ASSERT(m_surface);
    applyBaseTransformToContext();
}

ImageBufferIOSurfaceBackend::~ImageBufferIOSurfaceBackend()
{
    ensureNativeImagesHaveCopiedBackingStore();
    IOSurface::moveToPool(WTFMove(m_surface), m_ioSurfacePool.get());
}

GraphicsContext& ImageBufferIOSurfaceBackend::context() const
{
    GraphicsContext& context = m_surface->ensureGraphicsContext();
    if (m_needsSetupContext) {
        m_needsSetupContext = false;
        applyBaseTransformToContext();
    }
    return context;
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

void ImageBufferIOSurfaceBackend::invalidateCachedNativeImage() const
{
    // Force QuartzCore to invalidate its cached CGImageRef for this IOSurface.
    // This is necessary in cases where we know (a priori) that the IOSurface has been
    // modified, but QuartzCore may have a cached CGImageRef that does not reflect the
    // current state of the IOSurface.
    // See https://webkit.org/b/157966 and https://webkit.org/b/228682 for more context.
    context().fillRect({ });
    m_mayHaveOutstandingBackingStoreReferences = false;
}

void ImageBufferIOSurfaceBackend::invalidateCachedNativeImageIfNeeded() const
{
    if (m_mayHaveOutstandingBackingStoreReferences)
        invalidateCachedNativeImage();
}

RefPtr<NativeImage> ImageBufferIOSurfaceBackend::copyNativeImage(BackingStoreCopy) const
{
    m_mayHaveOutstandingBackingStoreReferences = true;
    return NativeImage::create(m_surface->createImage());
}

RefPtr<NativeImage> ImageBufferIOSurfaceBackend::copyNativeImageForDrawing(BackingStoreCopy) const
{
    return NativeImage::create(m_surface->createImage());
}

RefPtr<NativeImage> ImageBufferIOSurfaceBackend::sinkIntoNativeImage()
{
    return NativeImage::create(IOSurface::sinkIntoImage(WTFMove(m_surface)));
}

void ImageBufferIOSurfaceBackend::finalizeDrawIntoContext(GraphicsContext& destinationContext)
{
    // Accelerated to unaccelerated image buffers need complex caching. We trust that
    // this is a one-off draw, and as such we clear the caches of the source image after each draw.
    if (destinationContext.renderingMode() == RenderingMode::Unaccelerated)
        invalidateCachedNativeImage();
}

RefPtr<PixelBuffer> ImageBufferIOSurfaceBackend::getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect, const ImageBufferAllocator& allocator) const
{
    IOSurface::Locker lock(*m_surface);
    return ImageBufferBackend::getPixelBuffer(outputFormat, srcRect, lock.surfaceBaseAddress(), allocator);
}

void ImageBufferIOSurfaceBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    invalidateCachedNativeImageIfNeeded();
    IOSurface::Locker lock(*m_surface, IOSurface::Locker::AccessMode::ReadWrite);
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, lock.surfaceBaseAddress());
}

IOSurface* ImageBufferIOSurfaceBackend::surface()
{
    flushContext();
    return m_surface.get();
}

bool ImageBufferIOSurfaceBackend::isInUse() const
{
    return m_surface->isInUse();
}

void ImageBufferIOSurfaceBackend::releaseGraphicsContext()
{
    m_needsSetupContext = true;
    return m_surface->releaseGraphicsContext();
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
    if (!m_mayHaveOutstandingBackingStoreReferences)
        return;
    invalidateCachedNativeImage();
    flushContext();
}

} // namespace WebCore

#endif // HAVE(IOSURFACE)
