// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "APICast.h"
#include "JSCallbackObject.h"
#include "JSInternalStringRef.h"
#include "JSClassRef.h"
#include "JSObjectRef.h"
#include "internal.h"
#include "reference.h"
#include "reference_list.h"

namespace KJS {

const ClassInfo JSCallbackObject::info = { "CallbackObject", 0, 0, 0 };

JSCallbackObject::JSCallbackObject(JSContextRef context, JSClassRef jsClass)
    : JSObject()
{
    init(context, jsClass);
}

JSCallbackObject::JSCallbackObject(JSContextRef context, JSClassRef jsClass, JSValue* prototype)
    : JSObject(prototype)
{
    init(context, jsClass);
}

void JSCallbackObject::init(JSContextRef context, JSClassRef jsClass)
{
    ExecState* exec = toJS(context);
    
    m_privateData = 0;
    m_class = JSClassRetain(jsClass);

    JSObjectRef thisRef = toRef(this);
    
    do {
        if (JSInitializeCallback initialize = jsClass->callbacks.initialize)
            initialize(context, thisRef, toRef(exec->exceptionSlot()));
    } while ((jsClass = jsClass->parent));
}

JSCallbackObject::~JSCallbackObject()
{
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent)
        if (JSFinalizeCallback finalize = jsClass->callbacks.finalize)
            finalize(thisRef);
    
    JSClassRelease(m_class);
}

UString JSCallbackObject::className() const
{
    return classInfo()->className;
}

bool JSCallbackObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSContextRef context = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    JSInternalStringRef propertyNameRef = toRef(propertyName.ustring().rep());

    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent) {
        // optional optimization to bypass getProperty in cases when we only need to know if the property exists
        if (JSHasPropertyCallback hasPropertyCallback = jsClass->callbacks.hasProperty) {
            if (hasPropertyCallback(context, thisRef, propertyNameRef, toRef(exec->exceptionSlot()))) {
                slot.setCustom(this, callbackGetter);
                return true;
            }
        } else if (JSGetPropertyCallback getPropertyCallback = jsClass->callbacks.getProperty) {
            JSValueRef returnValue;
            if (getPropertyCallback(context, thisRef, propertyNameRef, &returnValue, toRef(exec->exceptionSlot()))) {
                // cache the value so we don't have to compute it again
                // FIXME: This violates the PropertySlot design a little bit.
                // We should either use this optimization everywhere, or nowhere.
                slot.setCustom(reinterpret_cast<JSObject*>(toJS(returnValue)), cachedValueGetter);
                return true;
            }
        }

        if (__JSClass::StaticValuesTable* staticValues = jsClass->staticValues) {
            if (StaticValueEntry* entry = staticValues->get(propertyName.ustring().rep())) {
                if (entry->getProperty) {
                    slot.setCustom(this, staticValueGetter);
                    return true;
                }
            }
        }
        
        if (__JSClass::StaticFunctionsTable* staticFunctions = jsClass->staticFunctions) {
            if (staticFunctions->contains(propertyName.ustring().rep())) {
                slot.setCustom(this, staticFunctionGetter);
                return true;
            }
        }
    }

    return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

bool JSCallbackObject::getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    return getOwnPropertySlot(exec, Identifier::from(propertyName), slot);
}

void JSCallbackObject::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    JSContextRef context = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    JSInternalStringRef propertyNameRef = toRef(propertyName.ustring().rep());
    JSValueRef valueRef = toRef(value);

    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent) {
        if (JSSetPropertyCallback setPropertyCallback = jsClass->callbacks.setProperty) {
            if (setPropertyCallback(context, thisRef, propertyNameRef, valueRef, toRef(exec->exceptionSlot())))
                return;
        }
    
        if (__JSClass::StaticValuesTable* staticValues = jsClass->staticValues) {
            if (StaticValueEntry* entry = staticValues->get(propertyName.ustring().rep())) {
                if (entry->attributes & kJSPropertyAttributeReadOnly)
                    return;
                if (JSSetPropertyCallback setPropertyCallback = entry->setProperty) {
                    if (setPropertyCallback(context, thisRef, propertyNameRef, valueRef, toRef(exec->exceptionSlot())))
                        return;
                }
            }
        }
        
        if (__JSClass::StaticFunctionsTable* staticFunctions = jsClass->staticFunctions) {
            if (StaticFunctionEntry* entry = staticFunctions->get(propertyName.ustring().rep())) {
                if (entry->attributes & kJSPropertyAttributeReadOnly)
                    return;
                putDirect(propertyName, value, attr); // put as override property
                return;
            }
        }
    }
    return JSObject::put(exec, propertyName, value, attr);
}

