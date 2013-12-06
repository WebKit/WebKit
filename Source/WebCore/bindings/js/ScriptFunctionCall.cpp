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

#include "JSMainThreadExecState.h"
#include "ScriptValue.h"

#include <runtime/JSLock.h>
#include <wtf/text/WTFString.h>

using namespace JSC;

namespace WebCore {

void ScriptCallArgumentHandler::appendArgument(const ScriptObject& argument)
{
    if (argument.scriptState() != m_exec) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_arguments.append(argument.jsObject());
}

void ScriptCallArgumentHandler::appendArgument(const ScriptValue& argument)
{
    m_arguments.append(argument.jsValue());
}

void ScriptCallArgumentHandler::appendArgument(const String& argument)
{
    JSLockHolder lock(m_exec);
    m_arguments.append(jsStringWithCache(m_exec, argument));
}

void ScriptCallArgumentHandler::appendArgument(const char* argument)
{
    JSLockHolder lock(m_exec);
    m_arguments.append(jsString(m_exec, String(argument)));
}

void ScriptCallArgumentHandler::appendArgument(JSC::JSValue argument)
{
    m_arguments.append(argument);
}

void ScriptCallArgumentHandler::appendArgument(long argument)
{
    JSLockHolder lock(m_exec);
    m_arguments.append(jsNumber(argument));
}

void ScriptCallArgumentHandler::appendArgument(long long argument)
{
    JSLockHolder lock(m_exec);
    m_arguments.append(jsNumber(argument));
}

void ScriptCallArgumentHandler::appendArgument(unsigned int argument)
{
    JSLockHolder lock(m_exec);
    m_arguments.append(jsNumber(argument));
}

void ScriptCallArgumentHandler::appendArgument(unsigned long argument)
{
    JSLockHolder lock(m_exec);
    m_arguments.append(jsNumber(argument));
}

void ScriptCallArgumentHandler::appendArgument(int argument)
{
    JSLockHolder lock(m_exec);
    m_arguments.append(jsNumber(argument));
}

void ScriptCallArgumentHandler::appendArgument(bool argument)
{
    m_arguments.append(jsBoolean(argument));
}

ScriptFunctionCall::ScriptFunctionCall(const ScriptObject& thisObject, const String& name)
    : ScriptCallArgumentHandler(thisObject.scriptState())
    , m_thisObject(thisObject)
    , m_name(name)
{
}

ScriptValue ScriptFunctionCall::call(bool& hadException)
{
    JSObject* thisObject = m_thisObject.jsObject();

    JSLockHolder lock(m_exec);

    JSValue function = thisObject->get(m_exec, Identifier(m_exec, m_name));
    if (m_exec->hadException()) {
        hadException = true;
        return ScriptValue();
    }

    CallData callData;
    CallType callType = getCallData(function, callData);
    if (callType == CallTypeNone)
        return ScriptValue();

    JSValue result;
    if (isMainThread())
        result = JSMainThreadExecState::call(m_exec, function, callType, callData, thisObject, m_arguments);
    else
        result = JSC::call(m_exec, function, callType, callData, thisObject, m_arguments);

    if (m_exec->hadException()) {
        hadException = true;
        return ScriptValue();
    }

    return ScriptValue(m_exec->vm(), result);
}

ScriptValue ScriptFunctionCall::call()
{
    bool hadException = false;
    return call(hadException);
}

} // namespace WebCore
