/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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
#include "DataReference.h"
#include "Logging.h"
#include "MessageArgumentDescriptions.h"
#include "MessageNames.h"
#include "StreamServerConnection.h"
#include <WebCore/RuntimeApplicationChecks.h>
#include <wtf/CompletionHandler.h>
#include <wtf/ProcessID.h>
#include <wtf/StdLibExtras.h>

namespace IPC {

class Connection;

// IPC message logging. Only enabled in DEBUG builds.
//
// Message argument values appear as "..." if no operator<<(TextStream&) is
// implemented for them.

constexpr unsigned loggingContainerSizeLimit = 200;

#if !LOG_DISABLED
enum class ForReply : bool { No, Yes };

inline TextStream textStreamForLogging(const Connection& connection, MessageName messageName, void* object, ForReply forReply)
{
    TextStream stream(TextStream::LineMode::SingleLine, { }, loggingContainerSizeLimit);
    stream << '[';
#if OS(DARWIN)
    // The remote process ID is not available when the connection was not made
    // for an XPC service, e.g. for the Web -> GPU process connection.
    if (connection.remoteProcessID())
        stream << connection.remoteProcessID() << ' ';
#else
    UNUSED_PARAM(connection);
#endif

    switch (forReply) {
    case ForReply::No:
        stream << "-> " << WebCore::processTypeDescription(WebCore::processType()) << ' ' << getCurrentProcessID() << " receiver " << object << "] " << description(messageName);
        break;
    case ForReply::Yes:
        stream << "<- " << WebCore::processTypeDescription(WebCore::processType()) << ' ' << getCurrentProcessID() << "] " << description(messageName) << " Reply";
        break;
    }

    return stream;
}
#endif

template<typename ArgsTuple, size_t... ArgsIndex>
void logMessageImpl(const Connection& connection, MessageName messageName, void* object, const ArgsTuple& args, std::index_sequence<ArgsIndex...>)
{
#if !LOG_DISABLED
    if (LOG_CHANNEL(IPCMessages).state != WTFLogChannelState::On)
        return;

    auto stream = textStreamForLogging(connection, messageName, object, ForReply::No);

    if (auto argumentDescriptions = messageArgumentDescriptions(messageName))
        (stream.dumpProperty((*argumentDescriptions)[ArgsIndex].name, ValueOrEllipsis(std::get<ArgsIndex>(args))), ...);

    LOG(IPCMessages, "%s", stream.release().utf8().data());
#else
    UNUSED_PARAM(connection);
    UNUSED_PARAM(messageName);
    UNUSED_PARAM(args);
#endif
}

template<typename ArgsTuple, typename ArgsIndices = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void logMessage(const Connection& connection, MessageName messageName, void* object, const ArgsTuple& args)
{
    logMessageImpl(connection, messageName, object, args, ArgsIndices());
}

template<typename... T>
void logReply(const Connection& connection, MessageName messageName, const T&... args)
{
#if !LOG_DISABLED
    if (!sizeof...(T))
        return;

    auto stream = textStreamForLogging(connection, messageName, nullptr, ForReply::Yes);

    unsigned argIndex = 0;
    if (auto argumentDescriptions = messageReplyArgumentDescriptions(messageName))
        (stream.dumpProperty((*argumentDescriptions)[argIndex++].name, ValueOrEllipsis(args)), ...);

    LOG(IPCMessages, "%s", stream.release().utf8().data());
#else
    UNUSED_PARAM(connection);
    UNUSED_PARAM(messageName);
    (UNUSED_PARAM(args), ...);
#endif
}

// Dispatch functions with no reply arguments.

template<typename T, typename U, typename MF, typename ArgsTuple>
void callMemberFunction(T* object, MF U::* function, ArgsTuple&& tuple)
{
    std::apply(
        [&](auto&&... args) {
            (object->*function)(std::forward<decltype(args)>(args)...);
        }, std::forward<ArgsTuple>(tuple));
}

// Dispatch functions with synchronous reply arguments.

template<typename T, typename U, typename MF, typename ArgsTuple, typename CH>
void callMemberFunction(T* object, MF U::* function, ArgsTuple&& tuple, CompletionHandler<CH>&& completionHandler)
{
    std::apply(
        [&](auto&&... args) {
            (object->*function)(std::forward<decltype(args)>(args)..., WTFMove(completionHandler));
        }, std::forward<ArgsTuple>(tuple));
}

// Dispatch functions with connection parameter with synchronous reply arguments.

template<typename T, typename U, typename MF, typename ArgsTuple, typename CH>
void callMemberFunction(T* object, MF U::* function, Connection& connection, ArgsTuple&& tuple, CompletionHandler<CH>&& completionHandler)
{
    std::apply(
        [&](auto&&... args) {
            (object->*function)(connection, std::forward<decltype(args)>(args)..., WTFMove(completionHandler));
        }, std::forward<ArgsTuple>(tuple));
}

// Dispatch functions with connection parameter with no reply arguments.

template<typename T, typename U, typename MF, typename ArgsTuple>
void callMemberFunction(T* object, MF U::* function, Connection& connection, ArgsTuple&& tuple)
{
    std::apply(
        [&](auto&&... args) {
            (object->*function)(connection, std::forward<decltype(args)>(args)...);
        }, std::forward<ArgsTuple>(tuple));
}

// The two CompletionHandlerTypeDeduction partial specializations enable retrieval of the parameter type
// at the specified index position. This allows retrieving the exact type of the parameter where the
// CompletionHandler is expected in a given synchronous-reply method.

template<typename T, size_t N> struct CompletionHandlerTypeDeduction { };

template<typename R, typename... Args, size_t N>
struct CompletionHandlerTypeDeduction<R(Args...), N> {
    using Type = std::remove_cvref_t<typename std::tuple_element_t<N, std::tuple<Args...>>>;
};

template<typename R, typename... Args, size_t N>
struct CompletionHandlerTypeDeduction<R(Args...) const, N> : CompletionHandlerTypeDeduction<R(Args...), N> { };

// The CompletionHandlerValidation is used to ensure that the provided completion handler's parameter
// types match the types used in the IPC message definition.

template<typename CHType, typename ReplyType> struct CompletionHandlerValidation : std::false_type { };

template<typename... CHArgumentTypes, typename ReplyArgumentsTupleType>
struct CompletionHandlerValidation<CompletionHandler<void(CHArgumentTypes...)>, ReplyArgumentsTupleType>
    : std::is_same<std::tuple<std::remove_cvref_t<CHArgumentTypes>...>, ReplyArgumentsTupleType> { };

// Main dispatch functions

template<typename T>
struct CodingType {
    using Type = std::remove_cvref_t<T>;
};

template<typename... Ts>
struct CodingType<std::tuple<Ts...>> {
    using Type = std::tuple<typename CodingType<Ts>::Type...>;
};

template<typename MessageType, typename T, typename U, typename MF>
void handleMessage(Connection& connection, Decoder& decoder, T* object, MF U::* function)
{
    auto arguments = decoder.decode<typename CodingType<typename MessageType::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;

    logMessage(connection, MessageType::name(), object, *arguments);
    callMemberFunction(object, function, WTFMove(*arguments));
}

template<typename MessageType, typename T, typename U, typename MF>
void handleMessageWantsConnection(Connection& connection, Decoder& decoder, T* object, MF U::* function)
{
    auto arguments = decoder.decode<typename CodingType<typename MessageType::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;

    logMessage(connection, MessageType::name(), object, *arguments);
    callMemberFunction(object, function, connection, WTFMove(*arguments));
}

template<typename MessageType, typename T, typename U, typename MF>
bool handleMessageSynchronous(Connection& connection, Decoder& decoder, UniqueRef<Encoder>& replyEncoder, T* object, MF U::* function)
{
    auto arguments = decoder.decode<typename CodingType<typename MessageType::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return false;

    using CompletionHandlerFromMF = typename CompletionHandlerTypeDeduction<MF, std::tuple_size_v<typename MessageType::Arguments>>::Type;
    static_assert(CompletionHandlerValidation<CompletionHandlerFromMF, typename MessageType::ReplyArguments>::value);

    logMessage(connection, MessageType::name(), object, *arguments);
    callMemberFunction(object, function, WTFMove(*arguments),
        CompletionHandlerFromMF([replyEncoder = WTFMove(replyEncoder), connection = Ref { connection }] (auto&&... args) mutable {
            logReply(connection, MessageType::name(), args...);
            (replyEncoder.get() << ... << std::forward<decltype(args)>(args));
            connection->sendSyncReply(WTFMove(replyEncoder));
        }));
    return true;
}

template<typename MessageType, typename T, typename U, typename MF>
bool handleMessageSynchronousWantsConnection(Connection& connection, Decoder& decoder, UniqueRef<Encoder>& replyEncoder, T* object, MF U::* function)
{
    auto arguments = decoder.decode<typename CodingType<typename MessageType::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return false;
    
    using CompletionHandlerFromMF = typename CompletionHandlerTypeDeduction<MF, 1 + std::tuple_size_v<typename MessageType::Arguments>>::Type;
    static_assert(CompletionHandlerValidation<CompletionHandlerFromMF, typename MessageType::ReplyArguments>::value);

    logMessage(connection, MessageType::name(), object, *arguments);
    callMemberFunction(object, function, connection, WTFMove(*arguments),
        CompletionHandlerFromMF([replyEncoder = WTFMove(replyEncoder), connection = Ref { connection }] (auto&&... args) mutable {
            logReply(connection, MessageType::name(), args...);
            (replyEncoder.get() << ... << std::forward<decltype(args)>(args));
            connection->sendSyncReply(WTFMove(replyEncoder));
        }));
    return true;
}

template<typename MessageType, typename T, typename U, typename MF>
void handleMessageSynchronous(StreamServerConnection& connection, Decoder& decoder, T* object, MF U::* function)
{
    Connection::SyncRequestID syncRequestID;
    if (UNLIKELY(!decoder.decode(syncRequestID)))
        return;

    auto arguments = decoder.decode<typename CodingType<typename MessageType::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;

    using CompletionHandlerFromMF = typename CompletionHandlerTypeDeduction<MF, std::tuple_size_v<typename MessageType::Arguments>>::Type;
    static_assert(CompletionHandlerValidation<CompletionHandlerFromMF, typename MessageType::ReplyArguments>::value);

    logMessage(connection.connection(), MessageType::name(), object, *arguments);
    callMemberFunction(object, function, WTFMove(*arguments),
        CompletionHandlerFromMF([syncRequestID, connection = Ref { connection }] (auto&&... args) mutable {
            logReply(connection->connection(), MessageType::name(), args...);
            connection->sendSyncReply<MessageType>(syncRequestID, std::forward<decltype(args)>(args)...);
        }));
}

template<typename MessageType, typename T, typename U, typename MF>
void handleMessageAsync(Connection& connection, Decoder& decoder, T* object, MF U::* function)
{
    auto arguments = decoder.decode<typename CodingType<typename MessageType::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;
    auto replyID = decoder.decode<Connection::AsyncReplyID>();
    if (UNLIKELY(!replyID))
        return;

    using CompletionHandlerFromMF = typename CompletionHandlerTypeDeduction<MF, std::tuple_size_v<typename MessageType::Arguments>>::Type;
    static_assert(CompletionHandlerValidation<CompletionHandlerFromMF, typename MessageType::ReplyArguments>::value);

    logMessage(connection, MessageType::name(), object, *arguments);
    callMemberFunction(object, function, WTFMove(*arguments),
        CompletionHandlerFromMF { [replyID = *replyID, connection = Ref { connection }] (auto&&... args) mutable {
            auto encoder = makeUniqueRef<Encoder>(MessageType::asyncMessageReplyName(), replyID.toUInt64());
            logReply(connection, MessageType::name(), args...);
            (encoder.get() << ... << std::forward<decltype(args)>(args));
            connection->sendSyncReply(WTFMove(encoder));
        }, MessageType::callbackThread });
}

template<typename MessageType, typename T, typename U, typename MF>
void handleMessageAsyncWantsConnection(Connection& connection, Decoder& decoder, T* object, MF U::* function)
{
    auto arguments = decoder.decode<typename CodingType<typename MessageType::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;
    auto replyID = decoder.decode<Connection::AsyncReplyID>();
    if (UNLIKELY(!replyID))
        return;

    using CompletionHandlerFromMF = typename CompletionHandlerTypeDeduction<MF, 1 + std::tuple_size_v<typename MessageType::Arguments>>::Type;
    static_assert(CompletionHandlerValidation<CompletionHandlerFromMF, typename MessageType::ReplyArguments>::value);

    logMessage(connection, MessageType::name(), object, *arguments);
    callMemberFunction(object, function, connection, WTFMove(*arguments),
        CompletionHandlerFromMF { [replyID = *replyID, connection = Ref { connection }] (auto&&... args) mutable {
            auto encoder = makeUniqueRef<Encoder>(MessageType::asyncMessageReplyName(), replyID.toUInt64());
            logReply(connection, MessageType::name(), args...);
            (encoder.get() << ... << std::forward<decltype(args)>(args));
            connection->sendSyncReply(WTFMove(encoder));
        }, MessageType::callbackThread });
}

} // namespace IPC
