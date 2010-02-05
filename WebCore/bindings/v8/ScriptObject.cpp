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
#include "ScriptObject.h"

#include "ScriptScope.h"
#include "ScriptState.h"

#include "Document.h"
#include "Frame.h"
#include "V8Binding.h"
#include "V8InjectedScriptHost.h"
#include "V8InspectorBackend.h"
#include "V8InspectorFrontendHost.h"
#include "V8Proxy.h"

#include <v8.h>

namespace WebCore {

ScriptObject::ScriptObject(ScriptState* scriptState, v8::Handle<v8::Object> v8Object)
    : ScriptValue(v8Object)
    , m_scriptState(scriptState)
{
}

v8::Local<v8::Object> ScriptObject::v8Object() const
{
    ASSERT(v8Value()->IsObject());
    return v8::Local<v8::Object>(v8::Object::Cast(*v8Value()));
}

bool ScriptObject::set(const String& name, const String& value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8String(name), v8String(value));
    return scope.success();
}

bool ScriptObject::set(const char* name, const ScriptObject& value)
{
    if (value.scriptState() != m_scriptState) {
        ASSERT_NOT_REACHED();
        return false;
    }
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::String::New(name), value.v8Value());
    return scope.success();
}

bool ScriptObject::set(const char* name, const String& value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::String::New(name), v8String(value));
    return scope.success();
}

bool ScriptObject::set(const char* name, double value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::String::New(name), v8::Number::New(value));
    return scope.success();
}

bool ScriptObject::set(const char* name, long value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::String::New(name), v8::Number::New(value));
    return scope.success();
}

bool ScriptObject::set(const char* name, long long value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::String::New(name), v8::Number::New(value));
    return scope.success();
}

bool ScriptObject::set(const char* name, int value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::String::New(name), v8::Number::New(value));
    return scope.success();
}

bool ScriptObject::set(const char* name, unsigned value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::String::New(name), v8::Number::New(value));
    return scope.success();
}

bool ScriptObject::set(const char* name, unsigned long value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::String::New(name), v8::Number::New(value));
    return scope.success();
}

bool ScriptObject::set(const char* name, bool value)
{
    ScriptScope scope(m_scriptState);
    v8Object()->Set(v8::String::New(name), v8Boolean(value));
    return scope.success();
}

ScriptObject ScriptObject::createNew(ScriptState* scriptState)
{
    ScriptScope scope(scriptState);
    return ScriptObject(scriptState, v8::Object::New());
}

bool ScriptGlobalObject::set(ScriptState* scriptState, const char* name, const ScriptObject& value)
{
    ScriptScope scope(scriptState);
    scope.global()->Set(v8::String::New(name), value.v8Value());
    return scope.success();
}

#if ENABLE(INSPECTOR)
bool ScriptGlobalObject::set(ScriptState* scriptState, const char* name, InspectorBackend* value)
{
    ScriptScope scope(scriptState);
    scope.global()->Set(v8::String::New(name), toV8(value));
    return scope.success();
}

bool ScriptGlobalObject::set(ScriptState* scriptState, const char* name, InspectorFrontendHost* value)
{
    ScriptScope scope(scriptState);
    scope.global()->Set(v8::String::New(name), toV8(value));
    return scope.success();
}

bool ScriptGlobalObject::set(ScriptState* scriptState, const char* name, InjectedScriptHost* value)
{
    ScriptScope scope(scriptState);
    scope.global()->Set(v8::String::New(name), toV8(value));
    return scope.success();
}
#endif

bool ScriptGlobalObject::get(ScriptState* scriptState, const char* name, ScriptObject& value)
{
    ScriptScope scope(scriptState);
    v8::Local<v8::Value> v8Value = scope.global()->Get(v8::String::New(name));
    if (v8Value.IsEmpty())
        return false;

    if (!v8Value->IsObject())
        return false;

    value = ScriptObject(scriptState, v8::Handle<v8::Object>(v8::Object::Cast(*v8Value)));
    return true;
}

bool ScriptGlobalObject::remove(ScriptState* scriptState, const char* name)
{
    ScriptScope scope(scriptState);
    return scope.global()->Delete(v8::String::New(name));
}

} // namespace WebCore
