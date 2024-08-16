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
#include <wtf/Scope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMalloc.h>
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
    WTF_MAKE_TZONE_ALLOCATED(StreamClientConnection);
    WTF_MAKE_NONCOPYABLE(StreamClientConnection);
public:
    struct StreamConnectionPair {
        Ref<StreamClientConnection> streamConnection;
        IPC::StreamServerConnection::Handle connectionHandle;
    };

    // The messages from the server are delivered to the caller through the passed IPC::MessageReceiver.
    static std::optional<StreamConnectionPair> create(unsigned bufferSizeLog2, Seconds defaultTimeoutDuration);

    ~StreamClientConnection();

    void setSemaphores(IPC::Semaphore&& wakeUp, IPC::Semaphore&& clientWait);
    bool hasSemaphores() const;
    void setMaxBatchSize(unsigned);

    void open(Connection::Client&, SerialFunctionDispatcher& = RunLoop::current());
    Error flushSentMessages();
    void invalidate();

    template<typename T, typename U, typename V, typename W, SupportsObjectIdentifierNullState supportsNullState>
    Error send(T&& message, ObjectIdentifierGeneric<U, V, W, supportsNullState> destinationID);
    using AsyncReplyID = Connection::AsyncReplyID;
    template<typename T, typename C, typename U, typename V, typename W, SupportsObjectIdentifierNullState supportsNullState>
    AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, ObjectIdentifierGeneric<U, V, W, supportsNullState> destinationID);

    template<typename T>
    using SendSyncResult = Connection::SendSyncResult<T>;
    template<typename T, typename U, typename V, typename W, SupportsObjectIdentifierNullState supportsNullState>
    SendSyncResult<T> sendSync(T&& message, ObjectIdentifierGeneric<U, V, W, supportsNullState> destinationID);
    template<typename T, typename U, typename V, typename W, SupportsObjectIdentifierNullState supportsNullState>
    Error waitForAndDispatchImmediately(ObjectIdentifierGeneric<U, V, W, supportsNullState> destinationID, OptionSet<WaitForOption> = { });
    template<typename>
    Error waitForAsyncReplyAndDispatchImmediately(AsyncReplyID);

    void addWorkQueueMessageReceiver(ReceiverName, WorkQueue&, WorkQueueMessageReceiver&, uint64_t destinationID = 0);
    void removeWorkQueueMessageReceiver(ReceiverName, uint64_t destinationID = 0);

    StreamClientConnectionBuffer& bufferForTesting();
    Connection& connectionForTesting();

    // Returns the timeout moment for current time.
    Timeout defaultTimeout() const { return m_defaultTimeoutDuration; }

    // Returns the timeout duration. Useful for waiting for consistent per-connection amounts with other APIs
    // used in conjunction with the connection.
    Seconds defaultTimeoutDuration() const { return m_defaultTimeoutDuration; }
private:
    StreamClientConnection(Ref<Connection>, StreamClientConnectionBuffer&&, Seconds defaultTimeoutDuration);

    template<typename T, typename... AdditionalData>
    bool trySendStream(std::span<uint8_t>, T& message, AdditionalData&&...);
    template<typename T>
    std::optional<SendSyncResult<T>> trySendSyncStream(T& message, Timeout, std::span<uint8_t>);
    Error trySendDestinationIDIfNeeded(uint64_t destinationID, Timeout);
    void sendProcessOutOfStreamMessage(std::span<uint8_t>);
    using WakeUpServer = StreamClientConnectionBuffer::WakeUpServer;
    void wakeUpServerBatched(WakeUpServer);
    void wakeUpServer(WakeUpServer);
    Ref<Connection> protectedConnection() const { return m_connection; }

    Ref<Connection> m_connection;
    class DedicatedConnectionClient final : public Connection::Client {
        WTF_MAKE_NONCOPYABLE(DedicatedConnectionClient);
    public:
        DedicatedConnectionClient(Connection::Client&);
        // Connection::Client overrides.
        void didReceiveMessage(Connection&, Decoder&) final;
        bool didReceiveSyncMessage(Connection&, Decoder&, UniqueRef<Encoder>&) final;
        void didClose(Connection&) final;
        void didReceiveInvalidMessage(Connection&, MessageName, int32_t indexOfObjectFailingDecoding) final;
    private:
        Connection::Client& m_receiver;
    };
    std::optional<DedicatedConnectionClient> m_dedicatedConnectionClient;
    uint64_t m_currentDestinationID { 0 };
    StreamClientConnectionBuffer m_buffer;
    unsigned m_maxBatchSize { 20 }; // Number of messages marked as StreamBatched to accumulate before notifying the server.
    unsigned m_batchSize { 0 };
    const Seconds m_defaultTimeoutDuration;

    friend class WebKit::IPCTestingAPI::JSIPCStreamClientConnection;
};

