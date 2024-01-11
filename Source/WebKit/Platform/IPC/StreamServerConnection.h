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
#include "StreamMessageReceiver.h"
#include "StreamServerConnectionBuffer.h"
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>

namespace IPC {

class StreamConnectionWorkQueue;

struct StreamServerConnectionHandle {
    WTF_MAKE_NONCOPYABLE(StreamServerConnectionHandle);
    StreamServerConnectionHandle(Connection::Handle&& connection, StreamConnectionBuffer::Handle&& bufferHandle)
        : outOfStreamConnection(WTFMove(connection))
        , buffer(WTFMove(bufferHandle))
    { }
    StreamServerConnectionHandle(StreamServerConnectionHandle&&) = default;
    StreamServerConnectionHandle& operator=(StreamServerConnectionHandle&&) = default;

    Connection::Handle outOfStreamConnection;
    StreamConnectionBuffer::Handle buffer;
};

struct StreamServerConnectionParameters {
#if ENABLE(IPC_TESTING_API)
    bool ignoreInvalidMessageForTesting { false };
#endif
};

// StreamServerConnection represents the connection between stream client and server, as used by the server.
//
// StreamServerConnection:
//  * Holds the messages towards the server.
//  * Sends the replies back to the client via the stream or normal Connection fallback.
//
// Receiver template contract:
//   void didReceiveStreamMessage(StreamServerConnection&, Decoder&);
//
// The StreamServerConnection does not trust the StreamClientConnection.
class StreamServerConnection final : public ThreadSafeRefCounted<StreamServerConnection>, private MessageReceiveQueue, private Connection::Client {
    WTF_MAKE_NONCOPYABLE(StreamServerConnection);
public:
    using AsyncReplyID = Connection::AsyncReplyID;
    using Handle = StreamServerConnectionHandle;

    static RefPtr<StreamServerConnection> tryCreate(Handle&&, const StreamServerConnectionParameters&);
    ~StreamServerConnection() final;

    void startReceivingMessages(StreamMessageReceiver&, ReceiverName, uint64_t destinationID);
    // Stops the message receipt. Note: already received messages might still be delivered.
    void stopReceivingMessages(ReceiverName, uint64_t destinationID);

    Connection& connection() { return m_connection; }
    Ref<Connection> protectedConnection() { return m_connection; }

    enum DispatchResult : bool {
        HasNoMessages,
        HasMoreMessages
    };
    DispatchResult dispatchStreamMessages(size_t messageLimit);

    void open(StreamConnectionWorkQueue&);
    void invalidate();
    template<typename T> Error send(T&& message, const ObjectIdentifierGenericBase& destinationID);

    template<typename T, typename... Arguments>
    void sendSyncReply(Connection::SyncRequestID, Arguments&&...);

#if ENABLE(IPC_TESTING_API)
    void sendDeserializationErrorSyncReply(Connection::SyncRequestID);
#endif

    template<typename T, typename... Arguments>
    void sendAsyncReply(AsyncReplyID, Arguments&&...);

    Semaphore& clientWaitSemaphore() { return m_clientWaitSemaphore; }

private:
    StreamServerConnection(Ref<Connection>, StreamServerConnectionBuffer&&);

    // MessageReceiveQueue
    void enqueueMessage(Connection&, UniqueRef<Decoder>&&) final;

    // Connection::Client
    void didReceiveMessage(Connection&, Decoder&) final;
    bool didReceiveSyncMessage(Connection&, Decoder&, UniqueRef<Encoder>&) final;
    void didClose(Connection&) final;
    void didReceiveInvalidMessage(Connection&, MessageName) final;

    bool processSetStreamDestinationID(Decoder&&, RefPtr<StreamMessageReceiver>& currentReceiver);
    bool dispatchStreamMessage(Decoder&&, StreamMessageReceiver&);
    bool dispatchOutOfStreamMessage(Decoder&&);

    using WakeUpClient = StreamServerConnectionBuffer::WakeUpClient;
    const Ref<IPC::Connection> m_connection;
    RefPtr<StreamConnectionWorkQueue> m_workQueue;
    StreamServerConnectionBuffer m_buffer;

    Lock m_outOfStreamMessagesLock;
    Deque<UniqueRef<Decoder>> m_outOfStreamMessages WTF_GUARDED_BY_LOCK(m_outOfStreamMessagesLock);

    bool m_isDispatchingStreamMessage { false };
    Lock m_receiversLock;
    using ReceiversMap = HashMap<std::pair<uint8_t, uint64_t>, Ref<StreamMessageReceiver>>;
    ReceiversMap m_receivers WTF_GUARDED_BY_LOCK(m_receiversLock);
    uint64_t m_currentDestinationID { 0 };
    Semaphore m_clientWaitSemaphore;

    friend class StreamConnectionWorkQueue;
};

template<typename T>
Error StreamServerConnection::send(T&& message, const ObjectIdentifierGenericBase& destinationID)
{
    return m_connection->send(std::forward<T>(message), destinationID);
}

template<typename T, typename... Arguments>
void StreamServerConnection::sendSyncReply(Connection::SyncRequestID syncRequestID, Arguments&&... arguments)
{
    if constexpr(T::isReplyStreamEncodable) {
        if (m_isDispatchingStreamMessage) {
            auto span = m_buffer.acquireAll();
            {
                StreamConnectionEncoder messageEncoder { MessageName::SyncMessageReply, span.data(), span.size() };
                if ((messageEncoder << ... << arguments))
                    return;
            }
            StreamConnectionEncoder outOfStreamEncoder { MessageName::ProcessOutOfStreamMessage, span.data(), span.size() };
        }
    }
    auto encoder = makeUniqueRef<Encoder>(MessageName::SyncMessageReply, syncRequestID.toUInt64());

    (encoder.get() << ... << std::forward<Arguments>(arguments));
    m_connection->sendSyncReply(WTFMove(encoder));
}

template<typename T, typename... Arguments>
void StreamServerConnection::sendAsyncReply(AsyncReplyID asyncReplyID, Arguments&&... arguments)
{
    auto encoder = makeUniqueRef<Encoder>(T::asyncMessageReplyName(), asyncReplyID.toUInt64());
    (encoder.get() << ... << std::forward<Arguments>(arguments));
    m_connection->sendSyncReply(WTFMove(encoder));
}

}
