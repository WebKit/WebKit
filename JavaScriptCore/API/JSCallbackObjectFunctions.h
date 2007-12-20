// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include <wtf/Platform.h>
#include "APICast.h"
#include "JSCallbackFunction.h"
#include "JSClassRef.h"
#include "JSObjectRef.h"
#include "JSGlobalObject.h"
#include "JSStringRef.h"
#include "PropertyNameArray.h"
#include "internal.h"
#include <wtf/Vector.h>

namespace KJS {

template <class Base>
JSCallbackObject<Base>::JSCallbackObject(ExecState* exec, JSClassRef jsClass, JSValue* prototype, void* data)
    : Base(prototype)
    , m_privateData(data)
    , m_class(JSClassRetain(jsClass))
{
    init(exec);
}

// Global object constructor. FIXME: Move this into a JSGlobalCallbackObject subclass.
template <class Base>
JSCallbackObject<Base>::JSCallbackObject(JSClassRef jsClass)
    : m_privateData(0)
    , m_class(JSClassRetain(jsClass))
{
    ASSERT(Base::isGlobalObject());
    init(static_cast<JSGlobalObject*>(this)->globalExec());
}

template <class Base>
void JSCallbackObject<Base>::init(ExecState* exec)
{
    ASSERT(exec);
    
    Vector<JSObjectInitializeCallback, 16> initRoutines;
    JSClassRef jsClass = m_class;
    do {
        if (JSObjectInitializeCallback initialize = jsClass->initialize)
            initRoutines.append(initialize);
    } while ((jsClass = jsClass->parentClass));
    
    // initialize from base to derived
    for (int i = static_cast<int>(initRoutines.size()) - 1; i >= 0; i--) {
        JSLock::DropAllLocks dropAllLocks;
        JSObjectInitializeCallback initialize = initRoutines[i];
        initialize(toRef(exec), toRef(this));
    }
}

template <class Base>
JSCallbackObject<Base>::~JSCallbackObject()
{
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass)
        if (JSObjectFinalizeCallback finalize = jsClass->finalize) {
            finalize(thisRef);
        }
            
    JSClassRelease(m_class);
}

template <class Base>
UString JSCallbackObject<Base>::className() const
{
    if (!m_class->className.isNull())
        return m_class->className;
    
    return Base::className();
}

template <class Base>
bool JSCallbackObject<Base>::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    JSStringRef propertyNameRef = toRef(propertyName.ustring().rep());
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass) {
        // optional optimization to bypass getProperty in cases when we only need to know if the property exists
        if (JSObjectHasPropertyCallback hasProperty = jsClass->hasProperty) {
            JSLock::DropAllLocks dropAllLocks;
            if (hasProperty(ctx, thisRef, propertyNameRef)) {
                slot.setCustom(this, callbackGetter);
                return true;
            }
        } else if (JSObjectGetPropertyCallback getProperty = jsClass->getProperty) {
            JSLock::DropAllLocks dropAllLocks;
            if (JSValueRef value = getProperty(ctx, thisRef, propertyNameRef, toRef(exec->exceptionSlot()))) {
                // cache the value so we don't have to compute it again
                // FIXME: This violates the PropertySlot design a little bit.
                // We should either use this optimization everywhere, or nowhere.
                slot.setCustom(reinterpret_cast<JSObject*>(toJS(value)), cachedValueGetter);
                return true;
            }
        }
        
        if (OpaqueJSClass::StaticValuesTable* staticValues = jsClass->staticValues) {
            if (staticValues->contains(propertyName.ustring().rep())) {
                slot.setCustom(this, staticValueGetter);
                return true;
            }
        }
        
        if (OpaqueJSClass::StaticFunctionsTable* staticFunctions = jsClass->staticFunctions) {
            if (staticFunctions->contains(propertyName.ustring().rep())) {
                slot.setCustom(this, staticFunctionGetter);
                return true;
            }
        }
    }
    
    return Base::getOwnPropertySlot(exec, propertyName, slot);
}

