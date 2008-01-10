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
#include "runtime_object.h"

#include "error_object.h"
#include "operations.h"
#include "runtime_method.h"
#include "runtime_root.h"

using namespace KJS;
using namespace Bindings;

const ClassInfo RuntimeObjectImp::info = { "RuntimeObject", 0, 0 };

RuntimeObjectImp::RuntimeObjectImp(Bindings::Instance *i)
: instance(i)
{
    instance->rootObject()->addRuntimeObject(this);
}

RuntimeObjectImp::~RuntimeObjectImp()
{
    if (instance)
        instance->rootObject()->removeRuntimeObject(this);
}

void RuntimeObjectImp::invalidate()
{
    ASSERT(instance);
    instance = 0;
}

JSValue *RuntimeObjectImp::fallbackObjectGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    RuntimeObjectImp *thisObj = static_cast<RuntimeObjectImp *>(slot.slotBase());
    RefPtr<Bindings::Instance> instance = thisObj->instance;

    if (!instance)
        return throwInvalidAccessError(exec);
    
    instance->begin();

    Class *aClass = instance->getClass();
    JSValue* result = aClass->fallbackObject(exec, instance.get(), propertyName);

    instance->end();
            
    return result;
}

JSValue *RuntimeObjectImp::fieldGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{    
    RuntimeObjectImp *thisObj = static_cast<RuntimeObjectImp *>(slot.slotBase());
    RefPtr<Bindings::Instance> instance = thisObj->instance;

    if (!instance)
        return throwInvalidAccessError(exec);
    
    instance->begin();

    Class *aClass = instance->getClass();
    Field* aField = aClass->fieldNamed(propertyName, instance.get());
    JSValue *result = instance->getValueOfField(exec, aField); 
    
    instance->end();
            
    return result;
}

JSValue *RuntimeObjectImp::methodGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    RuntimeObjectImp *thisObj = static_cast<RuntimeObjectImp *>(slot.slotBase());
    RefPtr<Bindings::Instance> instance = thisObj->instance;

    if (!instance)
        return throwInvalidAccessError(exec);
    
    instance->begin();

    Class *aClass = instance->getClass();
    MethodList methodList = aClass->methodsNamed(propertyName, instance.get());
    JSValue *result = new RuntimeMethod(exec, propertyName, methodList);

    instance->end();
            
    return result;
}

bool RuntimeObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (!instance) {
        throwInvalidAccessError(exec);
        return false;
    }
    
    instance->begin();
    
    Class *aClass = instance->getClass();
    
    if (aClass) {
        // See if the instance has a field with the specified name.
        Field *aField = aClass->fieldNamed(propertyName, instance.get());
        if (aField) {
            slot.setCustom(this, fieldGetter);
            instance->end();
            return true;
        } else {
            // Now check if a method with specified name exists, if so return a function object for
            // that method.
            MethodList methodList = aClass->methodsNamed(propertyName, instance.get());
            if (methodList.size() > 0) {
                slot.setCustom(this, methodGetter);
                
                instance->end();
                return true;
            }
        }

        // Try a fallback object.
        if (!aClass->fallbackObject(exec, instance.get(), propertyName)->isUndefined()) {
            slot.setCustom(this, fallbackObjectGetter);
            instance->end();
            return true;
        }
    }
        
    instance->end();

    // don't call superclass, because runtime objects can't have custom properties or a prototype
    return false;
}

void RuntimeObjectImp::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int)
{
    if (!instance) {
        throwInvalidAccessError(exec);
        return;
    }
    
    RefPtr<Bindings::Instance> protector(instance);
    instance->begin();

    // Set the value of the property.
    Field *aField = instance->getClass()->fieldNamed(propertyName, instance.get());
    if (aField)
        instance->setValueOfField(exec, aField, value);
    else if (instance->supportsSetValueOfUndefinedField())
        instance->setValueOfUndefinedField(exec, propertyName, value);

    instance->end();
}

bool RuntimeObjectImp::canPut(ExecState* exec, const Identifier& propertyName) const
{
    if (!instance) {
        throwInvalidAccessError(exec);
        return false;
    }
    
    instance->begin();

    Field *aField = instance->getClass()->fieldNamed(propertyName, instance.get());

    instance->end();

    return !!aField;
}

bool RuntimeObjectImp::deleteProperty(ExecState*, const Identifier&)
{
    // Can never remove a property of a RuntimeObject.
    return false;
}

JSValue *RuntimeObjectImp::defaultValue(ExecState* exec, JSType hint) const
{
    if (!instance)
        return throwInvalidAccessError(exec);
    
    JSValue *result;
    
    RefPtr<Bindings::Instance> protector(instance);
    instance->begin();

    result = instance->defaultValue(hint);
    
    instance->end();
    
    return result;
}
    
bool RuntimeObjectImp::implementsCall() const
{
    if (!instance)
        return false;
    
    return instance->implementsCall();
}

JSValue *RuntimeObjectImp::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    if (!instance)
        return throwInvalidAccessError(exec);

    RefPtr<Bindings::Instance> protector(instance);
    instance->begin();

    JSValue *aValue = instance->invokeDefaultMethod(exec, args);
    
    instance->end();
    
    return aValue;
}

void RuntimeObjectImp::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    if (!instance) {
        throwInvalidAccessError(exec);
        return;
    }
    
    instance->begin();
    instance->getPropertyNames(exec, propertyNames);
    instance->end();
}

JSObject* RuntimeObjectImp::throwInvalidAccessError(ExecState* exec)
{
    return throwError(exec, ReferenceError, "Trying to access object from destroyed plug-in.");
}
