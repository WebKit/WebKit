/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebCoreObjCExtras.h"

#import <utility>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/Threading.h>

#if ASSERT_ENABLED

// This is like the isKindOfClass: method, bypassing it to get the correct answer for our purposes even for classes that override it.
// At the time of this writing, that included WebKit's WKObject class.
static bool safeIsKindOfClass(id object, Class testClass)
{
    if (!object)
        return false;
    for (auto ancestorClass = object_getClass(object); ancestorClass; ancestorClass = class_getSuperclass(ancestorClass)) {
        if (ancestorClass == testClass)
            return true;
    }
    return false;
}

#endif

bool WebCoreObjCScheduleDeallocateOnMainThread(Class deallocMethodClass, id object)
{
    ASSERT(safeIsKindOfClass(object, deallocMethodClass));

    if (isMainThread())
        return false;

    callOnMainThread([deallocMethodClass, object] {
        auto deallocSelector = sel_registerName("dealloc");
        wtfCallIMP<void>(method_getImplementation(class_getInstanceMethod(deallocMethodClass, deallocSelector)), object, deallocSelector);
    });

    return true;
}

bool WebCoreObjCScheduleDeallocateOnMainRunLoop(Class deallocMethodClass, id object)
{
    ASSERT(safeIsKindOfClass(object, deallocMethodClass));

    if (isMainRunLoop())
        return false;

    callOnMainRunLoop([deallocMethodClass, object] {
        auto deallocSelector = sel_registerName("dealloc");
        wtfCallIMP<void>(method_getImplementation(class_getInstanceMethod(deallocMethodClass, deallocSelector)), object, deallocSelector);
    });

    return true;
}
