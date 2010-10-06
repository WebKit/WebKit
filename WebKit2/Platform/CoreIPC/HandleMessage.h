#ifndef HandleMessage_h
#define HandleMessage_h

namespace CoreIPC {

template<typename T> struct RemoveReference { typedef T Type; };
template<typename T> struct RemoveReference<T&> { typedef T Type; };

template<typename T, typename C>
void handleMessage(ArgumentDecoder* argumentDecoder, C* object, void (C::*function)())
{
    (object->*function)();
}

template<typename T, typename C, typename P>
void handleMessage(ArgumentDecoder* argumentDecoder, C* object, void (C::*function)(P))
{
    typename T::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;
    (object->*function)(arguments.argument1);
}

template<typename T, typename C, typename P1, typename P2>
void handleMessage(ArgumentDecoder* argumentDecoder, C* object, void (C::*function)(P1, P2))
{
    typename T::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;
    (object->*function)(arguments.argument1, arguments.argument2);
}

template<typename T, typename C, typename P1, typename P2, typename P3>
void handleMessage(ArgumentDecoder* argumentDecoder, C* object, void (C::*function)(P1, P2, P3))
{
    typename T::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;
    (object->*function)(arguments.argument1, arguments.argument2, arguments.argument3);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
void handleMessage(ArgumentDecoder* argumentDecoder, C* object, void (C::*function)(P1, P2, P3, P4, P5, P6))
{
    typename T::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;
    (object->*function)(arguments.argument1, arguments.argument2, arguments.argument3, arguments.argument4, arguments.argument5, arguments.argument6);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
void handleMessage(ArgumentDecoder* argumentDecoder, C* object, void (C::*function)(P1, P2, P3, P4, P5, P6, P7))
{
    typename T::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;
    (object->*function)(arguments.argument1, arguments.argument2, arguments.argument3, arguments.argument4, arguments.argument5, arguments.argument6, arguments.argument7);
}

template<typename T, typename C, typename P1>
void handleMessage(ArgumentDecoder* argumentDecoder, ArgumentEncoder* replyEncoder, C* object, void (C::*function)(P1))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!argumentDecoder->decode(firstArgument))
        return;

    (object->*function)(firstArgument);
}

template<typename T, typename C, typename P1, typename R1>
void handleMessage(ArgumentDecoder* argumentDecoder, ArgumentEncoder* replyEncoder, C* object, void (C::*function)(P1, R1&))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!argumentDecoder->decode(firstArgument))
        return;

    typename RemoveReference<typename T::Reply::FirstArgumentType>::Type firstReplyArgument;
    (object->*function)(firstArgument, firstReplyArgument);
    replyEncoder->encode(firstReplyArgument);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename R1>
void handleMessage(ArgumentDecoder* argumentDecoder, ArgumentEncoder* replyEncoder, C* object, void (C::*function)(P1, P2, P3, R1&))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!argumentDecoder->decode(firstArgument))
        return;

    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!argumentDecoder->decode(secondArgument))
        return;
    
    typename RemoveReference<typename T::ThirdArgumentType>::Type thirdArgument;
    if (!argumentDecoder->decode(thirdArgument))
        return;
    
    typename RemoveReference<typename T::Reply::FirstArgumentType>::Type firstReplyArgument;
    (object->*function)(firstArgument, secondArgument, thirdArgument, firstReplyArgument);
    replyEncoder->encode(firstReplyArgument);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename P4, typename R1>
void handleMessage(ArgumentDecoder* argumentDecoder, ArgumentEncoder* replyEncoder, C* object, void (C::*function)(P1, P2, P3, P4, R1&))
{
    typename T::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;
    
    typename RemoveReference<typename T::Reply::FirstArgumentType>::Type firstReplyArgument;
    (object->*function)(arguments.argument1, arguments.argument2, arguments.argument3, arguments.argument4, firstReplyArgument);
    replyEncoder->encode(firstReplyArgument);
}

template<typename T, typename C, typename P1, typename P2, typename R1>
void handleMessage(ArgumentDecoder* argumentDecoder, ArgumentEncoder* replyEncoder, C* object, void (C::*function)(P1, P2, R1&))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!argumentDecoder->decode(firstArgument))
        return;

    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!argumentDecoder->decode(secondArgument))
        return;
    
    typename RemoveReference<typename T::Reply::FirstArgumentType>::Type firstReplyArgument;
    (object->*function)(firstArgument, secondArgument, firstReplyArgument);
    replyEncoder->encode(firstReplyArgument);
}

} // namespace CoreIPC

#endif // HandleMessage_h
