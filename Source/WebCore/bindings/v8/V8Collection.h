/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#ifndef V8Collection_h
#define V8Collection_h

#include "HTMLFormElement.h"
#include "HTMLSelectElement.h"
#include "V8Binding.h"
#include "V8Node.h"
#include "V8Proxy.h"
#include <v8.h>

namespace WebCore {
// FIXME: These functions should be named using to* since they return the item (get* is used for method that take a ref param).
// See https://bugs.webkit.org/show_bug.cgi?id=24664.

template<class T> static v8::Handle<v8::Value> getV8Object(T* implementation)
{
    if (!implementation)
        return v8::Handle<v8::Value>();
    return toV8(implementation);
}

template<class Collection> static Collection* toNativeCollection(v8::Local<v8::Object> object)
{
    return reinterpret_cast<Collection*>(object->GetPointerFromInternalField(v8DOMWrapperObjectIndex));
}

template<class T> static v8::Handle<v8::Value> getV8Object(PassRefPtr<T> implementation)
{
    return getV8Object(implementation.get());
}

// Returns named property of a collection.
template<class Collection, class ItemType> static v8::Handle<v8::Value> getNamedPropertyOfCollection(v8::Local<v8::String> name, v8::Local<v8::Object> object)
{
    // FIXME: assert object is a collection type
    ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
    ASSERT(V8DOMWrapper::domWrapperType(object) != &V8Node::info);
    Collection* collection = toNativeCollection<Collection>(object);
    AtomicString propertyName = toAtomicWebCoreStringWithNullCheck(name);
    return getV8Object<ItemType>(collection->namedItem(propertyName));
}

// A template of named property accessor of collections.
template<class Collection, class ItemType> static v8::Handle<v8::Value> collectionNamedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Value> value = info.Holder()->GetRealNamedPropertyInPrototypeChain(name);

    if (!value.IsEmpty())
        return value;

    // Search local callback properties next to find IDL defined
    // properties.
    if (info.Holder()->HasRealNamedCallbackProperty(name))
        return notHandledByInterceptor();
    return getNamedPropertyOfCollection<Collection, ItemType>(name, info.Holder());
}

// Returns the property at the index of a collection.
template<class Collection, class ItemType> static v8::Handle<v8::Value> getIndexedPropertyOfCollection(uint32_t index, v8::Local<v8::Object> object)
{
    // FIXME: Assert that object must be a collection type.
    ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
    ASSERT(V8DOMWrapper::domWrapperType(object) != &V8Node::info);
    Collection* collection = toNativeCollection<Collection>(object);
    return getV8Object<ItemType>(collection->item(index));
}

// A template of index interceptor of collections.
template<class Collection, class ItemType> static v8::Handle<v8::Value> collectionIndexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    return getIndexedPropertyOfCollection<Collection, ItemType>(index, info.Holder());
}

// Get an array containing the names of indexed properties of HTMLSelectElement and HTMLFormElement.
template<class Collection> static v8::Handle<v8::Array> nodeCollectionIndexedPropertyEnumerator(const v8::AccessorInfo& info)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(info.Holder()));
    Collection* collection = toNativeCollection<Collection>(info.Holder());
    int length = collection->length();
    v8::Handle<v8::Array> properties = v8::Array::New(length);
    for (int i = 0; i < length; ++i) {
        // FIXME: Do we need to check that the item function returns a non-null value for this index?
        v8::Handle<v8::Integer> integer = v8::Integer::New(i);
        properties->Set(integer, integer);
    }
    return properties;
}

// Get an array containing the names of indexed properties in a collection.
template<class Collection> static v8::Handle<v8::Array> collectionIndexedPropertyEnumerator(const v8::AccessorInfo& info)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(info.Holder()));
    Collection* collection = toNativeCollection<Collection>(info.Holder());
    int length = collection->length();
    v8::Handle<v8::Array> properties = v8::Array::New(length);
    for (int i = 0; i < length; ++i) {
        // FIXME: Do we need to check that the item function returns a non-null value for this index?
        v8::Handle<v8::Integer> integer = v8::Integer::New(i);
        properties->Set(integer, integer);
    }
    return properties;
}


// A template for indexed getters on collections of strings that should return null if the resulting string is a null string.
template<class Collection> static v8::Handle<v8::Value> collectionStringOrNullIndexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    // FIXME: assert that object must be a collection type
    ASSERT(V8DOMWrapper::maybeDOMWrapper(info.Holder()));
    Collection* collection = toNativeCollection<Collection>(info.Holder());
    String result = collection->item(index);
    return v8StringOrNull(result);
}


// A template for indexed getters on collections of strings.
template<class Collection> static v8::Handle<v8::Value> collectionStringIndexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    // FIXME: assert that object must be a collection type
    ASSERT(V8DOMWrapper::maybeDOMWrapper(info.Holder()));
    Collection* collection = toNativeCollection<Collection>(info.Holder());
    String result = collection->item(index);
    return v8String(result);
}


// Add indexed getter to the function template for a collection.
template<class Collection, class ItemType> static void setCollectionIndexedGetter(v8::Handle<v8::FunctionTemplate> desc)
{
    desc->InstanceTemplate()->SetIndexedPropertyHandler(collectionIndexedPropertyGetter<Collection, ItemType>, 0, 0, 0, collectionIndexedPropertyEnumerator<Collection>);
}


// Add named getter to the function template for a collection.
template<class Collection, class ItemType> static void setCollectionNamedGetter(v8::Handle<v8::FunctionTemplate> desc)
{
    desc->InstanceTemplate()->SetNamedPropertyHandler(collectionNamedPropertyGetter<Collection, ItemType>, 0, 0, 0, 0);
}

// Add indexed getter returning a string or null to a function template for a collection.
template<class Collection> static void setCollectionStringOrNullIndexedGetter(v8::Handle<v8::FunctionTemplate> desc)
{
    desc->InstanceTemplate()->SetIndexedPropertyHandler(collectionStringOrNullIndexedPropertyGetter<Collection>, 0, 0, 0, collectionIndexedPropertyEnumerator<Collection>);
}


// Add indexed getter returning a string to a function template for a collection.
template<class Collection> static void setCollectionStringIndexedGetter(v8::Handle<v8::FunctionTemplate> desc)
{
    desc->InstanceTemplate()->SetIndexedPropertyHandler(collectionStringIndexedPropertyGetter<Collection>, 0, 0, 0, collectionIndexedPropertyEnumerator<Collection>);
}

v8::Handle<v8::Value> toOptionsCollectionSetter(uint32_t index, v8::Handle<v8::Value>, HTMLSelectElement*);

} // namespace WebCore

#endif // V8Collection_h
