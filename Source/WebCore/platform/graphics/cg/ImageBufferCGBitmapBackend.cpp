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
#include "ImageBufferCGBitmapBackend.h"

#if USE(CG)

#include "GraphicsContext.h"
#include "GraphicsContextCG.h"
#include "ImageBufferUtilitiesCG.h"
#include "ImageData.h"
#include "IntRect.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

#if PLATFORM(IOS_FAMILY)
constexpr const CGBitmapInfo DefaultBitmapInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
#else
constexpr const CGBitmapInfo DefaultBitmapInfo = kCGImageAlphaPremultipliedLast;
#endif

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferCGBitmapBackend);

std::unique_ptr<ImageBufferCGBitmapBackend> ImageBufferCGBitmapBackend::create(const FloatSize& size, float resolutionScale, ColorSpace colorSpace, CGColorSpaceRef cgColorSpace, const HostWindow*)
{
    IntSize backendSize = calculateBackendSize(size, resolutionScale);
    if (backendSize.isEmpty())
        return nullptr;

    void* data;
    unsigned bytesPerRow = 4 * backendSize.width();

    if (!tryFastCalloc(backendSize.height(), bytesPerRow).getValue(data))
        return nullptr;

    ASSERT(!(reinterpret_cast<intptr_t>(data) & 3));

    size_t numBytes = backendSize.height() * bytesPerRow;
    verifyImageBufferIsBigEnough(data, numBytes);

    auto cgContext = adoptCF(CGBitmapContextCreate(data, backendSize.width(), backendSize.height(), 8, bytesPerRow, cgColorSpace, DefaultBitmapInfo));
    if (!cgContext)
        return nullptr;

    auto context = makeUnique<GraphicsContext>(cgContext.get());

    const auto releaseImageData = [] (void*, const void* data, size_t) {
        fastFree(const_cast<void*>(data));
    };

    auto dataProvider = adoptCF(CGDataProviderCreateWithData(0, data, numBytes, releaseImageData));

    return std::unique_ptr<ImageBufferCGBitmapBackend>(new ImageBufferCGBitmapBackend(size, backendSize, resolutionScale, colorSpace, data, WTFMove(dataProvider), WTFMove(context)));
}

std::unique_ptr<ImageBufferCGBitmapBackend> ImageBufferCGBitmapBackend::create(const FloatSize& size, const GraphicsContext& context)
{
    if (auto cgColorSpace = contextColorSpace(context))
        return ImageBufferCGBitmapBackend::create(size, 1, ColorSpace::SRGB, cgColorSpace.get(), nullptr);
    
    return ImageBufferCGBitmapBackend::create(size, 1, ColorSpace::SRGB, nullptr);
}

std::unique_ptr<ImageBufferCGBitmapBackend> ImageBufferCGBitmapBackend::create(const FloatSize& size, float resolutionScale, ColorSpace colorSpace, const HostWindow* hostWindow)
{
    return ImageBufferCGBitmapBackend::create(size, resolutionScale, colorSpace, cachedCGColorSpace(colorSpace), hostWindow);
}

ImageBufferCGBitmapBackend::ImageBufferCGBitmapBackend(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace colorSpace, void* data, RetainPtr<CGDataProviderRef>&& dataProvider, std::unique_ptr<GraphicsContext>&& context)
    : ImageBufferCGBackend(logicalSize, backendSize, resolutionScale, colorSpace)
    , m_data(data)
    , m_dataProvider(WTFMove(dataProvider))
    , m_context(WTFMove(context))
{
    ASSERT(m_data);
    ASSERT(m_dataProvider);
    ASSERT(m_context);
    setupContext();
}

GraphicsContext& ImageBufferCGBitmapBackend::context() const
{
    return *m_context;
}

NativeImagePtr ImageBufferCGBitmapBackend::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    switch (copyBehavior) {
    case CopyBackingStore:
        return adoptCF(CGBitmapContextCreateImage(context().platformContext()));

    case DontCopyBackingStore:
        return adoptCF(CGImageCreate(
            m_backendSize.width(), m_backendSize.height(), 8, 32, bytesPerRow(),
            cachedCGColorSpace(m_colorSpace), DefaultBitmapInfo, m_dataProvider.get(),
            0, true, kCGRenderingIntentDefault));
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

Vector<uint8_t> ImageBufferCGBitmapBackend::toBGRAData() const
{
    return ImageBufferBackend::toBGRAData(m_data);
}

RefPtr<ImageData> ImageBufferCGBitmapBackend::getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect) const
{
    return ImageBufferBackend::getImageData(outputFormat, srcRect, m_data);
}

void ImageBufferCGBitmapBackend::putImageData(AlphaPremultiplication inputFormat, const ImageData& imageData, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    ImageBufferBackend::putImageData(inputFormat, imageData, srcRect, destPoint, destFormat, m_data);
}

} // namespace WebCore

#endif // USE(CG)
