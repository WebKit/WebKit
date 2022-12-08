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
#include <mutex>
#include <wtf/NeverDestroyed.h>
namespace IPC {
namespace {
// FIXME(http://webkit.org/b/238986): Workaround for not being able to deliver messages from the dedicated connection to the work queue the client uses.
class DedicatedConnectionClient final : public Connection::Client {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DedicatedConnectionClient);
public:
    DedicatedConnectionClient() = default;
    // Connection::Client.
    void didReceiveMessage(Connection& connection, Decoder& decoder) final { ASSERT_NOT_REACHED(); }
    bool didReceiveSyncMessage(Connection& connection, Decoder& decoder, UniqueRef<Encoder>& replyEncoder) final { ASSERT_NOT_REACHED(); return false; }
    void didClose(Connection&) final { } // Client is expected to listen to Connection::didClose() from the connection it sent to the dedicated connection to.
    void didReceiveInvalidMessage(Connection&, MessageName) final { ASSERT_NOT_REACHED(); } // The sender is expected to be trusted, so all invalid messages are programming errors.
private:
};

}

Ref<StreamServerConnection> StreamServerConnection::create(Handle&& handle, StreamConnectionWorkQueue& workQueue)
{
    auto connection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTFMove(handle.outOfStreamConnection) });
    auto buffer = StreamConnectionBuffer::map(WTFMove(handle.buffer));
    RELEASE_ASSERT(buffer); // FIXME: make callers call this outside constructor.
    return adoptRef(*new StreamServerConnection(WTFMove(connection), WTFMove(*buffer), workQueue));
}

StreamServerConnection::StreamServerConnection(Ref<Connection> connection, StreamConnectionBuffer&& stream, StreamConnectionWorkQueue& workQueue)
    : m_connection(WTFMove(connection))
    , m_workQueue(workQueue)
    , m_buffer(WTFMove(stream))
{
}

StreamServerConnection::~StreamServerConnection()
{
    ASSERT(!m_connection->isValid());
}

void StreamServerConnection::open()
{
    static LazyNeverDestroyed<DedicatedConnectionClient> s_dedicatedConnectionClient;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [] {
        s_dedicatedConnectionClient.construct();
    });
    // FIXME(http://webkit.org/b/238986): Workaround for not being able to deliver messages from the dedicated connection to the work queue the client uses.
    m_connection->addMessageReceiveQueue(*this, { });
    m_connection->open(s_dedicatedConnectionClient.get());
}

void StreamServerConnection::invalidate()
{
    m_connection->removeMessageReceiveQueue({ });
    m_connection->invalidate();
}

void StreamServerConnection::startReceivingMessages(StreamMessageReceiver& receiver, ReceiverName receiverName, UInt128 destinationID)
{
    {
        auto key = std::make_pair(static_cast<uint8_t>(receiverName), destinationID);
        Locker locker { m_receiversLock };
        ASSERT(!m_receivers.contains(key));
        m_receivers.add(key, receiver);
    }
    m_workQueue.addStreamConnection(*this);
}

void StreamServerConnection::stopReceivingMessages(ReceiverName receiverName, UInt128 destinationID)
{
    m_workQueue.removeStreamConnection(*this);

    auto key = std::make_pair(static_cast<uint8_t>(receiverName), destinationID);
    Locker locker { m_receiversLock };
    ASSERT(m_receivers.contains(key));
    m_receivers.remove(key);
}

void StreamServerConnection::enqueueMessage(Connection&, std::unique_ptr<Decoder>&& message)
{
    {
        Locker locker { m_outOfStreamMessagesLock };
        m_outOfStreamMessages.append(WTFMove(message));
    }
    m_workQueue.wakeUp();
}

std::optional<StreamServerConnection::Span> StreamServerConnection::tryAcquire()
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

StreamServerConnection::Span StreamServerConnection::acquireAll()
{
    return alignedSpan(0, dataSize() - 1);
}

void StreamServerConnection::release(size_t readSize)
{
    ASSERT(readSize);
    readSize = std::max(readSize, minimumMessageSize);
    ServerOffset serverOffset = static_cast<ServerOffset>(wrapOffset(alignOffset(m_serverOffset) + readSize));

    ServerOffset oldServerOffset = sharedServerOffset().exchange(serverOffset, std::memory_order_acq_rel);
    // If the client wrote over serverOffset, it means the client is waiting.
    if (oldServerOffset == ServerOffset::clientIsWaitingTag)
        m_clientWaitSemaphore.signal();
    else
        ASSERT(!(oldServerOffset & ServerOffset::clientIsWaitingTag));

    m_serverOffset = serverOffset;
}

void StreamServerConnection::releaseAll()
{
    sharedServerLimit().store(static_cast<ServerLimit>(0), std::memory_order_release);
    ServerOffset oldServerOffset = sharedServerOffset().exchange(static_cast<ServerOffset>(0), std::memory_order_acq_rel);
    // If the client wrote over serverOffset, it means the client is waiting.
    if (oldServerOffset == ServerOffset::clientIsWaitingTag)
        m_clientWaitSemaphore.signal();
    else
        ASSERT(!(oldServerOffset & ServerOffset::clientIsWaitingTag));
    m_serverOffset = 0;
}

StreamServerConnection::Span StreamServerConnection::alignedSpan(size_t offset, size_t limit)
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

