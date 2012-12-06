/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBBindingUtilities.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBKey.h"
#include "IDBKeyPath.h"
#include "IDBTracing.h"
#include "SerializedScriptValue.h"
#include "V8Binding.h"
#include "V8IDBKey.h"
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

static const size_t maximumDepth = 2000;

static PassRefPtr<IDBKey> createIDBKeyFromValue(v8::Handle<v8::Value> value, Vector<v8::Handle<v8::Array> >& stack)
{
    if (value->IsNumber() && !isnan(value->NumberValue()))
        return IDBKey::createNumber(value->NumberValue());
    if (value->IsString())
        return IDBKey::createString(toWebCoreString(value));
    if (value->IsDate() && !isnan(value->NumberValue()))
        return IDBKey::createDate(value->NumberValue());
    if (value->IsArray()) {
        v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);

        if (stack.contains(array))
            return 0;
        if (stack.size() >= maximumDepth)
            return 0;
        stack.append(array);

        IDBKey::KeyArray subkeys;
        uint32_t length = array->Length();
        for (uint32_t i = 0; i < length; ++i) {
            v8::Local<v8::Value> item = array->Get(v8::Int32::New(i));
            RefPtr<IDBKey> subkey = createIDBKeyFromValue(item, stack);
            if (!subkey)
                subkeys.append(IDBKey::createInvalid());
            else
                subkeys.append(subkey);
        }

        stack.removeLast();
        return IDBKey::createArray(subkeys);
    }
    return 0;
}

PassRefPtr<IDBKey> createIDBKeyFromValue(v8::Handle<v8::Value> value)
{
    Vector<v8::Handle<v8::Array> > stack;
    RefPtr<IDBKey> key = createIDBKeyFromValue(value, stack);
    if (key)
        return key;
    return IDBKey::createInvalid();
}

template<typename T>
static bool getValueFrom(T indexOrName, v8::Handle<v8::Value>& v8Value)
{
    v8::Local<v8::Object> object = v8Value->ToObject();
    if (!object->Has(indexOrName))
        return false;
    v8Value = object->Get(indexOrName);
    return true;
}

template<typename T>
static bool setValue(v8::Handle<v8::Value>& v8Object, T indexOrName, const v8::Handle<v8::Value>& v8Value)
{
    v8::Local<v8::Object> object = v8Object->ToObject();
    return object->Set(indexOrName, v8Value);
}

static bool get(v8::Handle<v8::Value>& object, const String& keyPathElement, v8::Handle<v8::Value>& result)
{
    if (object->IsString() && keyPathElement == "length") {
        int32_t length = v8::Handle<v8::String>::Cast(object)->Length();
        result = v8::Number::New(length);
        return true;
    }
    return object->IsObject() && getValueFrom(deprecatedV8String(keyPathElement), result);
}

static bool canSet(v8::Handle<v8::Value>& object, const String& keyPathElement)
{
    return object->IsObject();
}

static bool set(v8::Handle<v8::Value>& object, const String& keyPathElement, const v8::Handle<v8::Value>& v8Value)
{
    return canSet(object, keyPathElement) && setValue(object, deprecatedV8String(keyPathElement), v8Value);
}

static v8::Handle<v8::Value> getNthValueOnKeyPath(v8::Handle<v8::Value>& rootValue, const Vector<String>& keyPathElements, size_t index)
{
    v8::Handle<v8::Value> currentValue(rootValue);
    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; ++i) {
        v8::Handle<v8::Value> parentValue(currentValue);
        if (!get(parentValue, keyPathElements[i], currentValue))
            return v8Undefined();
    }

    return currentValue;
}

static bool canInjectNthValueOnKeyPath(v8::Handle<v8::Value>& rootValue, const Vector<String>& keyPathElements, size_t index)
{
    if (!rootValue->IsObject())
        return false;

    v8::Handle<v8::Value> currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; ++i) {
        v8::Handle<v8::Value> parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(parentValue, keyPathElement, currentValue))
            return canSet(parentValue, keyPathElement);
    }
    return true;
}


