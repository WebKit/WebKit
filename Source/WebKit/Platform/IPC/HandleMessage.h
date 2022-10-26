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

template<typename T>
struct CompletionHandlerTypeDeduction;

// The following two partial specializations enable retrieval of the parameter type at the specified index position.
// This allows retrieving the exact type of the parameter where the CompletionHandler is expected in a given
// synchronous-reply method.

template<typename R, typename C, typename... Args>
struct CompletionHandlerTypeDeduction<R(C::*)(Args...)> {
    template<size_t N, typename = std::enable_if_t<(N < sizeof...(Args))>>
    using Type = std::remove_cvref_t<typename std::tuple_element_t<N, std::tuple<Args...>>>;
};

template<typename R, typename C, typename... Args>
struct CompletionHandlerTypeDeduction<R(C::*)(Args...) const> {
    template<size_t N, typename = std::enable_if_t<(N < sizeof...(Args))>>
    using Type = std::remove_cvref_t<typename std::tuple_element_t<N, std::tuple<Args...>>>;
};

// CompletionHandlerValidation::matchingTypes<ReplyType>() expects both CHType and ReplyType
// to be specializations of the CompletionHandler template. CHType is the type specified in
// the IPC handling method on the C++ class. ReplyType is the type generated from the IPC
// message specification (and can still be used in the C++ method). Validation passes if both
// types are indeed specializations of the CompletionHandler template and all the decayed
// parameter types match between the two CompletionHandler specializations.

struct CompletionHandlerValidation {
    template<typename CHType>
    struct ValidCompletionHandlerType : std::false_type { };

    template<size_t N, typename TupleType>
    using ParameterType = std::remove_cvref_t<std::tuple_element_t<N, TupleType>>;

    template<typename CHArgsTuple, typename ReplyArgsTuple, size_t... Indices>
    static constexpr bool matchingParameters(std::index_sequence<Indices...>)
    {
        return (std::is_same_v<ParameterType<Indices, CHArgsTuple>, ParameterType<Indices, ReplyArgsTuple>> && ...);
    }

    template<typename CHType, typename ReplyType>
    static constexpr bool matchingTypes()
    {
        using CHInTypes = typename CHType::InTypes;
        using ReplyInTypes = typename ReplyType::InTypes;

        return ValidCompletionHandlerType<CHType>::value && ValidCompletionHandlerType<ReplyType>::value
            && (std::tuple_size_v<CHInTypes> == std::tuple_size_v<ReplyInTypes>)
            && matchingParameters<CHInTypes, ReplyInTypes>(std::make_index_sequence<std::tuple_size_v<CHInTypes>>());
    }
};

// This specialization is used in matchingTypes() and affirms that the passed-in CHType or
// ReplyType is indeed a proper CompletionHandler type.

template<typename... InTypes>
struct CompletionHandlerValidation::ValidCompletionHandlerType<CompletionHandler<void(InTypes...)>> : std::true_type { };

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

    logMessage(connection, T::name(), object, *arguments);
    callMemberFunction(WTFMove(*arguments), object, function);
}

template<typename T, typename C, typename MF>
void handleMessageWantsConnection(Connection& connection, Decoder& decoder, C* object, MF function)
{
    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;

    logMessage(connection, T::name(), object, *arguments);
    callMemberFunction(connection, WTFMove(*arguments), object, function);
}

template<typename T, typename C, typename MF>
bool handleMessageSynchronous(Connection& connection, Decoder& decoder, UniqueRef<Encoder>& replyEncoder, C* object, MF function)
{
    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return false;

    using CompletionHandlerFromMF = typename CompletionHandlerTypeDeduction<MF>::template Type<std::tuple_size_v<typename T::Arguments>>;
    static_assert(CompletionHandlerValidation::template matchingTypes<CompletionHandlerFromMF, typename T::DelayedReply>());

    logMessage(connection, T::name(), object, *arguments);
    callMemberFunction(WTFMove(*arguments),
        CompletionHandlerFromMF([replyEncoder = WTFMove(replyEncoder), connection = Ref { connection }] (auto&&... args) mutable {
            logReply(connection, T::name(), args...);
            (replyEncoder.get() << ... << std::forward<decltype(args)>(args));
            connection->sendSyncReply(WTFMove(replyEncoder));
        }),
        object, function);
    return true;
}

