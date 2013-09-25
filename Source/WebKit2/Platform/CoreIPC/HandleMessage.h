#ifndef HandleMessage_h
#define HandleMessage_h

#include "Arguments.h"
#include "MessageDecoder.h"
#include "MessageEncoder.h"

namespace CoreIPC {

// Dispatch functions with no reply arguments.
template<typename C, typename MF>
void callMemberFunction(std::tuple<>&&, C* object, MF function)
{
    (object->*function)();
}

template<typename C, typename MF, typename P1>
void callMemberFunction(std::tuple<P1>&& args, C* object, MF function)
{
    (object->*function)(std::get<0>(args));
}

template<typename C, typename MF, typename P1, typename P2>
void callMemberFunction(std::tuple<P1, P2>&& args, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename P3>
void callMemberFunction(std::tuple<P1, P2, P3>&& args, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4>
void callMemberFunction(std::tuple<P1, P2, P3, P4>&& args, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5>&& args, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args));
}
    
template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6>&& args, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6, P7>&& args, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<6>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6, P7, P8>&& args, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<6>(args), std::get<7>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6, P7, P8, P9, P10>&& args, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<6>(args), std::get<7>(args), std::get<8>(args), std::get<9>(args));
}

// Dispatch functions with reply arguments.

template<typename C, typename MF>
void callMemberFunction(std::tuple<>&&, std::tuple<>&, C* object, MF function)
{
    (object->*function)();
}

template<typename C, typename MF, typename R1>
void callMemberFunction(std::tuple<>&&, std::tuple<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(replyArgs));
}

template<typename C, typename MF, typename R1, typename R2>
void callMemberFunction(std::tuple<>&&, std::tuple<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(replyArgs), std::get<1>(replyArgs));
}

template<typename C, typename MF, typename P1>
void callMemberFunction(std::tuple<P1>&& args, std::tuple<>&, C* object, MF function)
{
    (object->*function)(std::get<0>(args));
}

template<typename C, typename MF, typename P1, typename R1>
void callMemberFunction(std::tuple<P1>&& args, std::tuple<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<0>(replyArgs));
}

template<typename C, typename MF, typename P1, typename R1, typename R2>
void callMemberFunction(std::tuple<P1>&& args, std::tuple<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<0>(replyArgs), std::get<1>(replyArgs));
}

template<typename C, typename MF, typename P1, typename R1, typename R2, typename R3>
void callMemberFunction(std::tuple<P1>&& args, std::tuple<R1, R2, R3>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<0>(replyArgs), std::get<1>(replyArgs), std::get<2>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2>
void callMemberFunction(std::tuple<P1, P2>&& args, std::tuple<>&, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args));
}

