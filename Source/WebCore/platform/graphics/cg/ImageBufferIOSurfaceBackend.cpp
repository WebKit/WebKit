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
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "IOSurface.h"
#include "IOSurfacePool.h"
#include "IntRect.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include "PlatformCALayer.h"
#include "PlatformCALayerDelegatedContents.h"
#include <CoreGraphics/CoreGraphics.h>
#include <pal/cg/CoreGraphicsSoftLink.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

#if USE(CA)
// Completes the copy to a display surface when the display surface is about to be shown.
class ImageBufferIOSurfaceDisplayFence final : public PlatformCALayerDelegatedContentsFence {
public:
    static Ref<ImageBufferIOSurfaceDisplayFence> create(RetainPtr<CGContextRef>&& context)
    {
        return adoptRef(*new ImageBufferIOSurfaceDisplayFence(WTF::move(context)));
    }

    bool waitFor(Seconds) final
    {
        Locker locker { m_lock };
        if (RetainPtr context = std::exchange(m_context, nullptr))
            CGContextFlush(context.get());
        return true;
    }

private:
    explicit ImageBufferIOSurfaceDisplayFence(RetainPtr<CGContextRef>&& context)
        : m_context(WTF::move(context))
    {
    }

    Lock m_lock;
    RetainPtr<CGContextRef> m_context WTF_GUARDED_BY_LOCK(m_lock);
};

// Shows the display surface that the backend copies to each time it is prepared for display.
class ImageBufferIOSurfaceDisplayDelegate final : public GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<ImageBufferIOSurfaceDisplayDelegate> create(ContentsFormat contentsFormat)
    {
        return adoptRef(*new ImageBufferIOSurfaceDisplayDelegate(contentsFormat));
    }

    // The send right keeps the display surface in use, so it is not reused while shown.
    void setContents(std::optional<PlatformCALayerDelegatedContents>&& contents)
    {
        m_contents = WTF::move(contents);
    }

    void display(PlatformCALayer& layer) final
    {
        if (!m_contents) {
            layer.clearContents();
            return;
        }
        layer.setContentsFormat(m_contentsFormat);
        layer.setDelegatedContents({ MachSendRight { m_contents->surface }, m_contents->finishedFence });
    }

    std::optional<PlatformCALayerDelegatedContents> delegatedContents() const final
    {
        return m_contents;
    }

private:
    explicit ImageBufferIOSurfaceDisplayDelegate(ContentsFormat contentsFormat)
        : m_contentsFormat(contentsFormat)
    {
    }

    std::optional<PlatformCALayerDelegatedContents> m_contents;
    const ContentsFormat m_contentsFormat;
};
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageBufferIOSurfaceBackend);

IntSize ImageBufferIOSurfaceBackend::calculateSafeBackendSize(const ImageBufferParameters& parameters)
{
    IntSize backendSize = parameters.backendSize();
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

std::unique_ptr<ImageBufferIOSurfaceBackend> ImageBufferIOSurfaceBackend::create(const ImageBufferParameters& parameters, const ImageBufferCreationContext& creationContext)
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

    return std::unique_ptr<ImageBufferIOSurfaceBackend> { new ImageBufferIOSurfaceBackend { parameters, WTF::move(surface), WTF::move(cgContext), creationContext.displayID, creationContext.surfacePool.get() } };
}

ImageBufferIOSurfaceBackend::ImageBufferIOSurfaceBackend(const ImageBufferParameters& parameters, std::unique_ptr<IOSurface> surface, RetainPtr<CGContextRef> platformContext, PlatformDisplayID displayID, IOSurfacePool* ioSurfacePool)
    : ImageBufferCGBackend(parameters)
    , m_surface(WTF::move(surface))
    , m_platformContext(WTF::move(platformContext))
    , m_displayID(displayID)
    , m_ioSurfacePool(ioSurfacePool)
{
    ASSERT(m_surface);
    ASSERT(!m_surface->isVolatile());
}

#if USE(CA)
RefPtr<GraphicsLayerContentsDisplayDelegate> ImageBufferIOSurfaceBackend::layerContentsDisplayDelegate()
{
    if (!m_layerContentsDisplayDelegate) {
        m_layerContentsDisplayDelegate = ImageBufferIOSurfaceDisplayDelegate::create(convertToContentsFormat(pixelFormat()));
        // The buffer may already have been prepared for display this frame.
        prepareForDisplay();
    }
    return m_layerContentsDisplayDelegate;
}

