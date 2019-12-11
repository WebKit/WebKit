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
#include <wtf/CompletionHandler.h>
#include <wtf/StdLibExtras.h>

namespace IPC {

class Connection;

// Dispatch functions with no reply arguments.

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(std::get<ArgsIndex>(std::forward<ArgsTuple>(args))...);
}

template<typename C, typename MF, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(ArgsTuple&& args, C* object, MF function)
{
    callMemberFunctionImpl(object, function, std::forward<ArgsTuple>(args), ArgsIndicies());
}

// Dispatch functions with synchronous reply arguments.

template <typename C, typename MF, typename CH, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, CompletionHandler<CH>&& completionHandler, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(std::get<ArgsIndex>(std::forward<ArgsTuple>(args))..., WTFMove(completionHandler));
}

template<typename C, typename MF, typename CH, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(ArgsTuple&& args, CompletionHandler<CH>&& completionHandler, C* object, MF function)
{
    callMemberFunctionImpl(object, function, WTFMove(completionHandler), std::forward<ArgsTuple>(args), ArgsIndicies());
}

// Dispatch functions with connection parameter with synchronous reply arguments.

template <typename C, typename MF, typename CH, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(Connection& connection, C* object, MF function, CompletionHandler<CH>&& completionHandler, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(connection, std::get<ArgsIndex>(std::forward<ArgsTuple>(args))..., WTFMove(completionHandler));
}

template<typename C, typename MF, typename CH, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(Connection& connection, ArgsTuple&& args, CompletionHandler<CH>&& completionHandler, C* object, MF function)
{
    callMemberFunctionImpl(connection, object, function, WTFMove(completionHandler), std::forward<ArgsTuple>(args), ArgsIndicies());
}

// Dispatch functions with connection parameter with no reply arguments.

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, Connection& connection, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(connection, std::get<ArgsIndex>(std::forward<ArgsTuple>(args))...);
}

template<typename C, typename MF, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(Connection& connection, ArgsTuple&& args, C* object, MF function)
{
    callMemberFunctionImpl(object, function, connection, std::forward<ArgsTuple>(args), ArgsIndicies());
}

// Main dispatch functions

template<typename T>
struct CodingType {
    typedef std::remove_const_t<std::remove_reference_t<T>> Type;
};

class DataReference;
class SharedBufferDataReference;
template<> struct CodingType<const SharedBufferDataReference&> {
    typedef DataReference Type;
};

template<typename... Ts>
struct CodingType<std::tuple<Ts...>> {
    typedef std::tuple<typename CodingType<Ts>::Type...> Type;
};

template<typename T, typename C, typename MF>
void handleMessage(Decoder& decoder, C* object, MF function)
{
    Optional<typename CodingType<typename T::Arguments>::Type> arguments;
    decoder >> arguments;
    if (!arguments) {
        ASSERT(decoder.isInvalid());
        return;
    }

    callMemberFunction(WTFMove(*arguments), object, function);
}

template<typename T, typename C, typename MF>
void handleMessage(Connection& connection, Decoder& decoder, C* object, MF function)
{
    Optional<typename CodingType<typename T::Arguments>::Type> arguments;
    decoder >> arguments;
    if (!arguments) {
        ASSERT(decoder.isInvalid());
        return;
    }
    callMemberFunction(connection, WTFMove(*arguments), object, function);
}

template<typename T, typename C, typename MF>
void handleMessageSynchronous(Connection& connection, Decoder& decoder, std::unique_ptr<Encoder>& replyEncoder, C* object, MF function)
{
    Optional<typename CodingType<typename T::Arguments>::Type> arguments;
    decoder >> arguments;
    if (!arguments) {
        ASSERT(decoder.isInvalid());
        return;
    }

    typename T::DelayedReply completionHandler = [replyEncoder = WTFMove(replyEncoder), connection = makeRef(connection)] (auto&&... args) mutable {
        T::send(WTFMove(replyEncoder), WTFMove(connection), args...);
    };
    callMemberFunction(WTFMove(*arguments), WTFMove(completionHandler), object, function);
}

template<typename T, typename C, typename MF>
void handleMessageSynchronousWantsConnection(Connection& connection, Decoder& decoder, std::unique_ptr<Encoder>& replyEncoder, C* object, MF function)
{
    Optional<typename CodingType<typename T::Arguments>::Type> arguments;
    decoder >> arguments;
    if (!arguments) {
        ASSERT(decoder.isInvalid());
        return;
    }
    
    typename T::DelayedReply completionHandler = [replyEncoder = WTFMove(replyEncoder), connection = makeRef(connection)] (auto&&... args) mutable {
        T::send(WTFMove(replyEncoder), WTFMove(connection), args...);
    };
    callMemberFunction(connection, WTFMove(*arguments), WTFMove(completionHandler), object, function);
}

template<typename T, typename C, typename MF>
void handleMessageAsync(Connection& connection, Decoder& decoder, C* object, MF function)
{
    Optional<uint64_t> listenerID;
    decoder >> listenerID;
    if (!listenerID) {
        ASSERT(decoder.isInvalid());
        return;
    }
    
    Optional<typename CodingType<typename T::Arguments>::Type> arguments;
    decoder >> arguments;
    if (!arguments) {
        ASSERT(decoder.isInvalid());
        return;
    }

    typename T::AsyncReply completionHandler = [listenerID = *listenerID, connection = makeRef(connection)] (auto&&... args) mutable {
        auto encoder = makeUnique<Encoder>("AsyncReply", T::asyncMessageReplyName(), 0);
        *encoder << listenerID;
        T::send(WTFMove(encoder), WTFMove(connection), args...);
    };
    callMemberFunction(WTFMove(*arguments), WTFMove(completionHandler), object, function);
}

} // namespace IPC