template<typename T, typename C, typename MF>
bool handleMessageSynchronousWantsConnection(Connection& connection, Decoder& decoder, UniqueRef<Encoder>& replyEncoder, C* object, MF function)
{
    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return false;
    
    using CompletionHandlerFromMF = typename CompletionHandlerTypeDeduction<MF>::template Type<1 + std::tuple_size_v<typename T::Arguments>>;
    static_assert(CompletionHandlerValidation::template matchingTypes<CompletionHandlerFromMF, typename T::DelayedReply>());

    logMessage(connection, T::name(), object, *arguments);
    callMemberFunction(connection, WTFMove(*arguments),
        CompletionHandlerFromMF([replyEncoder = WTFMove(replyEncoder), connection = Ref { connection }] (auto&&... args) mutable {
            logReply(connection, T::name(), args...);
            (replyEncoder.get() << ... << std::forward<decltype(args)>(args));
            connection->sendSyncReply(WTFMove(replyEncoder));
        }),
        object, function);
    return true;
}

template<typename T, typename C, typename MF>
void handleMessageSynchronous(StreamServerConnection& connection, Decoder& decoder, C* object, MF function)
{
    Connection::SyncRequestID syncRequestID;
    if (UNLIKELY(!decoder.decode(syncRequestID)))
        return;

    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;

    using CompletionHandlerFromMF = typename CompletionHandlerTypeDeduction<MF>::template Type<std::tuple_size_v<typename T::Arguments>>;
    static_assert(CompletionHandlerValidation::template matchingTypes<CompletionHandlerFromMF, typename T::DelayedReply>());

    logMessage(connection.connection(), T::name(), object, *arguments);
    callMemberFunction(WTFMove(*arguments),
        CompletionHandlerFromMF([syncRequestID, connection = Ref { connection }] (auto&&... args) mutable {
            logReply(connection->connection(), T::name(), args...);
            connection->sendSyncReply<T>(syncRequestID, std::forward<decltype(args)>(args)...);
        }),
        object, function);
}

template<typename T, typename C, typename MF>
void handleMessageAsync(Connection& connection, Decoder& decoder, C* object, MF function)
{
    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;
    auto replyID = decoder.decode<uint64_t>();
    if (UNLIKELY(!replyID))
        return;

    using CompletionHandlerFromMF = typename CompletionHandlerTypeDeduction<MF>::template Type<std::tuple_size_v<typename T::Arguments>>;
    static_assert(CompletionHandlerValidation::template matchingTypes<CompletionHandlerFromMF, typename T::AsyncReply>());

    logMessage(connection, T::name(), object, *arguments);
    callMemberFunction(WTFMove(*arguments),
        CompletionHandlerFromMF { [replyID = *replyID, connection = Ref { connection }] (auto&&... args) mutable {
            auto encoder = makeUniqueRef<Encoder>(T::asyncMessageReplyName(), replyID);
            logReply(connection, T::name(), args...);
            (encoder.get() << ... << std::forward<decltype(args)>(args));
            connection->sendSyncReply(WTFMove(encoder));
        }, T::callbackThread },
        object, function);
}

template<typename T, typename C, typename MF>
void handleMessageAsyncWantsConnection(Connection& connection, Decoder& decoder, C* object, MF function)
{
    auto arguments = decoder.decode<typename CodingType<typename T::Arguments>::Type>();
    if (UNLIKELY(!arguments))
        return;
    auto replyID = decoder.decode<uint64_t>();
    if (UNLIKELY(!replyID))
        return;

    using CompletionHandlerFromMF = typename CompletionHandlerTypeDeduction<MF>::template Type<1 + std::tuple_size_v<typename T::Arguments>>;
    static_assert(CompletionHandlerValidation::template matchingTypes<CompletionHandlerFromMF, typename T::AsyncReply>());

    logMessage(connection, T::name(), object, *arguments);
    callMemberFunction(connection, WTFMove(*arguments),
        CompletionHandlerFromMF { [replyID = *replyID, connection = Ref { connection }] (auto&&... args) mutable {
            auto encoder = makeUniqueRef<Encoder>(T::asyncMessageReplyName(), replyID);
            logReply(connection, T::name(), args...);
            (encoder.get() << ... << std::forward<decltype(args)>(args));
            connection->sendSyncReply(WTFMove(encoder));
        }, T::callbackThread },
        object, function);
}

} // namespace IPC