void JSCallbackObject::put(ExecState* exec, unsigned propertyName, JSValue* value, int attr)
{
    return put(exec, Identifier::from(propertyName), value, attr);
}

bool JSCallbackObject::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    JSContextRef context = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    JSInternalStringRef propertyNameRef = toRef(propertyName.ustring().rep());
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent) {
        if (JSDeletePropertyCallback deletePropertyCallback = jsClass->callbacks.deleteProperty) {
            if (deletePropertyCallback(context, thisRef, propertyNameRef, toRef(exec->exceptionSlot())))
                return true;
        }

        if (__JSClass::StaticValuesTable* staticValues = jsClass->staticValues) {
            if (StaticValueEntry* entry = staticValues->get(propertyName.ustring().rep())) {
                if (entry->attributes & kJSPropertyAttributeDontDelete)
                    return false;
                return true;
            }
        }
        
        if (__JSClass::StaticFunctionsTable* staticFunctions = jsClass->staticFunctions) {
            if (StaticFunctionEntry* entry = staticFunctions->get(propertyName.ustring().rep())) {
                if (entry->attributes & kJSPropertyAttributeDontDelete)
                    return false;
                return true;
            }
        }
    }
    return JSObject::deleteProperty(exec, propertyName);
}

bool JSCallbackObject::deleteProperty(ExecState* exec, unsigned propertyName)
{
    return deleteProperty(exec, Identifier::from(propertyName));
}

bool JSCallbackObject::implementsConstruct() const
{
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent)
        if (jsClass->callbacks.callAsConstructor)
            return true;

    return false;
}

JSObject* JSCallbackObject::construct(ExecState* exec, const List& args)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent) {
        if (JSCallAsConstructorCallback callAsConstructorCallback = jsClass->callbacks.callAsConstructor) {
            size_t argc = args.size();
            JSValueRef argv[argc];
            for (size_t i = 0; i < argc; i++)
                argv[i] = toRef(args[i]);
            return toJS(callAsConstructorCallback(execRef, thisRef, argc, argv, toRef(exec->exceptionSlot())));
        }
    }
    
    ASSERT(0); // implementsConstruct should prevent us from reaching here
    return 0;
}

bool JSCallbackObject::implementsCall() const
{
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent)
        if (jsClass->callbacks.callAsFunction)
            return true;
    
    return false;
}

JSValue* JSCallbackObject::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    JSObjectRef thisObjRef = toRef(thisObj);

    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent) {
        if (JSCallAsFunctionCallback callAsFunctionCallback = jsClass->callbacks.callAsFunction) {
            size_t argc = args.size();
            JSValueRef argv[argc];
            for (size_t i = 0; i < argc; i++)
                argv[i] = toRef(args[i]);
            return toJS(callAsFunctionCallback(execRef, thisRef, thisObjRef, argc, argv, toRef(exec->exceptionSlot())));
        }
    }

    ASSERT(0); // implementsCall should prevent us from reaching here
    return 0;
}

void JSCallbackObject::getPropertyList(ExecState* exec, ReferenceList& propertyList, bool recursive)
{
    JSContextRef context = toRef(exec);
    JSObjectRef thisRef = toRef(this);

    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent) {
        if (JSGetPropertyListCallback getPropertyListCallback = jsClass->callbacks.getPropertyList)
            getPropertyListCallback(context, thisRef, toRef(&propertyList), toRef(exec->exceptionSlot()));

        if (__JSClass::StaticValuesTable* staticValues = jsClass->staticValues) {
            typedef __JSClass::StaticValuesTable::const_iterator iterator;
            iterator end = staticValues->end();
            for (iterator it = staticValues->begin(); it != end; ++it) {
                UString::Rep* name = it->first.get();
                StaticValueEntry* entry = it->second;
                if (entry->getProperty && !(entry->attributes & kJSPropertyAttributeDontEnum))
                    propertyList.append(Reference(this, Identifier(name)));
            }
        }

        if (__JSClass::StaticFunctionsTable* staticFunctions = jsClass->staticFunctions) {
            typedef __JSClass::StaticFunctionsTable::const_iterator iterator;
            iterator end = staticFunctions->end();
            for (iterator it = staticFunctions->begin(); it != end; ++it) {
                UString::Rep* name = it->first.get();
                StaticFunctionEntry* entry = it->second;
                if (!(entry->attributes & kJSPropertyAttributeDontEnum))
                    propertyList.append(Reference(this, Identifier(name)));
            }
        }
    }

    JSObject::getPropertyList(exec, propertyList, recursive);
}

