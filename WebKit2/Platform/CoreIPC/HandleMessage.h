#ifndef HandleMessage_h
#define HandleMessage_h

namespace CoreIPC {

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

template<typename C, typename MF, typename P1, typename P2, typename P3, typename P4, typename R1>
void callMemberFunction(const Arguments4<P1, P2, P3, P4>& args, Arguments1<R1>& replyArgs, C* object, MF function)
{
    (object->*function)(args.argument1, args.argument2, args.argument3, args.argument4, replyArgs.argument1);
}

template<typename T, typename C, typename MF>
void handleMessage(ArgumentDecoder* argumentDecoder, C* object, MF function)
{
    typename T::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;
    callMemberFunction(arguments, object, function);
}

template<typename T, typename C, typename MF>
void handleMessage(ArgumentDecoder* argumentDecoder, ArgumentEncoder* replyEncoder, C* object, MF function)
{
    typename T::ValueType arguments;
    if (!argumentDecoder->decode(arguments))
        return;

    typename T::Reply::ValueType replyArguments;
    callMemberFunction(arguments, replyArguments, object, function);
    replyEncoder->encode(replyArguments);
}

} // namespace CoreIPC

#endif // HandleMessage_h
