/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size, ShareableBitmapConfiguration configuration)
{
    validateConfiguration(configuration);
    auto numBytes = numBytesForSize(size, configuration);
    if (numBytes.hasOverflowed())
        return nullptr;

    RefPtr<SharedMemory> sharedMemory = SharedMemory::allocate(numBytes);
    if (!sharedMemory)
        return nullptr;

    return adoptRef(new ShareableBitmap(size, configuration, sharedMemory.releaseNonNull()));
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size, ShareableBitmapConfiguration configuration, Ref<SharedMemory>&& sharedMemory)
{
    validateConfiguration(configuration);
    auto numBytes = numBytesForSize(size, configuration);
    if (numBytes.hasOverflowed())
        return nullptr;
    if (sharedMemory->size() < numBytes) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    
    return adoptRef(new ShareableBitmap(size, configuration, WTFMove(sharedMemory)));
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const ShareableBitmapHandle& handle, SharedMemory::Protection protection)
{
    auto sharedMemory = SharedMemory::map(handle.m_handle, protection);
    if (!sharedMemory)
        return nullptr;

    return create(handle.m_size, handle.m_configuration, sharedMemory.releaseNonNull());
}

std::optional<Ref<ShareableBitmap>> ShareableBitmap::createReadOnly(const std::optional<ShareableBitmapHandle>& handle)
{
    if (!handle)
        return std::nullopt;

    auto sharedMemory = SharedMemory::map(handle->m_handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return std::nullopt;
    
    return adoptRef(*new ShareableBitmap(handle->m_size, handle->m_configuration, sharedMemory.releaseNonNull()));
}

bool ShareableBitmap::createHandle(ShareableBitmapHandle& handle, SharedMemory::Protection protection) const
{
    if (!m_sharedMemory->createHandle(handle.m_handle, protection))
        return false;
    handle.m_size = m_size;
    handle.m_configuration = m_configuration;
    return true;
}

std::optional<ShareableBitmapHandle> ShareableBitmap::createReadOnlyHandle() const
{
    ShareableBitmapHandle handle;
    if (!m_sharedMemory->createHandle(handle.m_handle, SharedMemory::Protection::ReadOnly))
        return std::nullopt;
    handle.m_size = m_size;
    handle.m_configuration = m_configuration;
    return handle;
}

ShareableBitmap::ShareableBitmap(const IntSize& size, ShareableBitmapConfiguration configuration, Ref<SharedMemory>&& sharedMemory)
    : m_size(size)
    , m_configuration(configuration)
    , m_sharedMemory(WTFMove(sharedMemory))
{
}

void* ShareableBitmap::data() const
{
    return m_sharedMemory->data();
}

CheckedUint32 ShareableBitmap::numBytesForSize(WebCore::IntSize size, const ShareableBitmapConfiguration& configuration)
{
    return calculateBytesPerRow(size, configuration) * size.height();
}

} // namespace WebKit
