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
#include "completion.h"
#include "OpaqueJSString.h"
#include <kjs/ExecState.h>
#include <kjs/InitializeThreading.h>
#include <kjs/interpreter.h>
#include <kjs/JSGlobalObject.h>
#include <kjs/JSObject.h>

using namespace KJS;

JSValueRef JSEvaluateScript(JSContextRef ctx, JSStringRef script, JSObjectRef thisObject, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap->registerThread();

    JSObject* jsThisObject = toJS(thisObject);

    // Interpreter::evaluate sets "this" to the global object if it is NULL
    JSGlobalObject* globalObject = exec->dynamicGlobalObject();
    Completion completion = Interpreter::evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), sourceURL->ustring(), startingLineNumber, script->ustring(), jsThisObject);

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
    ExecState* exec = toJS(ctx);
    exec->globalData().heap->registerThread();

    Completion completion = Interpreter::checkSyntax(exec->dynamicGlobalObject()->globalExec(), sourceURL->ustring(), startingLineNumber, script->ustring());
    if (completion.complType() == Throw) {
        if (exception)
            *exception = toRef(completion.value());
        return false;
    }
    
    return true;
}

void JSGarbageCollect(JSContextRef ctx)
{
    // We used to recommend passing NULL as an argument here, which caused the only heap to be collected.
    // As there is no longer a shared heap, the previously recommended usage became a no-op (but the GC
    // will happen when the context group is destroyed).
    // Because the function argument was originally ignored, some clients may pass their released context here,
    // in which case there is a risk of crashing if another thread performs GC on the same heap in between.
    if (!ctx)
        return;

    ExecState* exec = toJS(ctx);
    JSGlobalData& globalData = exec->globalData();
    Heap* heap = globalData.heap;

    if (!heap->isBusy())
        heap->collect();

    // FIXME: Perhaps we should trigger a second mark and sweep
    // once the garbage collector is done if this is called when
    // the collector is busy.
}
