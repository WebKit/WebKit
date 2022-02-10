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

inline TextStream textStreamForLogging(const Connection& connection, MessageName messageName, ForReply forReply)
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
    auto arrow = forReply == ForReply::Yes ? "<- " : "-> ";
    stream << arrow << WebCore::processTypeDescription(WebCore::processType()) << ' ' << getCurrentProcessID() << "] " << description(messageName);
    if (forReply == ForReply::Yes)
        stream << " Reply";
    return stream;
}
#endif

template<typename ArgsTuple, size_t... ArgsIndex>
void logMessageImpl(const Connection& connection, MessageName messageName, const ArgsTuple& args, std::index_sequence<ArgsIndex...>)
{
#if !LOG_DISABLED
    if (LOG_CHANNEL(IPCMessages).state != WTFLogChannelState::On)
        return;

    auto stream = textStreamForLogging(connection, messageName, ForReply::No);

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
void logMessage(const Connection& connection, MessageName messageName, const ArgsTuple& args)
{
    logMessageImpl(connection, messageName, args, ArgsIndices());
}

template<typename... T>
void logReply(const Connection& connection, MessageName messageName, const T&... args)
{
#if !LOG_DISABLED
    if (!sizeof...(T))
        return;

    auto stream = textStreamForLogging(connection, messageName, ForReply::Yes);

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

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(std::get<ArgsIndex>(std::forward<ArgsTuple>(args))...);
}

template<typename C, typename MF, typename ArgsTuple, typename ArgsIndices = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(ArgsTuple&& args, C* object, MF function)
{
    callMemberFunctionImpl(object, function, std::forward<ArgsTuple>(args), ArgsIndices());
}

// Dispatch functions with synchronous reply arguments.

template <typename C, typename MF, typename CH, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, CompletionHandler<CH>&& completionHandler, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(std::get<ArgsIndex>(std::forward<ArgsTuple>(args))..., WTFMove(completionHandler));
}

template<typename C, typename MF, typename CH, typename ArgsTuple, typename ArgsIndices = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(ArgsTuple&& args, CompletionHandler<CH>&& completionHandler, C* object, MF function)
{
    callMemberFunctionImpl(object, function, WTFMove(completionHandler), std::forward<ArgsTuple>(args), ArgsIndices());
}

// Dispatch functions with connection parameter with synchronous reply arguments.

template <typename C, typename MF, typename CH, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(Connection& connection, C* object, MF function, CompletionHandler<CH>&& completionHandler, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(connection, std::get<ArgsIndex>(std::forward<ArgsTuple>(args))..., WTFMove(completionHandler));
}

template<typename C, typename MF, typename CH, typename ArgsTuple, typename ArgsIndices = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(Connection& connection, ArgsTuple&& args, CompletionHandler<CH>&& completionHandler, C* object, MF function)
{
    callMemberFunctionImpl(connection, object, function, WTFMove(completionHandler), std::forward<ArgsTuple>(args), ArgsIndices());
}

// Dispatch functions with connection parameter with no reply arguments.

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, Connection& connection, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(connection, std::get<ArgsIndex>(std::forward<ArgsTuple>(args))...);
}

template<typename C, typename MF, typename ArgsTuple, typename ArgsIndices = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(Connection& connection, ArgsTuple&& args, C* object, MF function)
{
    callMemberFunctionImpl(object, function, connection, std::forward<ArgsTuple>(args), ArgsIndices());
}

// Main dispatch functions

template<typename T>
struct CodingType {
    typedef std::remove_const_t<std::remove_reference_t<T>> Type;
};

template<typename... Ts>
struct CodingType<std::tuple<Ts...>> {
    typedef std::tuple<typename CodingType<Ts>::Type...> Type;
};

template<typename T, typename C, typename MF>
void handleMessage(Connection& connection, Decoder& decoder, C* object, MF function)
{
    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;

    logMessage(connection, T::name(), *arguments);
    callMemberFunction(WTFMove(*arguments), object, function);
}