void ImageBufferIOSurfaceBackend::releaseLayerContentsDisplayDelegate()
{
    m_layerContentsDisplayDelegate = nullptr;
}

void ImageBufferIOSurfaceBackend::prepareForDisplay()
{
    RefPtr displayDelegate = m_layerContentsDisplayDelegate;
    if (!displayDelegate)
        return;

    // Copy to a display surface, so that drawing can continue while it is shown. The surface goes back to the
    // pool right away: the pool does not hand it out while the send right keeps it in use.
    std::optional<PlatformCALayerDelegatedContents> contents;
    if (RefPtr image = copyNativeImage()) {
        if (auto surface = IOSurface::create(m_ioSurfacePool.get(), size(), colorSpace(), IOSurface::Name::ImageBuffer, convertToIOSurfaceFormat(pixelFormat()), bufferFormat().useLosslessCompression)) {
            if (RetainPtr context = surface->createPlatformContext(m_displayID)) {
                CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
                CGContextDrawImage(context.get(), CGRectMake(0, 0, size().width(), size().height()), image->platformImage().get());
                contents = PlatformCALayerDelegatedContents { surface->createSendRight(), ImageBufferIOSurfaceDisplayFence::create(WTF::move(context)) };
            }
            IOSurface::moveToPool(WTF::move(surface), m_ioSurfacePool.get());
        }
    }
    displayDelegate->setContents(WTF::move(contents));
}
#endif

ImageBufferIOSurfaceBackend::~ImageBufferIOSurfaceBackend()
{
    ensureNativeImagesHaveCopiedBackingStore();
    releaseGraphicsContext();
    IOSurface::moveToPool(WTF::move(m_surface), m_ioSurfacePool.get());
}

RenderingMode ImageBufferIOSurfaceBackend::renderingMode() const
{
    return RenderingMode::Accelerated;
}

size_t ImageBufferIOSurfaceBackend::memoryCost() const
{
    return ImageBufferBackend::calculateMemoryCost(size(), calculateBytesPerRow(size(), pixelFormat()));
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

void ImageBufferIOSurfaceBackend::submitDrawingCommands()
{
    if (PAL::canLoad_CoreGraphics_CGIOSurfaceContextFlushQueue())
        PAL::softLink_CoreGraphics_CGIOSurfaceContextFlushQueue(protect(ensurePlatformContext()));
    else {
        // Creating a snapshot image forces the rendering to commence
        createImage();
    }
}

bool ImageBufferIOSurfaceBackend::flushContextDraws()
{
    bool contextNeedsFlush = m_context && m_context->consumeHasDrawn();
    if (!contextNeedsFlush && !m_needsFirstFlush)
        return false;
    m_needsFirstFlush = false;
    CGContextFlush(protect(ensurePlatformContext()));
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
        PAL::softLink_CoreGraphics_CGIOSurfaceContextInvalidateSurface(protect(ensurePlatformContext()));
        return false;
    }

    CGContextFillRect(protect(ensurePlatformContext()), CGRect { });
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
    return NativeImage::create(IOSurface::sinkIntoImage(WTF::move(m_surface), WTF::move(m_platformContext)));
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
        CGContextFlush(protect(ensurePlatformContext()));
}

RetainPtr<CGImageRef> ImageBufferIOSurfaceBackend::createImage()
{
    // Consumers may hold on to the image, so mark external writes needing the invalidation marker.
    m_mayHaveOutstandingBackingStoreReferences = true;
    return m_surface->createImage(protect(ensurePlatformContext()));
}

RetainPtr<CGImageRef> ImageBufferIOSurfaceBackend::createImageReference()
{
    // The reference is used only in synchronized manner, so after the use ends, we can update
    // externally without invalidation marker. Thus we do not set m_mayHaveOutstandingBackingStoreReferences.
    RetainPtr image = adoptCF(CGIOSurfaceContextCreateImageReference(protect(ensurePlatformContext())));
    // CG has internal caches for some operations related to software bitmap draw.
    // One of these caches are per-image color matching cache. Since these will not get any hits
    // from an image that is recreated every time, mark the image transient to skip these caches.
    // This also skips WebKit GraphicsContext subimage cache.
    CGImageSetCachingFlags(image.get(), kCGImageCachingTransient);
    return image;
}

} // namespace WebCore

#endif // HAVE(IOSURFACE)
