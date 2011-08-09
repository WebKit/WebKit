/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#include "APIShims.h"
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
#include "JSStringRef.h"
#include "OpaqueJSString.h"
#include "PropertyNameArray.h"
#include <wtf/Vector.h>

namespace JSC {

template <class Parent>
inline JSCallbackObject<Parent>* JSCallbackObject<Parent>::asCallbackObject(JSValue value)
{
    ASSERT(asObject(value)->inherits(&s_info));
    return static_cast<JSCallbackObject*>(asObject(value));
}

template <class Parent>
JSCallbackObject<Parent>::JSCallbackObject(ExecState* exec, JSGlobalObject* globalObject, Structure* structure, JSClassRef jsClass, void* data)
    : Parent(globalObject, structure)
    , m_callbackObjectData(adoptPtr(new JSCallbackObjectData(data, jsClass)))
{
    ASSERT(Parent::inherits(&s_info));
    init(exec);
}

// Global object constructor.
// FIXME: Move this into a separate JSGlobalCallbackObject class derived from this one.
template <class Parent>
JSCallbackObject<Parent>::JSCallbackObject(JSGlobalData& globalData, JSClassRef jsClass, Structure* structure)
    : Parent(globalData, structure)
    , m_callbackObjectData(adoptPtr(new JSCallbackObjectData(0, jsClass)))
{
    ASSERT(Parent::inherits(&s_info));
    ASSERT(Parent::isGlobalObject());
    init(static_cast<JSGlobalObject*>(this)->globalExec());
}

template <class Parent>
void JSCallbackObject<Parent>::init(ExecState* exec)
{
    ASSERT(exec);
    
    Vector<JSObjectInitializeCallback, 16> initRoutines;
    JSClassRef jsClass = classRef();
    do {
        if (JSObjectInitializeCallback initialize = jsClass->initialize)
            initRoutines.append(initialize);
    } while ((jsClass = jsClass->parentClass));
    
    // initialize from base to derived
    for (int i = static_cast<int>(initRoutines.size()) - 1; i >= 0; i--) {
        APICallbackShim callbackShim(exec);
        JSObjectInitializeCallback initialize = initRoutines[i];
        initialize(toRef(exec), toRef(this));
    }

    bool needsFinalizer = false;
    for (JSClassRef jsClassPtr = classRef(); jsClassPtr && !needsFinalizer; jsClassPtr = jsClassPtr->parentClass)
        needsFinalizer = jsClassPtr->finalize;
    if (needsFinalizer) {
        HandleSlot slot = exec->globalData().allocateGlobalHandle();
        HandleHeap::heapFor(slot)->makeWeak(slot, m_callbackObjectData.get(), classRef());
        HandleHeap::heapFor(slot)->writeBarrier(slot, this);
        *slot = this;
    }
}

template <class Parent>
UString JSCallbackObject<Parent>::className() const
{
    UString thisClassName = classRef()->className();
    if (!thisClassName.isEmpty())
        return thisClassName;
    
    return Parent::className();
}

template <class Parent>
bool JSCallbackObject<Parent>::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    RefPtr<OpaqueJSString> propertyNameRef;
    
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass) {
        // optional optimization to bypass getProperty in cases when we only need to know if the property exists
        if (JSObjectHasPropertyCallback hasProperty = jsClass->hasProperty) {
            if (!propertyNameRef)
                propertyNameRef = OpaqueJSString::create(propertyName.ustring());
            APICallbackShim callbackShim(exec);
            if (hasProperty(ctx, thisRef, propertyNameRef.get())) {
                slot.setCustom(this, callbackGetter);
                return true;
            }
        } else if (JSObjectGetPropertyCallback getProperty = jsClass->getProperty) {
            if (!propertyNameRef)
                propertyNameRef = OpaqueJSString::create(propertyName.ustring());
            JSValueRef exception = 0;
            JSValueRef value;
            {
                APICallbackShim callbackShim(exec);
                value = getProperty(ctx, thisRef, propertyNameRef.get(), &exception);
            }
            if (exception) {
                throwError(exec, toJS(exec, exception));
                slot.setValue(jsUndefined());
                return true;
            }
            if (value) {
                slot.setValue(toJS(exec, value));
                return true;
            }
        }
        
        if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(exec)) {
            if (staticValues->contains(propertyName.impl())) {
                JSValue value = getStaticValue(exec, propertyName);
                if (value) {
                    slot.setValue(value);
                    return true;
                }
            }
        }
        
        if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(exec)) {
            if (staticFunctions->contains(propertyName.impl())) {
                slot.setCustom(this, staticFunctionGetter);
                return true;
            }
        }
    }
    
    return Parent::getOwnPropertySlot(exec, propertyName, slot);
}

