/*
 * Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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
#include "runtime_array.h"

#include "JSDOMBinding.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/ArrayPrototype.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/PropertyNameArray.h>

using namespace WebCore;

namespace JSC {

const ClassInfo RuntimeArray::s_info = { "RuntimeArray", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RuntimeArray) };

static EncodedJSValue JIT_OPERATION arrayLengthGetter(JSGlobalObject*, EncodedJSValue, PropertyName);

RuntimeArray::RuntimeArray(VM& vm, Structure* structure)
    : JSArray(vm, structure, nullptr)
    , m_array(nullptr)
{
}

void RuntimeArray::finishCreation(VM& vm, Bindings::Array* array)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    m_array = array;
}

RuntimeArray::~RuntimeArray()
{
    delete getConcreteArray();
}

void RuntimeArray::destroy(JSCell* cell)
{
    static_cast<RuntimeArray*>(cell)->RuntimeArray::~RuntimeArray();
}

EncodedJSValue JIT_OPERATION arrayLengthGetter(JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeArray* thisObject = jsDynamicCast<RuntimeArray*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(lexicalGlobalObject, scope);
    return JSValue::encode(jsNumber(thisObject->getLength()));
}

void RuntimeArray::getOwnPropertyNames(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = lexicalGlobalObject->vm();
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
    unsigned length = thisObject->getLength();
    for (unsigned i = 0; i < length; ++i)
        propertyNames.add(Identifier::from(vm, i));

    if (mode.includeDontEnumProperties())
        propertyNames.add(vm.propertyNames->length);

    JSObject::getOwnPropertyNames(thisObject, lexicalGlobalObject, propertyNames, mode);
}

bool RuntimeArray::getOwnPropertySlot(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
    if (propertyName == vm.propertyNames->length) {
        slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, arrayLengthGetter);
        return true;
    }
    
    Optional<uint32_t> index = parseIndex(propertyName);
    if (index && index.value() < thisObject->getLength()) {
        slot.setValue(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum,
            thisObject->getConcreteArray()->valueAt(lexicalGlobalObject, index.value()));
        return true;
    }
    
    return JSObject::getOwnPropertySlot(thisObject, lexicalGlobalObject, propertyName, slot);
}

bool RuntimeArray::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* lexicalGlobalObject, unsigned index, PropertySlot& slot)
{
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
    if (index < thisObject->getLength()) {
        slot.setValue(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum,
            thisObject->getConcreteArray()->valueAt(lexicalGlobalObject, index));
        return true;
    }
    
    return JSObject::getOwnPropertySlotByIndex(thisObject, lexicalGlobalObject, index, slot);
}

bool RuntimeArray::put(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeArray* thisObject = jsCast<RuntimeArray*>(cell);
    if (propertyName == vm.propertyNames->length) {
        throwException(lexicalGlobalObject, scope, createRangeError(lexicalGlobalObject, "Range error"));
        return false;
    }
    
    if (Optional<uint32_t> index = parseIndex(propertyName))
        return thisObject->getConcreteArray()->setValueAt(lexicalGlobalObject, index.value(), value);

    RELEASE_AND_RETURN(scope, JSObject::put(thisObject, lexicalGlobalObject, propertyName, value, slot));
}

bool RuntimeArray::putByIndex(JSCell* cell, JSGlobalObject* lexicalGlobalObject, unsigned index, JSValue value, bool)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeArray* thisObject = jsCast<RuntimeArray*>(cell);
    if (index >= thisObject->getLength()) {
        throwException(lexicalGlobalObject, scope, createRangeError(lexicalGlobalObject, "Range error"));
        return false;
    }
    
    return thisObject->getConcreteArray()->setValueAt(lexicalGlobalObject, index, value);
}

bool RuntimeArray::deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&)
{
    return false;
}

bool RuntimeArray::deletePropertyByIndex(JSCell*, JSGlobalObject*, unsigned)
{
    return false;
}

JSC::IsoSubspace* RuntimeArray::subspaceForImpl(JSC::VM& vm)
{
    return &static_cast<JSVMClientData*>(vm.clientData)->runtimeArraySpace();
}

}
