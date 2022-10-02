/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSObservableArray.h"

#include "JSDOMBinding.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/ArrayPrototype.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/PropertyNameArray.h>

using namespace WebCore;

namespace JSC {

const ClassInfo JSObservableArray::s_info = { "JSObservableArray"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSObservableArray) };

static JSC_DECLARE_CUSTOM_GETTER(arrayLengthGetter);

JSObservableArray::JSObservableArray(VM& vm, Structure* structure)
    : JSArray(vm, structure, nullptr)
{
}

void JSObservableArray::finishCreation(VM& vm, Ref<ObservableArray>&& array)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_array = WTFMove(array);
}

JSObservableArray::~JSObservableArray() = default;

void JSObservableArray::destroy(JSCell* cell)
{
    static_cast<JSObservableArray*>(cell)->JSObservableArray::~JSObservableArray();
}

JSC_DEFINE_CUSTOM_GETTER(arrayLengthGetter, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObservableArray* thisObject = jsDynamicCast<JSObservableArray*>(JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(lexicalGlobalObject, scope);
    return JSValue::encode(jsNumber(thisObject->length()));
}

void JSObservableArray::getOwnPropertyNames(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyNameArray& propertyNames, DontEnumPropertiesMode mode)
{
    VM& vm = lexicalGlobalObject->vm();
    JSObservableArray* thisObject = jsCast<JSObservableArray*>(object);
    unsigned length = thisObject->length();
    for (unsigned i = 0; i < length; ++i)
        propertyNames.add(Identifier::from(vm, i));

    if (mode == DontEnumPropertiesMode::Include)
        propertyNames.add(vm.propertyNames->length);

    thisObject->getOwnNonIndexPropertyNames(lexicalGlobalObject, propertyNames, mode);
}

bool JSObservableArray::getOwnPropertySlot(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    JSObservableArray* thisObject = jsCast<JSObservableArray*>(object);
    if (propertyName == vm.propertyNames->length) {
        slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, arrayLengthGetter);
        return true;
    }

    std::optional<uint32_t> index = parseIndex(propertyName);
    if (index && index.value() < thisObject->length()) {
        slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::DontDelete),
            thisObject->getConcreteArray().valueAt(lexicalGlobalObject, index.value()));
        return true;
    }

    return JSObject::getOwnPropertySlot(thisObject, lexicalGlobalObject, propertyName, slot);
}

bool JSObservableArray::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* lexicalGlobalObject, unsigned index, PropertySlot& slot)
{
    JSObservableArray* thisObject = jsCast<JSObservableArray*>(object);
    if (index < thisObject->length()) {
        slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::DontDelete),
            thisObject->getConcreteArray().valueAt(lexicalGlobalObject, index));
        return true;
    }

    return JSObject::getOwnPropertySlotByIndex(thisObject, lexicalGlobalObject, index, slot);
}

bool JSObservableArray::put(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObservableArray* thisObject = jsCast<JSObservableArray*>(cell);
    if (propertyName == vm.propertyNames->length)
        return false;

    if (std::optional<uint32_t> index = parseIndex(propertyName))
        return thisObject->getConcreteArray().setValueAt(lexicalGlobalObject, index.value(), value);

    RELEASE_AND_RETURN(scope, JSObject::put(thisObject, lexicalGlobalObject, propertyName, value, slot));
}

bool JSObservableArray::putByIndex(JSCell* cell, JSGlobalObject* lexicalGlobalObject, unsigned index, JSValue value, bool)
{
    JSObservableArray* thisObject = jsCast<JSObservableArray*>(cell);
    return thisObject->getConcreteArray().setValueAt(lexicalGlobalObject, index, value);
}

bool JSObservableArray::deleteProperty(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObservableArray* thisObject = jsCast<JSObservableArray*>(cell);
    if (std::optional<uint32_t> index = parseIndex(propertyName))
        return thisObject->getConcreteArray().deleteValueAt(lexicalGlobalObject, index.value());

    RELEASE_AND_RETURN(scope, JSObject::deleteProperty(thisObject, lexicalGlobalObject, propertyName, slot));
}

bool JSObservableArray::deletePropertyByIndex(JSCell* cell, JSGlobalObject* lexicalGlobalObject, unsigned index)
{
    JSObservableArray* thisObject = jsCast<JSObservableArray*>(cell);
    return thisObject->getConcreteArray().deleteValueAt(lexicalGlobalObject, index);
}

JSC::GCClient::IsoSubspace* JSObservableArray::subspaceForImpl(JSC::VM& vm)
{
    return &static_cast<JSVMClientData*>(vm.clientData)->observableArraySpace();
}

} // namespace JSC
