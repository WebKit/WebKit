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

#include "APICast.h"
#include "JSContextRef.h"

#include "JSCallbackObject.h"
#include "completion.h"
#include "interpreter.h"
#include "object.h"

using namespace KJS;

JSContextRef JSContextCreate(JSClassRef globalObjectClass, JSObjectRef globalObjectPrototype)
{
    JSLock lock;

    JSObject* jsPrototype = toJS(globalObjectPrototype);

    JSObject* globalObject;
    if (globalObjectClass) {
        if (jsPrototype)
            globalObject = new JSCallbackObject(globalObjectClass, jsPrototype);
        else
            globalObject = new JSCallbackObject(globalObjectClass);
    } else {
        // creates a slightly more efficient object
        if (jsPrototype)
            globalObject = new JSObject(jsPrototype);
        else
            globalObject = new JSObject();
    }

    Interpreter* interpreter = new Interpreter(globalObject); // adds the built-in object prototype to the global object
    return toRef(interpreter->globalExec());
}

void JSContextDestroy(JSContextRef context)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    delete exec->dynamicInterpreter();
}

JSObjectRef JSContextGetGlobalObject(JSContextRef context)
{
    ExecState* exec = toJS(context);
    return toRef(exec->dynamicInterpreter()->globalObject());
}

bool JSEvaluate(JSContextRef context, JSValueRef thisValue, JSCharBufferRef script, JSCharBufferRef sourceURL, int startingLineNumber, JSValueRef* returnValue)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSValue* jsThisValue = toJS(thisValue);
    UString::Rep* scriptRep = toJS(script);
    UString::Rep* sourceURLRep = toJS(sourceURL);
    Completion completion = exec->dynamicInterpreter()->evaluate(UString(sourceURLRep), startingLineNumber, UString(scriptRep), jsThisValue);

    if (returnValue)
        *returnValue = completion.value() ? toRef(completion.value()) : toRef(jsUndefined());

    return completion.complType() != Throw;
}

bool JSCheckSyntax(JSContextRef context, JSCharBufferRef script)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    UString::Rep* rep = toJS(script);
    return exec->dynamicInterpreter()->checkSyntax(UString(rep));
}

bool JSContextHasException(JSContextRef context)
{
    ExecState* exec = toJS(context);
    return exec->hadException();
}

JSValueRef JSContextGetException(JSContextRef context)
{
    ExecState* exec = toJS(context);
    return toRef(exec->exception());
}

void JSContextClearException(JSContextRef context)
{
    ExecState* exec = toJS(context);
    if (exec->hadException())
        exec->clearException();
}

void JSContextSetException(JSContextRef context, JSValueRef value)
{
    ExecState* exec = toJS(context);
    JSValue* jsValue = toJS(value);
    exec->setException(jsValue);
}

