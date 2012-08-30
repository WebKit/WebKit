/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_ObjcRuntimeExtras_h
#define WTF_ObjcRuntimeExtras_h

template<typename RetType = id>
RetType wtfObjcMsgSend(id target, SEL selector)
{
    return reinterpret_cast<RetType (*)(id, SEL)>(objc_msgSend)(target, selector);
}

template<typename RetType = id, typename Arg1Type>
RetType wtfObjcMsgSend(id target, SEL selector, Arg1Type arg1)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type)>(objc_msgSend)(target, selector, arg1);
}

template<typename RetType = id, typename Arg1Type, typename Arg2Type>
RetType wtfObjcMsgSend(id target, SEL selector, Arg1Type arg1, Arg2Type arg2)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type, Arg2Type)>(objc_msgSend)(target, selector, arg1, arg2);
}

template<typename RetType = id, typename Arg1Type, typename Arg2Type, typename Arg3Type>
RetType wtfObjcMsgSend(id target, SEL selector, Arg1Type arg1, Arg2Type arg2, Arg3Type arg3)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type, Arg2Type, Arg3Type)>(objc_msgSend)(target, selector, arg1, arg2, arg3);
}

template<typename RetType = id, typename Arg1Type, typename Arg2Type, typename Arg3Type, typename Arg4Type>
RetType wtfObjcMsgSend(id target, SEL selector, Arg1Type arg1, Arg2Type arg2, Arg3Type arg3, Arg4Type arg4)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type, Arg2Type, Arg3Type, Arg4Type)>(objc_msgSend)(target, selector, arg1, arg2, arg3, arg4);
}

template<typename RetType = id, typename Arg1Type, typename Arg2Type, typename Arg3Type, typename Arg4Type, typename Arg5Type>
RetType wtfObjcMsgSend(id target, SEL selector, Arg1Type arg1, Arg2Type arg2, Arg3Type arg3, Arg4Type arg4, Arg5Type arg5)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type)>(objc_msgSend)(target, selector, arg1, arg2, arg3, arg4, arg5);
}

template<typename RetType = id>
RetType wtfCallIMP(IMP implementation, id target, SEL selector)
{
    return reinterpret_cast<RetType (*)(id, SEL)>(implementation)(target, selector);
}

template<typename RetType = id, typename Arg1Type>
RetType wtfCallIMP(IMP implementation, id target, SEL selector, Arg1Type arg1)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type)>(implementation)(target, selector, arg1);
}

template<typename RetType = id, typename Arg1Type, typename Arg2Type>
RetType wtfCallIMP(IMP implementation, id target, SEL selector, Arg1Type arg1, Arg2Type arg2)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type, Arg2Type)>(implementation)(target, selector, arg1, arg2);
}

template<typename RetType = id, typename Arg1Type, typename Arg2Type, typename Arg3Type>
RetType wtfCallIMP(IMP implementation, id target, SEL selector, Arg1Type arg1, Arg2Type arg2, Arg3Type arg3)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type, Arg2Type, Arg3Type)>(implementation)(target, selector, arg1, arg2, arg3);
}

template<typename RetType = id, typename Arg1Type, typename Arg2Type, typename Arg3Type, typename Arg4Type>
RetType wtfCallIMP(IMP implementation, id target, SEL selector, Arg1Type arg1, Arg2Type arg2, Arg3Type arg3, Arg4Type arg4)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type, Arg2Type, Arg3Type, Arg4Type)>(implementation)(target, selector, arg1, arg2, arg3, arg4);
}

template<typename RetType = id, typename Arg1Type, typename Arg2Type, typename Arg3Type, typename Arg4Type, typename Arg5Type>
RetType wtfCallIMP(IMP implementation, id target, SEL selector, Arg1Type arg1, Arg2Type arg2, Arg3Type arg3, Arg4Type arg4, Arg5Type arg5)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type)>(implementation)(target, selector, arg1, arg2, arg3, arg4, arg5);
}

template<typename RetType = id, typename Arg1Type, typename Arg2Type, typename Arg3Type, typename Arg4Type, typename Arg5Type, typename Arg6Type>
RetType wtfCallIMP(IMP implementation, id target, SEL selector, Arg1Type arg1, Arg2Type arg2, Arg3Type arg3, Arg4Type arg4, Arg5Type arg5, Arg6Type arg6)
{
    return reinterpret_cast<RetType (*)(id, SEL, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type)>(implementation)(target, selector, arg1, arg2, arg3, arg4, arg5, arg6);
}

#endif
