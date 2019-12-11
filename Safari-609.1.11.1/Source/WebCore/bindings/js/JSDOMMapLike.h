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
#include "JSDOMGuardedObject.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/CommonIdentifiers.h>
#include <JavaScriptCore/JSMap.h>

namespace WebCore {

JSC::JSMap& createBackingMap(JSC::JSGlobalObject&, JSC::JSGlobalObject&, JSC::JSObject&);
void initializeBackingMap(JSC::VM&, JSC::JSObject&, JSC::JSMap&);
JSC::JSValue forwardAttributeGetterToBackingMap(JSC::JSGlobalObject&, JSC::JSObject&, const JSC::Identifier&);
JSC::JSValue forwardFunctionCallToBackingMap(JSC::JSGlobalObject&, JSC::CallFrame&, JSC::JSObject&, const JSC::Identifier&);
JSC::JSValue forwardForEachCallToBackingMap(JSC::JSGlobalObject&, JSC::CallFrame&, JSDOMGlobalObject&, JSC::JSObject&);

template<typename WrapperClass> void synchronizeBackingMap(JSC::JSGlobalObject&, JSDOMGlobalObject&, WrapperClass&);
template<typename WrapperClass> JSC::JSValue forwardSizeToMapLike(JSC::JSGlobalObject&, WrapperClass&);
template<typename WrapperClass> JSC::JSValue forwardEntriesToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&);
template<typename WrapperClass> JSC::JSValue forwardKeysToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&);
template<typename WrapperClass> JSC::JSValue forwardValuesToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&);
template<typename WrapperClass> JSC::JSValue forwardClearToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&);
template<typename WrapperClass, typename Callback> JSC::JSValue forwardForEachToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&, Callback&&);
template<typename WrapperClass, typename ItemType> JSC::JSValue forwardGetToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&, ItemType&&);
template<typename WrapperClass, typename ItemType> JSC::JSValue forwardHasToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&, ItemType&&);
template<typename WrapperClass, typename ItemType> JSC::JSValue forwardAddToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&, ItemType&&);
template<typename WrapperClass, typename ItemType> JSC::JSValue forwardDeleteToMapLike(JSC::JSGlobalObject&, JSC::CallFrame&, WrapperClass&, ItemType&&);

class DOMMapLike final : public DOMGuarded<JSC::JSMap> {
public:
    static Ref<DOMMapLike> create(JSDOMGlobalObject& globalObject, JSC::JSMap& map) { return adoptRef(*new DOMMapLike(globalObject, map)); }

    template<typename Key, typename Value> void set(typename Key::ParameterType&&, typename Value::ParameterType&&);

    JSC::JSMap* backingMap() { return guarded(); }

protected:
    DOMMapLike(JSDOMGlobalObject& globalObject, JSC::JSMap& map) : DOMGuarded<JSC::JSMap>(globalObject, map) { }
};

template<typename Key, typename Value> inline void DOMMapLike::set(typename Key::ParameterType&& key, typename Value::ParameterType&& value)
{
    if (isEmpty())
        return;
    auto* lexicalGlobalObject = globalObject();
    JSC::JSLockHolder locker(lexicalGlobalObject);
    backingMap()->set(lexicalGlobalObject,
        toJS<Key>(*lexicalGlobalObject, *globalObject(), std::forward<typename Key::ParameterType>(key)),
        toJS<Value>(*lexicalGlobalObject, *globalObject(), std::forward<typename Value::ParameterType>(value)));
}

template<typename WrapperClass> inline void synchronizeBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, WrapperClass& mapLike)
{
    auto backingMap = mapLike.wrapped().backingMap();
    if (backingMap) {
        ASSERT(backingMap->backingMap());
        initializeBackingMap(JSC::getVM(&lexicalGlobalObject), mapLike, *backingMap->backingMap());
        return;
    }
    auto& map = createBackingMap(lexicalGlobalObject, globalObject, mapLike);
    mapLike.wrapped().synchronizeBackingMap(DOMMapLike::create(globalObject, map));
}

template<typename WrapperClass> inline JSC::JSValue forwardSizeToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, WrapperClass& mapLike)
{
    JSC::VM& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardAttributeGetterToBackingMap(lexicalGlobalObject, mapLike, vm.propertyNames->size);
}

template<typename WrapperClass> inline JSC::JSValue forwardEntriesToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike)
{
    JSC::VM& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, mapLike, vm.propertyNames->builtinNames().entriesPublicName());
}

template<typename WrapperClass> inline JSC::JSValue forwardKeysToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike)
{
    JSC::VM& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, mapLike, vm.propertyNames->builtinNames().keysPublicName());
}

template<typename WrapperClass> inline JSC::JSValue forwardValuesToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike)
{
    JSC::VM& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, mapLike, vm.propertyNames->builtinNames().valuesPublicName());
}

template<typename WrapperClass> inline JSC::JSValue forwardClearToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike)
{
    mapLike.wrapped().clear();
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, mapLike, JSC::getVM(&lexicalGlobalObject).propertyNames->clear);
}

template<typename WrapperClass, typename Callback> inline JSC::JSValue forwardForEachToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike, Callback&&)
{
    return forwardForEachCallToBackingMap(lexicalGlobalObject, callFrame, *mapLike.globalObject(), mapLike);
}

template<typename WrapperClass, typename ItemType> inline JSC::JSValue forwardGetToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike, ItemType&&)
{
    JSC::VM& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, mapLike, vm.propertyNames->builtinNames().getPublicName());
}

template<typename WrapperClass, typename ItemType> inline JSC::JSValue forwardHasToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike, ItemType&&)
{
    JSC::VM& vm = JSC::getVM(&lexicalGlobalObject);
    return forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, mapLike, vm.propertyNames->builtinNames().hasPublicName());
}

template<typename WrapperClass, typename ItemType> inline JSC::JSValue forwardAddToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike, ItemType&& item)
{
    if (mapLike.wrapped().addFromMapLike(std::forward<ItemType>(item)))
        forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, mapLike, JSC::getVM(&lexicalGlobalObject).propertyNames->add);
    return &mapLike;
}

template<typename WrapperClass, typename ItemType> inline JSC::JSValue forwardDeleteToMapLike(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, WrapperClass& mapLike, ItemType&& item)
{
    auto isDeleted = mapLike.wrapped().remove(std::forward<ItemType>(item));
    UNUSED_PARAM(isDeleted);
    auto result = forwardFunctionCallToBackingMap(lexicalGlobalObject, callFrame, mapLike, JSC::getVM(&lexicalGlobalObject).propertyNames->deleteKeyword);
    ASSERT_UNUSED(result, result.asBoolean() == isDeleted);
    return result;
}

}
