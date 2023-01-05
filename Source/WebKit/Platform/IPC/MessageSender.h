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
#include <wtf/Int128.h>
#include <wtf/UniqueRef.h>

namespace IPC {

class MessageSender {
public:
    virtual ~MessageSender();

    template<typename T> bool send(T&& message, OptionSet<SendOption> sendOptions = { })
    {
        return send(WTFMove(message), messageSenderDestinationID(), sendOptions);
    }

    template<typename T> bool send(T&& message, UInt128 destinationID, OptionSet<SendOption> sendOptions = { })
    {
        static_assert(!T::isSync, "Message is sync!");

        auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID);
        encoder.get() << message.arguments();
        return sendMessage(WTFMove(encoder), sendOptions);
    }

    template<typename T, typename U>
    bool send(T&& message, ObjectIdentifier<U> destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return send(WTFMove(message), destinationID.toUInt64(), sendOptions);
    }

    template<typename T> using SendSyncResult = Connection::SendSyncResult<T>;

    template<typename T>
    SendSyncResult<T> sendSync(T&& message, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        static_assert(T::isSync, "Message is not sync!");

        return sendSync(std::forward<T>(message), messageSenderDestinationID(), timeout, sendSyncOptions);
    }

    template<typename T>
    SendSyncResult<T> sendSync(T&& message, UInt128 destinationID, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        if (auto* connection = messageSenderConnection())
            return connection->sendSync(WTFMove(message), destinationID, timeout, sendSyncOptions);

        return { };
    }

    template<typename T, typename U>
    SendSyncResult<T> sendSync(T&& message, ObjectIdentifier<U> destinationID, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        return sendSync<T>(std::forward<T>(message), destinationID.toUInt64(), timeout, sendSyncOptions);
    }

    using AsyncReplyID = Connection::AsyncReplyID;

    template<typename T, typename C>
    AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, OptionSet<SendOption> sendOptions = { })
    {
        return sendWithAsyncReply(WTFMove(message), WTFMove(completionHandler), messageSenderDestinationID(), sendOptions);
    }

    template<typename T, typename C>
    AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, UInt128 destinationID, OptionSet<SendOption> sendOptions = { })
    {
        static_assert(!T::isSync, "Async message expected");

        auto encoder = makeUniqueRef<IPC::Encoder>(T::name(), destinationID);
        encoder.get() << WTFMove(message).arguments();
        auto asyncHandler = Connection::makeAsyncReplyHandler<T>(WTFMove(completionHandler));
        auto replyID = asyncHandler.replyID;
        if (sendMessageWithAsyncReply(WTFMove(encoder), WTFMove(asyncHandler), sendOptions))
            return replyID;
        return { };
    }

    virtual bool sendMessage(UniqueRef<Encoder>&&, OptionSet<SendOption>);
    using AsyncReplyHandler = Connection::AsyncReplyHandler;
    virtual bool sendMessageWithAsyncReply(UniqueRef<Encoder>&&, AsyncReplyHandler, OptionSet<SendOption>);

private:
    virtual Connection* messageSenderConnection() const = 0;
    virtual uint64_t messageSenderDestinationID() const = 0;
};

} // namespace IPC
