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

} // namespace CoreIPC

#endif // HandleMessage_h
