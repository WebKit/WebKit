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

#include "Connection.h"
#include "Decoder.h"
#include "Encoder.h"
#include "MessageNames.h"
#include "StreamConnectionBuffer.h"
#include "StreamConnectionEncoder.h"
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>

namespace IPC {

class StreamConnectionWorkQueue;

class StreamServerConnectionBase : public ThreadSafeRefCounted<StreamServerConnectionBase>, protected MessageReceiveQueue {
    WTF_MAKE_NONCOPYABLE(StreamServerConnectionBase);
public:
    ~StreamServerConnectionBase() override = default;

    Connection& connection() { return m_connection; }

    enum DispatchResult : bool {
        HasNoMessages,
        HasMoreMessages
    };
    virtual DispatchResult dispatchStreamMessages(size_t messageLimit) = 0;

    template<typename T, typename... Arguments>
    void sendSyncReply(Connection::SyncRequestID, Arguments&&...);

protected:
    StreamServerConnectionBase(IPC::Connection&, StreamConnectionBuffer&&, StreamConnectionWorkQueue&);

    void startReceivingMessagesImpl(ReceiverName, uint64_t destinationID);
    void stopReceivingMessagesImpl(ReceiverName, uint64_t destinationID);

    // MessageReceiveQueue
    void enqueueMessage(Connection&, std::unique_ptr<Decoder>&&) final;

    struct Span {
        uint8_t* data;
        size_t size;
    };
    std::optional<Span> tryAcquire();
    Span acquireAll();

    void release(size_t readSize);
    void releaseAll();
    static constexpr size_t minimumMessageSize = StreamConnectionEncoder::minimumMessageSize;
    static constexpr size_t messageAlignment = StreamConnectionEncoder::messageAlignment;
    Span alignedSpan(size_t offset, size_t limit);
    size_t size(size_t offset, size_t limit);
    size_t wrapOffset(size_t offset) const { return m_buffer.wrapOffset(offset); }
    size_t alignOffset(size_t offset) const { return m_buffer.alignOffset<messageAlignment>(offset, minimumMessageSize); }
    using ServerLimit = StreamConnectionBuffer::ClientOffset;
    Atomic<ServerLimit>& sharedServerLimit() { return m_buffer.clientOffset(); }
    using ServerOffset = StreamConnectionBuffer::ServerOffset;
    Atomic<ServerOffset>& sharedServerOffset() { return m_buffer.serverOffset(); }
    size_t clampedLimit(ServerLimit) const;
    uint8_t* data() const { return m_buffer.data(); }
    size_t dataSize() const { return m_buffer.dataSize(); }

    Ref<IPC::Connection> m_connection;
    StreamConnectionWorkQueue& m_workQueue;

    size_t m_serverOffset { 0 };
    StreamConnectionBuffer m_buffer;

    Lock m_outOfStreamMessagesLock;
    Deque<std::unique_ptr<Decoder>> m_outOfStreamMessages WTF_GUARDED_BY_LOCK(m_outOfStreamMessagesLock);

    bool m_isDispatchingStreamMessage { false };

    friend class StreamConnectionWorkQueue;
};

template<typename T, typename... Arguments>
void StreamServerConnectionBase::sendSyncReply(Connection::SyncRequestID syncRequestID, Arguments&&... arguments)
{
    if constexpr(T::isReplyStreamEncodable) {
        if (m_isDispatchingStreamMessage) {
            auto span = acquireAll();
            {
                StreamConnectionEncoder messageEncoder { MessageName::SyncMessageReply, span.data, span.size };
                if ((messageEncoder << ... << arguments))
                    return;
            }
            StreamConnectionEncoder outOfStreamEncoder { MessageName::ProcessOutOfStreamMessage, span.data, span.size };
        }
    }
    auto encoder = makeUniqueRef<Encoder>(MessageName::SyncMessageReply, syncRequestID.toUInt64());

    (encoder.get() << ... << arguments);
    m_connection->sendSyncReply(WTFMove(encoder));
}

// StreamServerConnection represents the connection between stream client and server, as used by the server.
//
// StreamServerConnection:
//  * Holds the messages towards the server.
//  * Sends the replies back to the client via the stream or normal Connection fallback.
//
// Receiver template contract:
//   void didReceiveStreamMessage(StreamServerConnectionBase&, Decoder&);
//
// The StreamServerConnection does not trust the StreamClientConnection.
template<typename Receiver>
class StreamServerConnection final : public StreamServerConnectionBase {
public:
    static Ref<StreamServerConnection> create(Connection& connection, StreamConnectionBuffer&& streamBuffer, StreamConnectionWorkQueue& workQueue)
    {
        return adoptRef(*new StreamServerConnection(connection, WTFMove(streamBuffer), workQueue));
    }
    ~StreamServerConnection() final = default;

    void startReceivingMessages(Receiver&, ReceiverName, uint64_t destinationID);
    // Stops the message receipt. Note: already received messages might still be delivered.
    void stopReceivingMessages(ReceiverName, uint64_t destinationID);

    // StreamServerConnectionBase overrides.
    DispatchResult dispatchStreamMessages(size_t messageLimit) final;

private:
    StreamServerConnection(Connection& connection, StreamConnectionBuffer&& streamBuffer, StreamConnectionWorkQueue& workQueue)
        : StreamServerConnectionBase(connection, WTFMove(streamBuffer), workQueue)
    {
    }
    bool processSetStreamDestinationID(Decoder&&, RefPtr<Receiver>& currentReceiver);
    bool dispatchStreamMessage(Decoder&&, Receiver&);
    bool dispatchOutOfStreamMessage(Decoder&&);
    Lock m_receiversLock;
    using ReceiversMap = HashMap<std::pair<uint8_t, uint64_t>, Ref<Receiver>>;
    ReceiversMap m_receivers WTF_GUARDED_BY_LOCK(m_receiversLock);
    uint64_t m_currentDestinationID { 0 };
};

template<typename Receiver>
void StreamServerConnection<Receiver>::startReceivingMessages(Receiver& receiver, ReceiverName receiverName, uint64_t destinationID)
{
    {
        auto key = std::make_pair(static_cast<uint8_t>(receiverName), destinationID);
        Locker locker { m_receiversLock };
        ASSERT(!m_receivers.contains(key));
        m_receivers.add(key, makeRef(receiver));
    }
    StreamServerConnectionBase::startReceivingMessagesImpl(receiverName, destinationID);
}

template<typename Receiver>
void StreamServerConnection<Receiver>::stopReceivingMessages(ReceiverName receiverName, uint64_t destinationID)
{
    StreamServerConnectionBase::stopReceivingMessagesImpl(receiverName, destinationID);
    auto key = std::make_pair(static_cast<uint8_t>(receiverName), destinationID);
    Locker locker { m_receiversLock };
    ASSERT(m_receivers.contains(key));
    m_receivers.remove(key);
}

template<typename Receiver>
StreamServerConnectionBase::DispatchResult StreamServerConnection<Receiver>::dispatchStreamMessages(size_t messageLimit)
{
    RefPtr<Receiver> currentReceiver;
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

template<typename Receiver>
bool StreamServerConnection<Receiver>::processSetStreamDestinationID(Decoder&& decoder, RefPtr<Receiver>& currentReceiver)
{
    uint64_t destinationID = 0;
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

template<typename Receiver>
bool StreamServerConnection<Receiver>::dispatchStreamMessage(Decoder&& decoder, Receiver& receiver)
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

template<typename Receiver>
bool StreamServerConnection<Receiver>::dispatchOutOfStreamMessage(Decoder&& decoder)
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

    RefPtr<Receiver> receiver;
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
