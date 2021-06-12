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
        Locker locker { m_outOfStreamMessagesLock };
        m_outOfStreamMessages.append(WTFMove(message));
    }
    m_workQueue.wakeUp();
}

std::optional<StreamServerConnectionBase::Span> StreamServerConnectionBase::tryAcquire()
{
    ServerLimit serverLimit = sharedServerLimit().load(std::memory_order_acquire);
    if (serverLimit == ServerLimit::serverIsSleepingTag)
        return std::nullopt;

    auto result = alignedSpan(m_serverOffset, clampedLimit(serverLimit));
    if (result.size < minimumMessageSize) {
        serverLimit = sharedServerLimit().compareExchangeStrong(serverLimit, ServerLimit::serverIsSleepingTag, std::memory_order_acq_rel, std::memory_order_acq_rel);
        result = alignedSpan(m_serverOffset, clampedLimit(serverLimit));
    }

    if (result.size < minimumMessageSize)
        return std::nullopt;

    return result;
}

StreamServerConnectionBase::Span StreamServerConnectionBase::acquireAll()
{
    return alignedSpan(0, dataSize() - 1);
}

void StreamServerConnectionBase::release(size_t readSize)
{
    ASSERT(readSize);
    readSize = std::max(readSize, minimumMessageSize);
    ServerOffset serverOffset = static_cast<ServerOffset>(wrapOffset(alignOffset(m_serverOffset) + readSize));

    ServerOffset oldServerOffset = sharedServerOffset().exchange(serverOffset, std::memory_order_acq_rel);
    // If the client wrote over serverOffset, it means the client is waiting.
    if (oldServerOffset == ServerOffset::clientIsWaitingTag)
        m_buffer.clientWaitSemaphore().signal();
    else
        ASSERT(!(oldServerOffset & ServerOffset::clientIsWaitingTag));

    m_serverOffset = serverOffset;
}

void StreamServerConnectionBase::releaseAll()
{
    sharedServerLimit().store(static_cast<ServerLimit>(0), std::memory_order_release);
    ServerOffset oldServerOffset = sharedServerOffset().exchange(static_cast<ServerOffset>(0), std::memory_order_acq_rel);
    // If the client wrote over serverOffset, it means the client is waiting.
    if (oldServerOffset == ServerOffset::clientIsWaitingTag)
        m_buffer.clientWaitSemaphore().signal();
    else
        ASSERT(!(oldServerOffset & ServerOffset::clientIsWaitingTag));
    m_serverOffset = 0;
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

size_t StreamServerConnectionBase::clampedLimit(ServerLimit serverLimit) const
{
    ASSERT(!(serverLimit & ServerLimit::serverIsSleepingTag));
    size_t limit = static_cast<size_t>(serverLimit);
    ASSERT(limit <= dataSize() - 1);
    return std::min(limit, dataSize() - 1);
}

}
