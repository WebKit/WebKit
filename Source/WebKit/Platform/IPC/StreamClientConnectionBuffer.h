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

class StreamClientConnectionBuffer : public StreamConnectionBuffer {
public:
    static std::optional<StreamClientConnectionBuffer> create(unsigned dataSizeLog2);
    StreamClientConnectionBuffer(StreamClientConnectionBuffer&&) = default;
    StreamClientConnectionBuffer& operator=(StreamClientConnectionBuffer&&) = default;

    std::optional<std::span<uint8_t>> tryAcquire(Timeout);
    std::optional<std::span<uint8_t>> tryAcquireAll(Timeout);

    enum class WakeUpServer : bool { No, Yes };
    WakeUpServer release(size_t writeSize);
    void resetClientOffset();
    std::span<uint8_t> alignedSpan(size_t offset, size_t limit);
    void setSemaphores(IPC::Semaphore&& wakeUp, IPC::Semaphore&& clientWait);
    bool hasSemaphores() const { return m_semaphores.has_value(); }
    void wakeUpServer();

private:
    static constexpr size_t minimumMessageSize = StreamConnectionEncoder::minimumMessageSize;
    static constexpr size_t messageAlignment = StreamConnectionEncoder::messageAlignment;
    explicit StreamClientConnectionBuffer(Ref<WebCore::SharedMemory>);

    size_t size(size_t offset, size_t limit);
    size_t alignOffset(size_t offset) const { return StreamConnectionBuffer::alignOffset<messageAlignment>(offset, minimumMessageSize); }
    Atomic<ClientOffset>& sharedClientOffset() { return clientOffset(); }
    using ClientLimit = ServerOffset;
    Atomic<ClientLimit>& sharedClientLimit() { return serverOffset(); }
    size_t toLimit(ClientLimit) const;

    size_t m_clientOffset { 0 };
    struct Semaphores {
        Semaphore wakeUp;
        Semaphore clientWait;
    };
    std::optional<Semaphores> m_semaphores;
};

inline std::optional<StreamClientConnectionBuffer> StreamClientConnectionBuffer::create(unsigned dataSizeLog2)
{
    // Currently expected to be not that big, and offset to fit in size_t with the tag bits.
    if (dataSizeLog2 >= 31)
        return std::nullopt;

    // Currently the minimum message size is 16, and as such 32 bytes is not enough to hold one message.
    // The problem happens after initial write and read, because after the read the buffer is split between
    // 15 free bytes and 15 free bytes.
    if (dataSizeLog2 <= 5)
        return std::nullopt;

    auto size = (static_cast<size_t>(1) << dataSizeLog2) + headerSize();
    auto memory = WebCore::SharedMemory::allocate(size);
    if (!memory)
        return std::nullopt;
    return StreamClientConnectionBuffer { memory.releaseNonNull() };
}

inline StreamClientConnectionBuffer::StreamClientConnectionBuffer(Ref<WebCore::SharedMemory> memory)
    : StreamConnectionBuffer(WTFMove(memory))
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(sharedMemorySizeIsValid(m_sharedMemory->size()));

    // Read starts from 0 with limit of 0 and reader sleeping.
    sharedClientOffset().store(ClientOffset::serverIsSleepingTag, std::memory_order_relaxed);
    // Write starts from 0 with a limit of the whole buffer.
    sharedClientLimit().store(static_cast<ClientLimit>(0), std::memory_order_relaxed);
}

inline std::optional<std::span<uint8_t>> StreamClientConnectionBuffer::tryAcquire(Timeout timeout)
{
    ClientLimit clientLimit = sharedClientLimit().load(std::memory_order_acquire);
    // This would mean we try to send messages after a timeout. It is a programming error.
    // Since the value is trusted, we only assert.
    ASSERT(clientLimit != ClientLimit::clientIsWaitingTag);

    for (;;) {
        if (clientLimit != ClientLimit::clientIsWaitingTag) {
            auto result = alignedSpan(m_clientOffset, toLimit(clientLimit));
            if (result.size() >= minimumMessageSize)
                return result;
        }
        if (timeout.didTimeOut())
            break;
        ClientLimit oldClientLimit = sharedClientLimit().compareExchangeStrong(clientLimit, ClientLimit::clientIsWaitingTag, std::memory_order_acq_rel, std::memory_order_acquire);
        if (clientLimit == oldClientLimit) {
            if (!m_semaphores || !m_semaphores->clientWait.waitFor(timeout))
                return std::nullopt;
            clientLimit = sharedClientLimit().load(std::memory_order_acquire);
        } else
            clientLimit = oldClientLimit;
        // The alignedSpan uses the minimumMessageSize to calculate the next beginning position in the buffer,
        // and not the size. The size might be more or less what is needed, depending on where the reader is.
        // If there is no capacity for minimum message size, wait until more is available.
        // In the case where clientOffset < clientLimit we can arrive to a situation where
        // 0 < result.size() < minimumMessageSize.
    }
    return std::nullopt;
}

