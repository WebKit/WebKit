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
#include "runtime_array.h"

#include "JSGlobalObject.h"
#include "array_object.h"

using namespace KJS;

const ClassInfo RuntimeArray::info = { "RuntimeArray", &ArrayInstance::info, 0 };

RuntimeArray::RuntimeArray(ExecState *exec, Bindings::Array *a)
    : JSObject(exec->lexicalGlobalObject()->arrayPrototype())
    , _array(a)
{
}

JSValue *RuntimeArray::lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot)
{
    RuntimeArray *thisObj = static_cast<RuntimeArray *>(slot.slotBase());
    return jsNumber(thisObj->getLength());
}

JSValue *RuntimeArray::indexGetter(ExecState* exec, JSObject*, const Identifier&, const PropertySlot& slot)
{
    RuntimeArray *thisObj = static_cast<RuntimeArray *>(slot.slotBase());
    return thisObj->getConcreteArray()->valueAt(exec, slot.index());
}

bool RuntimeArray::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().length) {
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
    
    return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

bool RuntimeArray::getOwnPropertySlot(ExecState *exec, unsigned index, PropertySlot& slot)
{
    if (index < getLength()) {
        slot.setCustomIndex(this, index, indexGetter);
        return true;
    }
    
    return JSObject::getOwnPropertySlot(exec, index, slot);
}

void RuntimeArray::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    if (propertyName == exec->propertyNames().length) {
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

void RuntimeArray::put(ExecState* exec, unsigned index, JSValue* value, int)
{
    if (index >= getLength()) {
        throwError(exec, RangeError);
        return;
    }
    
    getConcreteArray()->setValueAt(exec, index, value);
}

bool RuntimeArray::deleteProperty(ExecState*, const Identifier&)
{
    return false;
}

bool RuntimeArray::deleteProperty(ExecState*, unsigned)
{
    return false;
}
