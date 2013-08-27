/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#include <objc/message.h>

#ifdef __cplusplus
template<typename RetType, typename... ArgTypes>
inline RetType wtfObjcMsgSend(id target, SEL selector, ArgTypes... args)
{
    return reinterpret_cast<RetType (*)(id, SEL, ArgTypes...)>(objc_msgSend)(target, selector, args...);
}

template<typename RetType, typename... ArgTypes>
inline RetType wtfCallIMP(IMP implementation, id target, SEL selector, ArgTypes... args)
{
    return reinterpret_cast<RetType (*)(id, SEL, ArgTypes...)>(implementation)(target, selector, args...);
}
#endif // __cplusplus

#ifdef __OBJC__
// Use HardAutorelease to return an object made by a Core Foundation
// "create" or "copy" function as an autoreleased and garbage collected
// object. CF objects need to be "made collectable" for autorelease to work
// properly under GC.
inline id HardAutorelease(CFTypeRef object)
{
#ifndef OBJC_NO_GC
    if (object)
        CFMakeCollectable(object);
#endif
#if !__has_feature(objc_arc)
    [(id)object autorelease];
#endif
    return (id)object;
}
#endif // __OBJC__

#endif // WTF_ObjcRuntimeExtras_h
