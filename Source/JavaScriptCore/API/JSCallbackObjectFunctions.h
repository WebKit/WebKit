/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#pragma once

#include "APICast.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSCallbackFunction.h"
#include "JSClassRef.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSObjectRef.h"
#include "JSString.h"
#include "OpaqueJSString.h"
#include "PropertyNameArray.h"
#include <wtf/Vector.h>

namespace JSC {

template <class Parent>
inline JSCallbackObject<Parent>* JSCallbackObject<Parent>::asCallbackObject(JSValue value)
{
    ASSERT(asObject(value)->inherits(value.getObject()->vm(), info()));
    return jsCast<JSCallbackObject*>(asObject(value));
}

template <class Parent>
inline JSCallbackObject<Parent>* JSCallbackObject<Parent>::asCallbackObject(EncodedJSValue encodedValue)
{
    JSValue value = JSValue::decode(encodedValue);
    ASSERT(asObject(value)->inherits(value.getObject()->vm(), info()));
    return jsCast<JSCallbackObject*>(asObject(value));
}

template <class Parent>
JSCallbackObject<Parent>::JSCallbackObject(JSGlobalObject* globalObject, Structure* structure, JSClassRef jsClass, void* data)
    : Parent(getVM(globalObject), structure)
    , m_callbackObjectData(makeUnique<JSCallbackObjectData>(data, jsClass))
{
}

// Global object constructor.
// FIXME: Move this into a separate JSGlobalCallbackObject class derived from this one.
template <class Parent>
JSCallbackObject<Parent>::JSCallbackObject(VM& vm, JSClassRef jsClass, Structure* structure)
    : Parent(vm, structure)
    , m_callbackObjectData(makeUnique<JSCallbackObjectData>(nullptr, jsClass))
{
}

template <class Parent>
JSCallbackObject<Parent>::~JSCallbackObject()
{
    VM& vm = this->HeapCell::vm();
    vm.currentlyDestructingCallbackObject = this;
    ASSERT(m_classInfo);
    vm.currentlyDestructingCallbackObjectClassInfo = m_classInfo;
    JSObjectRef thisRef = toRef(static_cast<JSObject*>(this));
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectFinalizeCallback finalize = jsClass->finalize)
            finalize(thisRef);
    }
    vm.currentlyDestructingCallbackObject = nullptr;
    vm.currentlyDestructingCallbackObjectClassInfo = nullptr;
}
    
template <class Parent>
void JSCallbackObject<Parent>::finishCreation(JSGlobalObject* globalObject)
{
    VM& vm = getVM(globalObject);
    Base::finishCreation(vm);
    ASSERT(Parent::inherits(vm, info()));
    init(globalObject);
}

// This is just for Global object, so we can assume that Base::finishCreation is JSGlobalObject::finishCreation.
template <class Parent>
void JSCallbackObject<Parent>::finishCreation(VM& vm)
{
    ASSERT(Parent::inherits(vm, info()));
    ASSERT(Parent::isGlobalObject());
    Base::finishCreation(vm);
    init(jsCast<JSGlobalObject*>(this));
}

template <class Parent>
void JSCallbackObject<Parent>::init(JSGlobalObject* globalObject)
{
    ASSERT(globalObject);
    
    Vector<JSObjectInitializeCallback, 16> initRoutines;
    JSClassRef jsClass = classRef();
    do {
        if (JSObjectInitializeCallback initialize = jsClass->initialize)
            initRoutines.append(initialize);
    } while ((jsClass = jsClass->parentClass));
    
    // initialize from base to derived
    for (int i = static_cast<int>(initRoutines.size()) - 1; i >= 0; i--) {
        JSLock::DropAllLocks dropAllLocks(globalObject);
        JSObjectInitializeCallback initialize = initRoutines[i];
        initialize(toRef(globalObject), toRef(jsCast<JSObject*>(this)));
    }
    
    m_classInfo = this->classInfo(getVM(globalObject));
}

template <class Parent>
String JSCallbackObject<Parent>::className(const JSObject* object, VM& vm)
{
    const JSCallbackObject* thisObject = jsCast<const JSCallbackObject*>(object);
    String thisClassName = thisObject->classRef()->className();
    if (!thisClassName.isEmpty())
        return thisClassName;
    
    return Parent::className(object, vm);
}

