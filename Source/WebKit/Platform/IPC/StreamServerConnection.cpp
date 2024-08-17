/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

RefPtr<StreamServerConnection> StreamServerConnection::tryCreate(Handle&& handle, const StreamServerConnectionParameters& params)
{
    auto buffer = StreamServerConnectionBuffer::map(WTFMove(handle.buffer));
    if (!buffer)
        return { };

    auto connection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTFMove(handle.outOfStreamConnection) });

#if ENABLE(IPC_TESTING_API)
    if (params.ignoreInvalidMessageForTesting)
        connection->setIgnoreInvalidMessageForTesting();
#endif

    return adoptRef(*new StreamServerConnection(WTFMove(connection), WTFMove(*buffer)));
}

StreamServerConnection::StreamServerConnection(Ref<Connection> connection, StreamServerConnectionBuffer&& stream)
    : m_connection(WTFMove(connection))
    , m_buffer(WTFMove(stream))
{
}

StreamServerConnection::~StreamServerConnection()
{
    ASSERT(!m_connection->isValid());
}

void StreamServerConnection::open(StreamConnectionWorkQueue& workQueue)
{
    m_workQueue = &workQueue;
    // FIXME(http://webkit.org/b/238986): Workaround for not being able to deliver messages from the dedicated connection to the work queue the client uses.
    Ref connection = m_connection;
    connection->addMessageReceiveQueue(*this, { });
    connection->open(*this, workQueue);
    workQueue.addStreamConnection(*this);
}

void StreamServerConnection::invalidate()
{
    Ref connection = m_connection;
    if (!m_workQueue) {
        connection->invalidate();
        return;
    }
    protectedWorkQueue()->removeStreamConnection(*this);
    connection->invalidate();
    connection->removeMessageReceiveQueue({ });
    m_workQueue = nullptr;
    Locker locker { m_outOfStreamMessagesLock };
    m_outOfStreamMessages.clear();
}

