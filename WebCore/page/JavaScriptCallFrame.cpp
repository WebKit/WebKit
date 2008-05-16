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

// Evaluate some JavaScript code in the scope of this frame.
JSValue* JavaScriptCallFrame::evaluate(const UString& script, JSValue*& exception) const
{
    if (!m_exec)
        return jsNull();

    JSLock lock;

    ExecState* exec = m_exec;
    JSGlobalObject* globalObject = exec->dynamicGlobalObject();

    JSValue* savedException = exec->exception();
    exec->clearException();

    List args;
    args.append(jsString(script));
    JSValue* result = eval(exec, exec->scopeChain(), globalObject, globalObject, exec->thisValue(), args);

    if (exec->hadException())
        exception = exec->takeException();
    exec->setException(savedException);

    return result;
}

} // namespace WebCore
