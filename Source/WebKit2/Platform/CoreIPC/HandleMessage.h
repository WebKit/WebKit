#ifndef HandleMessage_h
#define HandleMessage_h

#include "Arguments.h"

namespace CoreIPC {

// Dispatch functions with no reply arguments.

template<typename C, typename MF>
void callMemberFunction(const Arguments0&, C* object, MF function)
{
    (object->*function)();
}

template<typename C, typename MF, typename P1>
void callMemberFunction(const Arguments1<P1>& args, C* object, MF function)
{
    (object->*function)(args.argument1);
}

template<typename C, typename MF, typename P1, typename P2>
void callMemberFunction(const Arguments2<P1, P2>& args, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2);
}

template<typename C, typename MF, typename P1, typename P2, typename P3>
void callMemberFunction(const Arguments3<P1, P2, P3>& args, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4>
void callMemberFunction(const Arguments4<P1, P2, P3, P4>& args, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5>
void callMemberFunction(const Arguments5<P1, P2, P3, P4, P5>& args, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5);
}
    
template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
void callMemberFunction(const Arguments6<P1, P2, P3, P4, P5, P6>& args, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
void callMemberFunction(const Arguments7<P1, P2, P3, P4, P5, P6, P7>& args, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, args.argument7);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
void callMemberFunction(const Arguments8<P1, P2, P3, P4, P5, P6, P7, P8>& args, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, args.argument7, args.argument8);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
void callMemberFunction(const Arguments10<P1, P2, P3, P4, P5, P6, P7, P8, P9, P10>& args, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, args.argument7, args.argument8, args.argument9, args.argument10);
}

// Dispatch functions with reply arguments.

template<typename C, typename MF>
void callMemberFunction(const Arguments0&, Arguments0&, C* object, MF function)
{
    (object->*function)();
}

template<typename C, typename MF, typename R1>
void callMemberFunction(const Arguments0&, Arguments1<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(replyArgs.argument1);
}

template<typename C, typename MF, typename R1, typename R2>
void callMemberFunction(const Arguments0&, Arguments2<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(replyArgs.argument1, replyArgs.argument2);
}

template<typename C, typename MF, typename P1>
void callMemberFunction(const Arguments1<P1>& args, Arguments0&, C* object, MF function)
{
    (object->*function)(args.argument1);
}

template<typename C, typename MF, typename P1, typename R1>
void callMemberFunction(const Arguments1<P1>& args, Arguments1<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, replyArgs.argument1);
}

template<typename C, typename MF, typename P1, typename R1, typename R2>
void callMemberFunction(const Arguments1<P1>& args, Arguments2<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, replyArgs.argument1, replyArgs.argument2);
}

template<typename C, typename MF, typename P1, typename R1, typename R2, typename R3>
void callMemberFunction(const Arguments1<P1>& args, Arguments3<R1, R2, R3>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, replyArgs.argument1, replyArgs.argument2, replyArgs.argument3);
}

template<typename C, typename MF, typename P1, typename P2>
void callMemberFunction(const Arguments2<P1, P2>& args, Arguments0&, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2);
}

template<typename C, typename MF, typename P1, typename R1, typename R2, typename R3, typename R4>
void callMemberFunction(const Arguments1<P1>& args, Arguments4<R1, R2, R3, R4>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, replyArgs.argument1, replyArgs.argument2, replyArgs.argument3, replyArgs.argument4);
}

template<typename C, typename MF, typename P1, typename P2, typename R1>
void callMemberFunction(const Arguments2<P1, P2>& args, Arguments1<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, replyArgs.argument1);
}

template<typename C, typename MF, typename P1, typename P2, typename R1, typename R2>
void callMemberFunction(const Arguments2<P1, P2>& args, Arguments2<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, replyArgs.argument1, replyArgs.argument2);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename R1>
void callMemberFunction(const Arguments3<P1, P2, P3>& args, Arguments1<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, replyArgs.argument1);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename R1, typename R2>
void callMemberFunction(const Arguments3<P1, P2, P3>& args, Arguments2<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, replyArgs.argument1, replyArgs.argument2);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename R1>
void callMemberFunction(const Arguments4<P1, P2, P3, P4>& args, Arguments1<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, replyArgs.argument1);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename R1>
void callMemberFunction(const Arguments6<P1, P2, P3, P4, P5, P6>& args, Arguments1<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, replyArgs.argument1);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename R1>
void callMemberFunction(const Arguments7<P1, P2, P3, P4, P5, P6, P7>& args, Arguments1<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, args.argument7, replyArgs.argument1);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename R1>
void callMemberFunction(const Arguments8<P1, P2, P3, P4, P5, P6, P7, P8>& args, Arguments1<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, args.argument7, args.argument8, replyArgs.argument1);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename R1, typename R2>
void callMemberFunction(const Arguments4<P1, P2, P3, P4>& args, Arguments2<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, replyArgs.argument1, replyArgs.argument2);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename R1, typename R2>
void callMemberFunction(const Arguments5<P1, P2, P3, P4, P5>& args, Arguments2<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, replyArgs.argument1, replyArgs.argument2);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename R1, typename R2>
void callMemberFunction(const Arguments6<P1, P2, P3, P4, P5, P6>& args, Arguments2<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, replyArgs.argument1, replyArgs.argument2);
}
    
