/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "NativeImage.h"
#include "PixelBuffer.h"
#include <CoreGraphics/CoreGraphics.h>
#include <pal/cg/CoreGraphicsSoftLink.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ImageBufferIOSurfaceBackend);

IntSize ImageBufferIOSurfaceBackend::calculateSafeBackendSize(const Parameters& parameters)
{
    IntSize backendSize = parameters.backendSize;
    if (backendSize.isEmpty())
        return { };

    IntSize maxSize = IOSurface::maximumSize();
    if (backendSize.width() > maxSize.width() || backendSize.height() > maxSize.height())
        return { };

    return backendSize;
}

unsigned ImageBufferIOSurfaceBackend::calculateBytesPerRow(const IntSize& backendSize, PixelFormat imageBufferPixelFormat)
{
    unsigned bytesPerRow = ImageBufferCGBackend::calculateBytesPerRow(backendSize, imageBufferPixelFormat);
    size_t alignmentMask = IOSurface::bytesPerRowAlignment() - 1;
    return (bytesPerRow + alignmentMask) & ~alignmentMask;
}

size_t ImageBufferIOSurfaceBackend::calculateMemoryCost(const Parameters& parameters)
{
    return ImageBufferBackend::calculateMemoryCost(parameters.backendSize, calculateBytesPerRow(parameters.backendSize, parameters.bufferFormat.pixelFormat));
}

std::unique_ptr<ImageBufferIOSurfaceBackend> ImageBufferIOSurfaceBackend::create(const Parameters& parameters, const ImageBufferCreationContext& creationContext)
{
    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    auto surface = IOSurface::create(RefPtr { creationContext.surfacePool }.get(), backendSize, parameters.colorSpace, IOSurface::Name::ImageBuffer, convertToIOSurfaceFormat(parameters.bufferFormat.pixelFormat), parameters.bufferFormat.useLosslessCompression);
    if (!surface)
        return nullptr;

    RetainPtr<CGContextRef> cgContext = surface->createPlatformContext(creationContext.displayID);
    if (!cgContext)
        return nullptr;

    CGContextClearRect(cgContext.get(), FloatRect(FloatPoint::zero(), backendSize));

    return std::unique_ptr<ImageBufferIOSurfaceBackend> { new ImageBufferIOSurfaceBackend { parameters, WTFMove(surface), WTFMove(cgContext), creationContext.displayID, creationContext.surfacePool.get() } };
}

ImageBufferIOSurfaceBackend::ImageBufferIOSurfaceBackend(const Parameters& parameters, std::unique_ptr<IOSurface> surface, RetainPtr<CGContextRef> platformContext, PlatformDisplayID displayID, IOSurfacePool* ioSurfacePool)
    : ImageBufferCGBackend(parameters)
    , m_surface(WTFMove(surface))
    , m_platformContext(WTFMove(platformContext))
    , m_displayID(displayID)
    , m_ioSurfacePool(ioSurfacePool)
{
    ASSERT(m_surface);
    ASSERT(!m_surface->isVolatile());
}

ImageBufferIOSurfaceBackend::~ImageBufferIOSurfaceBackend()
{
    ensureNativeImagesHaveCopiedBackingStore();
    releaseGraphicsContext();
    IOSurface::moveToPool(WTFMove(m_surface), m_ioSurfacePool.get());
}


GraphicsContext& ImageBufferIOSurfaceBackend::context()
{
    if (!m_context) {
        m_context = makeUnique<GraphicsContextCG>(ensurePlatformContext());
        applyBaseTransform(*m_context);
    }
    return *m_context;
}

void ImageBufferIOSurfaceBackend::flushContext()
{
    flushContextDraws();
}

bool ImageBufferIOSurfaceBackend::flushContextDraws()
{
    bool contextNeedsFlush = m_context && m_context->consumeHasDrawn();
    if (!contextNeedsFlush && !m_needsFirstFlush)
        return false;
    m_needsFirstFlush = false;
    CGContextFlush(ensurePlatformContext());
    return true;
}

CGContextRef ImageBufferIOSurfaceBackend::ensurePlatformContext()
{
    if (!m_platformContext) {
        m_platformContext = m_surface->createPlatformContext(m_displayID);
        RELEASE_ASSERT(m_platformContext);
    }
    return m_platformContext.get();
}

unsigned ImageBufferIOSurfaceBackend::bytesPerRow() const
{
    return m_surface->bytesPerRow();
}

void ImageBufferIOSurfaceBackend::transferToNewContext(const ImageBufferCreationContext& creationContext)
{
    m_ioSurfacePool = creationContext.surfacePool;
    if (creationContext.resourceOwner)
        m_surface->setOwnershipIdentity(creationContext.resourceOwner);
}

