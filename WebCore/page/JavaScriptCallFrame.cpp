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

#include "config.h"
#include "JavaScriptCallFrame.h"

#include "PlatformString.h"
#include <kjs/JSGlobalObject.h>
#include <kjs/interpreter.h>
#include <kjs/object.h>
#include <kjs/value.h>

using namespace KJS;

namespace WebCore {

JavaScriptCallFrame::JavaScriptCallFrame(ExecState* exec, PassRefPtr<JavaScriptCallFrame> caller, int sourceID, int line)
    : m_exec(exec)
    , m_caller(caller)
    , m_sourceID(sourceID)
    , m_line(line)
{
}

JavaScriptCallFrame* JavaScriptCallFrame::caller()
{
    return m_caller.get();
}

String JavaScriptCallFrame::functionName() const
{
    if (!m_exec || !m_exec->scopeNode())
        return String();

    FunctionImp* function = m_exec->function();
    if (!function)
        return String();

    return String(function->functionName());
}

// Evaluate some JavaScript code in the context of this frame.
// The code is evaluated as if by "eval", and the result is returned.
// If there is an (uncaught) exception, it is returned as though _it_ were the result.

// FIXME: If "script" contains var declarations, the machinery to handle local variables
// efficiently in JavaScriptCore will not work properly. This could lead to crashes or
// incorrect variable values. So this is not appropriate for evaluating arbitrary scripts.

JSValue* JavaScriptCallFrame::evaluate(const UString& script) const
{
    if (!m_exec)
        return jsNull();

    JSLock lock;

    ExecState* exec = m_exec;
    JSGlobalObject* globalObject = exec->dynamicGlobalObject();

    // find "eval"
    JSObject* eval = 0;
    if (exec->scopeNode()) {  // "eval" won't work without context (i.e. at global scope)
        JSValue* v = globalObject->get(exec, "eval");
        if (v->isObject() && static_cast<JSObject*>(v)->implementsCall())
            eval = static_cast<JSObject*>(v);
        else
            // no "eval" - fallback operates on global exec state
            exec = globalObject->globalExec();
    }

    JSValue* savedException = exec->exception();
    exec->clearException();

    // evaluate
    JSValue* result = 0;
    if (eval) {
        List args;
        args.append(jsString(script));
        result = eval->call(exec, 0, args);
    } else
        // no "eval", or no context (i.e. global scope) - use global fallback
        result = Interpreter::evaluate(exec, UString(), 0, script.data(), script.size(), globalObject).value();

    if (exec->hadException())
        result = exec->exception();    // (may be redundant depending on which eval path was used)
    exec->setException(savedException);

    return result;
}

} // namespace WebCore
