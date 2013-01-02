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
#include "V8Storage.h"

#include "Storage.h"
#include "V8Binding.h"

namespace WebCore {

template<class T>
static v8::Handle<T> setDOMException(ExceptionCode ec, const v8::AccessorInfo& info)
{
    setDOMException(ec, info.GetIsolate());
    return v8::Handle<T>();
}

// Get an array containing the names of indexed properties in a collection.
v8::Handle<v8::Array> V8Storage::namedPropertyEnumerator(const v8::AccessorInfo& info)
{
    Storage* storage = V8Storage::toNative(info.Holder());
    ExceptionCode ec = 0;
    unsigned length = storage->length(ec);
    if (ec)
        return setDOMException<v8::Array>(ec, info);
    v8::Handle<v8::Array> properties = v8::Array::New(length);
    for (unsigned i = 0; i < length; ++i) {
        String key = storage->key(i, ec);
        if (ec)
            return setDOMException<v8::Array>(ec, info);
        ASSERT(!key.isNull());
        String val = storage->getItem(key, ec);
        if (ec)
            return setDOMException<v8::Array>(ec, info);
        properties->Set(v8Integer(i, info.GetIsolate()), v8String(key, info.GetIsolate()));
    }
    return properties;
}

static v8::Handle<v8::Value> storageGetter(v8::Local<v8::String> v8Name, const v8::AccessorInfo& info)
{
    Storage* storage = V8Storage::toNative(info.Holder());
    String name = toWebCoreString(v8Name);

    if (name == "length")
        return v8Undefined();
    ExceptionCode ec = 0;
    bool found = storage->contains(name, ec);
    if (ec)
        return setDOMException(ec, info.GetIsolate());
    if (!found)
        return v8Undefined();
    String result = storage->getItem(name, ec);
    if (ec)
        return setDOMException(ec, info.GetIsolate());
    return v8String(result, info.GetIsolate());
}

v8::Handle<v8::Value> V8Storage::indexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Integer> indexV8 = v8Integer(index, info.GetIsolate());
    return storageGetter(indexV8->ToString(), info);
}

v8::Handle<v8::Value> V8Storage::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(name).IsEmpty())
        return v8Undefined();
    return storageGetter(name, info);
}

v8::Handle<v8::Integer> V8Storage::namedPropertyQuery(v8::Local<v8::String> v8Name, const v8::AccessorInfo& info)
{

    Storage* storage = V8Storage::toNative(info.Holder());
    String name = toWebCoreString(v8Name);

    if (name == "length")
        return v8::Handle<v8::Integer>();
    ExceptionCode ec = 0;
    bool found = storage->contains(name, ec);
    if (ec)
        return setDOMException<v8::Integer>(ec, info);
    if (!found)
        return v8::Handle<v8::Integer>();
    return v8Integer(0, info.GetIsolate());
}

static v8::Handle<v8::Value> storageSetter(v8::Local<v8::String> v8Name, v8::Local<v8::Value> v8Value, const v8::AccessorInfo& info)
{
    Storage* storage = V8Storage::toNative(info.Holder());
    String name = toWebCoreString(v8Name);
    String value = toWebCoreString(v8Value);

    // Silently ignore length (rather than letting the bindings raise an exception).
    if (name == "length")
        return v8Value;

    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(v8Name).IsEmpty())
        return v8Undefined();

    ExceptionCode ec = 0;
    storage->setItem(name, value, ec);
    if (ec)
        return setDOMException(ec, info.GetIsolate());

    return v8Value;
}

v8::Handle<v8::Value> V8Storage::indexedPropertySetter(uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Integer> indexV8 = v8Integer(index, info.GetIsolate());
    return storageSetter(indexV8->ToString(), value, info);
}

v8::Handle<v8::Value> V8Storage::namedPropertySetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    return storageSetter(name, value, info);
}

static v8::Handle<v8::Boolean> storageDeleter(v8::Local<v8::String> v8Name, const v8::AccessorInfo& info)
{
    Storage* storage = V8Storage::toNative(info.Holder());
    String name = toWebCoreString(v8Name);
    
    ExceptionCode ec = 0;
    bool found = storage->contains(name, ec);
    if (ec)
        return setDOMException<v8::Boolean>(ec, info);
    if (!found)
        return v8::Handle<v8::Boolean>();
    storage->removeItem(name, ec);
    if (ec)
        return setDOMException<v8::Boolean>(ec, info);
    return v8Boolean(true, info.GetIsolate());
}

v8::Handle<v8::Boolean> V8Storage::indexedPropertyDeleter(uint32_t index, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Integer> indexV8 = v8Integer(index, info.GetIsolate());
    return storageDeleter(indexV8->ToString(), info);
}

v8::Handle<v8::Boolean> V8Storage::namedPropertyDeleter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    return storageDeleter(name, info);
}

} // namespace WebCore
