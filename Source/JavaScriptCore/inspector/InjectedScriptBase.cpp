/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "InjectedScriptBase.h"

#include "DebuggerEvalEnabler.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "ScriptFunctionCall.h"
#include <wtf/JSONValues.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

InjectedScriptBase::InjectedScriptBase(const String& name)
    : m_name(name)
{
}

InjectedScriptBase::InjectedScriptBase(const String& name, Deprecated::ScriptObject injectedScriptObject, InspectorEnvironment* environment)
    : m_name(name)
    , m_injectedScriptObject(injectedScriptObject)
    , m_environment(environment)
{
}

InjectedScriptBase::~InjectedScriptBase()
{
}

bool InjectedScriptBase::hasAccessToInspectedScriptState() const
{
    return m_environment && m_environment->canAccessInspectedScriptState(m_injectedScriptObject.scriptState());
}

const Deprecated::ScriptObject& InjectedScriptBase::injectedScriptObject() const
{
    return m_injectedScriptObject;
}

JSC::JSValue InjectedScriptBase::callFunctionWithEvalEnabled(Deprecated::ScriptFunctionCall& function, bool& hadException) const
{
    JSC::ExecState* scriptState = m_injectedScriptObject.scriptState();
    JSC::DebuggerEvalEnabler evalEnabler(scriptState);
    return function.call(hadException);
}

Ref<JSON::Value> InjectedScriptBase::makeCall(Deprecated::ScriptFunctionCall& function)
{
    if (hasNoValue() || !hasAccessToInspectedScriptState())
        return JSON::Value::null();

    bool hadException = false;
    auto resultJSValue = callFunctionWithEvalEnabled(function, hadException);

    ASSERT(!hadException);
    if (hadException)
        return JSON::Value::create("Exception while making a call.");

    RefPtr<JSON::Value> resultJSONValue = toInspectorValue(*m_injectedScriptObject.scriptState(), resultJSValue);
    if (!resultJSONValue)
        return JSON::Value::create(String::format("Object has too long reference chain (must not be longer than %d)", JSON::Value::maxDepth));

    return resultJSONValue.releaseNonNull();
}

void InjectedScriptBase::makeEvalCall(ErrorString& errorString, Deprecated::ScriptFunctionCall& function, RefPtr<Protocol::Runtime::RemoteObject>& out_resultObject, bool& out_wasThrown, std::optional<int>& out_savedResultIndex)
{
    RefPtr<JSON::Value> result = makeCall(function);
    if (!result) {
        errorString = "Internal error: result value is empty"_s;
        return;
    }

    if (result->type() == JSON::Value::Type::String) {
        result->asString(errorString);
        ASSERT(errorString.length());
        return;
    }

    RefPtr<JSON::Object> resultTuple;
    if (!result->asObject(resultTuple)) {
        errorString = "Internal error: result is not an Object"_s;
        return;
    }

    RefPtr<JSON::Object> resultObject;
    if (!resultTuple->getObject("result"_s, resultObject)) {
        errorString = "Internal error: result is not a pair of value and wasThrown flag"_s;
        return;
    }

    bool wasThrown = false;
    if (!resultTuple->getBoolean("wasThrown"_s, wasThrown)) {
        errorString = "Internal error: result is not a pair of value and wasThrown flag"_s;
        return;
    }

    out_resultObject = BindingTraits<Protocol::Runtime::RemoteObject>::runtimeCast(resultObject);
    out_wasThrown = wasThrown;

    int savedResultIndex;
    if (resultTuple->getInteger("savedResultIndex"_s, savedResultIndex))
        out_savedResultIndex = savedResultIndex;
}

} // namespace Inspector