template <class Base>
bool JSCallbackObject<Base>::getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    return getOwnPropertySlot(exec, Identifier::from(propertyName), slot);
}

template <class Base>
void JSCallbackObject<Base>::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    JSStringRef propertyNameRef = toRef(propertyName.ustring().rep());
    JSValueRef valueRef = toRef(value);
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectSetPropertyCallback setProperty = jsClass->setProperty) {
            JSLock::DropAllLocks dropAllLocks;
            if (setProperty(ctx, thisRef, propertyNameRef, valueRef, toRef(exec->exceptionSlot())))
                return;
        }
        
        if (OpaqueJSClass::StaticValuesTable* staticValues = jsClass->staticValues) {
            if (StaticValueEntry* entry = staticValues->get(propertyName.ustring().rep())) {
                if (entry->attributes & kJSPropertyAttributeReadOnly)
                    return;
                if (JSObjectSetPropertyCallback setProperty = entry->setProperty) {
                    JSLock::DropAllLocks dropAllLocks;
                    if (setProperty(ctx, thisRef, propertyNameRef, valueRef, toRef(exec->exceptionSlot())))
                        return;
                } else
                    throwError(exec, ReferenceError, "Attempt to set a property that is not settable.");
            }
        }
        
        if (OpaqueJSClass::StaticFunctionsTable* staticFunctions = jsClass->staticFunctions) {
            if (StaticFunctionEntry* entry = staticFunctions->get(propertyName.ustring().rep())) {
                if (entry->attributes & kJSPropertyAttributeReadOnly)
                    return;
                JSCallbackObject<Base>::putDirect(propertyName, value, attr); // put as override property
                return;
            }
        }
    }
    
    return Base::put(exec, propertyName, value, attr);
}

template <class Base>
void JSCallbackObject<Base>::put(ExecState* exec, unsigned propertyName, JSValue* value, int attr)
{
    return put(exec, Identifier::from(propertyName), value, attr);
}

template <class Base>
bool JSCallbackObject<Base>::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    JSStringRef propertyNameRef = toRef(propertyName.ustring().rep());
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectDeletePropertyCallback deleteProperty = jsClass->deleteProperty) {
            JSLock::DropAllLocks dropAllLocks;
            if (deleteProperty(ctx, thisRef, propertyNameRef, toRef(exec->exceptionSlot())))
                return true;
        }
        
        if (OpaqueJSClass::StaticValuesTable* staticValues = jsClass->staticValues) {
            if (StaticValueEntry* entry = staticValues->get(propertyName.ustring().rep())) {
                if (entry->attributes & kJSPropertyAttributeDontDelete)
                    return false;
                return true;
            }
        }
        
        if (OpaqueJSClass::StaticFunctionsTable* staticFunctions = jsClass->staticFunctions) {
            if (StaticFunctionEntry* entry = staticFunctions->get(propertyName.ustring().rep())) {
                if (entry->attributes & kJSPropertyAttributeDontDelete)
                    return false;
                return true;
            }
        }
    }
    
    return Base::deleteProperty(exec, propertyName);
}

template <class Base>
bool JSCallbackObject<Base>::deleteProperty(ExecState* exec, unsigned propertyName)
{
    return deleteProperty(exec, Identifier::from(propertyName));
}

template <class Base>
bool JSCallbackObject<Base>::implementsConstruct() const
{
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass)
        if (jsClass->callAsConstructor)
            return true;
    
    return false;
}

template <class Base>
JSObject* JSCallbackObject<Base>::construct(ExecState* exec, const List& args)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectCallAsConstructorCallback callAsConstructor = jsClass->callAsConstructor) {
            int argumentCount = static_cast<int>(args.size());
            Vector<JSValueRef, 16> arguments(argumentCount);
            for (int i = 0; i < argumentCount; i++)
                arguments[i] = toRef(args[i]);
            JSLock::DropAllLocks dropAllLocks;
            return toJS(callAsConstructor(execRef, thisRef, argumentCount, arguments.data(), toRef(exec->exceptionSlot())));
        }
    }
    
    ASSERT(0); // implementsConstruct should prevent us from reaching here
    return 0;
}