template<typename C, typename MF, typename P1, typename R1, typename R2, typename R3, typename R4>
void callMemberFunction(std::tuple<P1>&& args, Arguments4<R1, R2, R3, R4>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<0>(replyArgs), std::get<1>(replyArgs), std::get<2>(replyArgs), std::get<3>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename R1>
void callMemberFunction(std::tuple<P1, P2>&& args, std::tuple<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<0>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename R1, typename R2>
void callMemberFunction(std::tuple<P1, P2>&& args, std::tuple<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<0>(replyArgs), std::get<1>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename R1>
void callMemberFunction(std::tuple<P1, P2, P3>&& args, std::tuple<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<0>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename R1, typename R2>
void callMemberFunction(std::tuple<P1, P2, P3>&& args, std::tuple<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<0>(replyArgs), std::get<1>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename R1>
void callMemberFunction(std::tuple<P1, P2, P3, P4>&& args, std::tuple<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<0>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename R1>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6>&& args, std::tuple<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<0>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename R1>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6, P7>&& args, std::tuple<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<6>(args), std::get<0>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename R1>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6, P7, P8>&& args, std::tuple<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<6>(args), std::get<7>(args), std::get<0>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename R1, typename R2>
void callMemberFunction(std::tuple<P1, P2, P3, P4>&& args, std::tuple<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<0>(replyArgs), std::get<1>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename R1, typename R2>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5>&& args, std::tuple<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<0>(replyArgs), std::get<1>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename R1, typename R2, typename R3>
void callMemberFunction(std::tuple<P1, P2, P3, P4>&& args, std::tuple<R1, R2, R3>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<0>(replyArgs), std::get<1>(replyArgs), std::get<2>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename R1, typename R2, typename R3>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5>&& args, std::tuple<R1, R2, R3>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<0>(replyArgs), std::get<1>(replyArgs), std::get<2>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename R1, typename R2>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6>&& args, std::tuple<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<0>(replyArgs), std::get<1>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename R1, typename R2, typename R3>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6>&& args, std::tuple<R1, R2, R3>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<0>(replyArgs), std::get<1>(replyArgs), std::get<2>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename R1, typename R2, typename R3, typename R4>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6>&& args, std::tuple<R1, R2, R3, R4>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<0>(replyArgs), std::get<1>(replyArgs), std::get<2>(replyArgs), std::get<3>(replyArgs));
}

// Dispatch functions with delayed reply arguments.
template<typename C, typename MF, typename R>
void callMemberFunction(std::tuple<>&&, PassRefPtr<R> delayedReply, C* object, MF function)
{
    (object->*function)(delayedReply);
}

template<typename C, typename MF, typename P1, typename R>
void callMemberFunction(std::tuple<P1>&& args, PassRefPtr<R> delayedReply, C* object, MF function)
{
    (object->*function)(std::get<0>(args), delayedReply);
}

template<typename C, typename MF, typename P1, typename P2, typename R>
void callMemberFunction(std::tuple<P1, P2>&& args, PassRefPtr<R> delayedReply, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), delayedReply);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename R>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6, P7, P8>&& args, PassRefPtr<R> delayedReply, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<6>(args), std::get<7>(args), delayedReply);
}

// Dispatch functions with connection parameter.
template<typename C, typename MF>
void callMemberFunction(Connection* connection, std::tuple<>&&, C* object, MF function)
{
    (object->*function)(connection);
}

template<typename C, typename MF, typename P1>
void callMemberFunction(Connection* connection, std::tuple<P1>&& args, C* object, MF function)
{
    (object->*function)(connection, std::get<0>(args));
}

template<typename C, typename MF, typename P1, typename P2>
void callMemberFunction(Connection* connection, std::tuple<P1, P2>&& args, C* object, MF function)
{
    (object->*function)(connection, std::get<0>(args), std::get<1>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename P3>
void callMemberFunction(Connection* connection, std::tuple<P1, P2, P3>&& args, C* object, MF function)
{
    (object->*function)(connection, std::get<0>(args), std::get<1>(args), std::get<2>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4>
void callMemberFunction(Connection* connection, std::tuple<P1, P2, P3, P4>&& args, C* object, MF function)
{
    (object->*function)(connection, std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5>
void callMemberFunction(Connection* connection, std::tuple<P1, P2, P3, P4, P5>&& args, C* object, MF function)
{
    (object->*function)(connection, std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
void callMemberFunction(Connection* connection, std::tuple<P1, P2, P3, P4, P5, P6>&& args, C* object, MF function)
{
    (object->*function)(connection, std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args));
}

template<typename C, typename MF, typename P1, typename P2, typename R1>
void callMemberFunction(Connection* connection, std::tuple<P1, P2>&& args, std::tuple<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(connection, std::get<0>(args), std::get<1>(args), std::get<0>(replyArgs));
}

template<typename C, typename MF, typename P1, typename R1>
void callMemberFunction(Connection* connection, std::tuple<P1>&& args, std::tuple<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(connection, std::get<0>(args), std::get<0>(replyArgs));
}

// Variadic dispatch functions.

template<typename C, typename MF>
void callMemberFunction(std::tuple<>&&, MessageDecoder& decoder, C* object, MF function)
{
    (object->*function)(decoder);
}

template<typename C, typename MF, typename P1>
void callMemberFunction(std::tuple<P1>&& args, MessageDecoder& decoder, C* object, MF function)
{
    (object->*function)(std::get<0>(args), decoder);
}

template<typename C, typename MF, typename P1, typename P2>
void callMemberFunction(std::tuple<P1, P2>&& args, MessageDecoder& decoder, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), decoder);
}

template<typename C, typename MF, typename P1, typename P2, typename P3>
void callMemberFunction(std::tuple<P1, P2, P3>&& args, MessageDecoder& decoder, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), decoder);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4>
void callMemberFunction(std::tuple<P1, P2, P3, P4>&& args, MessageDecoder& decoder, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), decoder);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5>&& args, MessageDecoder& decoder, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), decoder);
}
    
template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6>&& args, MessageDecoder& decoder, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), decoder);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6, P7>&& args, MessageDecoder& decoder, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), std::get<6>(args), decoder);
}

