/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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

#include "SharedMemory.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsContext.h>
#include <wtf/DebugHeap.h>

namespace WebKit {
using namespace WebCore;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ShareableBitmap);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ShareableBitmap);

ShareableBitmapConfiguration::ShareableBitmapConfiguration(const IntSize& size, std::optional<DestinationColorSpace> colorSpace, bool isOpaque)
    : m_size(size)
    , m_colorSpace(validateColorSpace(colorSpace))
    , m_isOpaque(isOpaque)
    , m_bytesPerPixel(calculateBytesPerPixel(this->colorSpace()))
    , m_bytesPerRow(calculateBytesPerRow(size, this->colorSpace()))
#if USE(CG)
    , m_bitmapInfo(calculateBitmapInfo(this->colorSpace(), isOpaque))
#endif
{
}

ShareableBitmapConfiguration::ShareableBitmapConfiguration(const IntSize& size, std::optional<DestinationColorSpace> colorSpace, bool isOpaque, unsigned bytesPerPixel, unsigned bytesPerRow
#if USE(CG)
    , CGBitmapInfo bitmapInfo
#endif
)
    : m_size(size)
    , m_colorSpace(colorSpace)
    , m_isOpaque(isOpaque)
    , m_bytesPerPixel(bytesPerPixel)
    , m_bytesPerRow(bytesPerRow)
#if USE(CG)
    , m_bitmapInfo(bitmapInfo)
#endif
{
}

CheckedUint32 ShareableBitmapConfiguration::calculateSizeInBytes(const IntSize& size, const DestinationColorSpace& colorSpace)
{
    return calculateBytesPerRow(size, colorSpace) * size.height();
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const ShareableBitmapConfiguration& configuration)
{
    auto numBytes = configuration.sizeInBytes();
    if (numBytes.hasOverflowed())
        return nullptr;

    RefPtr<SharedMemory> sharedMemory = SharedMemory::allocate(numBytes);
    if (!sharedMemory)
        return nullptr;

    return adoptRef(new ShareableBitmap(configuration, sharedMemory.releaseNonNull()));
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const ShareableBitmapConfiguration& configuration, Ref<SharedMemory>&& sharedMemory)
{
    auto numBytes = configuration.sizeInBytes();
    if (numBytes.hasOverflowed())
        return nullptr;

    if (sharedMemory->size() < numBytes) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    
    return adoptRef(new ShareableBitmap(configuration, WTFMove(sharedMemory)));
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const ShareableBitmapHandle& handle, SharedMemory::Protection protection)
{
    auto sharedMemory = SharedMemory::map(handle.m_handle, protection);
    if (!sharedMemory)
        return nullptr;

    return create(handle.m_configuration, sharedMemory.releaseNonNull());
}

std::optional<Ref<ShareableBitmap>> ShareableBitmap::createReadOnly(const std::optional<ShareableBitmapHandle>& handle)
{
    if (!handle)
        return std::nullopt;

    auto sharedMemory = SharedMemory::map(handle->m_handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return std::nullopt;
    
    return adoptRef(*new ShareableBitmap(handle->m_configuration, sharedMemory.releaseNonNull()));
}

std::optional<ShareableBitmapHandle> ShareableBitmap::createHandle(SharedMemory::Protection protection) const
{
    auto memoryHandle = m_sharedMemory->createHandle(protection);
    if (!memoryHandle)
        return std::nullopt;
    ShareableBitmapHandle handle;
    handle.m_handle = WTFMove(*memoryHandle);
    handle.m_configuration = m_configuration;
    return { WTFMove(handle) };
}

std::optional<ShareableBitmapHandle> ShareableBitmap::createReadOnlyHandle() const
{
    ShareableBitmapHandle handle;
    auto memoryHandle = m_sharedMemory->createHandle(SharedMemory::Protection::ReadOnly);
    if (!memoryHandle)
        return std::nullopt;
    handle.m_handle = WTFMove(*memoryHandle);
    handle.m_configuration = m_configuration;
    return handle;
}

ShareableBitmap::ShareableBitmap(ShareableBitmapConfiguration configuration, Ref<SharedMemory>&& sharedMemory)
    : m_configuration(configuration)
    , m_sharedMemory(WTFMove(sharedMemory))
{
}

void* ShareableBitmap::data() const
{
    return m_sharedMemory->data();
}

} // namespace WebKit
