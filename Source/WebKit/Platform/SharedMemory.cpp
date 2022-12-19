/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "StreamConnectionEncoder.h"

#include <WebCore/SharedBuffer.h>

namespace WebKit {

using namespace WebCore;

RefPtr<SharedMemory> SharedMemory::copyBuffer(const FragmentedSharedBuffer& buffer)
{
    if (buffer.isEmpty())
        return nullptr;

    auto sharedMemory = allocate(buffer.size());
    if (!sharedMemory)
        return nullptr;

    auto sharedMemoryPtr = static_cast<char*>(sharedMemory->data());
    buffer.forEachSegment([sharedMemoryPtr] (Span<const uint8_t> segment) mutable {
        memcpy(sharedMemoryPtr, segment.data(), segment.size());
        sharedMemoryPtr += segment.size();
    });

    return sharedMemory;
}

Ref<SharedBuffer> SharedMemory::createSharedBuffer(size_t dataSize) const
{
    ASSERT(dataSize <= size());
    return SharedBuffer::create(DataSegment::Provider {
        [protectedThis = Ref { *this }] () -> const uint8_t* {
            return static_cast<const uint8_t*>(protectedThis->data());
        },
        [dataSize] () -> size_t {
            return dataSize;
        }
    });
}

#if !PLATFORM(COCOA)
void SharedMemory::Handle::takeOwnershipOfMemory(MemoryLedger) const
{
}

void SharedMemory::Handle::setOwnershipOfMemory(const ProcessIdentity&, MemoryLedger) const
{
}
#endif

void SharedMemory::Handle::encode(IPC::StreamConnectionEncoder& encoder) const
{
    // This is useful when sending messages with std::optional<SharedMemory::Handle> instances.
    // Messages with std::nullopt std::optional instances can be sent as stream messages
    // and messages with engaged std::optional instances are sent as out-of-stream.
    encoder.markInvalid();
}

} // namespace WebKit
