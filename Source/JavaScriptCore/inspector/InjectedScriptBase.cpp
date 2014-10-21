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

#if ENABLE(INSPECTOR)

#include "DebuggerEvalEnabler.h"
#include "InspectorValues.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "ScriptFunctionCall.h"
#include <wtf/text/WTFString.h>

namespace Inspector {

InjectedScriptBase::InjectedScriptBase(const String& name)
    : m_name(name)
    , m_environment(nullptr)
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

void InjectedScriptBase::initialize(Deprecated::ScriptObject injectedScriptObject, InspectorEnvironment* environment)
{
    m_injectedScriptObject = injectedScriptObject;
    m_environment = environment;
}

bool InjectedScriptBase::hasAccessToInspectedScriptState() const
{
    return m_environment && m_environment->canAccessInspectedScriptState(m_injectedScriptObject.scriptState());
}

const Deprecated::ScriptObject& InjectedScriptBase::injectedScriptObject() const
{
    return m_injectedScriptObject;
}

Deprecated::ScriptValue InjectedScriptBase::callFunctionWithEvalEnabled(Deprecated::ScriptFunctionCall& function, bool& hadException) const
{
    if (m_environment)
        m_environment->willCallInjectedScriptFunction(m_injectedScriptObject.scriptState(), name(), 1);

    JSC::ExecState* scriptState = m_injectedScriptObject.scriptState();
    Deprecated::ScriptValue resultValue;

    {
        JSC::DebuggerEvalEnabler evalEnabler(scriptState);
        resultValue = function.call(hadException);
    }

    if (m_environment)
        m_environment->didCallInjectedScriptFunction(m_injectedScriptObject.scriptState());

    return resultValue;
}

void InjectedScriptBase::makeCall(Deprecated::ScriptFunctionCall& function, RefPtr<InspectorValue>* result)
{
    if (hasNoValue() || !hasAccessToInspectedScriptState()) {
        *result = InspectorValue::null();
        return;
    }

    bool hadException = false;
    Deprecated::ScriptValue resultValue = callFunctionWithEvalEnabled(function, hadException);

    ASSERT(!hadException);
    if (!hadException) {
        *result = resultValue.toInspectorValue(m_injectedScriptObject.scriptState());
        if (!*result)
            *result = InspectorString::create(String::format("Object has too long reference chain (must not be longer than %d)", InspectorValue::maxDepth));
    } else
        *result = InspectorString::create("Exception while making a call.");
}

void InjectedScriptBase::makeEvalCall(ErrorString* errorString, Deprecated::ScriptFunctionCall& function, RefPtr<Protocol::Runtime::RemoteObject>* objectResult, Protocol::OptOutput<bool>* wasThrown)
{
    RefPtr<InspectorValue> result;
    makeCall(function, &result);
    if (!result) {
        *errorString = ASCIILiteral("Internal error: result value is empty");
        return;
    }

    if (result->type() == InspectorValue::Type::String) {
        result->asString(*errorString);
        ASSERT(errorString->length());
        return;
    }

    RefPtr<InspectorObject> resultPair;
    if (!result->asObject(resultPair)) {
        *errorString = ASCIILiteral("Internal error: result is not an Object");
        return;
    }

    RefPtr<InspectorObject> resultObject = resultPair->getObject(ASCIILiteral("result"));
    bool wasThrownVal = false;
    if (!resultObject || !resultPair->getBoolean(ASCIILiteral("wasThrown"), wasThrownVal)) {
        *errorString = ASCIILiteral("Internal error: result is not a pair of value and wasThrown flag");
        return;
    }

    *objectResult = BindingTraits<Protocol::Runtime::RemoteObject>::runtimeCast(resultObject);
    *wasThrown = wasThrownVal;
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