// Variadic dispatch functions with non-variadic reply arguments.

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename R1, typename R2, typename R3>
void callMemberFunction(std::tuple<P1, P2, P3, P4>&& args, MessageDecoder& decoder, std::tuple<R1, R2, R3>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), decoder, std::get<0>(replyArgs), std::get<1>(replyArgs), std::get<2>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename R1, typename R2>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6>&& args, MessageDecoder& decoder, std::tuple<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), decoder, std::get<0>(replyArgs), std::get<1>(replyArgs));
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename R1, typename R2, typename R3>
void callMemberFunction(std::tuple<P1, P2, P3, P4, P5, P6>&& args, MessageDecoder& decoder, std::tuple<R1, R2, R3>& replyArgs, C* object, MF function)
{
    (object->*function)(std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args), std::get<4>(args), std::get<5>(args), decoder, std::get<0>(replyArgs), std::get<1>(replyArgs), std::get<2>(replyArgs));
}

// Main dispatch functions
template<typename T, typename C, typename MF>
void handleMessage(MessageDecoder& decoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments))
        return;
    callMemberFunction(std::move(arguments), object, function);
}

template<typename T, typename C, typename MF>
void handleMessage(MessageDecoder& decoder, MessageEncoder& replyEncoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments))
        return;

    typename T::Reply::ValueType replyArguments;
    callMemberFunction(std::move(arguments), replyArguments, object, function);
    replyEncoder << replyArguments;
}

template<typename T, typename C, typename MF>
void handleMessage(Connection* connection, MessageDecoder& decoder, MessageEncoder& replyEncoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments))
        return;

    typename T::Reply::ValueType replyArguments;
    callMemberFunction(connection, std::move(arguments), replyArguments, object, function);
    replyEncoder << replyArguments;
}

template<typename T, typename C, typename MF>
void handleMessage(Connection* connection, MessageDecoder& decoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments))
        return;
    callMemberFunction(connection, std::move(arguments), object, function);
}

template<typename T, typename C, typename MF>
void handleMessageVariadic(MessageDecoder& decoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments))
        return;
    callMemberFunction(std::move(arguments), decoder, object, function);
}

template<typename T, typename C, typename MF>
void handleMessageVariadic(MessageDecoder& decoder, MessageEncoder& replyEncoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments))
        return;

    typename T::Reply::ValueType replyArguments;
    callMemberFunction(std::move(arguments), decoder, replyArguments, object, function);
    replyEncoder << replyArguments;
}

template<typename T, typename C, typename MF>
void handleMessageDelayed(Connection* connection, MessageDecoder& decoder, std::unique_ptr<MessageEncoder>& replyEncoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments))
        return;

    RefPtr<typename T::DelayedReply> delayedReply = adoptRef(new typename T::DelayedReply(connection, std::move(replyEncoder)));
    callMemberFunction(std::move(arguments), delayedReply.release(), object, function);
}

} // namespace CoreIPC

#endif // HandleMessage_h
