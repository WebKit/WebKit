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
#include "OptionsObject.h"

#include "DOMStringList.h"
#include "V8Binding.h"
#include "V8DOMWindow.h"
#include "V8Storage.h"
#include "V8Utilities.h"
#include <wtf/MathExtras.h>

#if ENABLE(INDEXED_DATABASE)
#include "IDBKeyRange.h"
#include "V8IDBKeyRange.h"
#endif

#if ENABLE(VIDEO_TRACK)
#include "TrackBase.h"
#include "V8TextTrack.h"
#endif

namespace WebCore {

OptionsObject::OptionsObject()
{
}

OptionsObject::OptionsObject(const v8::Local<v8::Value>& options)
    : m_options(options)
{
}

OptionsObject::~OptionsObject()
{
}

OptionsObject& OptionsObject::operator=(const OptionsObject& optionsObject)
{
    m_options = optionsObject.m_options;
    return *this;
}

bool OptionsObject::isUndefinedOrNull() const
{
    if (m_options.IsEmpty())
        return true;
    return WebCore::isUndefinedOrNull(m_options);
}

bool OptionsObject::getKey(const String& key, v8::Local<v8::Value>& value) const
{
    if (isUndefinedOrNull())
        return false;
    v8::Local<v8::Object> options = m_options->ToObject();
    ASSERT(!options.IsEmpty());

    v8::Handle<v8::String> v8Key = v8String(key);
    if (!options->Has(v8Key))
        return false;
    value = options->Get(v8Key);
    if (value.IsEmpty())
        return false;
    return true;
}

bool OptionsObject::get(const String& key, bool& value) const
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

bool OptionsObject::get(const String& key, int32_t& value) const
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

bool OptionsObject::get(const String& key, double& value) const
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

bool OptionsObject::get(const String& key, String& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    // FIXME: It is possible for this to throw in which case we'd be getting back
    //        an empty string and returning true when we should be returning false.
    //        See fast/dom/Geolocation/script-tests/argument-types.js for a similar
    //        example.
    value = v8ValueToWebCoreString(v8Value);
    return true;
}

bool OptionsObject::get(const String& key, ScriptValue& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = ScriptValue(v8Value);
    return true;
}

bool OptionsObject::get(const String& key, unsigned short& value) const
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

bool OptionsObject::get(const String& key, unsigned& value) const
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

bool OptionsObject::get(const String& key, unsigned long long& value) const
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

bool OptionsObject::get(const String& key, RefPtr<DOMWindow>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    DOMWindow* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> window = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), wrapper);
        if (!window.IsEmpty())
            source = V8DOMWindow::toNative(window);
    }
    value = source;
    return true;
}

bool OptionsObject::get(const String& key, RefPtr<Storage>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    Storage* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> storage = V8DOMWrapper::lookupDOMWrapper(V8Storage::GetTemplate(), wrapper);
        if (!storage.IsEmpty())
            source = V8Storage::toNative(storage);
    }
    value = source;
    return true;
}

bool OptionsObject::get(const String& key, MessagePortArray& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    return getMessagePortArray(v8Value, value);
}

bool OptionsObject::get(const String& key, HashSet<AtomicString>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    // FIXME: Support array-like objects
    if (!v8Value->IsArray())
        return false;

    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> indexedValue = v8Array->Get(v8::Integer::New(i));
        value.add(v8ValueToWebCoreString(indexedValue));
    }

    return true;
}

bool OptionsObject::getWithUndefinedOrNullCheck(const String& key, String& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value) || v8Value->IsNull() || v8Value->IsUndefined())
        return false;

    // FIXME: It is possible for this to throw in which case we'd be getting back
    //        an empty string and returning true when we should be returning false.
    //        See fast/dom/Geolocation/script-tests/argument-types.js for a similar
    //        example.
    value = WebCore::isUndefinedOrNull(v8Value) ? String() : v8ValueToWebCoreString(v8Value);
    return true;
}

#if ENABLE(VIDEO_TRACK)
bool OptionsObject::get(const String& key, RefPtr<TrackBase>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    TrackBase* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);

        // FIXME: this will need to be changed so it can also return an AudioTrack or a VideoTrack once
        // we add them.
        v8::Handle<v8::Object> track = V8DOMWrapper::lookupDOMWrapper(V8TextTrack::GetTemplate(), wrapper);
        if (!track.IsEmpty())
            source = V8TextTrack::toNative(track);
    }
    value = source;
    return true;
}
#endif

} // namespace WebCore
