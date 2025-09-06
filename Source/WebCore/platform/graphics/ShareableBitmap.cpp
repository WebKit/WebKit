/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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
#include "ShareableBitmap.h"

#include "GraphicsContext.h"
#include "NativeImage.h"
#include "SharedMemory.h"
#include <wtf/DebugHeap.h>

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(ShareableBitmap, WTF_INTERNAL);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ShareableBitmap);

RefPtr<ShareableBitmap> ShareableBitmap::create(const ShareableBitmapConfiguration& configuration)
{
    RefPtr<SharedMemory> sharedMemory = SharedMemory::allocate(configuration.sizeInBytes());
    if (!sharedMemory)
        return nullptr;

    return adoptRef(new ShareableBitmap(configuration, sharedMemory.releaseNonNull()));
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const ShareableBitmapConfiguration& configuration, Ref<SharedMemory>&& sharedMemory)
{
    if (sharedMemory->size() < configuration.sizeInBytes()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return adoptRef(new ShareableBitmap(configuration, WTFMove(sharedMemory)));
}

RefPtr<ShareableBitmap> ShareableBitmap::createFromImageDraw(NativeImage& image, const DestinationColorSpace& colorSpace)
{
    return createFromImageDraw(image, colorSpace, image.size());
}

RefPtr<ShareableBitmap> ShareableBitmap::createFromImageDraw(NativeImage& image, const DestinationColorSpace& colorSpace, const IntSize& destinationSize)
{
    return createFromImageDraw(image, colorSpace, destinationSize, destinationSize);
}

RefPtr<ShareableBitmap> ShareableBitmap::createFromImageDraw(NativeImage& image, const DestinationColorSpace& colorSpace, const IntSize& destinationSize, const IntSize& sourceSize)
{
    auto bitmap = ShareableBitmap::create(destinationSize, colorSpace);
    if (!bitmap)
        return nullptr;

    auto context = bitmap->createGraphicsContext();
    if (!context)
        return nullptr;

    context->drawNativeImage(image, FloatRect({ }, destinationSize), FloatRect({ }, sourceSize), { CompositeOperator::Copy });
    return bitmap;
}

RefPtr<ShareableBitmap> ShareableBitmap::create(Handle&& handle, SharedMemory::Protection protection)
{
    auto sharedMemory = SharedMemory::map(WTFMove(handle.m_handle), protection);
    if (!sharedMemory)
        return nullptr;

    return create(handle.m_configuration, sharedMemory.releaseNonNull());
}

std::optional<Ref<ShareableBitmap>> ShareableBitmap::createReadOnly(std::optional<Handle>&& handle)
{
    if (!handle)
        return std::nullopt;

    auto sharedMemory = SharedMemory::map(WTFMove(handle->m_handle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return std::nullopt;

    return adoptRef(*new ShareableBitmap(handle->m_configuration, sharedMemory.releaseNonNull()));
}

auto ShareableBitmap::createHandle(SharedMemory::Protection protection) const -> std::optional<Handle>
{
    auto memoryHandle = m_sharedMemory->createHandle(protection);
    if (!memoryHandle)
        return std::nullopt;
    return { Handle(WTFMove(*memoryHandle), m_configuration) };
}

auto ShareableBitmap::createReadOnlyHandle() const -> std::optional<Handle>
{
    return createHandle(SharedMemory::Protection::ReadOnly);
}

ShareableBitmap::ShareableBitmap(ShareableBitmapConfiguration configuration, Ref<SharedMemory>&& sharedMemory)
    : m_configuration(configuration)
    , m_sharedMemory(WTFMove(sharedMemory))
{
}

std::span<const uint8_t> ShareableBitmap::span() const
{
    return m_sharedMemory->span();
}

std::span<uint8_t> ShareableBitmap::mutableSpan()
{
    return m_sharedMemory->mutableSpan();
}

#if !USE(CG)
std::optional<ShareableBitmapConfiguration> ShareableBitmapConfiguration::create(const IntSize& size, PlatformColorSpace&& colorSpace, size_t bytesPerRow, bool isOpaque)
{
    if (size.isEmpty())
        return std::nullopt;
#if !USE(CAIRO)
    if (!colorSpace)
        return std::nullopt;
#endif
    if (!bytesPerRow)
        return std::nullopt;
    CheckedUint32 bytesPerRowUInt32 { bytesPerRow };
    auto sizeInBytes = bytesPerRowUInt32 * size.height();
    if (sizeInBytes.hasOverflowed())
        return std::nullopt;
    auto bytesPerRowTight = calculateBytesPerRow(size, DestinationColorSpace(colorSpace));
    if (bytesPerRowTight.hasOverflowed())
        return std::nullopt;
    if (bytesPerRow < bytesPerRowTight)
        return std::nullopt;
    return ShareableBitmapConfiguration { size, WTFMove(colorSpace), bytesPerRow, isOpaque };
}

ShareableBitmapConfiguration::ShareableBitmapConfiguration(const IntSize& size, PlatformColorSpace&& colorSpace, size_t bytesPerRow, bool isOpaque)
    : m_size(size)
    , m_colorSpace(WTFMove(colorSpace))
    , m_bytesPerRow(bytesPerRow)
    , m_isOpaque(isOpaque)
{
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size, const DestinationColorSpace& colorSpace, bool isOpaque)
{
    auto configuration = ShareableBitmapConfiguration::create(size, colorSpace.platformColorSpace(), ShareableBitmapConfiguration::calculateBytesPerRow(size, colorSpace), isOpaque);
    if (!configuration)
        return nullptr;
    return create(*configuration);
}
#endif

} // namespace WebCore
