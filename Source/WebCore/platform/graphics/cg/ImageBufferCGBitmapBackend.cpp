/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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
#include "IntRect.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/MallocSpan.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ImageBufferCGBitmapBackend);

size_t ImageBufferCGBitmapBackend::calculateMemoryCost(const Parameters& parameters)
{
    return ImageBufferBackend::calculateMemoryCost(parameters.backendSize, calculateBytesPerRow(parameters.backendSize, parameters.bufferFormat.pixelFormat));
}

std::unique_ptr<ImageBufferCGBitmapBackend> ImageBufferCGBitmapBackend::create(const Parameters& parameters, const ImageBufferCreationContext&)
{
    ASSERT(parameters.bufferFormat.pixelFormat == ImageBufferPixelFormat::BGRA8);

    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    CheckedSize bytesPerRow = checkedProduct<size_t>(4, backendSize.width());
    if (bytesPerRow.hasOverflowed())
        return nullptr;

    CheckedSize numBytes = checkedProduct<size_t>(backendSize.height(), bytesPerRow);
    if (numBytes.hasOverflowed())
        return nullptr;

    auto data = MallocSpan<uint8_t>::tryZeroedMalloc(numBytes);
    if (!data)
        return nullptr;

    ASSERT(!(reinterpret_cast<intptr_t>(data.span().data()) & 3));

    verifyImageBufferIsBigEnough(data.span());

    RetainPtr cgContext = adoptCF(CGBitmapContextCreate(data.mutableSpan().data(), backendSize.width(), backendSize.height(), 8, bytesPerRow, parameters.colorSpace.platformColorSpace(), static_cast<uint32_t>(kCGImageAlphaPremultipliedFirst) | static_cast<uint32_t>(kCGBitmapByteOrder32Host)));
    if (!cgContext)
        return nullptr;

    auto context = makeUnique<GraphicsContextCG>(cgContext.get());

    RetainPtr dataProvider = adoptCF(CGDataProviderCreateWithData(nullptr, data.mutableSpan().data(), numBytes, [] (void*, const void* data, size_t) {
        fastFree(const_cast<void*>(data));
    }));

    return std::unique_ptr<ImageBufferCGBitmapBackend>(new ImageBufferCGBitmapBackend(parameters, data.leakSpan(), WTFMove(dataProvider), WTFMove(context)));
}

ImageBufferCGBitmapBackend::ImageBufferCGBitmapBackend(const Parameters& parameters, std::span<uint8_t> data, RetainPtr<CGDataProviderRef>&& dataProvider, std::unique_ptr<GraphicsContextCG>&& context)
    : ImageBufferCGBackend(parameters, WTFMove(context))
    , m_data(data)
    , m_dataProvider(WTFMove(dataProvider))
{
    ASSERT(m_data.data());
    ASSERT(m_dataProvider);
    ASSERT(m_context);
    applyBaseTransform(*m_context);
}

ImageBufferCGBitmapBackend::~ImageBufferCGBitmapBackend() = default;

GraphicsContext& ImageBufferCGBitmapBackend::context()
{
    return *m_context;
}

unsigned ImageBufferCGBitmapBackend::bytesPerRow() const
{
    return calculateBytesPerRow(m_parameters.backendSize, m_parameters.bufferFormat.pixelFormat);
}

bool ImageBufferCGBitmapBackend::canMapBackingStore() const
{
    return true;
}

RefPtr<NativeImage> ImageBufferCGBitmapBackend::copyNativeImage()
{
    return NativeImage::create(adoptCF(CGBitmapContextCreateImage(context().platformContext())));
}

RefPtr<NativeImage> ImageBufferCGBitmapBackend::createNativeImageReference()
{
    auto backendSize = size();
    return NativeImage::create(adoptCF(CGImageCreate(
        backendSize.width(), backendSize.height(), 8, 32, bytesPerRow(),
        colorSpace().platformColorSpace(), static_cast<uint32_t>(kCGImageAlphaPremultipliedFirst) | static_cast<uint32_t>(kCGBitmapByteOrder32Host), m_dataProvider.get(),
        0, true, kCGRenderingIntentDefault)));
}

void ImageBufferCGBitmapBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    ImageBufferBackend::getPixelBuffer(srcRect, m_data, destination);
}

void ImageBufferCGBitmapBackend::putPixelBuffer(const PixelBufferSourceView& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, m_data);
}

} // namespace WebCore

#endif // USE(CG)
