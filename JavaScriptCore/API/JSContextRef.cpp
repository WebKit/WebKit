/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSContextRef.h"
#include "JSContextRefPrivate.h"

#include "APICast.h"
#include "InitializeThreading.h"
#include "JSCallbackObject.h"
#include "JSClassRef.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include <wtf/Platform.h>

#if PLATFORM(DARWIN)
#include <mach-o/dyld.h>

static const int32_t webkitFirstVersionWithConcurrentGlobalContexts = 0x2100500; // 528.5.0
#endif

using namespace JSC;

JSContextGroupRef JSContextGroupCreate()
{
    initializeThreading();
    return toRef(JSGlobalData::create().releaseRef());
}

JSContextGroupRef JSContextGroupRetain(JSContextGroupRef group)
{
    toJS(group)->ref();
    return group;
}

void JSContextGroupRelease(JSContextGroupRef group)
{
    toJS(group)->deref();
}

JSGlobalContextRef JSGlobalContextCreate(JSClassRef globalObjectClass)
{
    initializeThreading();
#if PLATFORM(DARWIN)
    // When running on Tiger or Leopard, or if the application was linked before JSGlobalContextCreate was changed
    // to use a unique JSGlobalData, we use a shared one for compatibility.
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    if (NSVersionOfLinkTimeLibrary("JavaScriptCore") <= webkitFirstVersionWithConcurrentGlobalContexts) {
#else
    {
#endif
        JSLock lock(LockForReal);
        return JSGlobalContextCreateInGroup(toRef(&JSGlobalData::sharedInstance()), globalObjectClass);
    }
#endif // PLATFORM(DARWIN)

    return JSGlobalContextCreateInGroup(0, globalObjectClass);
}

JSGlobalContextRef JSGlobalContextCreateInGroup(JSContextGroupRef group, JSClassRef globalObjectClass)
{
    initializeThreading();

    JSLock lock(LockForReal);

    RefPtr<JSGlobalData> globalData = group ? PassRefPtr<JSGlobalData>(toJS(group)) : JSGlobalData::create();

#if ENABLE(JSC_MULTIPLE_THREADS)
    globalData->makeUsableFromMultipleThreads();
#endif

    if (!globalObjectClass) {
        JSGlobalObject* globalObject = new (globalData.get()) JSGlobalObject;
        return JSGlobalContextRetain(toGlobalRef(globalObject->globalExec()));
    }

    JSGlobalObject* globalObject = new (globalData.get()) JSCallbackObject<JSGlobalObject>(globalObjectClass);
    ExecState* exec = globalObject->globalExec();
    JSValue prototype = globalObjectClass->prototype(exec);
    if (!prototype)
        prototype = jsNull();
    globalObject->resetPrototype(prototype);
    return JSGlobalContextRetain(toGlobalRef(exec));
}

JSGlobalContextRef JSGlobalContextRetain(JSGlobalContextRef ctx)
{
    ExecState* exec = toJS(ctx);
    JSLock lock(exec);

    JSGlobalData& globalData = exec->globalData();

    globalData.heap.registerThread();

    gcProtect(exec->dynamicGlobalObject());
    globalData.ref();
    return ctx;
}

void JSGlobalContextRelease(JSGlobalContextRef ctx)
{
    ExecState* exec = toJS(ctx);
    JSLock lock(exec);

    gcUnprotect(exec->dynamicGlobalObject());

    JSGlobalData& globalData = exec->globalData();
    if (globalData.refCount() == 2) { // One reference is held by JSGlobalObject, another added by JSGlobalContextRetain().
        // The last reference was released, this is our last chance to collect.
        ASSERT(!globalData.heap.protectedObjectCount());
        ASSERT(!globalData.heap.isBusy());
        globalData.heap.destroy();
    } else
        globalData.heap.collect();

    globalData.deref();
}

JSObjectRef JSContextGetGlobalObject(JSContextRef ctx)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    // It is necessary to call toThisObject to get the wrapper object when used with WebCore.
    return toRef(exec->lexicalGlobalObject()->toThisObject(exec));
}

JSContextGroupRef JSContextGetGroup(JSContextRef ctx)
{
    ExecState* exec = toJS(ctx);
    return toRef(&exec->globalData());
}

JSGlobalContextRef JSContextGetGlobalContext(JSContextRef ctx)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    return toGlobalRef(exec->lexicalGlobalObject()->globalExec());
}