bool JSCallbackObject::toBoolean(ExecState* exec) const
{
    JSContextRef context = toRef(exec);
    JSObjectRef thisRef = toRef(this);

    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent) {
        if (JSConvertToTypeCallback convertToTypeCallback = jsClass->callbacks.convertToType) {
            JSValueRef returnValue;
            if (convertToTypeCallback(context, thisRef, kJSTypeBoolean, &returnValue, toRef(exec->exceptionSlot())))
                return toJS(returnValue)->getBoolean();
        }
    }
    return JSObject::toBoolean(exec);
}

double JSCallbackObject::toNumber(ExecState* exec) const
{
    JSContextRef context = toRef(exec);
    JSObjectRef thisRef = toRef(this);

    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent) {
        if (JSConvertToTypeCallback convertToTypeCallback = jsClass->callbacks.convertToType) {
            JSValueRef returnValue;
            if (convertToTypeCallback(context, thisRef, kJSTypeNumber, &returnValue, toRef(exec->exceptionSlot())))
                return toJS(returnValue)->getNumber();
        }
    }
    return JSObject::toNumber(exec);
}

UString JSCallbackObject::toString(ExecState* exec) const
{
    JSContextRef context = toRef(exec);
    JSObjectRef thisRef = toRef(this);

    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent) {
        if (JSConvertToTypeCallback convertToTypeCallback = jsClass->callbacks.convertToType) {
            JSValueRef returnValue;
            if (convertToTypeCallback(context, thisRef, kJSTypeString, &returnValue, toRef(exec->exceptionSlot())))
                return toJS(returnValue)->getString();
        }
    }
    return JSObject::toString(exec);
}

void JSCallbackObject::setPrivate(void* data)
{
    m_privateData = data;
}

void* JSCallbackObject::getPrivate()
{
    return m_privateData;
}

bool JSCallbackObject::inherits(JSClassRef c) const
{
    for (JSClassRef jsClass = m_class; jsClass; jsClass = jsClass->parent)
        if (jsClass == c)
            return true;
    return false;
}

JSValue* JSCallbackObject::cachedValueGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot)
{
    JSValue* v = slot.slotBase();
    ASSERT(v);
    return v;
}

JSValue* JSCallbackObject::staticValueGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    ASSERT(slot.slotBase()->inherits(&JSCallbackObject::info));
    JSCallbackObject* thisObj = static_cast<JSCallbackObject*>(slot.slotBase());

    JSObjectRef thisRef = toRef(thisObj);
    JSInternalStringRef propertyNameRef = toRef(propertyName.ustring().rep());

    for (JSClassRef jsClass = thisObj->m_class; jsClass; jsClass = jsClass->parent) {
        JSValueRef returnValue;
        
        if (__JSClass::StaticValuesTable* staticValues = jsClass->staticValues)
            if (StaticValueEntry* entry = staticValues->get(propertyName.ustring().rep()))
                if (JSGetPropertyCallback getPropertyCallback = entry->getProperty)
                    if (getPropertyCallback(toRef(exec), thisRef, propertyNameRef, &returnValue, toRef(exec->exceptionSlot())))
                        return toJS(returnValue);
    }

    return jsUndefined();
}

JSValue* JSCallbackObject::staticFunctionGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    ASSERT(slot.slotBase()->inherits(&JSCallbackObject::info));
    JSCallbackObject* thisObj = static_cast<JSCallbackObject*>(slot.slotBase());

    if (JSValue* cachedOrOverrideValue = thisObj->getDirect(propertyName))
        return cachedOrOverrideValue;

    for (JSClassRef jsClass = thisObj->m_class; jsClass; jsClass = jsClass->parent) {
        if (__JSClass::StaticFunctionsTable* staticFunctions = jsClass->staticFunctions) {
            if (StaticFunctionEntry* entry = staticFunctions->get(propertyName.ustring().rep())) {
                JSValue* v = toJS(JSFunctionMake(toRef(exec), entry->callAsFunction));
                thisObj->putDirect(propertyName, v, entry->attributes);
                return v;
            }
        }
    }

    return jsUndefined();
}

JSValue* JSCallbackObject::callbackGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    ASSERT(slot.slotBase()->inherits(&JSCallbackObject::info));
    JSCallbackObject* thisObj = static_cast<JSCallbackObject*>(slot.slotBase());

    JSObjectRef thisRef = toRef(thisObj);
    JSInternalStringRef propertyNameRef = toRef(propertyName.ustring().rep());

    for (JSClassRef jsClass = thisObj->m_class; jsClass; jsClass = jsClass->parent) {
        JSValueRef returnValue;

        if (JSGetPropertyCallback getPropertyCallback = jsClass->callbacks.getProperty)
            if (getPropertyCallback(toRef(exec), thisRef, propertyNameRef, &returnValue, toRef(exec->exceptionSlot())))
                return toJS(returnValue);
    }

    return jsUndefined();
}

} // namespace KJS