void StreamServerConnection::startReceivingMessages(StreamMessageReceiver& receiver, ReceiverName receiverName, uint64_t destinationID)
{
    auto key = std::make_pair(static_cast<uint8_t>(receiverName), destinationID);
    Locker locker { m_receiversLock };
    auto result = m_receivers.add(key, receiver);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void StreamServerConnection::stopReceivingMessages(ReceiverName receiverName, uint64_t destinationID)
{
    auto key = std::make_pair(static_cast<uint8_t>(receiverName), destinationID);
    Locker locker { m_receiversLock };
    bool didRemove = m_receivers.remove(key);
    ASSERT_UNUSED(didRemove, didRemove);
}

void StreamServerConnection::enqueueMessage(Connection&, UniqueRef<Decoder>&& message)
{
    {
        Locker locker { m_outOfStreamMessagesLock };
        m_outOfStreamMessages.append(WTFMove(message));
    }
    ASSERT(m_workQueue);
    protectedWorkQueue()->wakeUp();
}

void StreamServerConnection::didReceiveMessage(Connection&, Decoder&)
{
    // All messages go to message queue.
    ASSERT_NOT_REACHED();
}
bool StreamServerConnection::didReceiveSyncMessage(Connection&, Decoder&, UniqueRef<Encoder>&)
{
    // All messages go to message queue.
    ASSERT_NOT_REACHED();
    return false;
}

void StreamServerConnection::didClose(Connection&)
{
    // Client is expected to listen to didClose from the main connection.
}

void StreamServerConnection::didReceiveInvalidMessage(Connection&, MessageName, int32_t)
{
    // The sender is expected to be trusted, so all invalid messages are programming errors.
    ASSERT_NOT_REACHED();
}

StreamServerConnection::DispatchResult StreamServerConnection::dispatchStreamMessages(size_t messageLimit)
{
    RefPtr<StreamMessageReceiver> currentReceiver;
    // FIXME: Implement WTF::isValid(ReceiverName).
    uint8_t currentReceiverName = static_cast<uint8_t>(ReceiverName::Invalid);

    for (size_t i = 0; i < messageLimit; ++i) {
        auto span = m_buffer.tryAcquire();
        if (!span)
            return DispatchResult::HasNoMessages;
        IPC::Decoder decoder { *span, m_currentDestinationID };
        if (!decoder.isValid()) {
            protectedConnection()->dispatchDidReceiveInvalidMessage(decoder.messageName(), decoder.indexOfObjectFailingDecoding());
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
                protectedConnection()->dispatchDidReceiveInvalidMessage(decoder.messageName(), decoder.indexOfObjectFailingDecoding());
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
    auto destinationID = decoder.decode<uint64_t>();
    if (!destinationID) {
        protectedConnection()->dispatchDidReceiveInvalidMessage(decoder.messageName(), decoder.indexOfObjectFailingDecoding());
        return false;
    }
    if (m_currentDestinationID != *destinationID) {
        m_currentDestinationID = *destinationID;
        currentReceiver = nullptr;
    }
    auto result = m_buffer.release(decoder.currentBufferOffset());
    if (result == WakeUpClient::Yes)
        m_clientWaitSemaphore.signal();
    return true;
}

bool StreamServerConnection::dispatchStreamMessage(Decoder&& decoder, StreamMessageReceiver& receiver)
{
    ASSERT(!m_isDispatchingStreamMessage);
    m_isDispatchingStreamMessage = true;
    receiver.didReceiveStreamMessage(*this, decoder);
    m_isDispatchingStreamMessage = false;
    if (!decoder.isValid()) {
        Ref connection = m_connection;
#if ENABLE(IPC_TESTING_API)
        if (connection->ignoreInvalidMessageForTesting())
            return false;
#endif // ENABLE(IPC_TESTING_API)
        connection->dispatchDidReceiveInvalidMessage(decoder.messageName(), decoder.indexOfObjectFailingDecoding());
        return false;
    }
    WakeUpClient result = WakeUpClient::No;
    if (decoder.isSyncMessage())
        result = m_buffer.releaseAll();
    else
        result = m_buffer.release(decoder.currentBufferOffset());
    if (result == WakeUpClient::Yes)
        m_clientWaitSemaphore.signal();
    return true;
}

bool StreamServerConnection::dispatchOutOfStreamMessage(Decoder&& decoder)
{
    std::unique_ptr<Decoder> message;
    {
        Locker locker { m_outOfStreamMessagesLock };
        if (m_outOfStreamMessages.isEmpty())
            return false;
        message = m_outOfStreamMessages.takeFirst().moveToUniquePtr();
    }

    RefPtr<StreamMessageReceiver> receiver;
    {
        auto key = std::make_pair(static_cast<uint8_t>(message->messageReceiverName()), static_cast<uint64_t>(message->destinationID()));
        Locker locker { m_receiversLock };
        receiver = m_receivers.get(key);
    }
    if (receiver) {
        receiver->didReceiveStreamMessage(*this, *message);
        if (!message->isValid()) {
            Ref connection = m_connection;
#if ENABLE(IPC_TESTING_API)
            if (connection->ignoreInvalidMessageForTesting())
                return false;
#endif // ENABLE(IPC_TESTING_API)
            connection->dispatchDidReceiveInvalidMessage(message->messageName(), message->indexOfObjectFailingDecoding());
            return false;
        }
    }
    // If receiver does not exist if it has been removed but messages are still pending to be
    // processed. It's ok to skip such messages.
    // FIXME: Note, corresponding skip is not possible at the moment for stream messages.
    auto result = m_buffer.release(decoder.currentBufferOffset());
    if (result == WakeUpClient::Yes)
        m_clientWaitSemaphore.signal();
    return true;
}

RefPtr<StreamConnectionWorkQueue> StreamServerConnection::protectedWorkQueue() const
{
    return m_workQueue;
}

#if ENABLE(IPC_TESTING_API)
void StreamServerConnection::sendDeserializationErrorSyncReply(Connection::SyncRequestID syncRequestID)
{
    auto encoder = makeUniqueRef<Encoder>(MessageName::SyncMessageReply, syncRequestID.toUInt64());
    encoder->setSyncMessageDeserializationFailure();
    protectedConnection()->sendSyncReply(WTFMove(encoder));
}
#endif


}
