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

#include "Document.h"
#include "Frame.h"
#include "ScriptScope.h"
#include "ScriptState.h"
#include "ScriptString.h"
#include "ScriptValue.h"

#include "V8Binding.h"
#include "V8Proxy.h"

#include <v8.h>
#include <wtf/OwnArrayPtr.h>

namespace WebCore {

static void reportException(ScriptState* scriptState, v8::TryCatch &exceptionCatcher)
{
    v8::Local<v8::Message> message = exceptionCatcher.Message();
    scriptState->frame()->document()->reportException(toWebCoreString(message->Get()), message->GetLineNumber(), toWebCoreString(message->GetScriptResourceName()));
    exceptionCatcher.Reset();
}

ScriptFunctionCall::ScriptFunctionCall(ScriptState* scriptState, const ScriptObject& thisObject, const String& name)
    : m_scriptState(scriptState)
    , m_thisObject(thisObject)
    , m_name(name)
{
}

void ScriptFunctionCall::appendArgument(const ScriptObject& argument)
{
    m_arguments.append(argument);
}

void ScriptFunctionCall::appendArgument(const ScriptString& argument)
{
    ScriptScope scope(m_scriptState);
    m_arguments.append(v8String(argument));
}

void ScriptFunctionCall::appendArgument(const ScriptValue& argument)
{
    m_arguments.append(argument);
}

void ScriptFunctionCall::appendArgument(const String& argument)
{
    ScriptScope scope(m_scriptState);
    m_arguments.append(v8String(argument));
}

void ScriptFunctionCall::appendArgument(long long argument)
{
    ScriptScope scope(m_scriptState);
    m_arguments.append(v8::Number::New(argument));
}

void ScriptFunctionCall::appendArgument(unsigned int argument)
{
    ScriptScope scope(m_scriptState);
    m_arguments.append(v8::Number::New(argument));
}

void ScriptFunctionCall::appendArgument(int argument)
{
    ScriptScope scope(m_scriptState);
    m_arguments.append(v8::Number::New(argument));
}

void ScriptFunctionCall::appendArgument(bool argument)
{
    m_arguments.append(v8Boolean(argument));
}

ScriptValue ScriptFunctionCall::call(bool& hadException, bool reportExceptions)
{
    ScriptScope scope(m_scriptState, reportExceptions);

    v8::Local<v8::Object> thisObject = m_thisObject.v8Object();
    v8::Local<v8::Value> value = thisObject->Get(v8String(m_name));
    if (!scope.success()) {
        hadException = true;
        return ScriptValue();
    }

    ASSERT(value->IsFunction());

    v8::Local<v8::Function> function(v8::Function::Cast(*value));
    OwnArrayPtr<v8::Handle<v8::Value> > args(new v8::Handle<v8::Value>[m_arguments.size()]);
    for (size_t i = 0; i < m_arguments.size(); ++i)
        args[i] = m_arguments[i].v8Value();

    v8::Local<v8::Value> result = function->Call(thisObject, m_arguments.size(), args.get());
    if (!scope.success()) {
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
    ScriptScope scope(m_scriptState, reportExceptions);

    v8::Local<v8::Object> thisObject = m_thisObject.v8Object();
    v8::Local<v8::Value> value = thisObject->Get(v8String(m_name));
    if (!scope.success()) {
        hadException = true;
        return ScriptObject();
    }

    ASSERT(value->IsFunction());

    v8::Local<v8::Function> constructor(v8::Function::Cast(*value));
    OwnArrayPtr<v8::Handle<v8::Value> > args(new v8::Handle<v8::Value>[m_arguments.size()]);
    for (size_t i = 0; i < m_arguments.size(); ++i)
        args[i] = m_arguments[i].v8Value();

    v8::Local<v8::Object> result = SafeAllocation::NewInstance(constructor, m_arguments.size(), args.get());
    if (!scope.success()) {
        hadException = true;
        return ScriptObject();
    }

    return ScriptObject(result);
}

} // namespace WebCore
