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
#include "runtime_object.h"

#include "JSDOMBinding.h"
#include "runtime_method.h"
#include "runtime_root.h"
#include <runtime/Error.h>
#include <runtime/ObjectPrototype.h>

using namespace WebCore;

namespace JSC {

using namespace Bindings;

const ClassInfo RuntimeObjectImp::s_info = { "RuntimeObject", 0, 0, 0 };

RuntimeObjectImp::RuntimeObjectImp(ExecState* exec, PassRefPtr<Instance> i)
    // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object
    // We need to pass in the right global object for "i".
    : JSObject(deprecatedGetDOMStructure<RuntimeObjectImp>(exec))
    , instance(i)
{
    instance->rootObject()->addRuntimeObject(this);
}
    
RuntimeObjectImp::RuntimeObjectImp(ExecState*, PassRefPtr<Structure> structure, PassRefPtr<Instance> i)
    : JSObject(structure)
    , instance(i)
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

JSValue RuntimeObjectImp::fallbackObjectGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    RuntimeObjectImp* thisObj = static_cast<RuntimeObjectImp*>(asObject(slot.slotBase()));
    RefPtr<Instance> instance = thisObj->instance;

    if (!instance)
        return throwInvalidAccessError(exec);
    
    instance->begin();

    Class *aClass = instance->getClass();
    JSValue result = aClass->fallbackObject(exec, instance.get(), propertyName);

    instance->end();
            
    return result;
}

JSValue RuntimeObjectImp::fieldGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{    
    RuntimeObjectImp* thisObj = static_cast<RuntimeObjectImp*>(asObject(slot.slotBase()));
    RefPtr<Instance> instance = thisObj->instance;

    if (!instance)
        return throwInvalidAccessError(exec);
    
    instance->begin();

    Class *aClass = instance->getClass();
    Field* aField = aClass->fieldNamed(propertyName, instance.get());
    JSValue result = aField->valueFromInstance(exec, instance.get());
    
    instance->end();
            
    return result;
}

JSValue RuntimeObjectImp::methodGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    RuntimeObjectImp* thisObj = static_cast<RuntimeObjectImp*>(asObject(slot.slotBase()));
    RefPtr<Instance> instance = thisObj->instance;

    if (!instance)
        return throwInvalidAccessError(exec);
    
    instance->begin();

    Class *aClass = instance->getClass();
    MethodList methodList = aClass->methodsNamed(propertyName, instance.get());
    JSValue result = new (exec) RuntimeMethod(exec, propertyName, methodList);

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
        if (!aClass->fallbackObject(exec, instance.get(), propertyName).isUndefined()) {
            slot.setCustom(this, fallbackObjectGetter);
            instance->end();
            return true;
        }
    }
        
    instance->end();
    
    return instance->getOwnPropertySlot(this, exec, propertyName, slot);
}

bool RuntimeObjectImp::getOwnPropertyDescriptor(ExecState *exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
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
            PropertySlot slot;
            slot.setCustom(this, fieldGetter);
            instance->end();
            descriptor.setDescriptor(slot.getValue(exec, propertyName), DontDelete);
            return true;
        } else {
            // Now check if a method with specified name exists, if so return a function object for
            // that method.
            MethodList methodList = aClass->methodsNamed(propertyName, instance.get());
            if (methodList.size() > 0) {
                PropertySlot slot;
                slot.setCustom(this, methodGetter);
                instance->end();
                descriptor.setDescriptor(slot.getValue(exec, propertyName), DontDelete | ReadOnly);
                return true;
            }
        }
        
        // Try a fallback object.
        if (!aClass->fallbackObject(exec, instance.get(), propertyName).isUndefined()) {
            PropertySlot slot;
            slot.setCustom(this, fallbackObjectGetter);
            instance->end();
            descriptor.setDescriptor(slot.getValue(exec, propertyName), DontDelete | ReadOnly | DontEnum);
            return true;
        }
    }
    
    instance->end();
    
    return instance->getOwnPropertyDescriptor(this, exec, propertyName, descriptor);
}

void RuntimeObjectImp::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    if (!instance) {
        throwInvalidAccessError(exec);
        return;
    }
    
    RefPtr<Instance> protector(instance);
    instance->begin();

    // Set the value of the property.
    Field *aField = instance->getClass()->fieldNamed(propertyName, instance.get());
    if (aField)
        aField->setValueToInstance(exec, instance.get(), value);
    else if (!instance->setValueOfUndefinedField(exec, propertyName, value))
        instance->put(this, exec, propertyName, value, slot);

    instance->end();
}

bool RuntimeObjectImp::deleteProperty(ExecState*, const Identifier&)
{
    // Can never remove a property of a RuntimeObject.
    return false;
}

JSValue RuntimeObjectImp::defaultValue(ExecState* exec, PreferredPrimitiveType hint) const
{
    if (!instance)
        return throwInvalidAccessError(exec);
    
    RefPtr<Instance> protector(instance);
    instance->begin();
    JSValue result = instance->defaultValue(exec, hint);
    instance->end();
    return result;
}

static JSValue JSC_HOST_CALL callRuntimeObject(ExecState* exec, JSObject* function, JSValue, const ArgList& args)
{
    RefPtr<Instance> instance(static_cast<RuntimeObjectImp*>(function)->getInternalInstance());
    instance->begin();
    JSValue result = instance->invokeDefaultMethod(exec, args);
    instance->end();
    return result;
}

CallType RuntimeObjectImp::getCallData(CallData& callData)
{
    if (!instance || !instance->supportsInvokeDefaultMethod())
        return CallTypeNone;
    callData.native.function = callRuntimeObject;
    return CallTypeHost;
}

static JSObject* callRuntimeConstructor(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    RefPtr<Instance> instance(static_cast<RuntimeObjectImp*>(constructor)->getInternalInstance());
    instance->begin();
    JSValue result = instance->invokeConstruct(exec, args);
    instance->end();
    
    ASSERT(result);
    return result.isObject() ? static_cast<JSObject*>(result.asCell()) : constructor;
}

ConstructType RuntimeObjectImp::getConstructData(ConstructData& constructData)
{
    if (!instance || !instance->supportsConstruct())
        return ConstructTypeNone;
    constructData.native.function = callRuntimeConstructor;
    return ConstructTypeHost;
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

void RuntimeObjectImp::getOwnPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    getOwnPropertyNames(exec, propertyNames);
}

JSObject* RuntimeObjectImp::throwInvalidAccessError(ExecState* exec)
{
    return throwError(exec, ReferenceError, "Trying to access object from destroyed plug-in.");
}

}