template <class Parent>
String JSCallbackObject<Parent>::toStringName(const JSObject* object, JSGlobalObject* globalObject)
{
    VM& vm = getVM(globalObject);
    const ClassInfo* info = object->classInfo(vm);
    ASSERT(info);
    return info->methodTable.className(object, vm);
}

template <class Parent>
bool JSCallbackObject<Parent>::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSCallbackObject* thisObject = jsCast<JSCallbackObject*>(object);
    JSContextRef ctx = toRef(globalObject);
    JSObjectRef thisRef = toRef(jsCast<JSObject*>(thisObject));
    RefPtr<OpaqueJSString> propertyNameRef;
    
    if (StringImpl* name = propertyName.uid()) {
        for (JSClassRef jsClass = thisObject->classRef(); jsClass; jsClass = jsClass->parentClass) {
            // optional optimization to bypass getProperty in cases when we only need to know if the property exists
            if (JSObjectHasPropertyCallback hasProperty = jsClass->hasProperty) {
                if (!propertyNameRef)
                    propertyNameRef = OpaqueJSString::tryCreate(name);
                JSLock::DropAllLocks dropAllLocks(globalObject);
                if (hasProperty(ctx, thisRef, propertyNameRef.get())) {
                    slot.setCustom(thisObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, callbackGetter);
                    return true;
                }
            } else if (JSObjectGetPropertyCallback getProperty = jsClass->getProperty) {
                if (!propertyNameRef)
                    propertyNameRef = OpaqueJSString::tryCreate(name);
                JSValueRef exception = nullptr;
                JSValueRef value;
                {
                    JSLock::DropAllLocks dropAllLocks(globalObject);
                    value = getProperty(ctx, thisRef, propertyNameRef.get(), &exception);
                }
                if (exception) {
                    throwException(globalObject, scope, toJS(globalObject, exception));
                    slot.setValue(thisObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, jsUndefined());
                    return true;
                }
                if (value) {
                    slot.setValue(thisObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, toJS(globalObject, value));
                    return true;
                }
            }
            
            if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(globalObject)) {
                if (staticValues->contains(name)) {
                    JSValue value = thisObject->getStaticValue(globalObject, propertyName);
                    RETURN_IF_EXCEPTION(scope, false);
                    if (value) {
                        slot.setValue(thisObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, value);
                        return true;
                    }
                }
            }
            
            if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(globalObject)) {
                if (staticFunctions->contains(name)) {
                    slot.setCustom(thisObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, staticFunctionGetter);
                    return true;
                }
            }
        }
    }

    RELEASE_AND_RETURN(scope, Parent::getOwnPropertySlot(thisObject, globalObject, propertyName, slot));
}

template <class Parent>
bool JSCallbackObject<Parent>::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* globalObject, unsigned propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    return object->methodTable(vm)->getOwnPropertySlot(object, globalObject, Identifier::from(vm, propertyName), slot);
}

template <class Parent>
JSValue JSCallbackObject<Parent>::defaultValue(const JSObject* object, JSGlobalObject* globalObject, PreferredPrimitiveType hint)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const JSCallbackObject* thisObject = jsCast<const JSCallbackObject*>(object);
    JSContextRef ctx = toRef(globalObject);
    JSObjectRef thisRef = toRef(jsCast<const JSObject*>(thisObject));
    ::JSType jsHint = hint == PreferString ? kJSTypeString : kJSTypeNumber;

    for (JSClassRef jsClass = thisObject->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectConvertToTypeCallback convertToType = jsClass->convertToType) {
            JSValueRef exception = nullptr;
            JSValueRef result = convertToType(ctx, thisRef, jsHint, &exception);
            if (exception) {
                throwException(globalObject, scope, toJS(globalObject, exception));
                return jsUndefined();
            }
            if (result)
                return toJS(globalObject, result);
        }
    }
    
    RELEASE_AND_RETURN(scope, Parent::defaultValue(object, globalObject, hint));
}