template <class Base>
bool JSCallbackObject<Base>::implementsHasInstance() const
{
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass)
        if (jsClass->hasInstance)
            return true;
    
    return false;
}

template <class Base>
bool JSCallbackObject<Base>::hasInstance(ExecState *exec, JSValue *value)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass)
        if (JSObjectHasInstanceCallback hasInstance = jsClass->hasInstance) {
            JSLock::DropAllLocks dropAllLocks;
            return hasInstance(execRef, thisRef, toRef(value), toRef(exec->exceptionSlot()));
        }
            
    ASSERT_NOT_REACHED(); // implementsHasInstance should prevent us from reaching here
    return 0;
}


template <class Base>
bool JSCallbackObject<Base>::implementsCall() const
{
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass)
        if (jsClass->callAsFunction)
            return true;
    
    return false;
}

template <class Base>
JSValue* JSCallbackObject<Base>::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    JSObjectRef thisObjRef = toRef(thisObj);
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectCallAsFunctionCallback callAsFunction = jsClass->callAsFunction) {
            int argumentCount = static_cast<int>(args.size());
            Vector<JSValueRef, 16> arguments(argumentCount);
            for (int i = 0; i < argumentCount; i++)
                arguments[i] = toRef(args[i]);
            JSLock::DropAllLocks dropAllLocks;
            return toJS(callAsFunction(execRef, thisRef, thisObjRef, argumentCount, arguments.data(), toRef(exec->exceptionSlot())));
        }
    }
    
    ASSERT_NOT_REACHED(); // implementsCall should prevent us from reaching here
    return 0;
}

template <class Base>
void JSCallbackObject<Base>::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass) {
        if (JSObjectGetPropertyNamesCallback getPropertyNames = jsClass->getPropertyNames) {
            JSLock::DropAllLocks dropAllLocks;
            getPropertyNames(execRef, thisRef, toRef(&propertyNames));
        }
        
        if (OpaqueJSClass::StaticValuesTable* staticValues = jsClass->staticValues) {
            typedef OpaqueJSClass::StaticValuesTable::const_iterator iterator;
            iterator end = staticValues->end();
            for (iterator it = staticValues->begin(); it != end; ++it) {
                UString::Rep* name = it->first.get();
                StaticValueEntry* entry = it->second;
                if (entry->getProperty && !(entry->attributes & kJSPropertyAttributeDontEnum))
                    propertyNames.add(Identifier(name));
            }
        }
        
        if (OpaqueJSClass::StaticFunctionsTable* staticFunctions = jsClass->staticFunctions) {
            typedef OpaqueJSClass::StaticFunctionsTable::const_iterator iterator;
            iterator end = staticFunctions->end();
            for (iterator it = staticFunctions->begin(); it != end; ++it) {
                UString::Rep* name = it->first.get();
                StaticFunctionEntry* entry = it->second;
                if (!(entry->attributes & kJSPropertyAttributeDontEnum))
                    propertyNames.add(Identifier(name));
            }
        }
    }
    
    Base::getPropertyNames(exec, propertyNames);
}

template <class Base>
double JSCallbackObject<Base>::toNumber(ExecState* exec) const
{
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass)
        if (JSObjectConvertToTypeCallback convertToType = jsClass->convertToType) {
            JSLock::DropAllLocks dropAllLocks;
            if (JSValueRef value = convertToType(ctx, thisRef, kJSTypeNumber, toRef(exec->exceptionSlot())))
                return toJS(value)->getNumber();
        }
            
    return Base::toNumber(exec);
}

template <class Base>
UString JSCallbackObject<Base>::toString(ExecState* exec) const
{
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass)
        if (JSObjectConvertToTypeCallback convertToType = jsClass->convertToType) {
            JSLock::DropAllLocks dropAllLocks;
            if (JSValueRef value = convertToType(ctx, thisRef, kJSTypeString, toRef(exec->exceptionSlot())))
                return toJS(value)->getString();
        }
            
    return Base::toString(exec);
}

