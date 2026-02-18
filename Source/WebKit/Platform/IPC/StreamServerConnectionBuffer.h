/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "IPCSemaphore.h"
#include "StreamConnectionBuffer.h"
#include "StreamConnectionBufferRingWrapper.h"
#include "StreamConnectionEncoder.h"

namespace IPC {

class StreamServerConnectionBuffer : public StreamConnectionBuffer {
public:
    static std::optional<StreamServerConnectionBuffer> map(Handle&&);

    StreamServerConnectionBuffer(StreamServerConnectionBuffer&&) = default;
    StreamServerConnectionBuffer& operator=(StreamServerConnectionBuffer&&) = default;
    std::optional<std::span<uint8_t>> tryAcquire();
    std::span<uint8_t> acquireAll();
    enum class WakeUpClient : bool { No, Yes };
    WakeUpClient release(size_t readSize);
    WakeUpClient releaseAll();

private:
    StreamServerConnectionBuffer(Ref<WebCore::SharedMemory>&&);
    static constexpr size_t minimumMessageSize = StreamConnectionEncoder::minimumMessageSize;
    static constexpr size_t messageAlignment = StreamConnectionEncoder::messageAlignment;
    std::span<uint8_t> alignedMutableSpan(size_t offset, size_t limit);
    using ServerLimit = ClientOffset;
    Atomic<ServerLimit>& sharedServerLimit() { return clientOffset(); }
    Atomic<ServerOffset>& sharedServerOffset() { return serverOffset(); }

    size_t m_serverOffset { 0 };

    RingBuffer m_ringBuffer;
    size_t lastOffset { 0 };
    size_t lastLimit { 0 };
};

inline StreamServerConnectionBuffer::StreamServerConnectionBuffer(Ref<WebCore::SharedMemory>&& memory)
    : StreamConnectionBuffer(WTF::move(memory))
    , m_ringBuffer(mutableSpan())
{ }

inline std::optional<StreamServerConnectionBuffer> StreamServerConnectionBuffer::map(Handle&& handle)
{
    auto sharedMemory = WebCore::SharedMemory::map(WTF::move(handle), WebCore::SharedMemory::Protection::ReadWrite);
    if (!sharedMemory) [[unlikely]]
        return std::nullopt;
    return StreamServerConnectionBuffer { sharedMemory.releaseNonNull() };
}

inline std::optional<std::span<uint8_t>> StreamServerConnectionBuffer::tryAcquire()
{
    ServerLimit serverLimit = sharedServerLimit().load(std::memory_order_acquire);
    if (serverLimit == ServerLimit::serverIsSleepingTag)
        return std::nullopt;

    auto result = alignedMutableSpan(m_serverOffset, serverLimit);
    if (result.size() < minimumMessageSize) {
        serverLimit = sharedServerLimit().compareExchangeStrong(serverLimit, ServerLimit::serverIsSleepingTag, std::memory_order_acq_rel, std::memory_order_acquire);
        result = alignedMutableSpan(m_serverOffset, serverLimit);
    }

    if (result.size() < minimumMessageSize)
        return std::nullopt;

    return result;
}

inline std::span<uint8_t> StreamServerConnectionBuffer::acquireAll()
{
    return alignedMutableSpan(0, dataSize() - 1);
}

inline StreamServerConnectionBuffer::WakeUpClient StreamServerConnectionBuffer::release(size_t readSize)
{
    ASSERT(readSize);
    readSize = std::max(readSize, minimumMessageSize);
    size_t serverOffset = roundUpToMultipleOf<messageAlignment>(m_serverOffset + readSize);
    if (serverOffset >= dataSize())
        serverOffset -= dataSize();

    ASSERT(serverOffset < dataSize());
    ServerOffset oldServerOffset = sharedServerOffset().exchange(static_cast<ServerOffset>(serverOffset), std::memory_order_acq_rel);
    WakeUpClient wakeUpClient = WakeUpClient::No;
    // If the client wrote over serverOffset, it means the client is waiting.
    if (oldServerOffset == ServerOffset::clientIsWaitingTag)
        wakeUpClient = WakeUpClient::Yes;
    else
        ASSERT(!(oldServerOffset & ServerOffset::clientIsWaitingTag));

    m_serverOffset = serverOffset;
    return wakeUpClient;
}

inline StreamServerConnectionBuffer::WakeUpClient StreamServerConnectionBuffer::releaseAll()
{
    sharedServerLimit().store(static_cast<ServerLimit>(0), std::memory_order_release);
    ServerOffset oldServerOffset = sharedServerOffset().exchange(static_cast<ServerOffset>(0), std::memory_order_acq_rel);
    WakeUpClient wakeUpClient = WakeUpClient::No;
    // If the client wrote over serverOffset, it means the client is waiting.
    if (oldServerOffset == ServerOffset::clientIsWaitingTag)
        wakeUpClient = WakeUpClient::Yes;
    else
        ASSERT(!(oldServerOffset & ServerOffset::clientIsWaitingTag));
    m_serverOffset = 0;
    return wakeUpClient;
}

#include <stdio.h>

inline std::span<uint8_t> StreamServerConnectionBuffer::alignedMutableSpan(size_t offset, size_t limit)
{
    ASSERT(offset < dataSize());
    ASSERT(limit < dataSize());
    lastOffset = offset;
    lastLimit = limit;
    ASSERT(offset == roundUpToMultipleOf<messageAlignment>(offset));
    // fprintf(stderr, "%06lx : [%06lx, %06lx)\n", reinterpret_cast<size_t>(&m_ringBuffer.getSpan(0, 1)[0]), offset, limit);
    if (offset <= limit)
        return m_ringBuffer.getSpan(offset, limit - offset);
    else
        return m_ringBuffer.getSpan(offset, limit + dataSize() - offset);
}

}