inline std::optional<std::span<uint8_t>> StreamClientConnectionBuffer::tryAcquireAll(Timeout timeout)
{
    // This would mean we try to send messages after a timeout. It is a programming error.
    // Since the value is trusted, we only assert.
    ASSERT(sharedClientLimit().load(std::memory_order_acquire) != ClientLimit::clientIsWaitingTag);

    // The server acknowledges that sync message has been processed by setting clientOffset == clientLimit == 0.
    // Wait for this condition, or then the condition where server says that it started to sleep after setting that condition.
    // The wait sequence involves two variables, so form a transaction by setting clientLimit == clientIsWaitingTag.
    // The transaction is cancelled if the server has already set clientOffset == clientLimit == 0, otherwise it commits.
    // If the transaction commits, server is guaranteed to signal.

    for (;;) {
        ClientLimit clientLimit = sharedClientLimit().exchange(ClientLimit::clientIsWaitingTag, std::memory_order_acq_rel);
        ClientOffset clientOffset = sharedClientOffset().load(std::memory_order_acquire);
        if (!clientLimit && (clientOffset == ClientOffset::serverIsSleepingTag || !clientOffset))
            break;

        if (!m_semaphores || !m_semaphores->clientWait.waitFor(timeout))
            return std::nullopt;
        if (timeout.didTimeOut())
            return std::nullopt;
    }
    // In case the transaction was cancelled, undo the transaction marker.
    sharedClientLimit().store(static_cast<ClientLimit>(0), std::memory_order_release);
    m_clientOffset = 0;
    return alignedSpan(m_clientOffset, 0);
}

inline StreamClientConnectionBuffer::WakeUpServer StreamClientConnectionBuffer::release(size_t size)
{
    size = std::max(size, minimumMessageSize);
    m_clientOffset = wrapOffset(alignOffset(m_clientOffset) + size);
    ASSERT(m_clientOffset < dataSize());
    // If the server wrote over the clientOffset with serverIsSleepingTag, we know it is sleeping.
    ClientOffset oldClientOffset = sharedClientOffset().exchange(static_cast<ClientOffset>(m_clientOffset), std::memory_order_acq_rel);
    if (oldClientOffset == ClientOffset::serverIsSleepingTag)
        return WakeUpServer::Yes;
    ASSERT(!(oldClientOffset & ClientOffset::serverIsSleepingTag));
    return WakeUpServer::No;
}

inline void StreamClientConnectionBuffer::resetClientOffset()
{
    // For synchronous send protocols with replies out-of-band.
    m_clientOffset = 0;
}

inline std::span<uint8_t> StreamClientConnectionBuffer::alignedSpan(size_t offset, size_t limit)
{
    ASSERT(offset < dataSize());
    ASSERT(limit < dataSize());
    size_t aligned = alignOffset(offset);
    size_t resultSize = 0;
    if (offset < limit) {
        if (aligned >= offset && aligned < limit)
            resultSize = size(aligned, limit);
    } else {
        if (aligned >= offset || aligned < limit)
            resultSize = size(aligned, limit);
    }
    return { data() + aligned, resultSize };
}

inline size_t StreamClientConnectionBuffer::size(size_t offset, size_t limit)
{
    if (!limit)
        return dataSize() - 1 - offset;
    if (limit <= offset)
        return dataSize() - offset;
    return limit - offset - 1;
}

inline size_t StreamClientConnectionBuffer::toLimit(ClientLimit clientLimit) const
{
    ASSERT(!(clientLimit & ClientLimit::clientIsWaitingTag));
    ASSERT(static_cast<size_t>(clientLimit) <= dataSize() - 1);
    return static_cast<size_t>(clientLimit);
}

inline void StreamClientConnectionBuffer::setSemaphores(IPC::Semaphore&& wakeUp, IPC::Semaphore&& clientWait)
{
    m_semaphores = { WTFMove(wakeUp), WTFMove(clientWait) };
    m_semaphores->wakeUp.signal();
}

inline void StreamClientConnectionBuffer::wakeUpServer()
{
    if (!m_semaphores)
        return;
    m_semaphores->wakeUp.signal();
}

}