template<typename T, typename U, typename V, typename W, SupportsObjectIdentifierNullState supportsNullState>
Error StreamClientConnection::send(T&& message, ObjectIdentifierGeneric<U, V, W, supportsNullState> destinationID)
{
#if ENABLE(CORE_IPC_SIGNPOSTS)
    auto signpostIdentifier = Connection::generateSignpostIdentifier();
    WTFBeginSignpost(signpostIdentifier, StreamClientConnection, "send: %{public}s", description(message.name()).characters());
    auto endSignpost = makeScopeExit([&] {
        WTFEndSignpost(signpostIdentifier, StreamClientConnection);
    });
#endif

    static_assert(!T::isSync, "Message is sync!");
    Timeout timeout = defaultTimeout();
    auto error = trySendDestinationIDIfNeeded(destinationID.toUInt64(), timeout);
    if (error != Error::NoError)
        return error;

    auto span = m_buffer.tryAcquire(timeout);
    if (!span)
        return Error::FailedToAcquireBufferSpan;
    if constexpr (T::isStreamEncodable) {
        if (trySendStream(*span, message))
            return Error::NoError;
    }
    sendProcessOutOfStreamMessage(WTFMove(*span));
    return protectedConnection()->send(std::forward<T>(message), destinationID, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

template<typename T, typename C, typename U, typename V, typename W, SupportsObjectIdentifierNullState supportsNullState>
StreamClientConnection::AsyncReplyID StreamClientConnection::sendWithAsyncReply(T&& message, C&& completionHandler, ObjectIdentifierGeneric<U, V, W, supportsNullState> destinationID)
{
#if ENABLE(CORE_IPC_SIGNPOSTS)
    auto signpostIdentifier = Connection::generateSignpostIdentifier();
    WTFBeginSignpost(signpostIdentifier, StreamClientConnection, "sendWithAsyncReply: %{public}s", description(message.name()).characters());
#endif

    static_assert(!T::isSync, "Message is sync!");
    Timeout timeout = defaultTimeout();
    auto error = trySendDestinationIDIfNeeded(destinationID.toUInt64(), timeout);
    if (error != Error::NoError)
        return { }; // FIXME: Propagate errors.

    auto span = m_buffer.tryAcquire(timeout);
    if (!span)
        return { }; // FIXME: Propagate errors.

    Ref connection = m_connection;
    auto handler = Connection::makeAsyncReplyHandler<T>(std::forward<C>(completionHandler));
    auto replyID = handler.replyID;
#if ENABLE(CORE_IPC_SIGNPOSTS)
    handler.completionHandler = CompletionHandler<void(Decoder*)>([signpostIdentifier, handler = WTFMove(handler.completionHandler)](Decoder* decoder) mutable {
        WTFEndSignpost(signpostIdentifier, StreamClientConnection);
        handler(decoder);
    });
#endif
    connection->addAsyncReplyHandler(WTFMove(handler));

    if constexpr (T::isStreamEncodable) {
        if (trySendStream(*span, message, replyID))
            return replyID;
    }

    sendProcessOutOfStreamMessage(WTFMove(*span));
    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID.toUInt64());
    encoder.get() << message.arguments() << replyID;
    if (connection->sendMessage(WTFMove(encoder), IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply, { }) == Error::NoError)
        return replyID;

    // replyHandlerToCancel might be already cancelled if invalidate() happened in-between.
    if (auto replyHandlerToCancel = connection->takeAsyncReplyHandler(replyID)) {
        // FIXME(https://bugs.webkit.org/show_bug.cgi?id=248947): Current contract is that completionHandler
        // is called on the connection run loop.
        // This does not make sense. However, this needs a change that is done later.
        RunLoop::protectedMain()->dispatch([completionHandler = WTFMove(replyHandlerToCancel)]() mutable {
            completionHandler(nullptr);
        });
    }
    return { };
}

template<typename T, typename... AdditionalData>
bool StreamClientConnection::trySendStream(std::span<uint8_t> span, T& message, AdditionalData&&... args)
{
    StreamConnectionEncoder messageEncoder { T::name(), span.data(), span.size() };
    if (((messageEncoder << message.arguments()) << ... << std::forward<decltype(args)>(args))) {
        auto wakeUpResult = m_buffer.release(messageEncoder.size());
        if constexpr (T::isStreamBatched)
            wakeUpServerBatched(wakeUpResult);
        else
            wakeUpServer(wakeUpResult);
        return true;
    }
    return false;
}

template<typename T, typename U, typename V, typename W, SupportsObjectIdentifierNullState supportsNullState>
StreamClientConnection::SendSyncResult<T> StreamClientConnection::sendSync(T&& message, ObjectIdentifierGeneric<U, V, W, supportsNullState> destinationID)
{
#if ENABLE(CORE_IPC_SIGNPOSTS)
    auto signpostIdentifier = Connection::generateSignpostIdentifier();
    WTFBeginSignpost(signpostIdentifier, StreamClientConnection, "sendSync: %{public}s", description(message.name()).characters());
    auto endSignpost = makeScopeExit([&] {
        WTFEndSignpost(signpostIdentifier, StreamClientConnection);
    });
#endif

    static_assert(T::isSync, "Message is not sync!");
    Timeout timeout = defaultTimeout();
    auto error = trySendDestinationIDIfNeeded(destinationID.toUInt64(), timeout);
    if (error != Error::NoError)
        return { error };

    auto span = m_buffer.tryAcquire(timeout);
    if (!span)
        return { Error::FailedToAcquireBufferSpan };

    if constexpr (T::isStreamEncodable) {
        auto maybeSendResult = trySendSyncStream(message, timeout, *span);
        if (maybeSendResult)
            return WTFMove(*maybeSendResult);
    }
    sendProcessOutOfStreamMessage(WTFMove(*span));
    return protectedConnection()->sendSync(std::forward<T>(message), destinationID.toUInt64(), timeout);
}

template<typename T, typename U, typename V, typename W, SupportsObjectIdentifierNullState supportsNullState>
Error StreamClientConnection::waitForAndDispatchImmediately(ObjectIdentifierGeneric<U, V, W, supportsNullState> destinationID, OptionSet<WaitForOption> waitForOptions)
{
    Timeout timeout = defaultTimeout();
    return protectedConnection()->waitForAndDispatchImmediately<T>(destinationID, timeout, waitForOptions);
}

template<typename T>
Error StreamClientConnection::waitForAsyncReplyAndDispatchImmediately(AsyncReplyID replyID)
{
    Timeout timeout = defaultTimeout();
    return protectedConnection()->waitForAsyncReplyAndDispatchImmediately<T>(replyID, timeout);
}

template<typename T>
std::optional<StreamClientConnection::SendSyncResult<T>> StreamClientConnection::trySendSyncStream(T& message, Timeout timeout, std::span<uint8_t> span)
{
    Ref connection = m_connection;
    // In this function, SendSyncResult<T> { } means error happened and caller should stop processing.
    // std::nullopt means we couldn't send through the stream, so try sending out of stream.
    auto syncRequestID = connection->makeSyncRequestID();
    if (!connection->pushPendingSyncRequestID(syncRequestID))
        return { { Error::CantWaitForSyncReplies } };

    auto decoderResult = [&]() -> std::optional<Connection::DecoderOrError> {
        StreamConnectionEncoder messageEncoder { T::name(), span.data(), span.size() };
        if (!(messageEncoder << syncRequestID << message.arguments()))
            return std::nullopt;

        auto wakeUpResult = m_buffer.release(messageEncoder.size());
        wakeUpServer(wakeUpResult);
        if constexpr (T::isReplyStreamEncodable) {
            auto replySpan = m_buffer.tryAcquireAll(timeout);
            if (!replySpan)
                return makeUnexpected(Error::FailedToAcquireReplyBufferSpan);

            auto decoder = makeUniqueRef<Decoder>(*replySpan, m_currentDestinationID);
            if (decoder->messageName() != MessageName::ProcessOutOfStreamMessage) {
                ASSERT(decoder->messageName() == MessageName::SyncMessageReply);
                return decoder;
            }
        } else
            m_buffer.resetClientOffset();

        return connection->waitForSyncReply(syncRequestID, T::name(), timeout, { });
    }();
    connection->popPendingSyncRequestID(syncRequestID);

    if (!decoderResult)
        return std::nullopt;

    if (decoderResult->has_value()) {
        std::optional<typename T::ReplyArguments> replyArguments;
        auto& decoder = decoderResult->value();
        *decoder >> replyArguments;
        if (replyArguments)
            return { { WTFMove(decoderResult->value()), WTFMove(*replyArguments) } };
        return { Error::FailedToDecodeReplyArguments };
    }
    return { decoderResult->error() };
}

inline Error StreamClientConnection::trySendDestinationIDIfNeeded(uint64_t destinationID, Timeout timeout)
{
    if (destinationID == m_currentDestinationID)
        return Error::NoError;

    auto span = m_buffer.tryAcquire(timeout);
    if (!span)
        return Error::FailedToAcquireBufferSpan;

    StreamConnectionEncoder encoder { MessageName::SetStreamDestinationID, span->data(), span->size() };
    if (!(encoder << destinationID)) {
        ASSERT_NOT_REACHED(); // Size of the minimum allocation is incorrect. Likely an alignment issue.
        return Error::StreamConnectionEncodingError;
    }
    auto wakeUpResult = m_buffer.release(encoder.size());
    wakeUpServerBatched(wakeUpResult);
    m_currentDestinationID = destinationID;
    return Error::NoError;
}

inline void StreamClientConnection::sendProcessOutOfStreamMessage(std::span<uint8_t> span)
{
    StreamConnectionEncoder encoder { MessageName::ProcessOutOfStreamMessage, span.data(), span.size() };
    // Not notifying on wake up since the out-of-stream message will do that.
    auto result = m_buffer.release(encoder.size());
    UNUSED_VARIABLE(result);
    m_batchSize = 0;
}

}