template <class Parent>
bool JSCallbackObject<Parent>::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    PropertySlot slot;
    if (getOwnPropertySlot(exec, propertyName, slot)) {
        // Ideally we should return an access descriptor, but returning a value descriptor is better than nothing.
        JSValue value = slot.getValue(exec, propertyName);
        if (!exec->hadException())
            descriptor.setValue(value);
        // We don't know whether the property is configurable, but assume it is.
        descriptor.setConfigurable(true);
        // We don't know whether the property is enumerable (we could call getOwnPropertyNames() to find out), but assume it isn't.
        descriptor.setEnumerable(false);
        return true;
    }

    return Parent::getOwnPropertyDescriptor(exec, propertyName, descriptor);
}

template <class Parent>
void JSCallbackObject<Parent>::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    RefPtr<OpaqueJSString> propertyNameRef;
    JSValueRef valueRef = toRef(exec, value);
    
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectSetPropertyCallback setProperty = jsClass->setProperty) {
            if (!propertyNameRef)
                propertyNameRef = OpaqueJSString::create(propertyName.ustring());
            JSValueRef exception = 0;
            bool result;
            {
                APICallbackShim callbackShim(exec);
                result = setProperty(ctx, thisRef, propertyNameRef.get(), valueRef, &exception);
            }
            if (exception)
                throwError(exec, toJS(exec, exception));
            if (result || exception)
                return;
        }
        
        if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(exec)) {
            if (StaticValueEntry* entry = staticValues->get(propertyName.impl())) {
                if (entry->attributes & kJSPropertyAttributeReadOnly)
                    return;
                if (JSObjectSetPropertyCallback setProperty = entry->setProperty) {
                    if (!propertyNameRef)
                        propertyNameRef = OpaqueJSString::create(propertyName.ustring());
                    JSValueRef exception = 0;
                    bool result;
                    {
                        APICallbackShim callbackShim(exec);
                        result = setProperty(ctx, thisRef, propertyNameRef.get(), valueRef, &exception);
                    }
                    if (exception)
                        throwError(exec, toJS(exec, exception));
                    if (result || exception)
                        return;
                }
            }
        }
        
        if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(exec)) {
            if (StaticFunctionEntry* entry = staticFunctions->get(propertyName.impl())) {
                if (entry->attributes & kJSPropertyAttributeReadOnly)
                    return;
                JSCallbackObject<Parent>::putDirect(exec->globalData(), propertyName, value); // put as override property
                return;
            }
        }
    }
    
    return Parent::put(exec, propertyName, value, slot);
}

template <class Parent>
bool JSCallbackObject<Parent>::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    RefPtr<OpaqueJSString> propertyNameRef;
    
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectDeletePropertyCallback deleteProperty = jsClass->deleteProperty) {
            if (!propertyNameRef)
                propertyNameRef = OpaqueJSString::create(propertyName.ustring());
            JSValueRef exception = 0;
            bool result;
            {
                APICallbackShim callbackShim(exec);
                result = deleteProperty(ctx, thisRef, propertyNameRef.get(), &exception);
            }
            if (exception)
                throwError(exec, toJS(exec, exception));
            if (result || exception)
                return true;
        }
        
        if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(exec)) {
            if (StaticValueEntry* entry = staticValues->get(propertyName.impl())) {
                if (entry->attributes & kJSPropertyAttributeDontDelete)
                    return false;
                return true;
            }
        }
        
        if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(exec)) {
            if (StaticFunctionEntry* entry = staticFunctions->get(propertyName.impl())) {
                if (entry->attributes & kJSPropertyAttributeDontDelete)
                    return false;
                return true;
            }
        }
    }
    
    return Parent::deleteProperty(exec, propertyName);
}

template <class Parent>
bool JSCallbackObject<Parent>::deleteProperty(ExecState* exec, unsigned propertyName)
{
    return deleteProperty(exec, Identifier::from(exec, propertyName));
}

template <class Parent>
ConstructType JSCallbackObject<Parent>::getConstructData(ConstructData& constructData)
{
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (jsClass->callAsConstructor) {
            constructData.native.function = construct;
            return ConstructTypeHost;
        }
    }
    return ConstructTypeNone;
}

