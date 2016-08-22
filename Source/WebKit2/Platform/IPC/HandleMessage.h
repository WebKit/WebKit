#pragma once

#include "ArgumentCoders.h"
#include <wtf/StdLibExtras.h>

namespace IPC {

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

// Dispatch functions with reply arguments.

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex, typename ReplyArgsTuple, size_t... ReplyArgsIndex>
void callMemberFunctionImpl(C* object, MF function, ArgsTuple&& args, ReplyArgsTuple& replyArgs, std::index_sequence<ArgsIndex...>, std::index_sequence<ReplyArgsIndex...>)
{
    (object->*function)(std::get<ArgsIndex>(std::forward<ArgsTuple>(args))..., std::get<ReplyArgsIndex>(replyArgs)...);
}

template <typename C, typename MF, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>, typename ReplyArgsTuple, typename ReplyArgsIndicies = std::make_index_sequence<std::tuple_size<ReplyArgsTuple>::value>>
void callMemberFunction(ArgsTuple&& args, ReplyArgsTuple& replyArgs, C* object, MF function)
{
    callMemberFunctionImpl(object, function, std::forward<ArgsTuple>(args), replyArgs, ArgsIndicies(), ReplyArgsIndicies());
}

// Dispatch functions with delayed reply arguments.

template <typename C, typename MF, typename R, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, PassRefPtr<R> delayedReply, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(std::get<ArgsIndex>(args)..., delayedReply);
}

template<typename C, typename MF, typename R, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(ArgsTuple&& args, PassRefPtr<R> delayedReply, C* object, MF function)
{
    callMemberFunctionImpl(object, function, delayedReply, std::forward<ArgsTuple>(args), ArgsIndicies());
}

// Dispatch functions with connection parameter with no reply arguments.

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, Connection& connection, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(connection, std::get<ArgsIndex>(args)...);
}

template<typename C, typename MF, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(Connection& connection, ArgsTuple&& args, C* object, MF function)
{
    callMemberFunctionImpl(object, function, connection, std::forward<ArgsTuple>(args), ArgsIndicies());
}

// Dispatch functions with connection parameter with reply arguments.

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex, typename ReplyArgsTuple, size_t... ReplyArgsIndex>
void callMemberFunctionImpl(C* object, MF function, Connection& connection, ArgsTuple&& args, ReplyArgsTuple& replyArgs, std::index_sequence<ArgsIndex...>, std::index_sequence<ReplyArgsIndex...>)
{
    (object->*function)(connection, std::get<ArgsIndex>(std::forward<ArgsTuple>(args))..., std::get<ReplyArgsIndex>(replyArgs)...);
}

template <typename C, typename MF, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>, typename ReplyArgsTuple, typename ReplyArgsIndicies = std::make_index_sequence<std::tuple_size<ReplyArgsTuple>::value>>
void callMemberFunction(Connection& connection, ArgsTuple&& args, ReplyArgsTuple& replyArgs, C* object, MF function)
{
    callMemberFunctionImpl(object, function, connection, std::forward<ArgsTuple>(args), replyArgs, ArgsIndicies(), ReplyArgsIndicies());
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
void handleMessage(Decoder& decoder, C* object, MF function)
{
    typename CodingType<typename T::Arguments>::Type arguments;
    if (!decoder.decode(arguments)) {
        ASSERT(decoder.isInvalid());
        return;
    }

    callMemberFunction(WTFMove(arguments), object, function);
}

template<typename T, typename C, typename MF>
void handleMessage(Decoder& decoder, Encoder& replyEncoder, C* object, MF function)
{
    typename CodingType<typename T::Arguments>::Type arguments;
    if (!decoder.decode(arguments)) {
        ASSERT(decoder.isInvalid());
        return;
    }

    typename CodingType<typename T::Reply>::Type replyArguments;
    callMemberFunction(WTFMove(arguments), replyArguments, object, function);
    replyEncoder << replyArguments;
}

template<typename T, typename C, typename MF>
void handleMessage(Connection& connection, Decoder& decoder, Encoder& replyEncoder, C* object, MF function)
{
    typename CodingType<typename T::Arguments>::Type arguments;
    if (!decoder.decode(arguments)) {
        ASSERT(decoder.isInvalid());
        return;
    }

    typename CodingType<typename T::Reply>::Type replyArguments;
    callMemberFunction(connection, WTFMove(arguments), replyArguments, object, function);
    replyEncoder << replyArguments;
}

template<typename T, typename C, typename MF>
void handleMessage(Connection& connection, Decoder& decoder, C* object, MF function)
{
    typename CodingType<typename T::Arguments>::Type arguments;
    if (!decoder.decode(arguments)) {
        ASSERT(decoder.isInvalid());
        return;
    }
    callMemberFunction(connection, WTFMove(arguments), object, function);
}

template<typename T, typename C, typename MF>
void handleMessageDelayed(Connection& connection, Decoder& decoder, std::unique_ptr<Encoder>& replyEncoder, C* object, MF function)
{
    typename CodingType<typename T::Arguments>::Type arguments;
    if (!decoder.decode(arguments)) {
        ASSERT(decoder.isInvalid());
        return;
    }

    RefPtr<typename T::DelayedReply> delayedReply = adoptRef(new typename T::DelayedReply(&connection, WTFMove(replyEncoder)));
    callMemberFunction(WTFMove(arguments), PassRefPtr<typename T::DelayedReply>(WTFMove(delayedReply)), object, function);
}

} // namespace IPC
