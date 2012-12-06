/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "Dictionary.h"

#include "ArrayValue.h"
#include "DOMStringList.h"
#include "V8Binding.h"
#include "V8DOMWindow.h"
#include "V8Storage.h"
#include "V8Uint8Array.h"
#include "V8Utilities.h"
#include <wtf/MathExtras.h>

#if ENABLE(INDEXED_DATABASE)
#include "IDBKeyRange.h"
#include "V8IDBKeyRange.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "V8MediaKeyError.h"
#endif

#if ENABLE(VIDEO_TRACK)
#include "TrackBase.h"
#include "V8TextTrack.h"
#endif

#if ENABLE(SCRIPTED_SPEECH)
#include "SpeechRecognitionError.h"
#include "SpeechRecognitionResult.h"
#include "SpeechRecognitionResultList.h"
#include "V8SpeechRecognitionError.h"
#include "V8SpeechRecognitionResult.h"
#include "V8SpeechRecognitionResultList.h"
#endif

namespace WebCore {

Dictionary::Dictionary()
    : m_isolate(0)
{
}

Dictionary::Dictionary(const v8::Local<v8::Value>& options, v8::Isolate* isolate)
    : m_options(options)
    , m_isolate(isolate)
{
    ASSERT(m_isolate);
}

Dictionary::~Dictionary()
{
}

Dictionary& Dictionary::operator=(const Dictionary& optionsObject)
{
    m_options = optionsObject.m_options;
    m_isolate = optionsObject.m_isolate;
    return *this;
}

bool Dictionary::isObject() const
{
    return !isUndefinedOrNull() && m_options->IsObject();
}

bool Dictionary::isUndefinedOrNull() const
{
    if (m_options.IsEmpty())
        return true;
    return WebCore::isUndefinedOrNull(m_options);
}

bool Dictionary::getKey(const String& key, v8::Local<v8::Value>& value) const
{
    if (isUndefinedOrNull())
        return false;
    v8::Local<v8::Object> options = m_options->ToObject();
    ASSERT(!options.IsEmpty());

    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    v8::Handle<v8::String> v8Key = v8String(key, m_isolate);
    if (!options->Has(v8Key))
        return false;
    value = options->Get(v8Key);
    if (value.IsEmpty())
        return false;
    return true;
}

bool Dictionary::get(const String& key, bool& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Boolean> v8Bool = v8Value->ToBoolean();
    if (v8Bool.IsEmpty())
        return false;
    value = v8Bool->Value();
    return true;
}

bool Dictionary::get(const String& key, int32_t& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Int32> v8Int32 = v8Value->ToInt32();
    if (v8Int32.IsEmpty())
        return false;
    value = v8Int32->Value();
    return true;
}

bool Dictionary::get(const String& key, double& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Number> v8Number = v8Value->ToNumber();
    if (v8Number.IsEmpty())
        return false;
    value = v8Number->Value();
    return true;
}

bool Dictionary::get(const String& key, String& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    // FIXME: It is possible for this to throw in which case we'd be getting back
    //        an empty string and returning true when we should be returning false.
    //        See fast/dom/Geolocation/script-tests/argument-types.js for a similar
    //        example.
    value = toWebCoreString(v8Value);
    return true;
}

bool Dictionary::get(const String& key, ScriptValue& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = ScriptValue(v8Value);
    return true;
}

bool Dictionary::get(const String& key, unsigned short& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Int32> v8Int32 = v8Value->ToInt32();
    if (v8Int32.IsEmpty())
        return false;
    value = static_cast<unsigned short>(v8Int32->Value());
    return true;
}

bool Dictionary::get(const String& key, short& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Int32> v8Int32 = v8Value->ToInt32();
    if (v8Int32.IsEmpty())
        return false;
    value = static_cast<short>(v8Int32->Value());
    return true;
}

bool Dictionary::get(const String& key, unsigned& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Int32> v8Int32 = v8Value->ToInt32();
    if (v8Int32.IsEmpty())
        return false;
    value = static_cast<unsigned>(v8Int32->Value());
    return true;
}

bool Dictionary::get(const String& key, unsigned long& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Integer> v8Integer = v8Value->ToInteger();
    if (v8Integer.IsEmpty())
        return false;
    value = static_cast<unsigned long>(v8Integer->Value());
    return true;
}

bool Dictionary::get(const String& key, unsigned long long& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Number> v8Number = v8Value->ToNumber();
    if (v8Number.IsEmpty())
        return false;
    double d = v8Number->Value();
    doubleToInteger(d, value);
    return true;
}

bool Dictionary::get(const String& key, RefPtr<DOMWindow>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    DOMWindow* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> window = wrapper->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate());
        if (!window.IsEmpty())
            source = V8DOMWindow::toNative(window);
    }
    value = source;
    return true;
}

bool Dictionary::get(const String& key, RefPtr<Storage>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    Storage* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> storage = wrapper->FindInstanceInPrototypeChain(V8Storage::GetTemplate());
        if (!storage.IsEmpty())
            source = V8Storage::toNative(storage);
    }
    value = source;
    return true;
}

bool Dictionary::get(const String& key, MessagePortArray& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    return getMessagePortArray(v8Value, value, m_isolate);
}

bool Dictionary::get(const String& key, HashSet<AtomicString>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    // FIXME: Support array-like objects
    if (!v8Value->IsArray())
        return false;

    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> indexedValue = v8Array->Get(v8Integer(i, m_isolate));
        value.add(toWebCoreString(indexedValue));
    }

    return true;
}

