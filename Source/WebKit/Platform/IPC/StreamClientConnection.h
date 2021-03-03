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

#include "ArgumentCoders.h"
#include "Connection.h"
#include "Decoder.h"
#include "IPCSemaphore.h"
#include "MessageNames.h"
#include "StreamConnectionBuffer.h"
#include "StreamConnectionEncoder.h"
#include <wtf/MonotonicTime.h>
#include <wtf/Threading.h>
namespace IPC {

// A message stream is a half-duplex two-way stream of messages to a between a sender and the
// destination receiver.
//
// StreamClientConnection can send messages and receive synchronous replies
// through this message stream or through IPC::Connection.
//
// The destination receiver will receive messages in order _for the destination messages_.
// The whole IPC::Connection message order is not preserved.
//
// The StreamClientConnection trusts the StreamServerConnection.
class StreamClientConnection final {
    WTF_MAKE_NONCOPYABLE(StreamClientConnection);
public:
    StreamClientConnection(Connection&, size_t bufferSize);

    StreamConnectionBuffer& streamBuffer() { return m_buffer; }
    void setWakeUpSemaphore(IPC::Semaphore&&);

    template<typename T, typename U> bool send(T&& message, ObjectIdentifier<U> destinationID, Seconds timeout);

    using SendSyncResult = Connection::SendSyncResult;
    template<typename T, typename U>
    SendSyncResult sendSync(T&& message, typename T::Reply&&, ObjectIdentifier<U> destinationID, Seconds timeout);

private:
    struct Span {
        uint8_t* data;
        size_t size;
    };
    static constexpr size_t minimumMessageSize = StreamConnectionEncoder::minimumMessageSize;
    static constexpr size_t messageAlignment = StreamConnectionEncoder::messageAlignment;
    bool trySendDestinationIDIfNeeded(uint64_t destinationID, MonotonicTime timeout);
    void sendProcessOutOfStreamMessage(Span&&);

    Optional<Span> tryAcquire(MonotonicTime timeout);
    enum class WakeUpReceiver : bool {
        No,
        Yes
    };
    WakeUpReceiver release(size_t writeSize);
    void wakeUpReceiver();

    Span alignedSpan(size_t offset, size_t limit);
    size_t size(size_t offset, size_t limit);
    size_t clampedLimit(size_t untrustedLimit) const;

    size_t wrapOffset(size_t offset) const { return m_buffer.wrapOffset(offset); }
    size_t alignOffset(size_t offset) const { return m_buffer.alignOffset<messageAlignment>(offset, minimumMessageSize); }
    Atomic<size_t>& sharedSenderOffset() { return m_buffer.senderOffset(); }
    Atomic<size_t>& sharedReceiverOffset() { return m_buffer.receiverOffset(); }
    uint8_t* data() const { return m_buffer.data(); }
    size_t dataSize() const { return m_buffer.dataSize(); }

    Connection& m_connection;
    uint64_t m_currentDestinationID { 0 };

