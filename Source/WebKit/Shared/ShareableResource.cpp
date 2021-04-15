/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "ShareableResource.h"

#if ENABLE(SHAREABLE_RESOURCE)

#include "ArgumentCoders.h"
#include <WebCore/SharedBuffer.h>

namespace WebKit {
using namespace WebCore;

ShareableResource::Handle::Handle()
{
}

void ShareableResource::Handle::encode(IPC::Encoder& encoder) const
{
    encoder << SharedMemory::IPCHandle { WTFMove(m_handle), m_size };
    encoder << m_offset;
}

bool ShareableResource::Handle::decode(IPC::Decoder& decoder, Handle& handle)
{
    SharedMemory::IPCHandle ipcHandle;
    if (!decoder.decode(ipcHandle))
        return false;
    if (!decoder.decode(handle.m_offset))
        return false;

    handle.m_size = ipcHandle.dataSize;
    handle.m_handle = WTFMove(ipcHandle.handle);
    return true;
}

#if USE(CF)
static void shareableResourceDeallocate(void *ptr, void *info)
{
    static_cast<ShareableResource*>(info)->deref(); // Balanced by ref() in createShareableResourceDeallocator()
}
    
static CFAllocatorRef createShareableResourceDeallocator(ShareableResource* resource)
{
    CFAllocatorContext context = { 0,
        resource,
        NULL, // retain
        NULL, // release
        NULL, // copyDescription
        NULL, // allocate
        NULL, // reallocate
        shareableResourceDeallocate,
        NULL, // preferredSize
    };

    return CFAllocatorCreate(kCFAllocatorDefault, &context);
}
#endif

RefPtr<SharedBuffer> ShareableResource::wrapInSharedBuffer()
{
    ref(); // Balanced by deref when SharedBuffer is deallocated.

#if USE(CF)
    RetainPtr<CFAllocatorRef> deallocator = adoptCF(createShareableResourceDeallocator(this));
    RetainPtr<CFDataRef> cfData = adoptCF(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(data()), static_cast<CFIndex>(size()), deallocator.get()));
    return SharedBuffer::create(cfData.get());
#elif USE(SOUP)
    return SharedBuffer::wrapSoupBuffer(soup_buffer_new_with_owner(data(), size(), this, [](void* data) { static_cast<ShareableResource*>(data)->deref(); }));
#else
    ASSERT_NOT_REACHED();
    return nullptr;
#endif
}

RefPtr<SharedBuffer> ShareableResource::Handle::tryWrapInSharedBuffer() const
{
    RefPtr<ShareableResource> resource = ShareableResource::map(*this);
    if (!resource) {
        LOG_ERROR("Failed to recreate ShareableResource from handle.");
        return nullptr;
    }

    return resource->wrapInSharedBuffer();
}

RefPtr<ShareableResource> ShareableResource::create(Ref<SharedMemory>&& sharedMemory, unsigned offset, unsigned size)
{
    auto totalSize = CheckedSize(offset) + size;
    if (totalSize.hasOverflowed()) {
        LOG_ERROR("Failed to create ShareableResource from SharedMemory due to overflow.");
        return nullptr;
    }
    if (totalSize.unsafeGet() > sharedMemory->size()) {
        LOG_ERROR("Failed to create ShareableResource from SharedMemory due to mismatched buffer size.");
        return nullptr;
    }
    return adoptRef(*new ShareableResource(WTFMove(sharedMemory), offset, size));
}

RefPtr<ShareableResource> ShareableResource::map(const Handle& handle)
{
    auto sharedMemory = SharedMemory::map(handle.m_handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return nullptr;

    return create(sharedMemory.releaseNonNull(), handle.m_offset, handle.m_size);
}

ShareableResource::ShareableResource(Ref<SharedMemory>&& sharedMemory, unsigned offset, unsigned size)
    : m_sharedMemory(WTFMove(sharedMemory))
    , m_offset(offset)
    , m_size(size)
{
}

ShareableResource::~ShareableResource()
{
}

bool ShareableResource::createHandle(Handle& handle)
{
    if (!m_sharedMemory->createHandle(handle.m_handle, SharedMemory::Protection::ReadOnly))
        return false;

    handle.m_offset = m_offset;
    handle.m_size = m_size;

    return true;
}

const char* ShareableResource::data() const
{
    return static_cast<const char*>(m_sharedMemory->data()) + m_offset;
}

unsigned ShareableResource::size() const
{
    return m_size;
}
    
} // namespace WebKit

#endif // ENABLE(SHAREABLE_RESOURCE)
