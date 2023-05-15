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
    using StreamConnectionBuffer::StreamConnectionBuffer;
    static constexpr size_t minimumMessageSize = StreamConnectionEncoder::minimumMessageSize;
    static constexpr size_t messageAlignment = StreamConnectionEncoder::messageAlignment;
    std::span<uint8_t> alignedSpan(size_t offset, size_t limit);
    size_t size(size_t offset, size_t limit);
    size_t alignOffset(size_t offset) const { return StreamConnectionBuffer::alignOffset<messageAlignment>(offset, minimumMessageSize); }
    using ServerLimit = ClientOffset;
    Atomic<ServerLimit>& sharedServerLimit() { return clientOffset(); }
    Atomic<ServerOffset>& sharedServerOffset() { return serverOffset(); }
    size_t clampedLimit(ServerLimit) const;

    size_t m_serverOffset { 0 };
};

inline std::optional<StreamServerConnectionBuffer> StreamServerConnectionBuffer::map(Handle&& handle)
{
    auto sharedMemory = WebKit::SharedMemory::map(WTFMove(handle.memory), WebKit::SharedMemory::Protection::ReadWrite);
    if (UNLIKELY(!sharedMemory))
        return std::nullopt;
    return StreamServerConnectionBuffer { sharedMemory.releaseNonNull() };
}

inline std::optional<std::span<uint8_t>> StreamServerConnectionBuffer::tryAcquire()
{
    ServerLimit serverLimit = sharedServerLimit().load(std::memory_order_acquire);
    if (serverLimit == ServerLimit::serverIsSleepingTag)
        return std::nullopt;

    auto result = alignedSpan(m_serverOffset, clampedLimit(serverLimit));
    if (result.size() < minimumMessageSize) {
        serverLimit = sharedServerLimit().compareExchangeStrong(serverLimit, ServerLimit::serverIsSleepingTag, std::memory_order_acq_rel, std::memory_order_acq_rel);
        result = alignedSpan(m_serverOffset, clampedLimit(serverLimit));
    }

    if (result.size() < minimumMessageSize)
        return std::nullopt;

    return result;
}

inline std::span<uint8_t> StreamServerConnectionBuffer::acquireAll()
{
    return alignedSpan(0, dataSize() - 1);
}

inline StreamServerConnectionBuffer::WakeUpClient StreamServerConnectionBuffer::release(size_t readSize)
{
    ASSERT(readSize);
    readSize = std::max(readSize, minimumMessageSize);
    ServerOffset serverOffset = static_cast<ServerOffset>(wrapOffset(alignOffset(m_serverOffset) + readSize));

    ServerOffset oldServerOffset = sharedServerOffset().exchange(serverOffset, std::memory_order_acq_rel);
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

inline std::span<uint8_t> StreamServerConnectionBuffer::alignedSpan(size_t offset, size_t limit)
{
    ASSERT(offset < dataSize());
    ASSERT(limit < dataSize());
    size_t aligned = alignOffset(offset);
    size_t resultSize = 0;
    if (offset < limit) {
        if (offset <= aligned && aligned < limit)
            resultSize = size(aligned, limit);
    } else if (offset > limit) {
        if (aligned >= offset || aligned < limit)
            resultSize = size(aligned, limit);
    }
    return { data() + aligned, resultSize };
}

inline size_t StreamServerConnectionBuffer::size(size_t offset, size_t limit)
{
    if (offset <= limit)
        return limit - offset;
    return dataSize() - offset;
}

inline size_t StreamServerConnectionBuffer::clampedLimit(ServerLimit serverLimit) const
{
    ASSERT(!(serverLimit & ServerLimit::serverIsSleepingTag));
    size_t limit = static_cast<size_t>(serverLimit);
    ASSERT(limit <= dataSize() - 1);
    return std::min(limit, dataSize() - 1);
}

}
