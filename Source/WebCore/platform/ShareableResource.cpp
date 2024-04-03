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

#include "SharedBuffer.h"
#include <wtf/CheckedArithmetic.h>

namespace WebCore {

ShareableResourceHandle::ShareableResourceHandle(SharedMemory::Handle&& handle, unsigned offset, unsigned size)
    : m_handle(WTFMove(handle))
    , m_offset(offset)
    , m_size(size)
{
}

RefPtr<SharedBuffer> ShareableResource::wrapInSharedBuffer()
{
    return SharedBuffer::create(DataSegment::Provider {
        [self = Ref { *this }]() { return self->data(); },
        [self = Ref { *this }]() { return self->size(); }
    });
}

RefPtr<SharedBuffer> ShareableResourceHandle::tryWrapInSharedBuffer() &&
{
    RefPtr<ShareableResource> resource = ShareableResource::map(WTFMove(*this));
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
    if (totalSize > sharedMemory->size()) {
        LOG_ERROR("Failed to create ShareableResource from SharedMemory due to mismatched buffer size.");
        return nullptr;
    }
    return adoptRef(*new ShareableResource(WTFMove(sharedMemory), offset, size));
}

RefPtr<ShareableResource> ShareableResource::map(Handle&& handle)
{
    auto sharedMemory = SharedMemory::map(WTFMove(handle.m_handle), SharedMemory::Protection::ReadOnly);
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

ShareableResource::~ShareableResource() = default;

auto ShareableResource::createHandle() -> std::optional<Handle>
{
    auto memoryHandle = m_sharedMemory->createHandle(SharedMemory::Protection::ReadOnly);
    if (!memoryHandle)
        return std::nullopt;

    return { Handle { WTFMove(*memoryHandle), m_offset, m_size } };
}

const uint8_t* ShareableResource::data() const
{
    return static_cast<const uint8_t*>(m_sharedMemory->data()) + m_offset;
}

unsigned ShareableResource::size() const
{
    return m_size;
}

} // namespace WebCore

#endif // ENABLE(SHAREABLE_RESOURCE)
