/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

template<typename T, typename C>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(CoreIPC::ArgumentDecoder*))
{
    (object->*function)(arguments);
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

template<typename T, typename C, typename P1>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P1, CoreIPC::ArgumentDecoder*))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;
    (object->*function)(firstArgument, arguments);
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

template<typename T, typename C, typename P1, typename P2>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P1, P2, CoreIPC::ArgumentDecoder*))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;
    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!arguments->decode(secondArgument))
        return;
    (object->*function)(firstArgument, secondArgument, arguments);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename P4>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P1, P2, P3, P4))
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
    (object->*function)(firstArgument, secondArgument, thirdArgument, fourthArgument);
}

template<typename T, typename C, typename P1, typename P2, typename P3>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P1, P2, P3, CoreIPC::ArgumentDecoder*))
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
    (object->*function)(firstArgument, secondArgument, thirdArgument, arguments);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename P4, typename P5>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P1, P2, P3, P4, P5))
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
    (object->*function)(firstArgument, secondArgument, thirdArgument, fourthArgument, fifthArgument);
}

template<typename T, typename C, typename P1, typename P2, typename P3, typename P4>
void handleMessage(ArgumentDecoder* arguments, C* object, void (C::*function)(P1, P2, P3, P4, CoreIPC::ArgumentDecoder*))
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
    (object->*function)(firstArgument, secondArgument, thirdArgument, fourthArgument, arguments);
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

template<typename T, typename C, typename P1, typename P2>
void handleMessage(ArgumentDecoder* arguments, ArgumentEncoder* reply, C* object, void (C::*function)(P1, P2))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;

    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!arguments->decode(secondArgument))
        return;

    (object->*function)(firstArgument, secondArgument);
}

template<typename T, typename C, typename P1, typename P2>
void handleMessage(ArgumentDecoder* arguments, ArgumentEncoder* reply, C* object, void (C::*function)(P1, const P2&))
{
    typename RemoveReference<typename T::FirstArgumentType>::Type firstArgument;
    if (!arguments->decode(firstArgument))
        return;

    typename RemoveReference<typename T::SecondArgumentType>::Type secondArgument;
    if (!arguments->decode(secondArgument))
        return;

    (object->*function)(firstArgument, secondArgument);
}

template<typename T, typename C, typename R1>
void handleMessage(ArgumentDecoder* arguments, ArgumentEncoder* reply, C* object, void (C::*function)(R1&))
{
    typename RemoveReference<typename T::Reply::FirstArgumentType>::Type firstReplyArgument;
    (object->*function)(firstReplyArgument);
    reply->encode(firstReplyArgument);
}

template<typename T, typename C, typename R1, typename R2>
void handleMessage(ArgumentDecoder* arguments, ArgumentEncoder* reply, C* object, void (C::*function)(R1&, R2&))
{
    typename RemoveReference<typename T::Reply::FirstArgumentType>::Type firstReplyArgument;
    typename RemoveReference<typename T::Reply::SecondArgumentType>::Type secondReplyArgument;
    (object->*function)(firstReplyArgument, secondReplyArgument);
    reply->encode(firstReplyArgument);
    reply->encode(secondReplyArgument);
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

} // namespace CoreIPC

#endif // HandleMessage_h