template <class Parent>
bool JSCallbackObject<Parent>::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSCallbackObject* thisObject = jsCast<JSCallbackObject*>(cell);
    JSContextRef ctx = toRef(globalObject);
    JSObjectRef thisRef = toRef(jsCast<JSObject*>(thisObject));
    RefPtr<OpaqueJSString> propertyNameRef;
    JSValueRef valueRef = toRef(globalObject, value);
    
    if (StringImpl* name = propertyName.uid()) {
        for (JSClassRef jsClass = thisObject->classRef(); jsClass; jsClass = jsClass->parentClass) {
            if (JSObjectSetPropertyCallback setProperty = jsClass->setProperty) {
                if (!propertyNameRef)
                    propertyNameRef = OpaqueJSString::tryCreate(name);
                JSValueRef exception = nullptr;
                bool result;
                {
                    JSLock::DropAllLocks dropAllLocks(globalObject);
                    result = setProperty(ctx, thisRef, propertyNameRef.get(), valueRef, &exception);
                }
                if (exception)
                    throwException(globalObject, scope, toJS(globalObject, exception));
                if (result || exception)
                    return result;
            }
            
            if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(globalObject)) {
                if (StaticValueEntry* entry = staticValues->get(name)) {
                    if (entry->attributes & kJSPropertyAttributeReadOnly)
                        return false;
                    if (JSObjectSetPropertyCallback setProperty = entry->setProperty) {
                        JSValueRef exception = nullptr;
                        bool result;
                        {
                            JSLock::DropAllLocks dropAllLocks(globalObject);
                            result = setProperty(ctx, thisRef, entry->propertyNameRef.get(), valueRef, &exception);
                        }
                        if (exception)
                            throwException(globalObject, scope, toJS(globalObject, exception));
                        if (result || exception)
                            return result;
                    }
                }
            }
            
            if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(globalObject)) {
                if (StaticFunctionEntry* entry = staticFunctions->get(name)) {
                    PropertySlot getSlot(thisObject, PropertySlot::InternalMethodType::VMInquiry, &vm);
                    bool found = Parent::getOwnPropertySlot(thisObject, globalObject, propertyName, getSlot);
                    RETURN_IF_EXCEPTION(scope, false);
                    getSlot.disallowVMEntry.reset();
                    if (found)
                        RELEASE_AND_RETURN(scope, Parent::put(thisObject, globalObject, propertyName, value, slot));
                    if (entry->attributes & kJSPropertyAttributeReadOnly)
                        return false;
                    return thisObject->JSCallbackObject<Parent>::putDirect(vm, propertyName, value); // put as override property
                }
            }
        }
    }

    RELEASE_AND_RETURN(scope, Parent::put(thisObject, globalObject, propertyName, value, slot));
}

template <class Parent>
bool JSCallbackObject<Parent>::putByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyIndex, JSValue value, bool shouldThrow)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSCallbackObject* thisObject = jsCast<JSCallbackObject*>(cell);
    JSContextRef ctx = toRef(globalObject);
    JSObjectRef thisRef = toRef(jsCast<JSObject*>(thisObject));
    RefPtr<OpaqueJSString> propertyNameRef;
    JSValueRef valueRef = toRef(globalObject, value);
    Identifier propertyName = Identifier::from(vm, propertyIndex);

    for (JSClassRef jsClass = thisObject->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectSetPropertyCallback setProperty = jsClass->setProperty) {
            if (!propertyNameRef)
                propertyNameRef = OpaqueJSString::tryCreate(propertyName.impl());
            JSValueRef exception = nullptr;
            bool result;
            {
                JSLock::DropAllLocks dropAllLocks(globalObject);
                result = setProperty(ctx, thisRef, propertyNameRef.get(), valueRef, &exception);
            }
            if (exception)
                throwException(globalObject, scope, toJS(globalObject, exception));
            if (result || exception)
                return result;
        }

        if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(globalObject)) {
            if (StaticValueEntry* entry = staticValues->get(propertyName.impl())) {
                if (entry->attributes & kJSPropertyAttributeReadOnly)
                    return false;
                if (JSObjectSetPropertyCallback setProperty = entry->setProperty) {
                    JSValueRef exception = nullptr;
                    bool result;
                    {
                        JSLock::DropAllLocks dropAllLocks(globalObject);
                        result = setProperty(ctx, thisRef, entry->propertyNameRef.get(), valueRef, &exception);
                    }
                    if (exception)
                        throwException(globalObject, scope, toJS(globalObject, exception));
                    if (result || exception)
                        return result;
                }
            }
        }

        if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(globalObject)) {
            if (StaticFunctionEntry* entry = staticFunctions->get(propertyName.impl())) {
                if (entry->attributes & kJSPropertyAttributeReadOnly)
                    return false;
                break;
            }
        }
    }

    RELEASE_AND_RETURN(scope, Parent::putByIndex(thisObject, globalObject, propertyIndex, value, shouldThrow));
}

