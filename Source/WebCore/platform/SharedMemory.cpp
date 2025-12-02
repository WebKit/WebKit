/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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
#include "SharedMemory.h"

#include "SharedBuffer.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/ParsingUtilities.h>

namespace WebCore {

bool isMemoryAttributionDisabled()
{
    static bool result = []() {
        auto value = unsafeSpan(getenv("WEBKIT_DISABLE_MEMORY_ATTRIBUTION"));
        if (!value.data())
            return false;
        return equalSpans(value, "1"_span);
    }();
    return result;
}

SharedMemoryHandle::SharedMemoryHandle(SharedMemoryHandle::Type&& handle, uint64_t size)
    : m_handle(WTFMove(handle))
    , m_size(size)
{
    RELEASE_ASSERT(!!m_handle);
}

RefPtr<SharedMemory> SharedMemory::copyBuffer(const FragmentedSharedBuffer& buffer)
{
    if (buffer.isEmpty())
        return nullptr;

    auto sharedMemory = allocate(buffer.size());
    if (!sharedMemory)
        return nullptr;

    auto destination = sharedMemory->mutableSpan();
    buffer.forEachSegment([&] (std::span<const uint8_t> segment) mutable {
        memcpySpan(consumeSpan(destination, segment.size()), segment);
    });

    return sharedMemory;
}

Ref<SharedBuffer> SharedMemory::createSharedBuffer(size_t dataSize) const
{
    ASSERT(dataSize <= size());
    return SharedBuffer::create(DataSegment::Provider {
        [protectedThis = Ref { *this }, dataSize]() {
            return protectedThis->span().first(dataSize);
        }
    });
}

#if !PLATFORM(COCOA)
void SharedMemoryHandle::takeOwnershipOfMemory(MemoryLedger) const
{
}

void SharedMemoryHandle::setOwnershipOfMemory(const ProcessIdentity&, MemoryLedger) const
{
}

std::optional<SharedMemoryHandle> SharedMemoryHandle::createVMShare(std::span<const uint8_t>, SharedMemoryProtection)
{
    return std::nullopt;
}

std::optional<SharedMemoryHandle> SharedMemoryHandle::createVMCopy(std::span<const uint8_t>, SharedMemoryProtection)
{
    return std::nullopt;
}
#endif

std::optional<SharedMemoryHandle> SharedMemoryHandle::createCopy(std::span<const uint8_t> data, SharedMemoryProtection protection)
{
    if (data.empty())
        return std::nullopt;
    RefPtr memory = SharedMemory::allocate(data.size());
    if (!memory)
        return std::nullopt;
    auto handle = memory->createHandle(protection);
    if (!handle)
        return std::nullopt;
    memcpySpan(memory->mutableSpan(), data);
    return handle;
}

} // namespace WebCore