template<typename T, typename C, typename MF>
void handleMessageWantsConnection(Connection& connection, Decoder& decoder, C* object, MF function)
{
    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;

    logMessage(connection, T::name(), *arguments);
    callMemberFunction(connection, WTFMove(*arguments), object, function);
}

template<typename T, typename C, typename MF>
bool handleMessageSynchronous(Connection& connection, Decoder& decoder, UniqueRef<Encoder>& replyEncoder, C* object, MF function)
{
    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return false;

    typename T::DelayedReply completionHandler = [replyEncoder = WTFMove(replyEncoder), connection = Ref { connection }] (auto&&... args) mutable {
        logReply(connection, T::name(), args...);
        T::send(WTFMove(replyEncoder), WTFMove(connection), args...);
    };
    logMessage(connection, T::name(), *arguments);
    callMemberFunction(WTFMove(*arguments), WTFMove(completionHandler), object, function);
    return true;
}

template<typename T, typename C, typename MF>
bool handleMessageSynchronousWantsConnection(Connection& connection, Decoder& decoder, UniqueRef<Encoder>& replyEncoder, C* object, MF function)
{
    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return false;
    
    typename T::DelayedReply completionHandler = [replyEncoder = WTFMove(replyEncoder), connection = Ref { connection }] (auto&&... args) mutable {
        logReply(connection, T::name(), args...);
        T::send(WTFMove(replyEncoder), WTFMove(connection), args...);
    };
    logMessage(connection, T::name(), *arguments);
    callMemberFunction(connection, WTFMove(*arguments), WTFMove(completionHandler), object, function);
    return true;
}

template<typename T, typename C, typename MF>
void handleMessageSynchronous(StreamServerConnectionBase& connection, Decoder& decoder, C* object, MF function)
{
    Connection::SyncRequestID syncRequestID;
    if (UNLIKELY(!decoder.decode(syncRequestID)))
        return;

    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;

    typename T::DelayedReply completionHandler = [syncRequestID, connection = Ref { connection }] (auto&&... args) mutable {
        logReply(connection->connection(), T::name(), args...);
        connection->sendSyncReply<T>(syncRequestID, args...);
    };
    logMessage(connection.connection(), T::name(), *arguments);
    callMemberFunction(WTFMove(*arguments), WTFMove(completionHandler), object, function);
}

template<typename T, typename C, typename MF>
void handleMessageAsync(Connection& connection, Decoder& decoder, C* object, MF function)
{
    auto listenerID = decoder.decode<uint64_t>();
    if (UNLIKELY(!listenerID))
        return;

    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;

    typename T::AsyncReply completionHandler = { [listenerID = *listenerID, connection = Ref { connection }] (auto&&... args) mutable {
        auto encoder = makeUniqueRef<Encoder>(T::asyncMessageReplyName(), listenerID);
        logReply(connection, T::name(), args...);
        T::send(WTFMove(encoder), WTFMove(connection), args...);
    }, T::callbackThread };
    logMessage(connection, T::name(), *arguments);
    callMemberFunction(WTFMove(*arguments), WTFMove(completionHandler), object, function);
}

template<typename T, typename C, typename MF>
void handleMessageAsyncWantsConnection(Connection& connection, Decoder& decoder, C* object, MF function)
{
    auto listenerID = decoder.decode<uint64_t>();
    if (UNLIKELY(!listenerID))
        return;

    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;

    typename T::AsyncReply completionHandler = { [listenerID = *listenerID, connection = Ref { connection }] (auto&&... args) mutable {
        auto encoder = makeUniqueRef<Encoder>(T::asyncMessageReplyName(), listenerID);
        logReply(connection, T::name(), args...);
        T::send(WTFMove(encoder), WTFMove(connection), args...);
    }, T::callbackThread };
    logMessage(connection, T::name(), *arguments);
    callMemberFunction(connection, WTFMove(*arguments), WTFMove(completionHandler), object, function);
}

} // namespace IPC
