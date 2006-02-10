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

#include "error_object.h"
#include "function.h"
#include "interpreter.h"
#include "object.h"
#include "operations.h"
#include "runtime_method.h"
#include "runtime_object.h"
#include "types.h"
#include "value.h"


#include <assert.h>

using namespace KJS;
using namespace Bindings;

const ClassInfo RuntimeObjectImp::info = {"RuntimeObject", 0, 0, 0};

RuntimeObjectImp::RuntimeObjectImp(JSObject *proto)
  : JSObject(proto)
{
    instance = 0;
}

RuntimeObjectImp::~RuntimeObjectImp()
{
    if (ownsInstance) {
        delete instance;
    }
}

RuntimeObjectImp::RuntimeObjectImp(Bindings::Instance *i, bool oi)
{
    ownsInstance = oi;
    instance = i;
}

JSValue *RuntimeObjectImp::fallbackObjectGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    RuntimeObjectImp *thisObj = static_cast<RuntimeObjectImp *>(slot.slotBase());
    Bindings::Instance *instance = thisObj->instance;

    instance->begin();

    Class *aClass = instance->getClass();
    JSValue *result = aClass->fallbackObject(exec, instance, propertyName);

    instance->end();
            
    return result;
}

JSValue *RuntimeObjectImp::fieldGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    RuntimeObjectImp *thisObj = static_cast<RuntimeObjectImp *>(slot.slotBase());
    Bindings::Instance *instance = thisObj->instance;

    instance->begin();

    Class *aClass = instance->getClass();
    Field *aField = aClass->fieldNamed(propertyName.ascii(), instance);
    JSValue *result = instance->getValueOfField(exec, aField); 
    
    instance->end();
            
    return result;
}

JSValue *RuntimeObjectImp::methodGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    RuntimeObjectImp *thisObj = static_cast<RuntimeObjectImp *>(slot.slotBase());
    Bindings::Instance *instance = thisObj->instance;

    instance->begin();

    Class *aClass = instance->getClass();
    MethodList methodList = aClass->methodsNamed(propertyName.ascii(), instance);
    JSValue *result = new RuntimeMethod(exec, propertyName, methodList);

    instance->end();
            
    return result;
}

bool RuntimeObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    instance->begin();
    
    Class *aClass = instance->getClass();
    
    if (aClass) {
        // See if the instance has a field with the specified name.
        Field *aField = aClass->fieldNamed(propertyName.ascii(), instance);
        if (aField) {
            slot.setCustom(this, fieldGetter);
            instance->end();
            return true;
        } else {
            // Now check if a method with specified name exists, if so return a function object for
            // that method.
            MethodList methodList = aClass->methodsNamed(propertyName.ascii(), instance);
            if (methodList.length() > 0) {
                slot.setCustom(this, methodGetter);
                instance->end();
                return true;
            }
        }

        // Try a fallback object.
        if (!aClass->fallbackObject(exec, instance, propertyName)->isUndefined()) {
            slot.setCustom(this, fallbackObjectGetter);
            instance->end();
            return true;
        }
    }
        
    instance->end();

    // don't call superclass, because runtime objects can't have custom properties or a prototype
    return false;
}

void RuntimeObjectImp::put(ExecState *exec, const Identifier &propertyName,
                    JSValue *value, int attr)
{
    instance->begin();

    // Set the value of the property.
    Field *aField = instance->getClass()->fieldNamed(propertyName.ascii(), instance);
    if (aField) {
        getInternalInstance()->setValueOfField(exec, aField, value);
    }
    else {
        if (getInternalInstance()->supportsSetValueOfUndefinedField()){
            getInternalInstance()->setValueOfUndefinedField(exec, propertyName, value);
        }
    }

    instance->end();
}

bool RuntimeObjectImp::canPut(ExecState *exec, const Identifier &propertyName) const
{
    bool result = false;

    instance->begin();

    Field *aField = instance->getClass()->fieldNamed(propertyName.ascii(), instance);

    instance->end();

    if (aField)
        return true;
    
    return result;
}

bool RuntimeObjectImp::deleteProperty(ExecState *exec,
                            const Identifier &propertyName)
{
    // Can never remove a property of a RuntimeObject.
    return false;
}

JSValue *RuntimeObjectImp::defaultValue(ExecState *exec, JSType hint) const
{
    JSValue *result;
    
    instance->begin();

    result = getInternalInstance()->defaultValue(hint);
    
    instance->end();
    
    return result;
}
    
bool RuntimeObjectImp::implementsCall() const
{
    // Only true for default functions.
    return true;
}

JSValue *RuntimeObjectImp::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
    instance->begin();

    JSValue *aValue = getInternalInstance()->invokeDefaultMethod(exec, args);
    
    instance->end();
    
    return aValue;
}