bool Dictionary::getWithUndefinedOrNullCheck(const String& key, String& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value) || v8Value->IsNull() || v8Value->IsUndefined())
        return false;

    // FIXME: It is possible for this to throw in which case we'd be getting back
    //        an empty string and returning true when we should be returning false.
    //        See fast/dom/Geolocation/script-tests/argument-types.js for a similar
    //        example.
    value = WebCore::isUndefinedOrNull(v8Value) ? String() : toWebCoreString(v8Value);
    return true;
}

bool Dictionary::get(const String& key, RefPtr<Uint8Array>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    Uint8Array* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> array = wrapper->FindInstanceInPrototypeChain(V8Uint8Array::GetTemplate());
        if (!array.IsEmpty())
            source = V8Uint8Array::toNative(array);
    }
    value = source;
    return true;
}

#if ENABLE(ENCRYPTED_MEDIA)
bool Dictionary::get(const String& key, RefPtr<MediaKeyError>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    MediaKeyError* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> error = wrapper->FindInstanceInPrototypeChain(V8MediaKeyError::GetTemplate());
        if (!error.IsEmpty())
            source = V8MediaKeyError::toNative(error);
    }
    value = source;
    return true;
}
#endif

#if ENABLE(VIDEO_TRACK)
bool Dictionary::get(const String& key, RefPtr<TrackBase>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    TrackBase* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);

        // FIXME: this will need to be changed so it can also return an AudioTrack or a VideoTrack once
        // we add them.
        v8::Handle<v8::Object> track = wrapper->FindInstanceInPrototypeChain(V8TextTrack::GetTemplate());
        if (!track.IsEmpty())
            source = V8TextTrack::toNative(track);
    }
    value = source;
    return true;
}
#endif

#if ENABLE(SCRIPTED_SPEECH)
bool Dictionary::get(const String& key, RefPtr<SpeechRecognitionError>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    SpeechRecognitionError* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> speechRecognitionError = wrapper->FindInstanceInPrototypeChain(V8SpeechRecognitionError::GetTemplate());
        if (!speechRecognitionError.IsEmpty())
            source = V8SpeechRecognitionError::toNative(speechRecognitionError);
    }
    value = source;
    return true;
}

bool Dictionary::get(const String& key, RefPtr<SpeechRecognitionResult>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    SpeechRecognitionResult* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> speechRecognitionResult = wrapper->FindInstanceInPrototypeChain(V8SpeechRecognitionResult::GetTemplate());
        if (!speechRecognitionResult.IsEmpty())
            source = V8SpeechRecognitionResult::toNative(speechRecognitionResult);
    }
    value = source;
    return true;
}

bool Dictionary::get(const String& key, RefPtr<SpeechRecognitionResultList>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    SpeechRecognitionResultList* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> speechRecognitionResultList = wrapper->FindInstanceInPrototypeChain(V8SpeechRecognitionResultList::GetTemplate());
        if (!speechRecognitionResultList.IsEmpty())
            source = V8SpeechRecognitionResultList::toNative(speechRecognitionResultList);
    }
    value = source;
    return true;
}

#endif

bool Dictionary::get(const String& key, Dictionary& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    if (v8Value->IsObject()) {
        ASSERT(m_isolate);
        ASSERT(m_isolate == v8::Isolate::GetCurrent());
        value = Dictionary(v8Value, m_isolate);
    }

    return true;
}

bool Dictionary::get(const String& key, Vector<String>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    if (!v8Value->IsArray())
        return false;

    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> indexedValue = v8Array->Get(v8::Uint32::New(i));
        value.append(toWebCoreString(indexedValue));
    }

    return true;
}

bool Dictionary::get(const String& key, ArrayValue& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    if (!v8Value->IsArray())
        return false;

    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    value = ArrayValue(v8::Local<v8::Array>::Cast(v8Value), m_isolate);
    return true;
}

bool Dictionary::getOwnPropertiesAsStringHashMap(HashMap<String, String>& hashMap) const
{
    if (!isObject())
        return false;

    v8::Handle<v8::Object> options = m_options->ToObject();
    if (options.IsEmpty())
        return false;

    v8::Local<v8::Array> properties = options->GetOwnPropertyNames();
    if (properties.IsEmpty())
        return true;
    for (uint32_t i = 0; i < properties->Length(); ++i) {
        v8::Local<v8::String> key = properties->Get(i)->ToString();
        if (!options->Has(key))
            continue;

        v8::Local<v8::Value> value = options->Get(key);
        String stringKey = toWebCoreString(key);
        String stringValue = toWebCoreString(value);
        if (!stringKey.isEmpty())
            hashMap.set(stringKey, stringValue);
    }

    return true;
}

bool Dictionary::getOwnPropertyNames(Vector<String>& names) const
{
    if (!isObject())
        return false;

    v8::Handle<v8::Object> options = m_options->ToObject();
    if (options.IsEmpty())
        return false;

    v8::Local<v8::Array> properties = options->GetOwnPropertyNames();
    if (properties.IsEmpty())
        return true;
    for (uint32_t i = 0; i < properties->Length(); ++i) {
        v8::Local<v8::String> key = properties->Get(i)->ToString();
        if (!options->Has(key))
            continue;
        names.append(toWebCoreString(key));
    }

    return true;
}

} // namespace WebCore
