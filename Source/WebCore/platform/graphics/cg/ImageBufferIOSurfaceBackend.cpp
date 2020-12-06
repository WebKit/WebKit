/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
#include "ImageData.h"
#include "IntRect.h"
#include "IOSurface.h"
#include <CoreGraphics/CoreGraphics.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferIOSurfaceBackend);

IntSize ImageBufferIOSurfaceBackend::calculateBackendSize(const FloatSize& size, float resolutionScale)
{
    IntSize backendSize = ImageBufferCGBackend::calculateBackendSize(size, resolutionScale);
    if (backendSize.isEmpty())
        return { };

    IntSize maxSize = IOSurface::maximumSize();
    if (backendSize.width() > maxSize.width() || backendSize.height() > maxSize.height())
        return { };

    return backendSize;
}

RetainPtr<CGColorSpaceRef> ImageBufferIOSurfaceBackend::contextColorSpace(const GraphicsContext& context)
{
    CGContextRef cgContext = context.platformContext();

    if (CGContextGetType(cgContext) == kCGContextTypeIOSurface)
        return CGIOSurfaceContextGetColorSpace(cgContext);
    
    return ImageBufferCGBackend::contextColorSpace(context);
}

std::unique_ptr<ImageBufferIOSurfaceBackend> ImageBufferIOSurfaceBackend::create(const Parameters& parameters, CGColorSpaceRef cgColorSpace, const HostWindow* hostWindow)
{
    IntSize backendSize = calculateBackendSize(parameters.logicalSize, parameters.resolutionScale);
    if (backendSize.isEmpty())
        return nullptr;

    auto surface = IOSurface::create(backendSize, backendSize, cgColorSpace, IOSurface::formatForPixelFormat(parameters.pixelFormat));
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
    if (auto cgColorSpace = context.hasPlatformContext() ? contextColorSpace(context) : nullptr)
        return ImageBufferIOSurfaceBackend::create(parameters, cgColorSpace.get(), nullptr);

    return ImageBufferIOSurfaceBackend::create(parameters, nullptr);
}

std::unique_ptr<ImageBufferIOSurfaceBackend> ImageBufferIOSurfaceBackend::create(const Parameters& parameters, const HostWindow* hostWindow)
{
    return ImageBufferIOSurfaceBackend::create(parameters, cachedCGColorSpace(parameters.colorSpace), hostWindow);
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

size_t ImageBufferIOSurfaceBackend::memoryCost() const
{
    return m_surface->totalBytes();
}

size_t ImageBufferIOSurfaceBackend::externalMemoryCost() const
{
    return memoryCost();
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

RetainPtr<CFDataRef> ImageBufferIOSurfaceBackend::toCFData(const String& mimeType, Optional<double> quality, PreserveResolution preserveResolution) const
{
    if (m_requiresDrawAfterPutImageData) {
        // Force recreating the IOSurface cached image.
        // See https://bugs.webkit.org/show_bug.cgi?id=157966 for explaining why this is necessary.
        context().fillRect(FloatRect(1, 1, 0, 0));
        m_requiresDrawAfterPutImageData = false;
    }
    return ImageBufferCGBackend::toCFData(mimeType, quality, preserveResolution);
}

Vector<uint8_t> ImageBufferIOSurfaceBackend::toBGRAData() const
{
    IOSurface::Locker lock(*m_surface);
    return ImageBufferBackend::toBGRAData(lock.surfaceBaseAddress());
}

RefPtr<ImageData> ImageBufferIOSurfaceBackend::getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect) const
{
    IOSurface::Locker lock(*m_surface);
    return ImageBufferBackend::getImageData(outputFormat, srcRect, lock.surfaceBaseAddress());
}

void ImageBufferIOSurfaceBackend::putImageData(AlphaPremultiplication inputFormat, const ImageData& imageData, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    IOSurface::Locker lock(*m_surface, IOSurface::Locker::AccessMode::ReadWrite);
    ImageBufferBackend::putImageData(inputFormat, imageData, srcRect, destPoint, destFormat, lock.surfaceBaseAddress());
    m_requiresDrawAfterPutImageData = true;
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