template <class Base>
void JSCallbackObject<Base>::setPrivate(void* data)
{
    m_privateData = data;
}

template <class Base>
void* JSCallbackObject<Base>::getPrivate()
{
    return m_privateData;
}

template <class Base>
bool JSCallbackObject<Base>::inherits(JSClassRef c) const
{
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parentClass)
        if (jsClass == c)
            return true;
    
    return false;
}

template <class Base>
JSValue* JSCallbackObject<Base>::cachedValueGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot)
{
    JSValue* v = slot.slotBase();
    ASSERT(v);
    return v;
}

template <class Base>
JSValue* JSCallbackObject<Base>::staticValueGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    ASSERT(slot.slotBase()->inherits(&JSCallbackObject::info));
    JSCallbackObject* thisObj = static_cast<JSCallbackObject*>(slot.slotBase());
    
    JSObjectRef thisRef = toRef(thisObj);
    JSStringRef propertyNameRef = toRef(propertyName.ustring().rep());
    
    for (JSClassRef jsClass = thisObj->m_class; jsClass; jsClass = jsClass->parentClass)
        if (OpaqueJSClass::StaticValuesTable* staticValues = jsClass->staticValues)
            if (StaticValueEntry* entry = staticValues->get(propertyName.ustring().rep()))
                if (JSObjectGetPropertyCallback getProperty = entry->getProperty) {
                    JSLock::DropAllLocks dropAllLocks;
                    if (JSValueRef value = getProperty(toRef(exec), thisRef, propertyNameRef, toRef(exec->exceptionSlot())))
                        return toJS(value);
                }
                    
    return throwError(exec, ReferenceError, "Static value property defined with NULL getProperty callback.");
}

template <class Base>
JSValue* JSCallbackObject<Base>::staticFunctionGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    ASSERT(slot.slotBase()->inherits(&JSCallbackObject::info));
    JSCallbackObject* thisObj = static_cast<JSCallbackObject*>(slot.slotBase());
    
    // Check for cached or override property.
    PropertySlot slot2;
    if (thisObj->Base::getOwnPropertySlot(exec, propertyName, slot2))
        return slot2.getValue(exec, thisObj, propertyName);
    
    for (JSClassRef jsClass = thisObj->m_class; jsClass; jsClass = jsClass->parentClass) {
        if (OpaqueJSClass::StaticFunctionsTable* staticFunctions = jsClass->staticFunctions) {
            if (StaticFunctionEntry* entry = staticFunctions->get(propertyName.ustring().rep())) {
                if (JSObjectCallAsFunctionCallback callAsFunction = entry->callAsFunction) {
                    JSObject* o = new JSCallbackFunction(exec, callAsFunction, propertyName);
                    thisObj->putDirect(propertyName, o, entry->attributes);
                    return o;
                }
            }
        }
    }
    
    return throwError(exec, ReferenceError, "Static function property defined with NULL callAsFunction callback.");
}

template <class Base>
JSValue* JSCallbackObject<Base>::callbackGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    ASSERT(slot.slotBase()->inherits(&JSCallbackObject::info));
    JSCallbackObject* thisObj = static_cast<JSCallbackObject*>(slot.slotBase());
    
    JSObjectRef thisRef = toRef(thisObj);
    JSStringRef propertyNameRef = toRef(propertyName.ustring().rep());
    
    for (JSClassRef jsClass = thisObj->m_class; jsClass; jsClass = jsClass->parentClass)
        if (JSObjectGetPropertyCallback getProperty = jsClass->getProperty) {
            JSLock::DropAllLocks dropAllLocks;
            if (JSValueRef value = getProperty(toRef(exec), thisRef, propertyNameRef, toRef(exec->exceptionSlot())))
                return toJS(value);
        }
            
    return throwError(exec, ReferenceError, "hasProperty callback returned true for a property that doesn't exist.");
}

} // namespace KJS