template <class Parent>
bool JSCallbackObject<Parent>::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSCallbackObject* thisObject = jsCast<JSCallbackObject*>(cell);
    JSContextRef ctx = toRef(globalObject);
    JSObjectRef thisRef = toRef(jsCast<JSObject*>(thisObject));
    RefPtr<OpaqueJSString> propertyNameRef;
    
    if (StringImpl* name = propertyName.uid()) {
        for (JSClassRef jsClass = thisObject->classRef(); jsClass; jsClass = jsClass->parentClass) {
            if (JSObjectDeletePropertyCallback deleteProperty = jsClass->deleteProperty) {
                if (!propertyNameRef)
                    propertyNameRef = OpaqueJSString::tryCreate(name);
                JSValueRef exception = nullptr;
                bool result;
                {
                    JSLock::DropAllLocks dropAllLocks(globalObject);
                    result = deleteProperty(ctx, thisRef, propertyNameRef.get(), &exception);
                }
                if (exception)
                    throwException(globalObject, scope, toJS(globalObject, exception));
                if (result || exception)
                    return true;
            }
            
            if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(globalObject)) {
                if (StaticValueEntry* entry = staticValues->get(name)) {
                    if (entry->attributes & kJSPropertyAttributeDontDelete)
                        return false;
                    return true;
                }
            }
            
            if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(globalObject)) {
                if (StaticFunctionEntry* entry = staticFunctions->get(name)) {
                    if (entry->attributes & kJSPropertyAttributeDontDelete)
                        return false;
                    return true;
                }
            }
        }
    }

    static_assert(std::is_final_v<JSCallbackObject<Parent>>, "Ensure no derived classes have custom deletePropertyByIndex implementation");
    if (Optional<uint32_t> index = parseIndex(propertyName))
        RELEASE_AND_RETURN(scope, Parent::deletePropertyByIndex(thisObject, globalObject, index.value()));
    RELEASE_AND_RETURN(scope, Parent::deleteProperty(thisObject, globalObject, propertyName, slot));
}

template <class Parent>
bool JSCallbackObject<Parent>::deletePropertyByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName)
{
    VM& vm = getVM(globalObject);
    JSCallbackObject* thisObject = jsCast<JSCallbackObject*>(cell);
    return JSCell::deleteProperty(thisObject, globalObject, Identifier::from(vm, propertyName));
}

template <class Parent>
CallData JSCallbackObject<Parent>::getConstructData(JSCell* cell)
{
    CallData constructData;
    JSCallbackObject* thisObject = jsCast<JSCallbackObject*>(cell);
    for (JSClassRef jsClass = thisObject->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (jsClass->callAsConstructor) {
            constructData.type = CallData::Type::Native;
            constructData.native.function = construct;
            break;
        }
    }
    return constructData;
}

template <class Parent>
EncodedJSValue JSCallbackObject<Parent>::construct(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* constructor = callFrame->jsCallee();
    JSContextRef execRef = toRef(globalObject);
    JSObjectRef constructorRef = toRef(constructor);
    
    for (JSClassRef jsClass = jsCast<JSCallbackObject<Parent>*>(constructor)->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectCallAsConstructorCallback callAsConstructor = jsClass->callAsConstructor) {
            size_t argumentCount = callFrame->argumentCount();
            Vector<JSValueRef, 16> arguments;
            arguments.reserveInitialCapacity(argumentCount);
            for (size_t i = 0; i < argumentCount; ++i)
                arguments.uncheckedAppend(toRef(globalObject, callFrame->uncheckedArgument(i)));
            JSValueRef exception = nullptr;
            JSObject* result;
            {
                JSLock::DropAllLocks dropAllLocks(globalObject);
                result = toJS(callAsConstructor(execRef, constructorRef, argumentCount, arguments.data(), &exception));
            }
            if (exception) {
                throwException(globalObject, scope, toJS(globalObject, exception));
                return JSValue::encode(jsUndefined());
            }
            return JSValue::encode(result);
        }
    }
    
    RELEASE_ASSERT_NOT_REACHED(); // getConstructData should prevent us from reaching here
    return JSValue::encode(JSValue());
}