static v8::Handle<v8::Value> ensureNthValueOnKeyPath(v8::Handle<v8::Value>& rootValue, const Vector<String>& keyPathElements, size_t index)
{
    v8::Handle<v8::Value> currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; ++i) {
        v8::Handle<v8::Value> parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(parentValue, keyPathElement, currentValue)) {
            v8::Handle<v8::Object> object = v8::Object::New();
            if (!set(parentValue, keyPathElement, object))
                return v8Undefined();
            currentValue = object;
        }
    }

    return currentValue;
}

static PassRefPtr<IDBKey> createIDBKeyFromScriptValueAndKeyPath(const ScriptValue& value, const String& keyPath)
{
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath, keyPathElements, error);
    ASSERT(error == IDBKeyPathParseErrorNone);
    ASSERT(v8::Context::InContext());

    v8::HandleScope handleScope;
    v8::Handle<v8::Value> v8Value(value.v8Value());
    v8::Handle<v8::Value> v8Key(getNthValueOnKeyPath(v8Value, keyPathElements, keyPathElements.size()));
    if (v8Key.IsEmpty())
        return 0;
    return createIDBKeyFromValue(v8Key);
}

PassRefPtr<IDBKey> createIDBKeyFromScriptValueAndKeyPath(DOMRequestState*, const ScriptValue& value, const IDBKeyPath& keyPath)
{
    IDB_TRACE("createIDBKeyFromScriptValueAndKeyPath");
    ASSERT(!keyPath.isNull());
    ASSERT(v8::Context::InContext());


    v8::HandleScope handleScope;
    if (keyPath.type() == IDBKeyPath::ArrayType) {
        IDBKey::KeyArray result;
        const Vector<String>& array = keyPath.array();
        for (size_t i = 0; i < array.size(); ++i) {
            RefPtr<IDBKey> key = createIDBKeyFromScriptValueAndKeyPath(value, array[i]);
            if (!key)
                return 0;
            result.append(key);
        }
        return IDBKey::createArray(result);
    }

    ASSERT(keyPath.type() == IDBKeyPath::StringType);
    return createIDBKeyFromScriptValueAndKeyPath(value, keyPath.string());
}

ScriptValue deserializeIDBValue(DOMRequestState* state, PassRefPtr<SerializedScriptValue> prpValue)
{
    ASSERT(v8::Context::InContext());
    v8::HandleScope handleScope;
    RefPtr<SerializedScriptValue> serializedValue = prpValue;
    if (serializedValue)
        return ScriptValue(serializedValue->deserialize());
    return ScriptValue(v8::Null());
}

bool injectIDBKeyIntoScriptValue(DOMRequestState*, PassRefPtr<IDBKey> key, ScriptValue& value, const IDBKeyPath& keyPath)
{
    IDB_TRACE("injectIDBKeyIntoScriptValue");
    ASSERT(v8::Context::InContext());

    ASSERT(keyPath.type() == IDBKeyPath::StringType);
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseErrorNone);

    if (!keyPathElements.size())
        return 0;

    v8::HandleScope handleScope;
    v8::Handle<v8::Value> v8Value(value.v8Value());
    v8::Handle<v8::Value> parent(ensureNthValueOnKeyPath(v8Value, keyPathElements, keyPathElements.size() - 1));
    if (parent.IsEmpty())
        return false;

    if (!set(parent, keyPathElements.last(), toV8(key.get())))
        return false;

    return true;
}

bool canInjectIDBKeyIntoScriptValue(DOMRequestState*, const ScriptValue& scriptValue, const IDBKeyPath& keyPath)
{
    IDB_TRACE("canInjectIDBKeyIntoScriptValue");
    ASSERT(keyPath.type() == IDBKeyPath::StringType);
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseErrorNone);

    if (!keyPathElements.size())
        return false;

    v8::Handle<v8::Value> v8Value(scriptValue.v8Value());
    return canInjectNthValueOnKeyPath(v8Value, keyPathElements, keyPathElements.size() - 1);
}

ScriptValue idbKeyToScriptValue(DOMRequestState* state, PassRefPtr<IDBKey> key)
{
    ASSERT(v8::Context::InContext());
    v8::HandleScope handleScope;
    v8::Handle<v8::Value> v8Value(toV8(key.get()));
    return ScriptValue(v8Value);
}

} // namespace WebCore

#endif
