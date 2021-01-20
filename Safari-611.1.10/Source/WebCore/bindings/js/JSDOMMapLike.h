/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CANON INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSDOMBinding.h"
#include "JSDOMConvert.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/CommonIdentifiers.h>

namespace WebCore {

WEBCORE_EXPORT std::pair<bool, std::reference_wrapper<JSC::JSObject>> getBackingMap(JSC::JSGlobalObject&, JSC::JSObject& mapLike);
WEBCORE_EXPORT JSC::JSValue forwardAttributeGetterToBackingMap(JSC::JSGlobalObject&, JSC::JSObject&, const JSC::Identifier&);
WEBCORE_EXPORT JSC::JSValue forwardFunctionCallToBackingMap(JSC::JSGlobalObject&, JSC::CallFrame&, JSC::JSObject&, const JSC::Identifier&);
WEBCORE_EXPORT JSC::JSValue forwardForEachCallToBackingMap(JSDOMGlobalObject&, JSC::CallFrame&, JSC::JSObject&);
WEBCORE_EXPORT void clearBackingMap(JSC::JSGlobalObject&, JSC::JSObject&);
WEBCORE_EXPORT void setToBackingMap(JSC::JSGlobalObject&, JSC::JSObject&, JSC::JSValue, JSC::JSValue);

template<typename WrapperClass> JSC::JSValue forwardSizeToMapLike(JSC::JSGlobalObject&, WrapperClass&);
template<typename WrapperClass> JSC::JSValue forwardEntriesToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&);
template<typename WrapperClass> JSC::JSValue forwardKeysToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&);
template<typename WrapperClass> JSC::JSValue forwardValuesToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&);
template<typename WrapperClass> JSC::JSValue forwardClearToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&);
template<typename WrapperClass, typename Callback> JSC::JSValue forwardForEachToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&, Callback&&);
template<typename WrapperClass, typename ItemType> JSC::JSValue forwardGetToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&, ItemType&&);
template<typename WrapperClass, typename ItemType> JSC::JSValue forwardHasToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&, ItemType&&);
template<typename WrapperClass, typename ItemType, typename ValueType> JSC::JSValue forwardSetToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&, ItemType&&, ValueType&&);
template<typename WrapperClass, typename ItemType> JSC::JSValue forwardDeleteToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&, ItemType&&);

class DOMMapAdapter {
public:
    DOMMapAdapter(JSC::JSGlobalObject&, JSC::JSObject&);
    template<typename IDLKeyType, typename IDLValueType> void set(typename IDLKeyType::ParameterType, typename IDLValueType::ParameterType);
    void clear();

private:
    JSC::JSGlobalObject& m_lexicalGlobalObject;
    JSC::JSObject& m_backingMap;
};

inline DOMMapAdapter::DOMMapAdapter(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& backingMap)
    : m_lexicalGlobalObject(lexicalGlobalObject)
    , m_backingMap(backingMap)
{
}

template<typename IDLKeyType, typename IDLValueType>
void DOMMapAdapter::set(typename IDLKeyType::ParameterType key, typename IDLValueType::ParameterType value)
{
    JSC::JSLockHolder locker(&m_lexicalGlobalObject);
    auto jsKey = toJS<IDLKeyType>(m_lexicalGlobalObject, *JSC::jsCast<JSDOMGlobalObject*>(&m_lexicalGlobalObject), std::forward<typename IDLKeyType::ParameterType>(key));
    auto jsValue = toJS<IDLValueType>(m_lexicalGlobalObject, *JSC::jsCast<JSDOMGlobalObject*>(&m_lexicalGlobalObject), std::forward<typename IDLValueType::ParameterType>(value));
    setToBackingMap(m_lexicalGlobalObject, m_backingMap, jsKey, jsValue);
}

template<typename WrapperClass> JSC::JSObject& getAndInitializeBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, WrapperClass& mapLike)
{
    auto pair = getBackingMap(lexicalGlobalObject, mapLike);
    if (pair.first) {
        DOMMapAdapter adapter { lexicalGlobalObject, pair.second.get() };
        mapLike.wrapped().initializeMapLike(adapter);
    }
    return pair.second.get();
}

template<typename WrapperClass> JSC::JSValue forwardSizeToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, WrapperClass& mapLike)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardAttributeGetterToBackingMap(lexicalGlobalObject, getAndInitializeBackingMap(lexicalGlobalObject, mapLike), vm.propertyNames->size);
}

template<typename WrapperClass> JSC::JSValue forwardEntriesToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, getAndInitializeBackingMap(lexicalGlobalObject, mapLike), vm.propertyNames->builtinNames().entriesPublicName());
}

template<typename WrapperClass> JSC::JSValue forwardKeysToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, getAndInitializeBackingMap(lexicalGlobalObject, mapLike), vm.propertyNames->builtinNames().keysPublicName());
}

template<typename WrapperClass> JSC::JSValue forwardValuesToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, getAndInitializeBackingMap(lexicalGlobalObject, mapLike), vm.propertyNames->builtinNames().valuesPublicName());
}

template<typename WrapperClass> JSC::JSValue forwardClearToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike)
{
    mapLike.wrapped().clear();
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, getAndInitializeBackingMap(lexicalGlobalObject, mapLike), vm.propertyNames->clear);
}

template<typename WrapperClass, typename Callback> JSC::JSValue forwardForEachToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike, Callback&&)
{
    getAndInitializeBackingMap(lexicalGlobalObject, mapLike);
    return forwardForEachCallToBackingMap(*JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject), callFrame, mapLike);
}

template<typename WrapperClass, typename ItemType> JSC::JSValue forwardGetToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike, ItemType&&)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, getAndInitializeBackingMap(lexicalGlobalObject, mapLike), vm.propertyNames->builtinNames().getPublicName());
}

template<typename WrapperClass, typename ItemType> JSC::JSValue forwardHasToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike, ItemType&&)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, getAndInitializeBackingMap(lexicalGlobalObject, mapLike), vm.propertyNames->builtinNames().hasPublicName());
}

template<typename WrapperClass, typename KeyType, typename ValueType> JSC::JSValue forwardSetToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike, KeyType&& key, ValueType&& value)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    mapLike.wrapped().setFromMapLike(std::forward<KeyType>(key), std::forward<ValueType>(value));
    forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, getAndInitializeBackingMap(lexicalGlobalObject, mapLike), vm.propertyNames->set);
    return &mapLike;
}

template<typename WrapperClass, typename ItemType> JSC::JSValue forwardDeleteToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike, ItemType&& item)
{
    auto isDeleted = mapLike.wrapped().remove(std::forward<ItemType>(item));
    UNUSED_PARAM(isDeleted);

    auto& vm = JSC::getVM(&lexicalGlobalObject);
    auto result = forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, getAndInitializeBackingMap(lexicalGlobalObject, mapLike), vm.propertyNames->deleteKeyword);

    ASSERT_UNUSED(result, result.asBoolean() == isDeleted);
    return result;
}

}
