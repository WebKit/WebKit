/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptFunctionCall.h"

#include "JSDOMBinding.h"
#include "ScriptString.h"
#include "ScriptValue.h"

#include <runtime/JSLock.h>
#include <runtime/UString.h>

using namespace JSC;

namespace WebCore {

ScriptFunctionCall::ScriptFunctionCall(ScriptState* exec, const ScriptObject& thisObject, const String& name)
    : m_exec(exec)
    , m_thisObject(thisObject)
    , m_name(name)
{
}

void ScriptFunctionCall::appendArgument(const ScriptObject& argument)
{
    m_arguments.append(argument.jsObject());
}

void ScriptFunctionCall::appendArgument(const ScriptString& argument)
{
    m_arguments.append(jsString(m_exec, argument));
}

void ScriptFunctionCall::appendArgument(const ScriptValue& argument)
{
    m_arguments.append(argument.jsValue());
}

void ScriptFunctionCall::appendArgument(const String& argument)
{
    JSLock lock(false);
    m_arguments.append(jsString(m_exec, argument));
}

void ScriptFunctionCall::appendArgument(const JSC::UString& argument)
{
    m_arguments.append(jsString(m_exec, argument));
}

void ScriptFunctionCall::appendArgument(JSC::JSValue argument)
{
    m_arguments.append(argument);
}

void ScriptFunctionCall::appendArgument(long long argument)
{
    JSLock lock(false);
    m_arguments.append(jsNumber(m_exec, argument));
}

void ScriptFunctionCall::appendArgument(unsigned int argument)
{
    JSLock lock(false);
    m_arguments.append(jsNumber(m_exec, argument));
}

void ScriptFunctionCall::appendArgument(int argument)
{
    JSLock lock(false);
    m_arguments.append(jsNumber(m_exec, argument));
}

void ScriptFunctionCall::appendArgument(bool argument)
{
    m_arguments.append(jsBoolean(argument));
}

ScriptValue ScriptFunctionCall::call(bool& hadException, bool reportExceptions)
{
    JSObject* thisObject = m_thisObject.jsObject();

    JSLock lock(false);

    JSValue function = thisObject->get(m_exec, Identifier(m_exec, m_name));
    if (m_exec->hadException()) {
        if (reportExceptions)
            reportException(m_exec, m_exec->exception());

        hadException = true;
        return ScriptValue();
    }

    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return ScriptValue();

    JSValue result = JSC::call(m_exec, function, callType, callData, thisObject, m_arguments);
    if (m_exec->hadException()) {
        if (reportExceptions)
            reportException(m_exec, m_exec->exception());

        hadException = true;
        return ScriptValue();
    }

    return ScriptValue(result);
}

ScriptValue ScriptFunctionCall::call()
{
    bool hadException = false;
    return call(hadException);
}

ScriptObject ScriptFunctionCall::construct(bool& hadException, bool reportExceptions)
{
    JSObject* thisObject = m_thisObject.jsObject();

    JSLock lock(false);

    JSObject* constructor = asObject(thisObject->get(m_exec, Identifier(m_exec, m_name)));
    if (m_exec->hadException()) {
        if (reportExceptions)
            reportException(m_exec, m_exec->exception());

        hadException = true;
        return ScriptObject();
    }

    ConstructData constructData;
    ConstructType constructType = constructor->getConstructData(constructData);
    if (constructType == ConstructTypeNone)
        return ScriptObject();

    JSValue result = JSC::construct(m_exec, constructor, constructType, constructData, m_arguments);
    if (m_exec->hadException()) {
        if (reportExceptions)
            reportException(m_exec, m_exec->exception());

        hadException = true;
        return ScriptObject();
    }

    return ScriptObject(asObject(result));
}

} // namespace WebCore