    size_t m_senderOffset { 0 };
    StreamConnectionBuffer m_buffer;
#if PLATFORM(COCOA)
    Optional<Semaphore> m_wakeUpSemaphore;
#endif
};

template<typename T, typename U>
bool StreamClientConnection::send(T&& message, ObjectIdentifier<U> destinationID, Seconds timeout)
{
    static_assert(!T::isSync, "Message is sync!");
    MonotonicTime absoluteTimeout = MonotonicTime::now() + timeout;
    if (!trySendDestinationIDIfNeeded(destinationID.toUInt64(), absoluteTimeout))
        return false;
    auto span = tryAcquire(absoluteTimeout);
    if (!span)
        return false;
    {
        StreamConnectionEncoder messageEncoder { T::name(), span->data, span->size };
        if (messageEncoder << message.arguments()) {
            auto wakeupResult = release(messageEncoder.size());
            if (wakeupResult == StreamClientConnection::WakeUpReceiver::Yes)
                wakeUpReceiver();
            return true;
        }
    }
    sendProcessOutOfStreamMessage(WTFMove(*span));
    if (!m_connection.send(WTFMove(message), destinationID, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply))
        return false;
    return true;
}

template<typename T, typename U>
StreamClientConnection::SendSyncResult StreamClientConnection::sendSync(T&& message, typename T::Reply&& reply, ObjectIdentifier<U> destinationID, Seconds timeout)
{
    static_assert(T::isSync, "Message is not sync!");
    MonotonicTime absoluteTimeout = MonotonicTime::now() + timeout;
    if (!trySendDestinationIDIfNeeded(destinationID.toUInt64(), absoluteTimeout))
        return { };
    auto span = tryAcquire(absoluteTimeout);
    if (!span)
        return { };
    // FIXME: implement send through stream.
    // FIXME: implement receive through stream.
    sendProcessOutOfStreamMessage(WTFMove(*span));
    auto result = m_connection.sendSync(WTFMove(message), WTFMove(reply), destinationID.toUInt64(), timeout);
    ASSERT(result);
    return result;
}

inline bool StreamClientConnection::trySendDestinationIDIfNeeded(uint64_t destinationID, MonotonicTime absoluteTimeout)
{
    if (destinationID == m_currentDestinationID)
        return true;
    auto span = tryAcquire(absoluteTimeout);
    if (!span)
        return false;
    StreamConnectionEncoder encoder { MessageName::SetStreamDestinationID, span->data, span->size };
    if (!(encoder << destinationID)) {
        ASSERT_NOT_REACHED(); // Size of the minimum allocation is incorrect. Likely an alignment issue.
        return false;
    }
    auto wakeupResult = release(encoder.size());
    if (wakeupResult == StreamClientConnection::WakeUpReceiver::Yes)
        wakeUpReceiver();
    m_currentDestinationID = destinationID;
    return true;
}

inline void StreamClientConnection::sendProcessOutOfStreamMessage(Span&& span)
{
    StreamConnectionEncoder encoder { MessageName::ProcessOutOfStreamMessage, span.data, span.size };
    // Not notifying on wake up since the out-of-stream message will do that.
    auto result = release(encoder.size());
    UNUSED_VARIABLE(result);
}
inline Optional<StreamClientConnection::Span> StreamClientConnection::tryAcquire(MonotonicTime timeout)
{
    for (;;) {
        size_t senderLimit = clampedLimit(sharedReceiverOffset().load(std::memory_order_acquire));
        auto result = alignedSpan(m_senderOffset, senderLimit);
        if (result.size < minimumMessageSize) {
            auto now = MonotonicTime::now();
            if (now >= timeout)
                break;
#if PLATFORM(COCOA)
            size_t oldSenderLimit = sharedReceiverOffset().compareExchangeStrong(senderLimit, StreamConnectionBuffer::receiverOffsetSenderIsWaitingTag, std::memory_order_acq_rel, std::memory_order_acq_rel);
            if (senderLimit == oldSenderLimit)
                m_buffer.senderWaitSemaphore().waitFor(timeout - now);
            else {
                senderLimit = clampedLimit(oldSenderLimit);
                result = alignedSpan(m_senderOffset, senderLimit);
            }
#else
            Thread::yield();
#endif
        }
        if (result.size >= minimumMessageSize)
            return result;
        // The alignedSpan uses the minimumMessageSize to calculate the next beginning position in the buffer,
        // and not the size. The size might be more or less what is needed, depending on where the reader is.
        // If there is no capacity for minimum message size, wait until more is available.
        // In the case where senderOffset < receiverOffset we can arrive to a situation where
        // 0 < result.size < minimumMessageSize.
    }
    return WTF::nullopt;
}

inline StreamClientConnection::WakeUpReceiver StreamClientConnection::release(size_t size)
{
    size = std::max(size, minimumMessageSize);
    m_senderOffset = wrapOffset(alignOffset(m_senderOffset) + size);
    ASSERT(m_senderOffset < dataSize());
    // If the receiver wrote over the senderOffset with senderOffsetReceiverIsSleepingTag, we know it is sleeping.
    size_t receiverLimit = sharedSenderOffset().exchange(m_senderOffset, std::memory_order_acq_rel);
    if (receiverLimit == StreamConnectionBuffer::senderOffsetReceiverIsSleepingTag)
        return WakeUpReceiver::Yes;
    ASSERT(!(receiverLimit & StreamConnectionBuffer::senderOffsetReceiverIsSleepingTag));
    return WakeUpReceiver::No;
}

inline StreamClientConnection::Span StreamClientConnection::alignedSpan(size_t offset, size_t limit)
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

inline size_t StreamClientConnection::size(size_t offset, size_t limit)
{
    if (!limit)
        return dataSize() - 1 - offset;
    if (limit <= offset)
        return dataSize() - offset;
    return limit - offset - 1;
}

inline size_t StreamClientConnection::clampedLimit(size_t untrustedLimit) const
{
    ASSERT(untrustedLimit < (dataSize() - 1));
    return std::min(untrustedLimit, dataSize() - 1);
}

}
