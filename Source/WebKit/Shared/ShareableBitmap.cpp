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

ShareableBitmap::Handle::Handle()
{
}

void ShareableBitmap::Handle::takeOwnershipOfMemory(MemoryLedger ledger) const
{
    m_handle.takeOwnershipOfMemory(ledger);
}

void ShareableBitmap::Handle::encode(IPC::Encoder& encoder) const
{
    encoder << m_handle;
    encoder << m_size;
    encoder << m_configuration;
}

bool ShareableBitmap::Handle::decode(IPC::Decoder& decoder, Handle& handle)
{
    SharedMemory::Handle memoryHandle;
    if (!decoder.decode(memoryHandle))
        return false;
    if (!decoder.decode(handle.m_size))
        return false;
    if (handle.m_size.width() < 0 || handle.m_size.height() < 0)
        return false;
    if (!decoder.decode(handle.m_configuration))
        return false;
    auto numBytes = numBytesForSize(handle.m_size, handle.m_configuration);
    if (numBytes.hasOverflowed())
        return false;
    if (memoryHandle.size() < numBytes)
        return false;
    handle.m_handle = WTFMove(memoryHandle);
    return true;
}

void ShareableBitmap::Handle::clear()
{
    m_handle.clear();
    m_size = IntSize();
    m_configuration = { };
}

void ShareableBitmap::Configuration::encode(IPC::Encoder& encoder) const
{
    encoder << colorSpace << isOpaque;
}

bool ShareableBitmap::Configuration::decode(IPC::Decoder& decoder, Configuration& result)
{
    std::optional<std::optional<WebCore::DestinationColorSpace>> colorSpace;
    decoder >> colorSpace;
    if (!colorSpace)
        return false;

    std::optional<bool> isOpaque;
    decoder >> isOpaque;
    if (!isOpaque)
        return false;

    result = Configuration { WTFMove(*colorSpace), *isOpaque };
    return true;
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size, Configuration configuration)
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

RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size, Configuration configuration, Ref<SharedMemory>&& sharedMemory)
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

RefPtr<ShareableBitmap> ShareableBitmap::create(const Handle& handle, SharedMemory::Protection protection)
{
    auto sharedMemory = SharedMemory::map(handle.m_handle, protection);
    if (!sharedMemory)
        return nullptr;

    return create(handle.m_size, handle.m_configuration, sharedMemory.releaseNonNull());
}

bool ShareableBitmap::createHandle(Handle& handle, SharedMemory::Protection protection) const
{
    if (!m_sharedMemory->createHandle(handle.m_handle, protection))
        return false;
    handle.m_size = m_size;
    handle.m_configuration = m_configuration;
    return true;
}

ShareableBitmap::ShareableBitmap(const IntSize& size, Configuration configuration, Ref<SharedMemory>&& sharedMemory)
    : m_size(size)
    , m_configuration(configuration)
    , m_sharedMemory(WTFMove(sharedMemory))
{
}

void* ShareableBitmap::data() const
{
    return m_sharedMemory->data();
}

CheckedUint32 ShareableBitmap::numBytesForSize(WebCore::IntSize size, const ShareableBitmap::Configuration& configuration)
{
    return calculateBytesPerRow(size, configuration) * size.height();
}

} // namespace WebKit