template <class Parent>
EncodedJSValue JSCallbackObject<Parent>::construct(ExecState* exec)
{
    JSObject* constructor = exec->callee();
    JSContextRef execRef = toRef(exec);
    JSObjectRef constructorRef = toRef(constructor);
    
    for (JSClassRef jsClass = static_cast<JSCallbackObject<Parent>*>(constructor)->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectCallAsConstructorCallback callAsConstructor = jsClass->callAsConstructor) {
            int argumentCount = static_cast<int>(exec->argumentCount());
            Vector<JSValueRef, 16> arguments(argumentCount);
            for (int i = 0; i < argumentCount; i++)
                arguments[i] = toRef(exec, exec->argument(i));
            JSValueRef exception = 0;
            JSObject* result;
            {
                APICallbackShim callbackShim(exec);
                result = toJS(callAsConstructor(execRef, constructorRef, argumentCount, arguments.data(), &exception));
            }
            if (exception)
                throwError(exec, toJS(exec, exception));
            return JSValue::encode(result);
        }
    }
    
    ASSERT_NOT_REACHED(); // getConstructData should prevent us from reaching here
    return JSValue::encode(JSValue());
}

template <class Parent>
bool JSCallbackObject<Parent>::hasInstance(ExecState* exec, JSValue value, JSValue)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectHasInstanceCallback hasInstance = jsClass->hasInstance) {
            JSValueRef valueRef = toRef(exec, value);
            JSValueRef exception = 0;
            bool result;
            {
                APICallbackShim callbackShim(exec);
                result = hasInstance(execRef, thisRef, valueRef, &exception);
            }
            if (exception)
                throwError(exec, toJS(exec, exception));
            return result;
        }
    }
    return false;
}

template <class Parent>
CallType JSCallbackObject<Parent>::getCallData(CallData& callData)
{
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (jsClass->callAsFunction) {
            callData.native.function = call;
            return CallTypeHost;
        }
    }
    return CallTypeNone;
}

template <class Parent>
EncodedJSValue JSCallbackObject<Parent>::call(ExecState* exec)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef functionRef = toRef(exec->callee());
    JSObjectRef thisObjRef = toRef(exec->hostThisValue().toThisObject(exec));
    
    for (JSClassRef jsClass = static_cast<JSCallbackObject<Parent>*>(toJS(functionRef))->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectCallAsFunctionCallback callAsFunction = jsClass->callAsFunction) {
            int argumentCount = static_cast<int>(exec->argumentCount());
            Vector<JSValueRef, 16> arguments(argumentCount);
            for (int i = 0; i < argumentCount; i++)
                arguments[i] = toRef(exec, exec->argument(i));
            JSValueRef exception = 0;
            JSValue result;
            {
                APICallbackShim callbackShim(exec);
                result = toJS(exec, callAsFunction(execRef, functionRef, thisObjRef, argumentCount, arguments.data(), &exception));
            }
            if (exception)
                throwError(exec, toJS(exec, exception));
            return JSValue::encode(result);
        }
    }
    
    ASSERT_NOT_REACHED(); // getCallData should prevent us from reaching here
    return JSValue::encode(JSValue());
}

template <class Parent>
void JSCallbackObject<Parent>::getOwnPropertyNames(ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectGetPropertyNamesCallback getPropertyNames = jsClass->getPropertyNames) {
            APICallbackShim callbackShim(exec);
            getPropertyNames(execRef, thisRef, toRef(&propertyNames));
        }
        
        if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(exec)) {
            typedef OpaqueJSClassStaticValuesTable::const_iterator iterator;
            iterator end = staticValues->end();
            for (iterator it = staticValues->begin(); it != end; ++it) {
                StringImpl* name = it->first.get();
                StaticValueEntry* entry = it->second;
                if (entry->getProperty && (!(entry->attributes & kJSPropertyAttributeDontEnum) || (mode == IncludeDontEnumProperties)))
                    propertyNames.add(Identifier(exec, name));
            }
        }
        
        if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(exec)) {
            typedef OpaqueJSClassStaticFunctionsTable::const_iterator iterator;
            iterator end = staticFunctions->end();
            for (iterator it = staticFunctions->begin(); it != end; ++it) {
                StringImpl* name = it->first.get();
                StaticFunctionEntry* entry = it->second;
                if (!(entry->attributes & kJSPropertyAttributeDontEnum) || (mode == IncludeDontEnumProperties))
                    propertyNames.add(Identifier(exec, name));
            }
        }
    }
    
    Parent::getOwnPropertyNames(exec, propertyNames, mode);
}

template <class Parent>
double JSCallbackObject<Parent>::toNumber(ExecState* exec) const
{
    // We need this check to guard against the case where this object is rhs of
    // a binary expression where lhs threw an exception in its conversion to
    // primitive
    if (exec->hadException())
        return std::numeric_limits<double>::quiet_NaN();
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass)
        if (JSObjectConvertToTypeCallback convertToType = jsClass->convertToType) {
            JSValueRef exception = 0;
            JSValueRef value;
            {
                APICallbackShim callbackShim(exec);
                value = convertToType(ctx, thisRef, kJSTypeNumber, &exception);
            }
            if (exception) {
                throwError(exec, toJS(exec, exception));
                return 0;
            }

            double dValue;
            if (value)
                return toJS(exec, value).getNumber(dValue) ? dValue : std::numeric_limits<double>::quiet_NaN();
        }
            
    return Parent::toNumber(exec);
}