size_t StreamServerConnection::size(size_t offset, size_t limit)
{
    if (offset <= limit)
        return limit - offset;
    return dataSize() - offset;
}

size_t StreamServerConnection::clampedLimit(ServerLimit serverLimit) const
{
    ASSERT(!(serverLimit & ServerLimit::serverIsSleepingTag));
    size_t limit = static_cast<size_t>(serverLimit);
    ASSERT(limit <= dataSize() - 1);
    return std::min(limit, dataSize() - 1);
}

StreamServerConnection::DispatchResult StreamServerConnection::dispatchStreamMessages(size_t messageLimit)
{
    RefPtr<StreamMessageReceiver> currentReceiver;
    // FIXME: Implement WTF::isValid(ReceiverName).
    uint8_t currentReceiverName = static_cast<uint8_t>(ReceiverName::Invalid);

    for (size_t i = 0; i < messageLimit; ++i) {
        auto span = tryAcquire();
        if (!span)
            return DispatchResult::HasNoMessages;
        IPC::Decoder decoder { span->data, span->size, m_currentDestinationID };
        if (!decoder.isValid()) {
            m_connection->dispatchDidReceiveInvalidMessage(decoder.messageName());
            return DispatchResult::HasNoMessages;
        }
        if (decoder.messageName() == MessageName::SetStreamDestinationID) {
            if (!processSetStreamDestinationID(WTFMove(decoder), currentReceiver))
                return DispatchResult::HasNoMessages;
            continue;
        }
        if (decoder.messageName() == MessageName::ProcessOutOfStreamMessage) {
            if (!dispatchOutOfStreamMessage(WTFMove(decoder)))
                return DispatchResult::HasNoMessages;
            continue;
        }
        if (currentReceiverName != static_cast<uint8_t>(decoder.messageReceiverName())) {
            currentReceiverName = static_cast<uint8_t>(decoder.messageReceiverName());
            currentReceiver = nullptr;
        }
        if (!currentReceiver) {
            auto key = std::make_pair(static_cast<uint8_t>(currentReceiverName), m_currentDestinationID);
            if (!ReceiversMap::isValidKey(key)) {
                m_connection->dispatchDidReceiveInvalidMessage(decoder.messageName());
                return DispatchResult::HasNoMessages;
            }
            Locker locker { m_receiversLock };
            currentReceiver = m_receivers.get(key);
        }
        if (!currentReceiver) {
            // Valid scenario is when receiver has been removed, but there are messages for it in the buffer.
            // FIXME: Since we do not have a receiver, we don't know how to decode the message.
            // This means we must timeout every receiver in the stream connection.
            // Currently we assert that the receivers are empty, as we only have up to one receiver in
            // a stream connection until possibility of skipping is implemented properly.
            Locker locker { m_receiversLock };
            ASSERT(m_receivers.isEmpty());
            return DispatchResult::HasNoMessages;
        }
        if (!dispatchStreamMessage(WTFMove(decoder), *currentReceiver))
            return DispatchResult::HasNoMessages;
    }
    return DispatchResult::HasMoreMessages;
}

bool StreamServerConnection::processSetStreamDestinationID(Decoder&& decoder, RefPtr<StreamMessageReceiver>& currentReceiver)
{
    UInt128 destinationID = 0;
    if (!decoder.decode(destinationID)) {
        m_connection->dispatchDidReceiveInvalidMessage(decoder.messageName());
        return false;
    }
    if (m_currentDestinationID != destinationID) {
        m_currentDestinationID = destinationID;
        currentReceiver = nullptr;
    }
    release(decoder.currentBufferPosition());
    return true;
}

bool StreamServerConnection::dispatchStreamMessage(Decoder&& decoder, StreamMessageReceiver& receiver)
{
    ASSERT(!m_isDispatchingStreamMessage);
    m_isDispatchingStreamMessage = true;
    receiver.didReceiveStreamMessage(*this, decoder);
    m_isDispatchingStreamMessage = false;
    if (!decoder.isValid()) {
        m_connection->dispatchDidReceiveInvalidMessage(decoder.messageName());
        return false;
    }
    if (decoder.isSyncMessage())
        releaseAll();
    else
        release(decoder.currentBufferPosition());
    return true;
}

bool StreamServerConnection::dispatchOutOfStreamMessage(Decoder&& decoder)
{
    std::unique_ptr<Decoder> message;
    {
        Locker locker { m_outOfStreamMessagesLock };
        if (m_outOfStreamMessages.isEmpty())
            return false;
        message = m_outOfStreamMessages.takeFirst();
    }
    if (!message)
        return false;

    RefPtr<StreamMessageReceiver> receiver;
    {
        auto key = std::make_pair(static_cast<uint8_t>(message->messageReceiverName()), message->destinationID());
        Locker locker { m_receiversLock };
        receiver = m_receivers.get(key);
    }
    if (receiver) {
        receiver->didReceiveStreamMessage(*this, *message);
        if (!message->isValid()) {
            m_connection->dispatchDidReceiveInvalidMessage(message->messageName());
            return false;
        }
    }
    // If receiver does not exist if it has been removed but messages are still pending to be
    // processed. It's ok to skip such messages.
    // FIXME: Note, corresponding skip is not possible at the moment for stream messages.
    release(decoder.currentBufferPosition());
    return true;
}

}
