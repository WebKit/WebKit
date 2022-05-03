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
#include "IPCSemaphore.h"
#include "MessageNames.h"
#include "StreamConnectionBuffer.h"
#include "StreamConnectionEncoder.h"
#include "StreamMessageReceiver.h"
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>

namespace IPC {

class StreamConnectionWorkQueue;

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
class StreamServerConnection final : public ThreadSafeRefCounted<StreamServerConnection>, private MessageReceiveQueue {
    WTF_MAKE_NONCOPYABLE(StreamServerConnection);
public:
    // Creates StreamClientConnection where the out of stream messages and server replies are
    // received through the passed IPC::Connection. The messages from the server are sent to
    // the passed IPC::Connection.
    // Note: This function should be used only in cases where the
    // stream server starts listening to messages with new identifiers on the same thread as
    // in which the server IPC::Connection dispatch messages. At the time of writing,
    // IPC::Connection dispatches messages only in main thread.
    static Ref<StreamServerConnection> create(Connection&, StreamConnectionBuffer&&, StreamConnectionWorkQueue&);

    // Creates StreamServerConnection where the out of stream messages and server replies are
    // received through a dedidcated, new IPC::Connection. The messages from the server are sent to
    // the dedicated conneciton.
    static Ref<StreamServerConnection> createWithDedicatedConnection(Attachment&& connectionIdentifier, StreamConnectionBuffer&&, StreamConnectionWorkQueue&);
    ~StreamServerConnection() final;

    void startReceivingMessages(StreamMessageReceiver&, ReceiverName, uint64_t destinationID);
    // Stops the message receipt. Note: already received messages might still be delivered.
    void stopReceivingMessages(ReceiverName, uint64_t destinationID);

    Connection& connection() { return m_connection; }

    enum DispatchResult : bool {
        HasNoMessages,
        HasMoreMessages
    };
    DispatchResult dispatchStreamMessages(size_t messageLimit);

    void open();
    void invalidate();

    template<typename T, typename... Arguments>
    void sendSyncReply(Connection::SyncRequestID, Arguments&&...);

    Semaphore& clientWaitSemaphore() { return m_clientWaitSemaphore; }

private:
    enum class HasDedicatedConnection : bool { No, Yes };
    StreamServerConnection(Ref<Connection>&&, StreamConnectionBuffer&&, StreamConnectionWorkQueue&, HasDedicatedConnection);

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
    bool processSetStreamDestinationID(Decoder&&, RefPtr<StreamMessageReceiver>& currentReceiver);
    bool dispatchStreamMessage(Decoder&&, StreamMessageReceiver&);
    bool dispatchOutOfStreamMessage(Decoder&&);

    Ref<IPC::Connection> m_connection;
    Semaphore m_clientWaitSemaphore;
    StreamConnectionWorkQueue& m_workQueue;

    size_t m_serverOffset { 0 };
    StreamConnectionBuffer m_buffer;

    Lock m_outOfStreamMessagesLock;
    Deque<std::unique_ptr<Decoder>> m_outOfStreamMessages WTF_GUARDED_BY_LOCK(m_outOfStreamMessagesLock);

    bool m_isDispatchingStreamMessage { false };
    const bool m_hasDedicatedConnection;
    Lock m_receiversLock;
    using ReceiversMap = HashMap<std::pair<uint8_t, uint64_t>, Ref<StreamMessageReceiver>>;
    ReceiversMap m_receivers WTF_GUARDED_BY_LOCK(m_receiversLock);
    uint64_t m_currentDestinationID { 0 };

    friend class StreamConnectionWorkQueue;
};

template<typename T, typename... Arguments>
void StreamServerConnection::sendSyncReply(Connection::SyncRequestID syncRequestID, Arguments&&... arguments)
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

    (encoder.get() << ... << std::forward<Arguments>(arguments));
    m_connection->sendSyncReply(WTFMove(encoder));
}

}
