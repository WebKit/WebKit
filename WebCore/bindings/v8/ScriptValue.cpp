/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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
#include "ScriptValue.h"

#include "InspectorValues.h"
#include "ScriptScope.h"
#include "SerializedScriptValue.h"
#include "V8Binding.h"

namespace WebCore {

PassRefPtr<SerializedScriptValue> ScriptValue::serialize(ScriptState* scriptState)
{
    ScriptScope scope(scriptState);
    return SerializedScriptValue::create(v8Value());
}

ScriptValue ScriptValue::deserialize(ScriptState* scriptState, SerializedScriptValue* value)
{
    ScriptScope scope(scriptState);
    return ScriptValue(value->deserialize());
}

bool ScriptValue::getString(String& result) const
{
    if (m_value.IsEmpty())
        return false;

    if (!m_value->IsString())
        return false;

    result = toWebCoreString(m_value);
    return true;
}

String ScriptValue::toString(ScriptState*) const
{
    return toWebCoreString(m_value);
}

#if ENABLE(INSPECTOR)
static PassRefPtr<InspectorValue> v8ToInspectorValue(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty()) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    if (value->IsNull() || value->IsUndefined())
        return InspectorValue::null();
    if (value->IsBoolean())
        return InspectorBasicValue::create(value->BooleanValue());
    if (value->IsNumber())
        return InspectorBasicValue::create(value->NumberValue());
    if (value->IsString())
        return InspectorString::create(toWebCoreString(value));
    if (value->IsArray()) {
        v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
        RefPtr<InspectorArray> inspectorArray = InspectorArray::create();
        uint32_t length = array->Length();
        for (uint32_t i = 0; i < length; i++) {
            v8::Local<v8::Value> value = array->Get(v8::Int32::New(i));
            RefPtr<InspectorValue> element = v8ToInspectorValue(value);
            if (!element) {
                ASSERT_NOT_REACHED();
                element = InspectorValue::null();
            }
            inspectorArray->pushValue(element);
        }
        return inspectorArray;
    }
    if (value->IsObject()) {
        RefPtr<InspectorObject> inspectorObject = InspectorObject::create();
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
        v8::Local<v8::Array> propertyNames = object->GetPropertyNames();
        uint32_t length = propertyNames->Length();
        for (uint32_t i = 0; i < length; i++) {
            v8::Local<v8::Value> name = propertyNames->Get(v8::Int32::New(i));
            // FIXME(yurys): v8::Object should support GetOwnPropertyNames
            if (name->IsString() && !object->HasRealNamedProperty(v8::Handle<v8::String>::Cast(name)))
                continue;
            RefPtr<InspectorValue> propertyValue = v8ToInspectorValue(object->Get(name));
            if (!propertyValue) {
                ASSERT_NOT_REACHED();
                continue;
            }
            inspectorObject->setValue(toWebCoreStringWithNullCheck(name), propertyValue);
        }
        return inspectorObject;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

PassRefPtr<InspectorValue> ScriptValue::toInspectorValue(ScriptState* scriptState) const
{
    v8::HandleScope handleScope;
    // v8::Object::GetPropertyNames() expects current context to be not null.
    v8::Context::Scope contextScope(scriptState->context());
    return v8ToInspectorValue(m_value);
}
#endif

} // namespace WebCore