template <class Parent>
bool JSCallbackObject<Parent>::customHasInstance(JSObject* object, JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSCallbackObject* thisObject = jsCast<JSCallbackObject*>(object);
    JSContextRef execRef = toRef(globalObject);
    JSObjectRef thisRef = toRef(jsCast<JSObject*>(thisObject));
    
    for (JSClassRef jsClass = thisObject->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectHasInstanceCallback hasInstance = jsClass->hasInstance) {
            JSValueRef valueRef = toRef(globalObject, value);
            JSValueRef exception = nullptr;
            bool result;
            {
                JSLock::DropAllLocks dropAllLocks(globalObject);
                result = hasInstance(execRef, thisRef, valueRef, &exception);
            }
            if (exception)
                throwException(globalObject, scope, toJS(globalObject, exception));
            return result;
        }
    }
    return false;
}

template <class Parent>
CallData JSCallbackObject<Parent>::getCallData(JSCell* cell)
{
    CallData callData;
    JSCallbackObject* thisObject = jsCast<JSCallbackObject*>(cell);
    for (JSClassRef jsClass = thisObject->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (jsClass->callAsFunction) {
            callData.type = CallData::Type::Native;
            callData.native.function = call;
            break;
        }
    }
    return callData;
}

template <class Parent>
EncodedJSValue JSCallbackObject<Parent>::call(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSContextRef execRef = toRef(globalObject);
    JSObjectRef functionRef = toRef(callFrame->jsCallee());
    JSObjectRef thisObjRef = toRef(jsCast<JSObject*>(callFrame->thisValue().toThis(globalObject, ECMAMode::sloppy())));
    
    for (JSClassRef jsClass = jsCast<JSCallbackObject<Parent>*>(toJS(functionRef))->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectCallAsFunctionCallback callAsFunction = jsClass->callAsFunction) {
            size_t argumentCount = callFrame->argumentCount();
            Vector<JSValueRef, 16> arguments;
            arguments.reserveInitialCapacity(argumentCount);
            for (size_t i = 0; i < argumentCount; ++i)
                arguments.uncheckedAppend(toRef(globalObject, callFrame->uncheckedArgument(i)));
            JSValueRef exception = nullptr;
            JSValue result;
            {
                JSLock::DropAllLocks dropAllLocks(globalObject);
                result = toJS(globalObject, callAsFunction(execRef, functionRef, thisObjRef, argumentCount, arguments.data(), &exception));
            }
            if (exception) {
                throwException(globalObject, scope, toJS(globalObject, exception));
                return JSValue::encode(jsUndefined());
            }
            return JSValue::encode(result);
        }
    }
    
    RELEASE_ASSERT_NOT_REACHED(); // getCallData should prevent us from reaching here
    return JSValue::encode(JSValue());
}

template <class Parent>
void JSCallbackObject<Parent>::getOwnNonIndexPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = getVM(globalObject);
    JSCallbackObject* thisObject = jsCast<JSCallbackObject*>(object);
    JSContextRef execRef = toRef(globalObject);
    JSObjectRef thisRef = toRef(jsCast<JSObject*>(thisObject));
    
    for (JSClassRef jsClass = thisObject->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectGetPropertyNamesCallback getPropertyNames = jsClass->getPropertyNames) {
            JSLock::DropAllLocks dropAllLocks(globalObject);
            getPropertyNames(execRef, thisRef, toRef(&propertyNames));
        }
        
        if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(globalObject)) {
            typedef OpaqueJSClassStaticValuesTable::const_iterator iterator;
            iterator end = staticValues->end();
            for (iterator it = staticValues->begin(); it != end; ++it) {
                StringImpl* name = it->key.get();
                StaticValueEntry* entry = it->value.get();
                if (entry->getProperty && (!(entry->attributes & kJSPropertyAttributeDontEnum) || mode.includeDontEnumProperties())) {
                    ASSERT(!name->isSymbol());
                    propertyNames.add(Identifier::fromString(vm, String(name)));
                }
            }
        }
        
        if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(globalObject)) {
            typedef OpaqueJSClassStaticFunctionsTable::const_iterator iterator;
            iterator end = staticFunctions->end();
            for (iterator it = staticFunctions->begin(); it != end; ++it) {
                StringImpl* name = it->key.get();
                StaticFunctionEntry* entry = it->value.get();
                if (!(entry->attributes & kJSPropertyAttributeDontEnum) || mode.includeDontEnumProperties()) {
                    ASSERT(!name->isSymbol());
                    propertyNames.add(Identifier::fromString(vm, String(name)));
                }
            }
        }
    }
    
    Parent::getOwnNonIndexPropertyNames(thisObject, globalObject, propertyNames, mode);
}

