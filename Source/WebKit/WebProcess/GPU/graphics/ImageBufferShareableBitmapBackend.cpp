/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include <WebCore/ImageData.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(COCOA)
#include <WebCore/GraphicsContextCG.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferShareableBitmapBackend);

std::unique_ptr<ImageBufferShareableBitmapBackend> ImageBufferShareableBitmapBackend::create(const FloatSize& size, float resolutionScale, ColorSpace colorSpace, const HostWindow*)
{
    IntSize backendSize = calculateBackendSize(size, resolutionScale);
    if (backendSize.isEmpty())
        return nullptr;

    ShareableBitmap::Configuration configuration;
#if PLATFORM(COCOA)
    configuration.colorSpace = { cachedCGColorSpace(colorSpace) };
#endif
    auto bitmap = ShareableBitmap::createShareable(backendSize, configuration);
    if (!bitmap)
        return nullptr;

    auto context = bitmap->createGraphicsContext();
    if (!context)
        return nullptr;

    return makeUnique<ImageBufferShareableBitmapBackend>(size, backendSize, resolutionScale, colorSpace, WTFMove(bitmap), WTFMove(context));
}

std::unique_ptr<ImageBufferShareableBitmapBackend> ImageBufferShareableBitmapBackend::create(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace colorSpace, ImageBufferBackendHandle handle)
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

    return makeUnique<ImageBufferShareableBitmapBackend>(logicalSize, backendSize, resolutionScale, colorSpace, WTFMove(bitmap), WTFMove(context));
}

ImageBufferShareableBitmapBackend::ImageBufferShareableBitmapBackend(const FloatSize& logicalSize, const IntSize& physicalSize, float resolutionScale, ColorSpace colorSpace, RefPtr<ShareableBitmap>&& bitmap, std::unique_ptr<GraphicsContext>&& context)
    : PlatformImageBufferBackend(logicalSize, physicalSize, resolutionScale, colorSpace)
    , m_bitmap(WTFMove(bitmap))
    , m_context(WTFMove(context))
{
}

ImageBufferBackendHandle ImageBufferShareableBitmapBackend::createImageBufferBackendHandle() const
{
    ShareableBitmap::Handle handle;
    m_bitmap->createHandle(handle);
    return ImageBufferBackendHandle(WTFMove(handle));
}

NativeImagePtr ImageBufferShareableBitmapBackend::copyNativeImage(BackingStoreCopy) const
{
#if USE(CG)
    return m_bitmap->makeCGImageCopy();
#elif USE(DIRECT2D)
    return nullptr;
#elif USE(CAIRO)
    return m_bitmap->createCairoSurface();
#endif
}

RefPtr<Image> ImageBufferShareableBitmapBackend::copyImage(BackingStoreCopy, PreserveResolution) const
{
    return m_bitmap->createImage();
}

Vector<uint8_t> ImageBufferShareableBitmapBackend::toBGRAData() const
{
    return ImageBufferBackend::toBGRAData(m_bitmap->data());
}

RefPtr<ImageData> ImageBufferShareableBitmapBackend::getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect) const
{
    return ImageBufferBackend::getImageData(outputFormat, srcRect, m_bitmap->data());
}

void ImageBufferShareableBitmapBackend::putImageData(AlphaPremultiplication inputFormat, const ImageData& imageData, const IntRect& srcRect, const IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat)
{
    ImageBufferBackend::putImageData(inputFormat, imageData, srcRect, destPoint, destFormat, m_bitmap->data());
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
