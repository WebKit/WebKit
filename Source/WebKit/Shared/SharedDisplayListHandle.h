/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include "SharedMemory.h"
#include <WebCore/DisplayList.h>
#include <wtf/Atomics.h>
#include <wtf/RefCounted.h>
#include <wtf/Threading.h>

namespace WebKit {

class SharedDisplayListHandle : public RefCounted<SharedDisplayListHandle> {
public:
    virtual ~SharedDisplayListHandle() = default;

    static constexpr auto reservedCapacityAtStart = 2 * sizeof(uint64_t);

    SharedMemory& sharedMemory() { return m_sharedMemory.get(); }
    const SharedMemory& sharedMemory() const { return m_sharedMemory.get(); }

    WebCore::DisplayList::ItemBufferIdentifier identifier() const { return m_identifier; }
    uint8_t* data() const { return reinterpret_cast<uint8_t*>(sharedMemory().data()); }

    uint64_t unreadBytes()
    {
        auto locker = SharedDisplayListHandle::Lock { *this };
        return header().unreadBytes;
    }

    virtual size_t advance(size_t amount) = 0;

protected:
    class Lock {
    public:
        Lock(SharedDisplayListHandle& handle)
            : m_handle(handle)
        {
            auto& atomicValue = m_handle.header().lock;
            while (true) {
                // FIXME: We need to avoid waiting forever in the case where the web content process
                // holds on to the lock indefinitely (or crashes while holding the lock).
                uint64_t unlocked = 0;
                if (atomicValue.compareExchangeWeak(unlocked, 1))
                    break;
                Thread::yield();
            }
        }

        ~Lock()
        {
            m_handle.header().lock.store(0);
        }

    private:
        SharedDisplayListHandle& m_handle;
    };

    SharedDisplayListHandle(WebCore::DisplayList::ItemBufferIdentifier identifier, Ref<SharedMemory>&& sharedMemory)
        : m_identifier(identifier)
        , m_sharedMemory(WTFMove(sharedMemory))
    {
    }

    struct DisplayListSharedMemoryHeader {
        Atomic<uint64_t> lock;
        uint64_t unreadBytes;
    };

    DisplayListSharedMemoryHeader& header() { return *reinterpret_cast<DisplayListSharedMemoryHeader*>(data()); }

private:
    WebCore::DisplayList::ItemBufferIdentifier m_identifier;
    Ref<SharedMemory> m_sharedMemory;
};

} // namespace WebKit
