/*
 * Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include <runtime/ArrayPrototype.h>
#include <runtime/Error.h>
#include <runtime/PropertyNameArray.h>
#include "JSDOMBinding.h"

using namespace WebCore;

namespace JSC {

const ClassInfo RuntimeArray::s_info = { "RuntimeArray", &JSArray::s_info, 0, 0, CREATE_METHOD_TABLE(RuntimeArray) };

RuntimeArray::RuntimeArray(ExecState* exec, Structure* structure)
    : JSArray(exec->globalData(), structure)
{
}

void RuntimeArray::finishCreation(JSGlobalData& globalData, Bindings::Array* array)
{
    Base::finishCreation(globalData);
    ASSERT(inherits(&s_info));
    setSubclassData(array);
}

RuntimeArray::~RuntimeArray()
{
    delete getConcreteArray();
}

JSValue RuntimeArray::lengthGetter(ExecState*, JSValue slotBase, const Identifier&)
{
    RuntimeArray* thisObj = static_cast<RuntimeArray*>(asObject(slotBase));
    return jsNumber(thisObj->getLength());
}

JSValue RuntimeArray::indexGetter(ExecState* exec, JSValue slotBase, unsigned index)
{
    RuntimeArray* thisObj = static_cast<RuntimeArray*>(asObject(slotBase));
    return thisObj->getConcreteArray()->valueAt(exec, index);
}

void RuntimeArray::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
    unsigned length = thisObject->getLength();
    for (unsigned i = 0; i < length; ++i)
        propertyNames.add(Identifier::from(exec, i));

    if (mode == IncludeDontEnumProperties)
        propertyNames.add(exec->propertyNames().length);

    JSObject::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

bool RuntimeArray::getOwnPropertySlot(JSCell* cell, ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(cell);
    if (propertyName == exec->propertyNames().length) {
        slot.setCacheableCustom(thisObject, thisObject->lengthGetter);
        return true;
    }
    
    bool ok;
    unsigned index = propertyName.toArrayIndex(ok);
    if (ok) {
        if (index < thisObject->getLength()) {
            slot.setCustomIndex(thisObject, index, thisObject->indexGetter);
            return true;
        }
    }
    
    return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

bool RuntimeArray::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
    if (propertyName == exec->propertyNames().length) {
        PropertySlot slot;
        slot.setCustom(thisObject, lengthGetter);
        descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
        return true;
    }
    
    bool ok;
    unsigned index = propertyName.toArrayIndex(ok);
    if (ok) {
        if (index < thisObject->getLength()) {
            PropertySlot slot;
            slot.setCustomIndex(thisObject, index, indexGetter);
            descriptor.setDescriptor(slot.getValue(exec, propertyName), DontDelete | DontEnum);
            return true;
        }
    }
    
    return JSObject::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
}

bool RuntimeArray::getOwnPropertySlotByIndex(JSCell* cell, ExecState *exec, unsigned index, PropertySlot& slot)
{
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(cell);
    if (index < thisObject->getLength()) {
        slot.setCustomIndex(thisObject, index, thisObject->indexGetter);
        return true;
    }
    
    return JSObject::getOwnPropertySlotByIndex(thisObject, exec, index, slot);
}

void RuntimeArray::put(JSCell* cell, ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(cell);
    if (propertyName == exec->propertyNames().length) {
        throwError(exec, createRangeError(exec, "Range error"));
        return;
    }
    
    bool ok;
    unsigned index = propertyName.toArrayIndex(ok);
    if (ok) {
        thisObject->getConcreteArray()->setValueAt(exec, index, value);
        return;
    }
    
    JSObject::put(thisObject, exec, propertyName, value, slot);
}

void RuntimeArray::putByIndex(JSCell* cell, ExecState* exec, unsigned index, JSValue value)
{
    RuntimeArray* thisObject = jsCast<RuntimeArray*>(cell);
    if (index >= thisObject->getLength()) {
        throwError(exec, createRangeError(exec, "Range error"));
        return;
    }
    
    thisObject->getConcreteArray()->setValueAt(exec, index, value);
}

bool RuntimeArray::deleteProperty(JSCell*, ExecState*, const Identifier&)
{
    return false;
}

bool RuntimeArray::deletePropertyByIndex(JSCell*, ExecState*, unsigned)
{
    return false;
}

}