template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename R1, typename R2, typename R3>
void callMemberFunction(const Arguments4<P1, P2, P3, P4>& args, Arguments3<R1, R2, R3>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, replyArgs.argument1, replyArgs.argument2, replyArgs.argument3);
}

// Dispatch functions with delayed reply arguments.
template<typename C, typename MF, typename P1, typename R>
void callMemberFunction(const Arguments1<P1>& args, PassRefPtr<R> delayedReply, C* object, MF function)
{
    (object->*function)(args.argument1, delayedReply);
}

// Dispatch functions with connection parameter.
template<typename C, typename MF, typename P1>
void callMemberFunction(Connection* connection, const Arguments1<P1>& args, C* object, MF function)
{
    (object->*function)(connection, args.argument1);
}

template<typename C, typename MF, typename P1, typename P2>
void callMemberFunction(Connection* connection, const Arguments2<P1, P2>& args, C* object, MF function)
{
    (object->*function)(connection, args.argument1, args.argument2);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4>
void callMemberFunction(Connection* connection, const Arguments4<P1, P2, P3, P4>& args, C* object, MF function)
{
    (object->*function)(connection, args.argument1, args.argument2, args.argument3, args.argument4);
}

// Variadic dispatch functions.

template<typename C, typename MF>
void callMemberFunction(const Arguments0&, ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    (object->*function)(argumentDecoder);
}

template<typename C, typename MF, typename P1>
void callMemberFunction(const Arguments1<P1>& args, ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    (object->*function)(args.argument1, argumentDecoder);
}

template<typename C, typename MF, typename P1, typename P2>
void callMemberFunction(const Arguments2<P1, P2>& args, ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, argumentDecoder);
}

template<typename C, typename MF, typename P1, typename P2, typename P3>
void callMemberFunction(const Arguments3<P1, P2, P3>& args, ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, argumentDecoder);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4>
void callMemberFunction(const Arguments4<P1, P2, P3, P4>& args, ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, argumentDecoder);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5>
void callMemberFunction(const Arguments5<P1, P2, P3, P4, P5>& args, ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, argumentDecoder);
}
    
template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
void callMemberFunction(const Arguments6<P1, P2, P3, P4, P5, P6>& args, ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, argumentDecoder);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
void callMemberFunction(const Arguments7<P1, P2, P3, P4, P5, P6, P7>& args, ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, args.argument7, argumentDecoder);
}

// Variadic dispatch functions with non-variadic reply arguments.

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename R1, typename R2, typename R3>
void callMemberFunction(const Arguments4<P1, P2, P3, P4>& args, ArgumentDecoder* argumentDecoder, Arguments3<R1, R2, R3>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, argumentDecoder, replyArgs.argument1, replyArgs.argument2, replyArgs.argument3);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename R1, typename R2>
void callMemberFunction(const Arguments6<P1, P2, P3, P4, P5, P6>& args, ArgumentDecoder* argumentDecoder, Arguments2<R1, R2>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, argumentDecoder, replyArgs.argument1, replyArgs.argument2);
}

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename R1, typename R2, typename R3>
void callMemberFunction(const Arguments6<P1, P2, P3, P4, P5, P6>& args, ArgumentDecoder* argumentDecoder, Arguments3<R1, R2, R3>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, args.argument5, args.argument6, argumentDecoder, replyArgs.argument1, replyArgs.argument2, replyArgs.argument3);
}

// Main dispatch functions

template<typename T, typename C, typename MF>
void handleMessage(ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    typename T::DecodeType::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;
    callMemberFunction(arguments, object, function);
}

template<typename T, typename C, typename MF>
void handleMessage(ArgumentDecoder* argumentDecoder, ArgumentEncoder* replyEncoder, C* object, MF function)
{
    typename T::DecodeType::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;

    typename T::Reply::ValueType replyArguments;
    callMemberFunction(arguments, replyArguments, object, function);
    replyEncoder->encode(replyArguments);
}

template<typename T, typename C, typename MF>
void handleMessageOnConnectionQueue(Connection* connection, ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    typename T::DecodeType::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;
    callMemberFunction(connection, arguments, object, function);
}

template<typename T, typename C, typename MF>
void handleMessageVariadic(ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    typename T::DecodeType::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;
    callMemberFunction(arguments, argumentDecoder, object, function);
}


template<typename T, typename C, typename MF>
void handleMessageVariadic(ArgumentDecoder* argumentDecoder, ArgumentEncoder* replyEncoder, C* object, MF function)
{
    typename T::DecodeType::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;

    typename T::Reply::ValueType replyArguments;
    callMemberFunction(arguments, argumentDecoder, replyArguments, object, function);
    replyEncoder->encode(replyArguments);
}

template<typename T, typename C, typename MF>
void handleMessageDelayed(Connection* connection, ArgumentDecoder* argumentDecoder, OwnPtr<ArgumentEncoder>& replyEncoder, C* object, MF function)
{
    typename T::DecodeType::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;

    RefPtr<typename T::DelayedReply> delayedReply = adoptRef(new typename T::DelayedReply(connection, replyEncoder.release()));
    callMemberFunction(arguments, delayedReply.release(), object, function);
}

} // namespace CoreIPC

#endif // HandleMessage_h
