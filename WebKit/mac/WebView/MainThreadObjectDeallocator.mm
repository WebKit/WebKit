/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "MainThreadObjectDeallocator.h"

#include <objc/objc-runtime.h>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>

#ifdef BUILDING_ON_TIGER
static inline IMP method_getImplementation(Method method) 
{
    return method->method_imp;
}
#endif

typedef std::pair<Class, id> ClassAndIdPair;

static void deallocCallback(void* context)
{
    ClassAndIdPair* pair = static_cast<ClassAndIdPair*>(context);
    
    Method method = class_getInstanceMethod(pair->first, @selector(dealloc));
    
    IMP imp = method_getImplementation(method);
    imp(pair->second, @selector(dealloc));
    
    delete pair;
}

bool scheduleDeallocateOnMainThread(Class cls, id object)
{
    ASSERT([object isKindOfClass:cls]);
    
    if (pthread_main_np() != 0)
        return false;

    ClassAndIdPair* pair = new ClassAndIdPair(cls, object);
    callOnMainThread(deallocCallback, pair);
    return true;
}
