/*
 * Copyright (C) 2003, 2008-2009, 2016 Apple Inc. All rights reserved.
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
#include "runtime_object.h"

#include "JSDOMBinding.h"
#include "WebCoreJSClientData.h"
#include "runtime_method.h"
#include <JavaScriptCore/Error.h>

using namespace WebCore;

namespace JSC {
namespace Bindings {

WEBCORE_EXPORT const ClassInfo RuntimeObject::s_info = { "RuntimeObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RuntimeObject) };

RuntimeObject::RuntimeObject(VM& vm, Structure* structure, RefPtr<Instance>&& instance)
    : Base(vm, structure)
    , m_instance(WTFMove(instance))
{
}

void RuntimeObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void RuntimeObject::destroy(JSCell* cell)
{
    static_cast<RuntimeObject*>(cell)->RuntimeObject::~RuntimeObject();
}

void RuntimeObject::invalidate()
{
    ASSERT(m_instance);
    if (m_instance)
        m_instance->willInvalidateRuntimeObject();
    m_instance = nullptr;
}

EncodedJSValue RuntimeObject::fallbackObjectGetter(JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName propertyName)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeObject* thisObj = jsCast<RuntimeObject*>(JSValue::decode(thisValue));
    RefPtr<Instance> instance = thisObj->m_instance;

    if (!instance)
        return JSValue::encode(throwInvalidAccessError(lexicalGlobalObject, scope));
    
    instance->begin();

    Class *aClass = instance->getClass();
    JSValue result = aClass->fallbackObject(lexicalGlobalObject, instance.get(), propertyName);

    instance->end();
            
    return JSValue::encode(result);
}

EncodedJSValue RuntimeObject::fieldGetter(JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName propertyName)
{    
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeObject* thisObj = jsCast<RuntimeObject*>(JSValue::decode(thisValue));
    RefPtr<Instance> instance = thisObj->m_instance;

    if (!instance)
        return JSValue::encode(throwInvalidAccessError(lexicalGlobalObject, scope));
    
    instance->begin();

    Class *aClass = instance->getClass();
    Field* aField = aClass->fieldNamed(propertyName, instance.get());
    JSValue result = aField->valueFromInstance(lexicalGlobalObject, instance.get());
    
    instance->end();
            
    return JSValue::encode(result);
}

EncodedJSValue RuntimeObject::methodGetter(JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName propertyName)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeObject* thisObj = jsCast<RuntimeObject*>(JSValue::decode(thisValue));
    RefPtr<Instance> instance = thisObj->m_instance;

    if (!instance)
        return JSValue::encode(throwInvalidAccessError(lexicalGlobalObject, scope));
    
    instance->begin();

    JSValue method = instance->getMethod(lexicalGlobalObject, propertyName);

    instance->end();
            
    return JSValue::encode(method);
}

bool RuntimeObject::getOwnPropertySlot(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeObject* thisObject = jsCast<RuntimeObject*>(object);
    if (!thisObject->m_instance) {
        throwInvalidAccessError(lexicalGlobalObject, scope);
        return false;
    }
    
    RefPtr<Instance> instance = thisObject->m_instance;

    instance->begin();
    
    Class *aClass = instance->getClass();
    
    if (aClass) {
        // See if the instance has a field with the specified name.
        Field *aField = aClass->fieldNamed(propertyName, instance.get());
        if (aField) {
            slot.setCustom(thisObject, static_cast<unsigned>(JSC::PropertyAttribute::DontDelete), thisObject->fieldGetter);
            instance->end();
            return true;
        } else {
            // Now check if a method with specified name exists, if so return a function object for
            // that method.
            if (aClass->methodNamed(propertyName, instance.get())) {
                slot.setCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly, thisObject->methodGetter);
                
                instance->end();
                return true;
            }
        }

        // Try a fallback object.
        if (!aClass->fallbackObject(lexicalGlobalObject, instance.get(), propertyName).isUndefined()) {
            slot.setCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, thisObject->fallbackObjectGetter);
            instance->end();
            return true;
        }
    }
        
    instance->end();
    
    return instance->getOwnPropertySlot(thisObject, lexicalGlobalObject, propertyName, slot);
}

bool RuntimeObject::put(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeObject* thisObject = jsCast<RuntimeObject*>(cell);
    if (!thisObject->m_instance) {
        throwInvalidAccessError(lexicalGlobalObject, scope);
        return false;
    }
    
    RefPtr<Instance> instance = thisObject->m_instance;
    instance->begin();

    // Set the value of the property.
    bool result = false;
    Field *aField = instance->getClass()->fieldNamed(propertyName, instance.get());
    if (aField)
        result = aField->setValueToInstance(lexicalGlobalObject, instance.get(), value);
    else if (!instance->setValueOfUndefinedField(lexicalGlobalObject, propertyName, value))
        result = instance->put(thisObject, lexicalGlobalObject, propertyName, value, slot);

    instance->end();
    return result;
}

bool RuntimeObject::deleteProperty(JSCell*, JSGlobalObject*, PropertyName)
{
    // Can never remove a property of a RuntimeObject.
    return false;
}

JSValue RuntimeObject::defaultValue(const JSObject* object, JSGlobalObject* lexicalGlobalObject, PreferredPrimitiveType hint)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    const RuntimeObject* thisObject = jsCast<const RuntimeObject*>(object);
    if (!thisObject->m_instance)
        return throwInvalidAccessError(lexicalGlobalObject, scope);
    
    RefPtr<Instance> instance = thisObject->m_instance;

    instance->begin();
    JSValue result = instance->defaultValue(lexicalGlobalObject, hint);
    instance->end();
    return result;
}

static EncodedJSValue JSC_HOST_CALL callRuntimeObject(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ASSERT_UNUSED(globalObject, callFrame->jsCallee()->inherits<RuntimeObject>(globalObject->vm()));
    RefPtr<Instance> instance(static_cast<RuntimeObject*>(callFrame->jsCallee())->getInternalInstance());
    instance->begin();
    JSValue result = instance->invokeDefaultMethod(globalObject, callFrame);
    instance->end();
    return JSValue::encode(result);
}

CallType RuntimeObject::getCallData(JSCell* cell, CallData& callData)
{
    RuntimeObject* thisObject = jsCast<RuntimeObject*>(cell);
    if (!thisObject->m_instance)
        return CallType::None;
    
    RefPtr<Instance> instance = thisObject->m_instance;
    if (!instance->supportsInvokeDefaultMethod())
        return CallType::None;
    
    callData.native.function = callRuntimeObject;
    return CallType::Host;
}

static EncodedJSValue JSC_HOST_CALL callRuntimeConstructor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSObject* constructor = callFrame->jsCallee();
    ASSERT_UNUSED(globalObject, constructor->inherits<RuntimeObject>(globalObject->vm()));
    RefPtr<Instance> instance(static_cast<RuntimeObject*>(callFrame->jsCallee())->getInternalInstance());
    instance->begin();
    ArgList args(callFrame);
    JSValue result = instance->invokeConstruct(globalObject, callFrame, args);
    instance->end();
    
    ASSERT(result);
    return JSValue::encode(result.isObject() ? jsCast<JSObject*>(result.asCell()) : constructor);
}

ConstructType RuntimeObject::getConstructData(JSCell* cell, ConstructData& constructData)
{
    RuntimeObject* thisObject = jsCast<RuntimeObject*>(cell);
    if (!thisObject->m_instance)
        return ConstructType::None;
    
    RefPtr<Instance> instance = thisObject->m_instance;
    if (!instance->supportsConstruct())
        return ConstructType::None;
    
    constructData.native.function = callRuntimeConstructor;
    return ConstructType::Host;
}

void RuntimeObject::getOwnPropertyNames(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyNameArray& propertyNames, EnumerationMode)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeObject* thisObject = jsCast<RuntimeObject*>(object);
    if (!thisObject->m_instance) {
        throwInvalidAccessError(lexicalGlobalObject, scope);
        return;
    }

    RefPtr<Instance> instance = thisObject->m_instance;
    
    instance->begin();
    instance->getPropertyNames(lexicalGlobalObject, propertyNames);
    instance->end();
}

Exception* RuntimeObject::throwInvalidAccessError(JSGlobalObject* lexicalGlobalObject, ThrowScope& scope)
{
    return throwException(lexicalGlobalObject, scope, createReferenceError(lexicalGlobalObject, "Trying to access object from destroyed plug-in."));
}

JSC::IsoSubspace* RuntimeObject::subspaceForImpl(JSC::VM& vm)
{
    return &static_cast<JSVMClientData*>(vm.clientData)->runtimeObjectSpace();
}

}
}
