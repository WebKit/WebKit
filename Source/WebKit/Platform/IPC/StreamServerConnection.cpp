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

#include "config.h"
#include "StreamServerConnection.h"

#include "Connection.h"
#include "StreamConnectionWorkQueue.h"

namespace IPC {

StreamServerConnectionBase::StreamServerConnectionBase(Connection& connection, StreamConnectionBuffer&& stream, StreamConnectionWorkQueue& workQueue)
    : m_connection(connection)
    , m_workQueue(workQueue)
    , m_buffer(WTFMove(stream))
{
}

void StreamServerConnectionBase::startReceivingMessagesImpl(ReceiverName receiverName, uint64_t destinationID)
{
    m_connection->addMessageReceiveQueue(*this, receiverName, destinationID);
    m_workQueue.addStreamConnection(*this);
}

void StreamServerConnectionBase::stopReceivingMessagesImpl(ReceiverName receiverName, uint64_t destinationID)
{
    m_connection->removeMessageReceiveQueue(receiverName, destinationID);
    m_workQueue.removeStreamConnection(*this);
}

void StreamServerConnectionBase::enqueueMessage(Connection&, std::unique_ptr<Decoder>&& message)
{
    {
        auto locker = holdLock(m_outOfStreamMessagesLock);
        m_outOfStreamMessages.append(WTFMove(message));
    }
    m_workQueue.wakeUp();
}

Optional<StreamServerConnectionBase::Span> StreamServerConnectionBase::tryAquire()
{
    size_t receiverLimit = sharedSenderOffset().load(std::memory_order_acquire);
    if (receiverLimit == StreamConnectionBuffer::senderOffsetReceiverIsSleepingTag)
        return WTF::nullopt;
    auto result = alignedSpan(m_receiverOffset, receiverLimit);
    if (result.size < minimumMessageSize) {
        receiverLimit = sharedSenderOffset().compareExchangeStrong(receiverLimit, StreamConnectionBuffer::senderOffsetReceiverIsSleepingTag, std::memory_order_acq_rel, std::memory_order_acq_rel);
        ASSERT(!(receiverLimit & StreamConnectionBuffer::senderOffsetReceiverIsSleepingTag));
        receiverLimit = clampedLimit(receiverLimit);
        result = alignedSpan(m_receiverOffset, receiverLimit);
    }

    if (result.size >= minimumMessageSize)
        return result;
    return WTF::nullopt;
}

void StreamServerConnectionBase::release(size_t readSize)
{
    ASSERT(readSize);
    readSize = std::max(readSize, minimumMessageSize);
    size_t receiverOffset = wrapOffset(alignOffset(m_receiverOffset) + readSize);

#if PLATFORM(COCOA)
    size_t oldReceiverOffset = sharedReceiverOffset().exchange(receiverOffset, std::memory_order_acq_rel);
    // If the sender wrote over receiverOffset, it means the sender is waiting.
    if (oldReceiverOffset == StreamConnectionBuffer::receiverOffsetSenderIsWaitingTag)
        m_buffer.senderWaitSemaphore().signal();
    else
        ASSERT(!(oldReceiverOffset & StreamConnectionBuffer::receiverOffsetSenderIsWaitingTag));
#else
    sharedReceiverOffset().store(receiverOffset, std::memory_order_release);
    // IPC::Semaphore not implemented for the platform. Client will poll and yield.
#endif

    m_receiverOffset = receiverOffset;
}

StreamServerConnectionBase::Span StreamServerConnectionBase::alignedSpan(size_t offset, size_t limit)
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

size_t StreamServerConnectionBase::size(size_t offset, size_t limit)
{
    if (offset <= limit)
        return limit - offset;
    return dataSize() - offset;
}

size_t StreamServerConnectionBase::clampedLimit(size_t untrustedLimit) const
{
    ASSERT(untrustedLimit < (dataSize() - 1));
    return std::min(untrustedLimit, dataSize() - 1);
}

}