template <class Parent>
void JSCallbackObject<Parent>::setPrivate(void* data)
{
    m_callbackObjectData->privateData = data;
}

template <class Parent>
void* JSCallbackObject<Parent>::getPrivate()
{
    return m_callbackObjectData->privateData;
}

template <class Parent>
bool JSCallbackObject<Parent>::inherits(JSClassRef c) const
{
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (jsClass == c)
            return true;
    }
    return false;
}

template <class Parent>
JSValue JSCallbackObject<Parent>::getStaticValue(JSGlobalObject* globalObject, PropertyName propertyName)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObjectRef thisRef = toRef(jsCast<JSObject*>(this));
    
    if (StringImpl* name = propertyName.uid()) {
        for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass) {
            if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(globalObject)) {
                if (StaticValueEntry* entry = staticValues->get(name)) {
                    if (JSObjectGetPropertyCallback getProperty = entry->getProperty) {
                        JSValueRef exception = nullptr;
                        JSValueRef value;
                        {
                            JSLock::DropAllLocks dropAllLocks(globalObject);
                            value = getProperty(toRef(globalObject), thisRef, entry->propertyNameRef.get(), &exception);
                        }
                        if (exception) {
                            throwException(globalObject, scope, toJS(globalObject, exception));
                            return jsUndefined();
                        }
                        if (value)
                            return toJS(globalObject, value);
                    }
                }
            }
        }
    }

    return JSValue();
}

template <class Parent>
EncodedJSValue JSCallbackObject<Parent>::staticFunctionGetter(JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName propertyName)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSCallbackObject* thisObj = asCallbackObject(thisValue);
    
    // Check for cached or override property.
    PropertySlot slot2(thisObj, PropertySlot::InternalMethodType::VMInquiry, &vm);
    bool found = Parent::getOwnPropertySlot(thisObj, globalObject, propertyName, slot2);
    RETURN_IF_EXCEPTION(scope, { });
    slot2.disallowVMEntry.reset();
    if (found)
        return JSValue::encode(slot2.getValue(globalObject, propertyName));

    if (StringImpl* name = propertyName.uid()) {
        for (JSClassRef jsClass = thisObj->classRef(); jsClass; jsClass = jsClass->parentClass) {
            if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(globalObject)) {
                if (StaticFunctionEntry* entry = staticFunctions->get(name)) {
                    if (JSObjectCallAsFunctionCallback callAsFunction = entry->callAsFunction) {
                        JSObject* o = JSCallbackFunction::create(vm, thisObj->globalObject(vm), callAsFunction, name);
                        thisObj->putDirect(vm, propertyName, o, entry->attributes);
                        return JSValue::encode(o);
                    }
                }
            }
        }
    }

    return JSValue::encode(throwException(globalObject, scope, createReferenceError(globalObject, "Static function property defined with NULL callAsFunction callback."_s)));
}

template <class Parent>
EncodedJSValue JSCallbackObject<Parent>::callbackGetter(JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName propertyName)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSCallbackObject* thisObj = asCallbackObject(thisValue);
    
    JSObjectRef thisRef = toRef(jsCast<JSObject*>(thisObj));
    RefPtr<OpaqueJSString> propertyNameRef;
    
    if (StringImpl* name = propertyName.uid()) {
        for (JSClassRef jsClass = thisObj->classRef(); jsClass; jsClass = jsClass->parentClass) {
            if (JSObjectGetPropertyCallback getProperty = jsClass->getProperty) {
                if (!propertyNameRef)
                    propertyNameRef = OpaqueJSString::tryCreate(name);
                JSValueRef exception = nullptr;
                JSValueRef value;
                {
                    JSLock::DropAllLocks dropAllLocks(globalObject);
                    value = getProperty(toRef(globalObject), thisRef, propertyNameRef.get(), &exception);
                }
                if (exception) {
                    throwException(globalObject, scope, toJS(globalObject, exception));
                    return JSValue::encode(jsUndefined());
                }
                if (value)
                    return JSValue::encode(toJS(globalObject, value));
            }
        }
    }

    return JSValue::encode(throwException(globalObject, scope, createReferenceError(globalObject, "hasProperty callback returned true for a property that doesn't exist."_s)));
}

} // namespace JSC
