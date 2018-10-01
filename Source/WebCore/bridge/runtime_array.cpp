/*
 * Copyright (C) 2003, 2008, 2016 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/ArrayPrototype.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/PropertyNameArray.h>

using namespace WebCore;

namespace JSC {

const ClassInfo RuntimeArray::s_info = { "RuntimeArray", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RuntimeArray) };

RuntimeArray::RuntimeArray(ExecState* exec, Structure* structure)
    : JSArray(exec->vm(), structure, 0)
    , m_array(0)
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

EncodedJSValue RuntimeArray::lengthGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeArray* thisObject = jsDynamicCast<RuntimeArray*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(exec, scope);
    return JSValue::encode(jsNumber(thisObject->getLength()));
}

void RuntimeArray::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = exec->vm();
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
    unsigned length = thisObject->getLength();
    for (unsigned i = 0; i < length; ++i)
        propertyNames.add(Identifier::from(exec, i));

    if (mode.includeDontEnumProperties())
        propertyNames.add(vm.propertyNames->length);

    JSObject::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

bool RuntimeArray::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
    if (propertyName == vm.propertyNames->length) {
        slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, thisObject->lengthGetter);
        return true;
    }
    
    std::optional<uint32_t> index = parseIndex(propertyName);
    if (index && index.value() < thisObject->getLength()) {
        slot.setValue(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum,
            thisObject->getConcreteArray()->valueAt(exec, index.value()));
        return true;
    }
    
    return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

bool RuntimeArray::getOwnPropertySlotByIndex(JSObject* object, ExecState *exec, unsigned index, PropertySlot& slot)
{
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
    if (index < thisObject->getLength()) {
        slot.setValue(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum,
            thisObject->getConcreteArray()->valueAt(exec, index));
        return true;
    }
    
    return JSObject::getOwnPropertySlotByIndex(thisObject, exec, index, slot);
}

bool RuntimeArray::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeArray* thisObject = jsCast<RuntimeArray*>(cell);
    if (propertyName == vm.propertyNames->length) {
        throwException(exec, scope, createRangeError(exec, "Range error"));
        return false;
    }
    
    if (std::optional<uint32_t> index = parseIndex(propertyName))
        return thisObject->getConcreteArray()->setValueAt(exec, index.value(), value);

    RELEASE_AND_RETURN(scope, JSObject::put(thisObject, exec, propertyName, value, slot));
}

bool RuntimeArray::putByIndex(JSCell* cell, ExecState* exec, unsigned index, JSValue value, bool)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeArray* thisObject = jsCast<RuntimeArray*>(cell);
    if (index >= thisObject->getLength()) {
        throwException(exec, scope, createRangeError(exec, "Range error"));
        return false;
    }
    
    return thisObject->getConcreteArray()->setValueAt(exec, index, value);
}

bool RuntimeArray::deleteProperty(JSCell*, ExecState*, PropertyName)
{
    return false;
}

bool RuntimeArray::deletePropertyByIndex(JSCell*, ExecState*, unsigned)
{
    return false;
}

}
