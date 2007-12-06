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
#include "JSBase.h"

#include "APICast.h"
#include <kjs/ExecState.h>
#include <kjs/JSGlobalObject.h>
#include <kjs/JSLock.h>
#include <kjs/interpreter.h>
#include <kjs/object.h>

using namespace KJS;

JSValueRef JSEvaluateScript(JSContextRef ctx, JSStringRef script, JSObjectRef thisObject, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsThisObject = toJS(thisObject);
    UString::Rep* scriptRep = toJS(script);
    UString::Rep* sourceURLRep = sourceURL ? toJS(sourceURL) : &UString::Rep::null;
    // Interpreter::evaluate sets "this" to the global object if it is NULL
    Completion completion = Interpreter::evaluate(exec->dynamicGlobalObject()->globalExec(), UString(sourceURLRep), startingLineNumber, UString(scriptRep), jsThisObject);

    if (completion.complType() == Throw) {
        if (exception)
            *exception = toRef(completion.value());
        return 0;
    }
    
    if (completion.value())
        return toRef(completion.value());
    
    // happens, for example, when the only statement is an empty (';') statement
    return toRef(jsUndefined());
}

bool JSCheckScriptSyntax(JSContextRef ctx, JSStringRef script, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception)
{
    JSLock lock;

    ExecState* exec = toJS(ctx);
    UString::Rep* scriptRep = toJS(script);
    UString::Rep* sourceURLRep = sourceURL ? toJS(sourceURL) : &UString::Rep::null;
    Completion completion = Interpreter::checkSyntax(exec->dynamicGlobalObject()->globalExec(), UString(sourceURLRep), startingLineNumber, UString(scriptRep));
    if (completion.complType() == Throw) {
        if (exception)
            *exception = toRef(completion.value());
        return false;
    }
    
    return true;
}

void JSGarbageCollect(JSContextRef)
{
    JSLock lock;
    if (!Collector::isBusy())
        Collector::collect();
    // FIXME: Perhaps we should trigger a second mark and sweep
    // once the garbage collector is done if this is called when
    // the collector is busy.
}
