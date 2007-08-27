// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include <wtf/Platform.h>
#include "APICast.h"
#include "JSContextRef.h"

#include "JSCallbackObject.h"
#include "completion.h"
#include "interpreter.h"
#include "object.h"

using namespace KJS;

JSGlobalContextRef JSGlobalContextCreate(JSClassRef globalObjectClass)
{
    JSLock lock;

    JSObject* globalObject;
    if (globalObjectClass)
        // Specify jsNull() as the prototype.  Interpreter will fix it up to point at builtinObjectPrototype() in its constructor
        globalObject = new JSCallbackObject(0, globalObjectClass, jsNull(), 0);
    else
        globalObject = new JSObject();

    Interpreter* interpreter = new Interpreter(globalObject); // adds the built-in object prototype to the global object
    if (globalObjectClass)
        static_cast<JSCallbackObject*>(globalObject)->initializeIfNeeded(interpreter->globalExec());
    JSGlobalContextRef ctx = reinterpret_cast<JSGlobalContextRef>(interpreter->globalExec());
    return JSGlobalContextRetain(ctx);
}

JSGlobalContextRef JSGlobalContextRetain(JSGlobalContextRef ctx)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    exec->dynamicInterpreter()->ref();
    return ctx;
}

void JSGlobalContextRelease(JSGlobalContextRef ctx)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    exec->dynamicInterpreter()->deref();
}

JSObjectRef JSContextGetGlobalObject(JSContextRef ctx)
{
    ExecState* exec = toJS(ctx);
    return toRef(exec->dynamicInterpreter()->globalObject());
}
