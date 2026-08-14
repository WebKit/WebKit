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

#include "IPCEvent.h"
#include "IPCSemaphore.h"
#include "StreamConnectionBuffer.h"
#include "StreamConnectionEncoder.h"
#include <atomic>

namespace IPC {

class StreamClientConnectionBuffer : public StreamConnectionBuffer {
public:
    static std::optional<StreamClientConnectionBuffer> create(unsigned dataSizeLog2, Event&& clientWait);
    StreamClientConnectionBuffer(StreamClientConnectionBuffer&&);
    StreamClientConnectionBuffer& operator=(StreamClientConnectionBuffer&&);

    std::optional<std::span<uint8_t>> tryAcquire(Timeout);
    std::optional<std::span<uint8_t>> tryAcquireAll(Timeout);

    enum class WakeUpServer : bool { No, Yes };
    WakeUpServer release(size_t writeSize);
    void resetClientOffset();
    std::span<uint8_t> alignedMutableSpan(size_t offset, size_t limit);
    // Called once, from the connection receive thread, when the server sends its wake up semaphore.
    void setWakeUpSemaphore(IPC::Semaphore&&);
    bool hasWakeUpSemaphore() const { return m_hasWakeUpSemaphore.load(std::memory_order_acquire); }
    void wakeUpServer();

private:
    static constexpr size_t minimumMessageSize = StreamConnectionEncoder::minimumMessageSize;
    static constexpr size_t messageAlignment = StreamConnectionEncoder::messageAlignment;
    StreamClientConnectionBuffer(Ref<WebCore::SharedMemory>, Event&& clientWait);

    size_t size(size_t offset, size_t limit) const;
    size_t alignOffset(size_t offset) const { return StreamConnectionBuffer::alignOffset<messageAlignment>(offset, minimumMessageSize); }
    Atomic<ClientOffset>& sharedClientOffset() { return clientOffset(); }
    using ClientLimit = ServerOffset;
    Atomic<ClientLimit>& sharedClientLimit() { return serverOffset(); }
    size_t toLimit(ClientLimit) const;

    size_t m_clientOffset { 0 };
    // Waited on for ring buffer backpressure. The client owns this end from the start, so senders can
    // always wait, even before the server has opened its end of the connection. The matching Signal
    // travels to the server in StreamServerConnectionHandle, so the wait is also interrupted if the
    // server process dies and no senders remain.
    Event m_clientWait;
    // The work queue's wake up semaphore is per work queue, so it can only come from the server, in
    // MessageName::InitializeStreamClientConnection. It is written by the connection receive thread
    // and read by the sender, exactly once and never reset, so m_hasWakeUpSemaphore publishes it:
    // only access m_wakeUp after hasWakeUpSemaphore() has returned true.
    std::optional<Semaphore> m_wakeUp;
    std::atomic<bool> m_hasWakeUpSemaphore { false };
};

inline StreamClientConnectionBuffer::StreamClientConnectionBuffer(StreamClientConnectionBuffer&& other)
    : StreamConnectionBuffer(WTF::move(other))
    , m_clientOffset(other.m_clientOffset)
    , m_clientWait(WTF::move(other.m_clientWait))
    , m_wakeUp(WTF::move(other.m_wakeUp))
    , m_hasWakeUpSemaphore(other.m_hasWakeUpSemaphore.load(std::memory_order_relaxed))
{
    // The buffer is moved only during construction, before it is shared with other threads.
}

inline StreamClientConnectionBuffer& StreamClientConnectionBuffer::operator=(StreamClientConnectionBuffer&& other)
{
    StreamConnectionBuffer::operator=(WTF::move(other));
    m_clientOffset = other.m_clientOffset;
    m_clientWait = WTF::move(other.m_clientWait);
    m_wakeUp = WTF::move(other.m_wakeUp);
    m_hasWakeUpSemaphore.store(other.m_hasWakeUpSemaphore.load(std::memory_order_relaxed), std::memory_order_relaxed);
    return *this;
}

inline std::optional<StreamClientConnectionBuffer> StreamClientConnectionBuffer::create(unsigned dataSizeLog2, Event&& clientWait)
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
    return StreamClientConnectionBuffer { memory.releaseNonNull(), WTF::move(clientWait) };
}

inline StreamClientConnectionBuffer::StreamClientConnectionBuffer(Ref<WebCore::SharedMemory> memory, Event&& clientWait)
    : StreamConnectionBuffer(WTF::move(memory))
    , m_clientWait(WTF::move(clientWait))
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
            auto result = alignedMutableSpan(m_clientOffset, toLimit(clientLimit));
            if (result.size() >= minimumMessageSize)
                return result;
        }
        if (timeout.didTimeOut())
            break;
        ClientLimit oldClientLimit = sharedClientLimit().compareExchangeStrong(clientLimit, ClientLimit::clientIsWaitingTag, std::memory_order_acq_rel, std::memory_order_acquire);
        if (clientLimit == oldClientLimit) {
            if (!m_clientWait.waitFor(timeout))
                return std::nullopt;
            clientLimit = sharedClientLimit().load(std::memory_order_acquire);
        } else
            clientLimit = oldClientLimit;
        // The alignedMutableSpan uses the minimumMessageSize to calculate the next beginning position in the buffer,
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

        if (!m_clientWait.waitFor(timeout))
            return std::nullopt;
        if (timeout.didTimeOut())
            return std::nullopt;
    }
    // In case the transaction was cancelled, undo the transaction marker.
    sharedClientLimit().store(static_cast<ClientLimit>(0), std::memory_order_release);
    m_clientOffset = 0;
    return alignedMutableSpan(m_clientOffset, 0);
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

inline std::span<uint8_t> StreamClientConnectionBuffer::alignedMutableSpan(size_t offset, size_t limit)
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
    return mutableSpan().subspan(aligned, resultSize);
}

inline size_t StreamClientConnectionBuffer::size(size_t offset, size_t limit) const
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

inline void StreamClientConnectionBuffer::setWakeUpSemaphore(IPC::Semaphore&& wakeUp)
{
    ASSERT(!hasWakeUpSemaphore());
    m_wakeUp = WTF::move(wakeUp);
    m_hasWakeUpSemaphore.store(true, std::memory_order_release);
    // Let the server drain whatever was written before its semaphore arrived.
    m_wakeUp->signal();
}

inline void StreamClientConnectionBuffer::wakeUpServer()
{
    if (!hasWakeUpSemaphore())
        return;
    m_wakeUp->signal();
}

}
