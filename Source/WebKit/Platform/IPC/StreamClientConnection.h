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
#include "StreamClientConnectionBuffer.h"
#include "StreamServerConnection.h"
#include <wtf/MonotonicTime.h>
#include <wtf/Threading.h>

namespace WebKit {
namespace IPCTestingAPI {
class JSIPCStreamClientConnection;
}
}

namespace IPC {

// A message stream is a half-duplex two-way stream of messages to a between the client and the
// server.
//
// StreamClientConnection can send messages and receive synchronous replies
// through this message stream or through IPC::Connection.
//
// The server will receive messages in order _for the destination messages_.
// The whole IPC::Connection message order is not preserved.
//
// The StreamClientConnection trusts the StreamServerConnection.
class StreamClientConnection final : public ThreadSafeRefCounted<StreamClientConnection> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(StreamClientConnection);
public:
    struct StreamConnectionPair {
        RefPtr<StreamClientConnection> streamConnection;
        IPC::StreamServerConnection::Handle connectionHandle;
    };

    // The messages from the server are delivered to the caller through the passed IPC::MessageReceiver.
    static StreamConnectionPair create(unsigned bufferSizeLog2);

    ~StreamClientConnection();

    void setSemaphores(IPC::Semaphore&& wakeUp, IPC::Semaphore&& clientWait);
    bool hasSemaphores() const;
    void setMaxBatchSize(unsigned);

    void open(Connection::Client&, SerialFunctionDispatcher& = RunLoop::current());
    void invalidate();

    template<typename T, typename U> bool send(T&& message, ObjectIdentifier<U> destinationID, Timeout);

    using AsyncReplyID = Connection::AsyncReplyID;
    template<typename T, typename C, typename U>
    AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, ObjectIdentifier<U> destinationID, Timeout);

    template<typename T> using SendSyncResult = Connection::SendSyncResult<T>;
    template<typename T, typename U>
    SendSyncResult<T> sendSync(T&& message, ObjectIdentifier<U> destinationID, Timeout);

    template<typename T, typename U>
    bool waitForAndDispatchImmediately(ObjectIdentifier<U> destinationID, Timeout, OptionSet<WaitForOption> = { });

    StreamClientConnectionBuffer& bufferForTesting();
    Connection& connectionForTesting();

private:
    StreamClientConnection(Ref<Connection>, unsigned bufferSizeLog2);

    template<typename T, typename... AdditionalData>
    bool trySendStream(Span<uint8_t>&, T& message, AdditionalData&&...);
    template<typename T>
    std::optional<SendSyncResult<T>> trySendSyncStream(T& message, Timeout, Span<uint8_t>&);
    bool trySendDestinationIDIfNeeded(uint64_t destinationID, Timeout);
    void sendProcessOutOfStreamMessage(Span<uint8_t>&&);
    using WakeUpServer = StreamClientConnectionBuffer::WakeUpServer;
    void wakeUpServerBatched(WakeUpServer);
    void wakeUpServer(WakeUpServer);

    Ref<Connection> m_connection;
    class DedicatedConnectionClient final : public Connection::Client {
        WTF_MAKE_NONCOPYABLE(DedicatedConnectionClient);
    public:
        DedicatedConnectionClient(Connection::Client&);
        // Connection::Client overrides.
        void didReceiveMessage(Connection&, Decoder&) final;
        bool didReceiveSyncMessage(Connection&, Decoder&, UniqueRef<Encoder>&) final;
        void didClose(Connection&) final;
        void didReceiveInvalidMessage(Connection&, MessageName) final;
    private:
        Connection::Client& m_receiver;
    };
    std::optional<DedicatedConnectionClient> m_dedicatedConnectionClient;
    uint64_t m_currentDestinationID { 0 };
    StreamClientConnectionBuffer m_buffer;
    unsigned m_maxBatchSize { 20 }; // Number of messages marked as StreamBatched to accumulate before notifying the server.
    unsigned m_batchSize { 0 };

    friend class WebKit::IPCTestingAPI::JSIPCStreamClientConnection;
};

template<typename T, typename U>
bool StreamClientConnection::send(T&& message, ObjectIdentifier<U> destinationID, Timeout timeout)
{
    static_assert(!T::isSync, "Message is sync!");
    if (!trySendDestinationIDIfNeeded(destinationID.toUInt64(), timeout))
        return false;
    auto span = m_buffer.tryAcquire(timeout);
    if (!span)
        return false;
    if constexpr(T::isStreamEncodable) {
        if (trySendStream(*span, message))
            return true;
    }
    sendProcessOutOfStreamMessage(WTFMove(*span));
    if (!m_connection->send(WTFMove(message), destinationID, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply))
        return false;
    return true;
}

template<typename T, typename C, typename U>
StreamClientConnection::AsyncReplyID StreamClientConnection::sendWithAsyncReply(T&& message, C&& completionHandler, ObjectIdentifier<U> destinationID, Timeout timeout)
{
    static_assert(!T::isSync, "Message is sync!");
    if (!trySendDestinationIDIfNeeded(destinationID.toUInt64(), timeout))
        return { };

    auto span = m_buffer.tryAcquire(timeout);
    if (!span)
        return { };
    auto handler = Connection::makeAsyncReplyHandler<T>(WTFMove(completionHandler));
    auto replyID = handler.replyID;
    m_connection->addAsyncReplyHandler(WTFMove(handler));
    if constexpr(T::isStreamEncodable) {
        if (trySendStream(*span, message, replyID))
            return replyID;
    }
    sendProcessOutOfStreamMessage(WTFMove(*span));
    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID.toUInt64());
    encoder.get() << message.arguments() << replyID;
    if (m_connection->sendMessage(WTFMove(encoder), IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply, { }))
        return replyID;
    // replyHandlerToCancel might be already cancelled if invalidate() happened in-between.
    if (auto replyHandlerToCancel = m_connection->takeAsyncReplyHandler(replyID)) {
        // FIXME(https://bugs.webkit.org/show_bug.cgi?id=248947): Current contract is that completionHandler
        // is called on the connection run loop.
        // This does not make sense. However, this needs a change that is done later.
        RunLoop::main().dispatch([completionHandler = WTFMove(replyHandlerToCancel)]() mutable {
            completionHandler(nullptr);
        });
    }
    return { };
}