template <class Parent>
UString JSCallbackObject<Parent>::toString(ExecState* exec) const
{
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass)
        if (JSObjectConvertToTypeCallback convertToType = jsClass->convertToType) {
            JSValueRef exception = 0;
            JSValueRef value;
            {
                APICallbackShim callbackShim(exec);
                value = convertToType(ctx, thisRef, kJSTypeString, &exception);
            }
            if (exception) {
                throwError(exec, toJS(exec, exception));
                return "";
            }
            if (value)
                return toJS(exec, value).getString(exec);
        }
            
    return Parent::toString(exec);
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
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass)
        if (jsClass == c)
            return true;
    
    return false;
}

template <class Parent>
JSValue JSCallbackObject<Parent>::getStaticValue(ExecState* exec, const Identifier& propertyName)
{
    JSObjectRef thisRef = toRef(this);
    RefPtr<OpaqueJSString> propertyNameRef;
    
    for (JSClassRef jsClass = classRef(); jsClass; jsClass = jsClass->parentClass)
        if (OpaqueJSClassStaticValuesTable* staticValues = jsClass->staticValues(exec))
            if (StaticValueEntry* entry = staticValues->get(propertyName.impl()))
                if (JSObjectGetPropertyCallback getProperty = entry->getProperty) {
                    if (!propertyNameRef)
                        propertyNameRef = OpaqueJSString::create(propertyName.ustring());
                    JSValueRef exception = 0;
                    JSValueRef value;
                    {
                        APICallbackShim callbackShim(exec);
                        value = getProperty(toRef(exec), thisRef, propertyNameRef.get(), &exception);
                    }
                    if (exception) {
                        throwError(exec, toJS(exec, exception));
                        return jsUndefined();
                    }
                    if (value)
                        return toJS(exec, value);
                }

    return JSValue();
}

template <class Parent>
JSValue JSCallbackObject<Parent>::staticFunctionGetter(ExecState* exec, JSValue slotParent, const Identifier& propertyName)
{
    JSCallbackObject* thisObj = asCallbackObject(slotParent);
    
    // Check for cached or override property.
    PropertySlot slot2(thisObj);
    if (thisObj->Parent::getOwnPropertySlot(exec, propertyName, slot2))
        return slot2.getValue(exec, propertyName);
    
    for (JSClassRef jsClass = thisObj->classRef(); jsClass; jsClass = jsClass->parentClass) {
        if (OpaqueJSClassStaticFunctionsTable* staticFunctions = jsClass->staticFunctions(exec)) {
            if (StaticFunctionEntry* entry = staticFunctions->get(propertyName.impl())) {
                if (JSObjectCallAsFunctionCallback callAsFunction = entry->callAsFunction) {
                    
                    JSObject* o = JSCallbackFunction::create(exec, asGlobalObject(thisObj->getAnonymousValue(0)), callAsFunction, propertyName);
                    thisObj->putDirect(exec->globalData(), propertyName, o, entry->attributes);
                    return o;
                }
            }
        }
    }
    
    return throwError(exec, createReferenceError(exec, "Static function property defined with NULL callAsFunction callback."));
}

template <class Parent>
JSValue JSCallbackObject<Parent>::callbackGetter(ExecState* exec, JSValue slotParent, const Identifier& propertyName)
{
    JSCallbackObject* thisObj = asCallbackObject(slotParent);
    
    JSObjectRef thisRef = toRef(thisObj);
    RefPtr<OpaqueJSString> propertyNameRef;
    
    for (JSClassRef jsClass = thisObj->classRef(); jsClass; jsClass = jsClass->parentClass)
        if (JSObjectGetPropertyCallback getProperty = jsClass->getProperty) {
            if (!propertyNameRef)
                propertyNameRef = OpaqueJSString::create(propertyName.ustring());
            JSValueRef exception = 0;
            JSValueRef value;
            {
                APICallbackShim callbackShim(exec);
                value = getProperty(toRef(exec), thisRef, propertyNameRef.get(), &exception);
            }
            if (exception) {
                throwError(exec, toJS(exec, exception));
                return jsUndefined();
            }
            if (value)
                return toJS(exec, value);
        }
            
    return throwError(exec, createReferenceError(exec, "hasProperty callback returned true for a property that doesn't exist."));
}

} // namespace JSC
