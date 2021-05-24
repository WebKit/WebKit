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

#include "GraphicsContextCG.h"
#include "IOSurface.h"
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
    return IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, bytesPerRow);
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

std::unique_ptr<ImageBufferIOSurfaceBackend> ImageBufferIOSurfaceBackend::create(const Parameters& parameters, const HostWindow* hostWindow)
{
    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    auto surface = IOSurface::create(backendSize, backendSize, parameters.colorSpace, IOSurface::formatForPixelFormat(parameters.pixelFormat));
    if (!surface)
        return nullptr;

    RetainPtr<CGContextRef> cgContext = surface->ensurePlatformContext(hostWindow);
    if (!cgContext)
        return nullptr;

    CGContextClearRect(cgContext.get(), FloatRect(FloatPoint::zero(), backendSize));

    return makeUnique<ImageBufferIOSurfaceBackend>(parameters, WTFMove(surface));
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

ImageBufferIOSurfaceBackend::ImageBufferIOSurfaceBackend(const Parameters& parameters, std::unique_ptr<IOSurface>&& surface)
    : ImageBufferCGBackend(parameters)
    , m_surface(WTFMove(surface))
{
    ASSERT(m_surface);
    setupContext();
}

GraphicsContext& ImageBufferIOSurfaceBackend::context() const
{
    GraphicsContext& context = m_surface->ensureGraphicsContext();
    if (m_needsSetupContext) {
        m_needsSetupContext = false;
        setupContext();
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

RefPtr<NativeImage> ImageBufferIOSurfaceBackend::copyNativeImage(BackingStoreCopy) const
{
    return NativeImage::create(m_surface->createImage());
}

RefPtr<NativeImage> ImageBufferIOSurfaceBackend::sinkIntoNativeImage()
{
    return NativeImage::create(IOSurface::sinkIntoImage(WTFMove(m_surface)));
}

void ImageBufferIOSurfaceBackend::drawConsuming(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(resolutionScale());

    auto backendSize = this->backendSize();
    if (auto image = sinkIntoNativeImage())
        destContext.drawNativeImage(*image, backendSize, destRect, adjustedSrcRect, options);
}

RetainPtr<CGImageRef> ImageBufferIOSurfaceBackend::copyCGImageForEncoding(CFStringRef destinationUTI, PreserveResolution preserveResolution) const
{
    if (m_requiresDrawAfterPutPixelBuffer) {
        // Force recreating the IOSurface cached image.
        // See https://bugs.webkit.org/show_bug.cgi?id=157966 for explaining why this is necessary.
        context().fillRect(FloatRect(1, 1, 0, 0));
        m_requiresDrawAfterPutPixelBuffer = false;
    }
    return ImageBufferCGBackend::copyCGImageForEncoding(destinationUTI, preserveResolution);
}

Optional<PixelBuffer> ImageBufferIOSurfaceBackend::getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect) const
{
    IOSurface::Locker lock(*m_surface);
    return ImageBufferBackend::getPixelBuffer(outputFormat, srcRect, lock.surfaceBaseAddress());
}

void ImageBufferIOSurfaceBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    IOSurface::Locker lock(*m_surface, IOSurface::Locker::AccessMode::ReadWrite);
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, lock.surfaceBaseAddress());
    m_requiresDrawAfterPutPixelBuffer = true;
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

VolatilityState ImageBufferIOSurfaceBackend::setVolatile(bool isVolatile)
{
    return m_surface->setVolatile(isVolatile);
}

void ImageBufferIOSurfaceBackend::releaseBufferToPool()
{
    IOSurface::moveToPool(WTFMove(m_surface));
}

} // namespace WebCore

#endif // HAVE(IOSURFACE)
