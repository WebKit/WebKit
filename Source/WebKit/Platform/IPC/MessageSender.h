/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include "Connection.h"
#include <wtf/UniqueRef.h>

namespace IPC {

class MessageSender {
public:
    virtual ~MessageSender();

    template<typename T, typename... ArgumentTypes>
    bool send(Connection::AsyncMessageOptions messageOptions, ArgumentTypes&&... arguments)
    {
        static_assert(!T::isSync);
        static_assert(std::is_same_v<std::tuple<std::remove_cvref_t<ArgumentTypes>...>, typename T::Arguments>);

        auto encoder = makeUniqueRef<Encoder>(T::name(), messageOptions.destinationID());
        (encoder.get() << ... << std::forward<ArgumentTypes>(arguments));
        return sendMessage(WTFMove(encoder), messageOptions.sendOptions());
    }

    template<typename T>
    auto send(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return std::apply([&](auto&&... arguments) {
            return send<T>({ destinationID, sendOptions }, arguments...);
        }, message.arguments());
    }

    template<typename T, typename U>
    auto send(T&& message, ObjectIdentifier<U> destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return send(WTFMove(message), destinationID.toUInt64(), sendOptions);
    }

    template<typename T>
    auto send(T&& message, OptionSet<SendOption> sendOptions = { })
    {
        return send(WTFMove(message), messageSenderDestinationID(), sendOptions);
    }

    template<typename T> using SendSyncResult = Connection::SendSyncResult<T>;
    template<typename T, typename... ArgumentTypes>
    SendSyncResult<T> sendSync(Connection::SyncMessageOptions messageOptions, ArgumentTypes&&... arguments)
    {
        static_assert(T::isSync);
        static_assert(std::is_same_v<std::tuple<std::remove_cvref_t<ArgumentTypes>...>, typename T::Arguments>);

        if (auto* connection = messageSenderConnection())
            return connection->sendSync<T>(messageOptions, std::forward<ArgumentTypes>(arguments)...);
        return { };
    }

    template<typename T>
    auto sendSync(T&& message, uint64_t destinationID, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        return std::apply([&](auto&&... arguments) {
            return sendSync<T>({ destinationID, timeout, sendSyncOptions }, arguments...);
        }, message.arguments());
    }

    template<typename T, typename U>
    auto sendSync(T&& message, ObjectIdentifier<U> destinationID, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        return sendSync(WTFMove(message), destinationID.toUInt64(), timeout, sendSyncOptions);
    }

    template<typename T>
    auto sendSync(T&& message, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        return sendSync(WTFMove(message), messageSenderDestinationID(), timeout, sendSyncOptions);
    }

    using AsyncReplyID = Connection::AsyncReplyID;
    template<typename T, typename C, typename... ArgumentTypes>
    AsyncReplyID sendWithAsyncReply(Connection::AsyncMessageOptions messageOptions, C&& completionHandler, ArgumentTypes&&... arguments)
    {
        static_assert(!T::isSync);
        static_assert(std::is_same_v<std::tuple<std::remove_cvref_t<ArgumentTypes>...>, typename T::Arguments>);

        auto encoder = makeUniqueRef<Encoder>(T::name(), messageOptions.destinationID());
        (encoder.get() << ... << std::forward<ArgumentTypes>(arguments));
        auto asyncHandler = Connection::makeAsyncReplyHandler<T>(WTFMove(completionHandler));
        auto replyID = asyncHandler.replyID;
        if (sendMessageWithAsyncReply(WTFMove(encoder), WTFMove(asyncHandler), messageOptions.sendOptions()))
            return replyID;
        return { };
    }

    template<typename T, typename C, typename U>
    AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, ObjectIdentifier<U> destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return sendWithAsyncReply(std::forward<T>(message), std::forward<C>(completionHandler), destinationID.toUInt64(), sendOptions);
    }

    template<typename T, typename C>
    auto sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return std::apply([&](auto&&... arguments) {
            return sendWithAsyncReply<T>({ destinationID, sendOptions }, WTFMove(completionHandler), arguments...);
        }, message.arguments());
    }

    template<typename T, typename C>
    auto sendWithAsyncReply(T&& message, C&& completionHandler, OptionSet<SendOption> sendOptions = { })
    {
        return sendWithAsyncReply(WTFMove(message), WTFMove(completionHandler), messageSenderDestinationID(), sendOptions);
    }

    virtual bool sendMessage(UniqueRef<Encoder>&&, OptionSet<SendOption>);
    using AsyncReplyHandler = Connection::AsyncReplyHandler;
    virtual bool sendMessageWithAsyncReply(UniqueRef<Encoder>&&, AsyncReplyHandler, OptionSet<SendOption>);

private:
    virtual Connection* messageSenderConnection() const = 0;
    virtual uint64_t messageSenderDestinationID() const = 0;
};

} // namespace IPC
