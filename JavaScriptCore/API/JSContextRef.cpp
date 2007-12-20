// -*- mode: c++; c-basic-offset: 4 -*-
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

#include "APICast.h"
#include "JSCallbackObject.h"
#include "JSClassRef.h"
#include "JSGlobalObject.h"
#include "object.h"
#include <wtf/Platform.h>

using namespace KJS;

JSGlobalContextRef JSGlobalContextCreate(JSClassRef globalObjectClass)
{
    JSLock lock;

    if (!globalObjectClass) {
        JSGlobalObject* globalObject = new JSGlobalObject;
        return JSGlobalContextRetain(toGlobalRef(globalObject->globalExec()));
    }

    JSGlobalObject* globalObject = new JSCallbackObject<JSGlobalObject>(globalObjectClass);
    JSGlobalContextRef ctx = toGlobalRef(globalObject->globalExec());
    JSValue* prototype = globalObjectClass->prototype(ctx);
    if (!prototype)
        prototype = jsNull();
    globalObject->reset(prototype);
    return JSGlobalContextRetain(ctx);
}

JSGlobalContextRef JSGlobalContextRetain(JSGlobalContextRef ctx)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    gcProtect(exec->dynamicGlobalObject());
    return ctx;
}

void JSGlobalContextRelease(JSGlobalContextRef ctx)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    gcUnprotect(exec->dynamicGlobalObject());
}

JSObjectRef JSContextGetGlobalObject(JSContextRef ctx)
{
    ExecState* exec = toJS(ctx);
    return toRef(exec->dynamicGlobalObject());
}
