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

    template<typename U> bool send(const U& message, OptionSet<SendOption> sendOptions = { })
    {
        return send(message, messageSenderDestinationID(), sendOptions);
    }

    template<typename U> bool send(const U& message, uint64_t destinationID, OptionSet<SendOption> sendOptions = { })
    {
        static_assert(!U::isSync, "Message is sync!");

        auto encoder = makeUniqueRef<Encoder>(U::name(), destinationID);
        encoder.get() << message.arguments();
        
        return sendMessage(WTFMove(encoder), sendOptions);
    }
    
    template<typename U, typename T>
    bool send(const U& message, ObjectIdentifier<T> destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return send<U>(message, destinationID.toUInt64(), sendOptions);
    }
    using SendSyncResult = Connection::SendSyncResult;

    template<typename T>
    SendSyncResult sendSync(T&& message, typename T::Reply&& reply, Timeout timeout = Seconds::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        static_assert(T::isSync, "Message is not sync!");

        return sendSync(std::forward<T>(message), WTFMove(reply), messageSenderDestinationID(), timeout, sendSyncOptions);
    }

    template<typename T>
    SendSyncResult sendSync(T&& message, typename T::Reply&& reply, uint64_t destinationID, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        if (auto* connection = messageSenderConnection())
            return connection->sendSync(WTFMove(message), WTFMove(reply), destinationID, timeout, sendSyncOptions);

        return { };
    }

    template<typename U, typename T>
    SendSyncResult sendSync(U&& message, typename U::Reply&& reply, ObjectIdentifier<T> destinationID, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        return sendSync<U>(std::forward<U>(message), WTFMove(reply), destinationID.toUInt64(), timeout, sendSyncOptions);
    }

    template<typename T, typename C>
    uint64_t sendWithAsyncReply(T&& message, C&& completionHandler, OptionSet<SendOption> sendOptions = { })
    {
        return sendWithAsyncReply(WTFMove(message), WTFMove(completionHandler), messageSenderDestinationID(), sendOptions);
    }

    template<typename T, typename C>
    uint64_t sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID, OptionSet<SendOption> sendOptions = { })
    {
        COMPILE_ASSERT(!T::isSync, AsyncMessageExpected);

        auto encoder = makeUniqueRef<IPC::Encoder>(T::name(), destinationID);
        uint64_t listenerID = IPC::nextAsyncReplyHandlerID();
        encoder.get() << listenerID;
        encoder.get() << message.arguments();
        sendMessage(WTFMove(encoder), sendOptions, {{ [completionHandler = WTFMove(completionHandler)] (IPC::Decoder* decoder) mutable {
            if (decoder && decoder->isValid())
                T::callReply(*decoder, WTFMove(completionHandler));
            else
                T::cancelReply(WTFMove(completionHandler));
        }, listenerID }});
        return listenerID;
    }

    virtual bool sendMessage(UniqueRef<Encoder>&&, OptionSet<SendOption>, std::optional<std::pair<CompletionHandler<void(IPC::Decoder*)>, uint64_t>>&& = std::nullopt);

private:
    virtual Connection* messageSenderConnection() const = 0;
    virtual uint64_t messageSenderDestinationID() const = 0;
};

} // namespace IPC
