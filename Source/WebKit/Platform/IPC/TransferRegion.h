/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"
#include "SharedMemory.h"
#include <optional>
#include <span>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSafetyAnalysis.h>

namespace WebCore {
class ProcessIdentity;
}

namespace IPC {

class ScopedTransferRegion;

enum TransferBufferIdentifierType { };
using TransferBufferIdentifier = ObjectIdentifier<TransferBufferIdentifierType>;

class TransferRegion {
public:
    TransferBufferIdentifier identifier;
    size_t size;
    std::optional<WebKit::SharedMemory::Handle> buffer; // Null when expected to be already mapped in the target process.
    static std::optional<TransferRegion> decode(Decoder&);
    template<typename Encoder>
    void encode(Encoder&) const;
};

class TransferBuffer {
public:
    static std::optional<TransferBuffer> create(size_t size)
    {
        if (!size)
            return TransferBuffer { };
        auto buffer = WebKit::SharedMemory::allocate(size);
        if (!buffer)
            return std::nullopt;
        return TransferBuffer { buffer.releaseNonNull() };
    }
    TransferBuffer(TransferBuffer&&) = default;
    TransferBuffer& operator=(TransferBuffer&&) = default;

    TransferBufferIdentifier identifier() const
    {
        return m_identifier;
    }

    std::optional<WebKit::SharedMemory::Handle> passHandle()
    {
        if (m_hasBeenPassed || !m_buffer)
            return std::nullopt;
        m_hasBeenPassed = true;
        return m_buffer->createHandle(WebKit::SharedMemory::Protection::ReadWrite);
    }

    std::span<uint8_t> data()
    {
        if (!m_buffer)
            return { };
        return std::span<uint8_t> { reinterpret_cast<uint8_t*>(m_buffer->data()), m_buffer->size() };
    }

    size_t size() const
    {
        if (!m_buffer)
            return 0;
        return m_buffer->size();
    }

    bool hasBeenPassed() const { return m_hasBeenPassed; }
private:
    TransferBuffer() = default;
    TransferBuffer(Ref<WebKit::SharedMemory> buffer)
        : m_buffer { WTFMove(buffer) }
    {
    }

    TransferBufferIdentifier m_identifier { TransferBufferIdentifier::generateThreadSafe() };
    RefPtr<WebKit::SharedMemory> m_buffer;
    bool m_hasBeenPassed { false };
};


class ScopedTransferRegionMapping {
public:
    ScopedTransferRegionMapping(Ref<WebKit::SharedMemory> memory, size_t size)
        : m_memory { WTFMove(memory) }
        , m_size { size }
    {
    }
    ScopedTransferRegionMapping() = default;

    std::span<uint8_t> data() const
    {
        if (!m_memory)
            return { };
        return std::span<uint8_t> { reinterpret_cast<uint8_t*>(m_memory->data()), m_size };
    }
private:
    RefPtr<WebKit::SharedMemory> m_memory;
    size_t m_size { 0 };
};

// Class to hold shared memory instances that have been sent to a particular receiver process.
class TransferRegionPool : public ThreadSafeRefCounted<TransferRegionPool> {
    WTF_MAKE_NONCOPYABLE(TransferRegionPool);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<TransferRegionPool> create(Connection&);
    std::optional<ScopedTransferRegion> reserveRegion(size_t);
    void invalidate();
    void releaseUnusedMemory();

private:
    explicit TransferRegionPool(Connection&);
    void releaseTransferBuffer(TransferBuffer&&);
    void scheduleReleaseUnusedMemory() WTF_REQUIRES_LOCK(m_lock);
    void unscheduleReleaseUnusedMemory() WTF_REQUIRES_LOCK(m_lock);

    Lock m_lock;
    Connection* m_connection WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<RunLoop::DispatchTimer> m_releaseUnusedTimer WTF_GUARDED_BY_LOCK(m_lock);
    std::optional<TransferBuffer> m_free WTF_GUARDED_BY_LOCK(m_lock);
    friend class ScopedTransferRegion;
};

// Class to hold shared memory mappings from a particular sender process.
class TransferRegionMapper : public ThreadSafeRefCounted<TransferRegionMapper> {
    WTF_MAKE_NONCOPYABLE(TransferRegionMapper);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<TransferRegionMapper> create();
    std::optional<ScopedTransferRegionMapping> mapRegion(TransferRegion&&, const WebCore::ProcessIdentity&);
    void releaseBufferMapping(TransferBufferIdentifier);
    void invalidate();
private:
    TransferRegionMapper() = default;
    using TransferBufferMap = HashMap<TransferBufferIdentifier, Ref<WebKit::SharedMemory>>;
    Lock m_lock;
    TransferBufferMap m_buffers WTF_GUARDED_BY_LOCK(m_lock);
};

class ScopedTransferRegion {
    WTF_MAKE_NONCOPYABLE(ScopedTransferRegion);
public:
    ScopedTransferRegion(TransferRegionPool& pool, TransferBuffer&& buffer, size_t size)
        : m_pool { &pool }
        , m_buffer { WTFMove(buffer) }
        , m_size { size }
    {
    }
    ScopedTransferRegion(ScopedTransferRegion&&) = default;

    ~ScopedTransferRegion()
    {
        if (m_pool)
            m_pool->releaseTransferBuffer(WTFMove(m_buffer));
    }

    TransferRegion passRegion()
    {
        return { m_buffer.identifier(),  m_size, m_buffer.passHandle() };
    }

    std::span<uint8_t> data()
    {
        return m_buffer.data().subspan(0, m_size);
    }

private:
    RefPtr<TransferRegionPool> m_pool;
    TransferBuffer m_buffer;
    size_t m_size { 0 };
};

inline std::optional<TransferRegion> TransferRegion::decode(Decoder& decoder)
{
    auto identifier = decoder.decode<TransferBufferIdentifier>();
    auto size = decoder.decode<size_t>();
    auto buffer = decoder.decode<std::optional<WebKit::SharedMemory::Handle>>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return TransferRegion { *identifier, *size, WTFMove(*buffer) };
}

template<typename Encoder>
void TransferRegion::encode(Encoder& encoder) const
{
    encoder << identifier << size << buffer;
}

}