template<typename T, typename... AdditionalData>
bool StreamClientConnection::trySendStream(Span<uint8_t>& span, T& message, AdditionalData&&... args)
{
    StreamConnectionEncoder messageEncoder { T::name(), span.data(), span.size() };
    if (((messageEncoder << message.arguments()) << ... << std::forward<decltype(args)>(args))) {
        auto wakeUpResult = m_buffer.release(messageEncoder.size());
        if constexpr(T::isStreamBatched)
            wakeUpServerBatched(wakeUpResult);
        else
            wakeUpServer(wakeUpResult);
        return true;
    }
    return false;
}

template<typename T, typename U>
StreamClientConnection::SendSyncResult<T> StreamClientConnection::sendSync(T&& message, ObjectIdentifier<U> destinationID, Timeout timeout)
{
    static_assert(T::isSync, "Message is not sync!");
    if (!trySendDestinationIDIfNeeded(destinationID.toUInt64(), timeout))
        return { };
    auto span = m_buffer.tryAcquire(timeout);
    if (!span)
        return { };
    if constexpr(T::isStreamEncodable) {
        auto maybeSendResult = trySendSyncStream(message, timeout, *span);
        if (maybeSendResult)
            return WTFMove(*maybeSendResult);
    }
    sendProcessOutOfStreamMessage(WTFMove(*span));
    return m_connection->sendSync(WTFMove(message), destinationID.toUInt64(), timeout);
}

template<typename T, typename U>
bool StreamClientConnection::waitForAndDispatchImmediately(ObjectIdentifier<U> destinationID, Timeout timeout, OptionSet<WaitForOption> waitForOptions)
{
    return m_connection->waitForAndDispatchImmediately<T>(destinationID, timeout, waitForOptions);
}

template<typename T>
std::optional<StreamClientConnection::SendSyncResult<T>> StreamClientConnection::trySendSyncStream(T& message, Timeout timeout, Span<uint8_t>& span)
{
    // In this function, SendSyncResult<T> { } means error happened and caller should stop processing.
    // std::nullopt means we couldn't send through the stream, so try sending out of stream.
    auto syncRequestID = m_connection->makeSyncRequestID();
    if (!m_connection->pushPendingSyncRequestID(syncRequestID))
        return SendSyncResult<T> { };

    auto decoderResult = [&]() -> std::optional<std::unique_ptr<Decoder>> {
        StreamConnectionEncoder messageEncoder { T::name(), span.data(), span.size() };
        if (!(messageEncoder << syncRequestID << message.arguments()))
            return std::nullopt;
        auto wakeUpResult = m_buffer.release(messageEncoder.size());
        wakeUpServer(wakeUpResult);
        if constexpr(T::isReplyStreamEncodable) {
            auto replySpan = m_buffer.tryAcquireAll(timeout);
            if (!replySpan)
                return std::unique_ptr<Decoder> { };
            auto decoder = std::unique_ptr<Decoder> { new Decoder(replySpan->data(), replySpan->size(), m_currentDestinationID) };
            if (decoder->messageName() != MessageName::ProcessOutOfStreamMessage) {
                ASSERT(decoder->messageName() == MessageName::SyncMessageReply);
                return decoder;
            }
        } else
            m_buffer.resetClientOffset();

        return m_connection->waitForSyncReply(syncRequestID, T::name(), timeout, { });
    }();
    m_connection->popPendingSyncRequestID(syncRequestID);

    if (!decoderResult)
        return std::nullopt;

    SendSyncResult<T> result;
    if (*decoderResult) {
        auto& decoder = *decoderResult;
        *decoder >> result.replyArguments;
        if (result.replyArguments)
            result.decoder = WTFMove(decoder);
    }
    return result;
}

inline bool StreamClientConnection::trySendDestinationIDIfNeeded(uint64_t destinationID, Timeout timeout)
{
    if (destinationID == m_currentDestinationID)
        return true;
    auto span = m_buffer.tryAcquire(timeout);
    if (!span)
        return false;
    StreamConnectionEncoder encoder { MessageName::SetStreamDestinationID, span->data(), span->size() };
    if (!(encoder << destinationID)) {
        ASSERT_NOT_REACHED(); // Size of the minimum allocation is incorrect. Likely an alignment issue.
        return false;
    }
    auto wakeUpResult = m_buffer.release(encoder.size());
    wakeUpServer(wakeUpResult);
    m_currentDestinationID = destinationID;
    return true;
}

inline void StreamClientConnection::sendProcessOutOfStreamMessage(Span<uint8_t>&& span)
{
    StreamConnectionEncoder encoder { MessageName::ProcessOutOfStreamMessage, span.data(), span.size() };
    // Not notifying on wake up since the out-of-stream message will do that.
    auto result = m_buffer.release(encoder.size());
    UNUSED_VARIABLE(result);
    m_batchSize = 0;
}

}