bool ImageBufferIOSurfaceBackend::invalidateCachedNativeImage()
{
    // Force QuartzCore to invalidate its cached CGImageRef for this IOSurface.
    // This is necessary in cases where we know (a priori) that the IOSurface has been
    // modified, but QuartzCore may have a cached CGImageRef that does not reflect the
    // current state of the IOSurface.
    // See https://webkit.org/b/157966 and https://webkit.org/b/228682 for more context.
    if (PAL::canLoad_CoreGraphics_CGIOSurfaceContextInvalidateSurface()) {
        PAL::softLink_CoreGraphics_CGIOSurfaceContextInvalidateSurface(ensurePlatformContext());
        return false;
    }

    CGContextFillRect(ensurePlatformContext(), CGRect { });
    return true;
}

RefPtr<NativeImage> ImageBufferIOSurfaceBackend::copyNativeImage()
{
    return NativeImage::create(createImage());
}

RefPtr<NativeImage> ImageBufferIOSurfaceBackend::createNativeImageReference()
{
    // The destination backend needs to read the actual pixels. Returning non-refence will
    // copy the pixels and but still cache the image to the context. This means we must
    // return the reference or cleanup later if we return the non-reference.
    return NativeImage::create(createImageReference());
}

RefPtr<NativeImage> ImageBufferIOSurfaceBackend::sinkIntoNativeImage()
{
    ensurePlatformContext();
    return NativeImage::create(IOSurface::sinkIntoImage(WTFMove(m_surface), WTFMove(m_platformContext)));
}

void ImageBufferIOSurfaceBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    const_cast<ImageBufferIOSurfaceBackend*>(this)->prepareForExternalRead();
    if (auto lock = m_surface->lock<IOSurface::AccessMode::ReadOnly>())
        ImageBufferBackend::getPixelBuffer(srcRect, lock->surfaceSpan(), destination);
}

void ImageBufferIOSurfaceBackend::putPixelBuffer(const PixelBufferSourceView& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    prepareForExternalWrite();
    if (auto lock = m_surface->lock<IOSurface::AccessMode::ReadWrite>())
        ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, lock->surfaceSpan());
}

bool ImageBufferIOSurfaceBackend::canMapBackingStore() const
{
    return true;
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
    m_platformContext = nullptr;
}

bool ImageBufferIOSurfaceBackend::setVolatile()
{
    if (m_surface->isInUse())
        return false;

    if (m_volatilityState == VolatilityState::Volatile) {
        ASSERT(m_surface->isVolatile());
        return true;
    }

    setVolatilityState(VolatilityState::Volatile);
    m_surface->setVolatile(true);
    return true;
}

SetNonVolatileResult ImageBufferIOSurfaceBackend::setNonVolatile()
{
    if (m_volatilityState == VolatilityState::Volatile) {
        setVolatilityState(VolatilityState::NonVolatile);

        auto previousState = m_surface->setVolatile(false);
        if (previousState == SetNonVolatileResult::Empty) {
            RetainPtr context = ensurePlatformContext();
            ASSERT(CGAffineTransformIsIdentity(CGContextGetCTM(context.get())));
            CGContextClearRect(context.get(), FloatRect({ }, size()));
        }

        return previousState;
    }

    ASSERT(!m_surface->isVolatile());
    return SetNonVolatileResult::Valid;
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
    flushContextDraws();
}

void ImageBufferIOSurfaceBackend::prepareForExternalWrite()
{
    bool needFlush = false;
    // Ensure that there are no future draws from the surface that would use the surface context image cache.
    if (m_mayHaveOutstandingBackingStoreReferences) {
        needFlush = invalidateCachedNativeImage();
        m_mayHaveOutstandingBackingStoreReferences = false;
    }

    // Ensure that there are no pending draws to this surface. This is ensured by flushing the context
    // through which the draws may have come.
    // Ensure that there are no pending draws from this surface. This is ensured by drawing the invalidation marker before
    // flushing the the context. The invalidation marker forces the draws from this surface to complete before
    // the invalidation marker completes.
    if (flushContextDraws())
        needFlush = false;
    if (needFlush)
        CGContextFlush(ensurePlatformContext());
}

RetainPtr<CGImageRef> ImageBufferIOSurfaceBackend::createImage()
{
    // Consumers may hold on to the image, so mark external writes needing the invalidation marker.
    m_mayHaveOutstandingBackingStoreReferences = true;
    return m_surface->createImage(ensurePlatformContext());
}

RetainPtr<CGImageRef> ImageBufferIOSurfaceBackend::createImageReference()
{
    // The reference is used only in synchronized manner, so after the use ends, we can update
    // externally without invalidation marker. Thus we do not set m_mayHaveOutstandingBackingStoreReferences.
    auto image = adoptCF(CGIOSurfaceContextCreateImageReference(ensurePlatformContext()));
    // CG has internal caches for some operations related to software bitmap draw.
    // One of these caches are per-image color matching cache. Since these will not get any hits
    // from an image that is recreated every time, mark the image transient to skip these caches.
    // This also skips WebKit GraphicsContext subimage cache.
    CGImageSetCachingFlags(image.get(), kCGImageCachingTransient);
    return image;
}

} // namespace WebCore

#endif // HAVE(IOSURFACE)
