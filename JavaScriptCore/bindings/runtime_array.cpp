/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#include <identifier.h>
#include <interpreter.h>
#include <runtime.h>
#include <runtime_array.h>

using namespace KJS;

const ClassInfo RuntimeArray::info = {"RuntimeArray", &ArrayInstance::info, 0, 0};

RuntimeArray::RuntimeArray(ExecState *exec, Bindings::Array *a)
    : ArrayInstance(exec->lexicalInterpreter()->builtinArrayPrototype(), a->getLength())
{
    // Always takes ownership of concrete array.
    _array = a;
}

RuntimeArray::~RuntimeArray()
{
    delete _array;
}

JSValue *RuntimeArray::lengthGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    RuntimeArray *thisObj = static_cast<RuntimeArray *>(slot.slotBase());
    return jsNumber(thisObj->getLength());
}

JSValue *RuntimeArray::indexGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    RuntimeArray *thisObj = static_cast<RuntimeArray *>(slot.slotBase());
    return thisObj->getConcreteArray()->valueAt(exec, slot.index());
}

bool RuntimeArray::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == lengthPropertyName) {
        slot.setCustom(this, lengthGetter);
        return true;
    }
    
    bool ok;
    unsigned index = propertyName.toArrayIndex(&ok);
    if (ok) {
        if (index < getLength()) {
            slot.setCustomIndex(this, index, indexGetter);
            return true;
        }
    }
    
    return ArrayInstance::getOwnPropertySlot(exec, propertyName, slot);
}

bool RuntimeArray::getOwnPropertySlot(ExecState *exec, unsigned index, PropertySlot& slot)
{
    if (index < getLength()) {
        slot.setCustomIndex(this, index, indexGetter);
        return true;
    }
    
    return ArrayInstance::getOwnPropertySlot(exec, index, slot);
}

void RuntimeArray::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
    if (propertyName == lengthPropertyName) {
        throwError(exec, RangeError);
        return;
    }
    
    bool ok;
    unsigned index = propertyName.toArrayIndex(&ok);
    if (ok) {
        getConcreteArray()->setValueAt(exec, index, value);
        return;
    }
    
    JSObject::put(exec, propertyName, value, attr);
}

void RuntimeArray::put(ExecState *exec, unsigned index, JSValue *value, int attr)
{
    if (index >= getLength()) {
        throwError(exec, RangeError);
        return;
    }
    
    getConcreteArray()->setValueAt(exec, index, value);
}

bool RuntimeArray::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
    return false;
}

bool RuntimeArray::deleteProperty(ExecState *exec, unsigned index)
{
    return false;
}
