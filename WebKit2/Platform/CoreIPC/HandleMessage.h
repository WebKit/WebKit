#ifndef HandleMessage_h
#define HandleMessage_h

namespace CoreIPC {

template<typename T> struct RemoveReference { typedef T Type; };
template<typename T> struct RemoveReference<T&> { typedef T Type; };

template<typename T, typename C>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)())
{
    (object->*function)();
}

template<typename T, typename C, typename P>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type argument;
    if (!arguments->decode(argument))
        return;
    (object->*function)(argument);
}

template<typename T, typename C, typename P1, typename P2>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P1, P2))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;
    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!arguments->decode(secondArgument))
        return;
    (object->*function)(firstArgument, secondArgument);
}

template<typename T, typename C, typename P1, typename P2, typename P3>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P1, P2, P3))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;
    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!arguments->decode(secondArgument))
        return;
    typename RemoveReference<typename T::ThirdArgumentType>::Type thirdArgument;
    if (!arguments->decode(thirdArgument))
        return;
    (object->*function)(firstArgument, secondArgument, thirdArgument);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P1, P2, P3, P4, P5, P6))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;
    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!arguments->decode(secondArgument))
        return;
    typename RemoveReference<typename T::ThirdArgumentType>::Type thirdArgument;
    if (!arguments->decode(thirdArgument))
        return;
    typename RemoveReference<typename T::FourthArgumentType>::Type fourthArgument;
    if (!arguments->decode(fourthArgument))
        return;
    typename RemoveReference<typename T::FifthArgumentType>::Type fifthArgument;
    if (!arguments->decode(fifthArgument))
        return;
    typename RemoveReference<typename T::SixthArgumentType>::Type sixthArgument;
    if (!arguments->decode(sixthArgument))
        return;
    (object->*function)(firstArgument, secondArgument, thirdArgument, fourthArgument, fifthArgument, sixthArgument);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P1, P2, P3, P4, P5, P6, P7))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;
    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!arguments->decode(secondArgument))
        return;
    typename RemoveReference<typename T::ThirdArgumentType>::Type thirdArgument;
    if (!arguments->decode(thirdArgument))
        return;
    typename RemoveReference<typename T::FourthArgumentType>::Type fourthArgument;
    if (!arguments->decode(fourthArgument))
        return;
    typename RemoveReference<typename T::FifthArgumentType>::Type fifthArgument;
    if (!arguments->decode(fifthArgument))
        return;
    typename RemoveReference<typename T::SixthArgumentType>::Type sixthArgument;
    if (!arguments->decode(sixthArgument))
        return;
    typename RemoveReference<typename T::SeventhArgumentType>::Type seventhArgument;
    if (!arguments->decode(seventhArgument))
        return;
    (object->*function)(firstArgument, secondArgument, thirdArgument, fourthArgument, fifthArgument, sixthArgument, seventhArgument);
}

template<typename T, typename C, typename P1>
void handleMessage(ArgumentDecoder* arguments, ArgumentEncoder* reply, C* object, void (C::*function)(P1))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;

    (object->*function)(firstArgument);
}

template<typename T, typename C, typename P1, typename R1>
void handleMessage(ArgumentDecoder* arguments, ArgumentEncoder* reply, C* object, void (C::*function)(P1, R1&))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;

    typename RemoveReference<typename T::Reply::FirstArgumentType>::Type firstReplyArgument;
    (object->*function)(firstArgument, firstReplyArgument);
    reply->encode(firstReplyArgument);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename R1>
void handleMessage(ArgumentDecoder* arguments, ArgumentEncoder* reply, C* object, void (C::*function)(P1, P2, P3, R1&))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;

    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!arguments->decode(secondArgument))
        return;
    
    typename RemoveReference<typename T::ThirdArgumentType>::Type thirdArgument;
    if (!arguments->decode(thirdArgument))
        return;
    
    typename RemoveReference<typename T::Reply::FirstArgumentType>::Type firstReplyArgument;
    (object->*function)(firstArgument, secondArgument, thirdArgument, firstReplyArgument);
    reply->encode(firstReplyArgument);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename P4, typename R1>
void handleMessage(ArgumentDecoder* arguments, ArgumentEncoder* reply, C* object, void (C::*function)(P1, P2, P3, P4, R1&))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;

    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!arguments->decode(secondArgument))
        return;
    
    typename RemoveReference<typename T::ThirdArgumentType>::Type thirdArgument;
    if (!arguments->decode(thirdArgument))
        return;

    typename RemoveReference<typename T::FourthArgumentType>::Type fourthArgument;
    if (!arguments->decode(fourthArgument))
        return;

    typename RemoveReference<typename T::Reply::FirstArgumentType>::Type firstReplyArgument;
    (object->*function)(firstArgument, secondArgument, thirdArgument, fourthArgument, firstReplyArgument);
    reply->encode(firstReplyArgument);
}

template<typename T, typename C, typename P1, typename P2, typename R1>
void handleMessage(ArgumentDecoder* arguments, ArgumentEncoder* reply, C* object, void (C::*function)(P1, P2, R1&))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;

    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!arguments->decode(secondArgument))
        return;
    
    typename RemoveReference<typename T::Reply::FirstArgumentType>::Type firstReplyArgument;
    (object->*function)(firstArgument, secondArgument, firstReplyArgument);
    reply->encode(firstReplyArgument);
}

} // namespace CoreIPC

#endif // HandleMessage_h
