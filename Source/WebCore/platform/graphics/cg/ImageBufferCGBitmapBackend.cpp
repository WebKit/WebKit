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
#include "ImageBufferCGBitmapBackend.h"

#if USE(CG)

#include "GraphicsContext.h"
#include "GraphicsContextCG.h"
#include "ImageBufferUtilitiesCG.h"
#include "ImageData.h"
#include "IntRect.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferCGBitmapBackend);

IntSize ImageBufferCGBitmapBackend::calculateSafeBackendSize(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    if (backendSize.isEmpty())
        return backendSize;
    
    Checked<unsigned, RecordOverflow> bytesPerRow = 4 * Checked<unsigned, RecordOverflow>(backendSize.width());
    if (bytesPerRow.hasOverflowed())
        return { };

    CheckedSize numBytes = Checked<unsigned, RecordOverflow>(backendSize.height()) * bytesPerRow;
    if (numBytes.hasOverflowed())
        return { };

    return backendSize;
}

size_t ImageBufferCGBitmapBackend::calculateMemoryCost(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    return ImageBufferBackend::calculateMemoryCost(backendSize, calculateBytesPerRow(backendSize));
}

std::unique_ptr<ImageBufferCGBitmapBackend> ImageBufferCGBitmapBackend::create(const Parameters& parameters, CGColorSpaceRef cgColorSpace, const HostWindow*)
{
    ASSERT(parameters.pixelFormat == PixelFormat::BGRA8);

    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    void* data;
    unsigned bytesPerRow = 4 * backendSize.width();

    if (!tryFastCalloc(backendSize.height(), bytesPerRow).getValue(data))
        return nullptr;

    ASSERT(!(reinterpret_cast<intptr_t>(data) & 3));

    size_t numBytes = backendSize.height() * bytesPerRow;
    verifyImageBufferIsBigEnough(data, numBytes);

    auto cgContext = adoptCF(CGBitmapContextCreate(data, backendSize.width(), backendSize.height(), 8, bytesPerRow, cgColorSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
    if (!cgContext)
        return nullptr;

    auto context = makeUnique<GraphicsContext>(cgContext.get());

    const auto releaseImageData = [] (void*, const void* data, size_t) {
        fastFree(const_cast<void*>(data));
    };

    auto dataProvider = adoptCF(CGDataProviderCreateWithData(0, data, numBytes, releaseImageData));

    return std::unique_ptr<ImageBufferCGBitmapBackend>(new ImageBufferCGBitmapBackend(parameters, data, WTFMove(dataProvider), WTFMove(context)));
}

std::unique_ptr<ImageBufferCGBitmapBackend> ImageBufferCGBitmapBackend::create(const Parameters& parameters, const GraphicsContext& context)
{
    if (auto cgColorSpace = context.hasPlatformContext() ? contextColorSpace(context) : nullptr)
        return ImageBufferCGBitmapBackend::create(parameters, cgColorSpace.get(), nullptr);

    return ImageBufferCGBitmapBackend::create(parameters, nullptr);
}

std::unique_ptr<ImageBufferCGBitmapBackend> ImageBufferCGBitmapBackend::create(const Parameters& parameters, const HostWindow* hostWindow)
{
    return ImageBufferCGBitmapBackend::create(parameters, cachedCGColorSpace(parameters.colorSpace), hostWindow);
}

ImageBufferCGBitmapBackend::ImageBufferCGBitmapBackend(const Parameters& parameters, void* data, RetainPtr<CGDataProviderRef>&& dataProvider, std::unique_ptr<GraphicsContext>&& context)
    : ImageBufferCGBackend(parameters)
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

IntSize ImageBufferCGBitmapBackend::backendSize() const
{
    CGContextRef cgContext = context().platformContext();
    return { static_cast<int>(CGBitmapContextGetWidth(cgContext)), static_cast<int>(CGBitmapContextGetHeight(cgContext)) };
}

unsigned ImageBufferCGBitmapBackend::bytesPerRow() const
{
    IntSize backendSize = calculateBackendSize(m_parameters);
    return calculateBytesPerRow(backendSize);
}

RefPtr<NativeImage> ImageBufferCGBitmapBackend::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    switch (copyBehavior) {
    case CopyBackingStore:
        return NativeImage::create(adoptCF(CGBitmapContextCreateImage(context().platformContext())));

    case DontCopyBackingStore:
        auto backendSize = this->backendSize();
        return NativeImage::create(adoptCF(CGImageCreate(
            backendSize.width(), backendSize.height(), 8, 32, bytesPerRow(),
            cachedCGColorSpace(colorSpace()), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host, m_dataProvider.get(),
            0, true, kCGRenderingIntentDefault)));
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
