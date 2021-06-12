/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "ShareableBitmap.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/PixelBuffer.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(COCOA)
#include <WebCore/GraphicsContextCG.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferShareableBitmapBackend);

ShareableBitmap::Configuration ImageBufferShareableBitmapBackend::configuration(const Parameters& parameters)
{
    return { parameters.colorSpace };
}

IntSize ImageBufferShareableBitmapBackend::calculateSafeBackendSize(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    if (backendSize.isEmpty())
        return { };

    CheckedUint32 bytesPerRow = ShareableBitmap::calculateBytesPerRow(backendSize, configuration(parameters));
    if (bytesPerRow.hasOverflowed())
        return { };

    CheckedSize numBytes = CheckedUint32(backendSize.height()) * bytesPerRow;
    if (numBytes.hasOverflowed())
        return { };

    return backendSize;
}

unsigned ImageBufferShareableBitmapBackend::calculateBytesPerRow(const Parameters& parameters, const IntSize& backendSize)
{
    ASSERT(!backendSize.isEmpty());
    return ShareableBitmap::calculateBytesPerRow(backendSize, configuration(parameters));
}

size_t ImageBufferShareableBitmapBackend::calculateMemoryCost(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    return ImageBufferBackend::calculateMemoryCost(backendSize, calculateBytesPerRow(parameters, backendSize));
}

std::unique_ptr<ImageBufferShareableBitmapBackend> ImageBufferShareableBitmapBackend::create(const Parameters& parameters, const HostWindow*)
{
    ASSERT(parameters.pixelFormat == PixelFormat::BGRA8);

    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    auto bitmap = ShareableBitmap::createShareable(backendSize, configuration(parameters));
    if (!bitmap)
        return nullptr;

    auto context = bitmap->createGraphicsContext();
    if (!context)
        return nullptr;

    return makeUnique<ImageBufferShareableBitmapBackend>(parameters, WTFMove(bitmap), WTFMove(context));
}

std::unique_ptr<ImageBufferShareableBitmapBackend> ImageBufferShareableBitmapBackend::create(const Parameters& parameters, ImageBufferBackendHandle handle)
{
    if (!WTF::holds_alternative<ShareableBitmap::Handle>(handle)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto bitmap = ShareableBitmap::create(WTFMove(WTF::get<ShareableBitmap::Handle>(handle)));
    if (!bitmap)
        return nullptr;

    auto context = bitmap->createGraphicsContext();
    if (!context)
        return nullptr;

    return makeUnique<ImageBufferShareableBitmapBackend>(parameters, WTFMove(bitmap), WTFMove(context));
}

ImageBufferShareableBitmapBackend::ImageBufferShareableBitmapBackend(const Parameters& parameters, RefPtr<ShareableBitmap>&& bitmap, std::unique_ptr<GraphicsContext>&& context)
    : PlatformImageBufferBackend(parameters)
    , m_bitmap(WTFMove(bitmap))
    , m_context(WTFMove(context))
{
    // ShareableBitmap ensures that the coordinate space in the context that we're adopting
    // has a top-left origin, so we don't ever need to flip here, so we don't call setupContext().
    // However, ShareableBitmap does not have a notion of scale, so we must apply the device
    // scale factor to the context ourselves.
    m_context->applyDeviceScaleFactor(resolutionScale());
}

ImageBufferBackendHandle ImageBufferShareableBitmapBackend::createImageBufferBackendHandle() const
{
    ShareableBitmap::Handle handle;
    m_bitmap->createHandle(handle);
    return ImageBufferBackendHandle(WTFMove(handle));
}

IntSize ImageBufferShareableBitmapBackend::backendSize() const
{
    return m_bitmap->size();
}

unsigned ImageBufferShareableBitmapBackend::bytesPerRow() const
{
    return m_bitmap->bytesPerRow();
}

RefPtr<NativeImage> ImageBufferShareableBitmapBackend::copyNativeImage(BackingStoreCopy) const
{
    return NativeImage::create(m_bitmap->createPlatformImage());
}

RefPtr<Image> ImageBufferShareableBitmapBackend::copyImage(BackingStoreCopy, PreserveResolution) const
{
    return m_bitmap->createImage();
}

std::optional<PixelBuffer> ImageBufferShareableBitmapBackend::getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect) const
{
    return ImageBufferBackend::getPixelBuffer(outputFormat, srcRect, m_bitmap->data());
}

void ImageBufferShareableBitmapBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat)
{
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, m_bitmap->data());
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
