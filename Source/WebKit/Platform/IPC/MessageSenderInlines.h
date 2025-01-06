/**
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#include "MessageSender.h"

namespace IPC {

template<typename MessageType> inline bool MessageSender::send(MessageType&& message, uint64_t destinationID, OptionSet<SendOption> options)
{
    static_assert(!MessageType::isSync);
    auto encoder = makeUniqueRef<Encoder>(MessageType::name(), destinationID);
    encoder.get() << std::forward<MessageType>(message).arguments();
    return sendMessage(WTFMove(encoder), options);
}

template<typename MessageType> inline auto MessageSender::sendSync(MessageType&& message, uint64_t destinationID, Timeout timeout, OptionSet<SendSyncOption> options) -> SendSyncResult<MessageType>
{
    static_assert(MessageType::isSync);
    if (auto* connection = messageSenderConnection())
        return connection->sendSync(std::forward<MessageType>(message), destinationID, timeout, options);
    return { Error::NoMessageSenderConnection };
}

template<typename MessageType, typename C> inline std::optional<AsyncReplyID> MessageSender::sendWithAsyncReply(MessageType&& message, C&& completionHandler, uint64_t destinationID, OptionSet<SendOption> options)
{
    static_assert(!MessageType::isSync);
    auto encoder = makeUniqueRef<IPC::Encoder>(MessageType::name(), destinationID);
    encoder.get() << std::forward<MessageType>(message).arguments();
    auto asyncHandler = Connection::makeAsyncReplyHandler<MessageType>(std::forward<C>(completionHandler));
    auto replyID = asyncHandler.replyID;
    if (sendMessageWithAsyncReply(WTFMove(encoder), WTFMove(asyncHandler), options))
        return replyID;
    return std::nullopt;
}

template<typename MessageType> inline bool MessageSender::sendWithoutUsingIPCConnection(MessageType&& message) const
{
    static_assert(!MessageType::isSync);
    auto encoder = makeUniqueRef<IPC::Encoder>(MessageType::name(), messageSenderDestinationID());
    encoder.get() << std::forward<MessageType>(message).arguments();

    return performSendWithoutUsingIPCConnection(WTFMove(encoder));
}

template<typename MessageType, typename C> inline bool MessageSender::sendWithAsyncReplyWithoutUsingIPCConnection(MessageType&& message, C&& completionHandler) const
{
    static_assert(!MessageType::isSync);
    auto encoder = makeUniqueRef<IPC::Encoder>(MessageType::name(), messageSenderDestinationID());
    encoder.get() << std::forward<MessageType>(message).arguments();

    auto asyncHandler = [completionHandler = std::forward<C>(completionHandler)] (Decoder* decoder) mutable {
        if (decoder && decoder->isValid())
            Connection::callReply<MessageType>(*decoder, WTFMove(completionHandler));
        else
            Connection::cancelReply<MessageType>(WTFMove(completionHandler));
    };

    return performSendWithAsyncReplyWithoutUsingIPCConnection(WTFMove(encoder), WTFMove(asyncHandler));
}

template<typename MessageType> inline bool MessageSender::send(MessageType&& message)
{
    return send(std::forward<MessageType>(message), messageSenderDestinationID(), { });
}

template<typename MessageType> inline bool MessageSender::send(MessageType&& message, OptionSet<SendOption> options)
{
    return send(std::forward<MessageType>(message), messageSenderDestinationID(), options);
}

template<typename MessageType> inline bool MessageSender::send(MessageType&& message, uint64_t destinationID)
{
    return send(std::forward<MessageType>(message), destinationID, { });
}

template<typename MessageType, typename U, typename V, typename W> inline bool MessageSender::send(MessageType&& message, ObjectIdentifierGeneric<U, V, W> destinationID)
{
    return send(std::forward<MessageType>(message), destinationID.toUInt64(), { });
}

template<typename MessageType, typename U, typename V, typename W> inline bool MessageSender::send(MessageType&& message, ObjectIdentifierGeneric<U, V, W> destinationID, OptionSet<SendOption> options)
{
    return send(std::forward<MessageType>(message), destinationID.toUInt64(), options);
}

template<typename MessageType> inline auto MessageSender::sendSync(MessageType&& message) -> SendSyncResult<MessageType>
{
    return sendSync(std::forward<MessageType>(message), messageSenderDestinationID(), Timeout::infinity(), { });
}

template<typename MessageType> inline auto MessageSender::sendSync(MessageType&& message, Timeout timeout) -> SendSyncResult<MessageType>
{
    return sendSync(std::forward<MessageType>(message), messageSenderDestinationID(), timeout, { });
}

template<typename MessageType> inline auto MessageSender::sendSync(MessageType&& message, Timeout timeout, OptionSet<SendSyncOption> options) -> SendSyncResult<MessageType>
{
    return sendSync(std::forward<MessageType>(message), messageSenderDestinationID(), timeout, options);
}

template<typename MessageType, typename U, typename V, typename W> inline auto MessageSender::sendSync(MessageType&& message, ObjectIdentifierGeneric<U, V, W> destinationID) -> SendSyncResult<MessageType>
{
    return sendSync(std::forward<MessageType>(message), destinationID.toUInt64(), Timeout::infinity(), { });
}

template<typename MessageType, typename U, typename V, typename W> inline auto MessageSender::sendSync(MessageType&& message, ObjectIdentifierGeneric<U, V, W> destinationID, Timeout timeout, OptionSet<SendSyncOption> options) -> SendSyncResult<MessageType>
{
    return sendSync(std::forward<MessageType>(message), destinationID.toUInt64(), timeout, options);
}

template<typename MessageType, typename C> inline std::optional<AsyncReplyID> MessageSender::sendWithAsyncReply(MessageType&& message, C&& completionHandler)
{
    return sendWithAsyncReply(std::forward<MessageType>(message), std::forward<C>(completionHandler), messageSenderDestinationID(), { });
}

template<typename MessageType, typename C> inline std::optional<AsyncReplyID> MessageSender::sendWithAsyncReply(MessageType&& message, C&& completionHandler, OptionSet<SendOption> options)
{
    return sendWithAsyncReply(std::forward<MessageType>(message), std::forward<C>(completionHandler), messageSenderDestinationID(), options);
}

template<typename MessageType, typename C> inline std::optional<AsyncReplyID> MessageSender::sendWithAsyncReply(MessageType&& message, C&& completionHandler, uint64_t destinationID)
{
    return sendWithAsyncReply(std::forward<MessageType>(message), std::forward<C>(completionHandler), destinationID, { });
}

template<typename MessageType, typename C, typename U, typename V, typename W> inline std::optional<AsyncReplyID> MessageSender::sendWithAsyncReply(MessageType&& message, C&& completionHandler, ObjectIdentifierGeneric<U, V, W> destinationID)
{
    return sendWithAsyncReply(std::forward<MessageType>(message), std::forward<C>(completionHandler), destinationID.toUInt64(), { });
}

template<typename MessageType, typename C, typename U, typename V, typename W> inline std::optional<AsyncReplyID> MessageSender::sendWithAsyncReply(MessageType&& message, C&& completionHandler, ObjectIdentifierGeneric<U, V, W> destinationID, OptionSet<SendOption> options)
{
    return sendWithAsyncReply(std::forward<MessageType>(message), std::forward<C>(completionHandler), destinationID.toUInt64(), options);
}

template<typename MessageType> Ref<typename MessageType::Promise> inline MessageSender::sendWithPromisedReply(MessageType&& message)
{
    return sendWithPromisedReply(std::forward<MessageType>(message), messageSenderDestinationID(), { });
}

template<typename MessageType> Ref<typename MessageType::Promise> inline MessageSender::sendWithPromisedReply(MessageType&& message, uint64_t destinationID)
{
    return sendWithPromisedReply(std::forward<MessageType>(message), destinationID, { });
}

template<typename MessageType> Ref<typename MessageType::Promise> inline MessageSender::sendWithPromisedReply(MessageType&& message, uint64_t destinationID, OptionSet<SendOption> options)
{
    static_assert(!MessageType::isSync);
    if (RefPtr connection = messageSenderConnection())
        return connection->sendWithPromisedReply<Connection::NoOpPromiseConverter, MessageType>(std::forward<MessageType>(message), destinationID, options);
    return MessageType::Promise::createAndReject(Error::NoMessageSenderConnection);
}

} // namespace IPC
