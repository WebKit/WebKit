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
#include "JSCharBufferRef.h"
#include "JSObjectRef.h"
#include "internal.h"
#include "reference_list.h"

namespace KJS {

const ClassInfo JSCallbackObject::info = { "JSCallbackObject", 0, 0, 0 };

JSCallbackObject::JSCallbackObject(const JSObjectCallbacks* callbacks)
    : JSObject()
    , m_privateData(0)
    , m_callbacks(*callbacks)
{
    JSObjectRef thisRef = toRef(this);
    
    do {
        if (JSInitializeCallback initialize = callbacks->initialize)
            initialize(thisRef);
    } while ((callbacks = callbacks->parentCallbacks));
}

JSCallbackObject::JSCallbackObject(const JSObjectCallbacks* callbacks, JSObject* prototype)
    : JSObject(prototype)
    , m_privateData(0)
    , m_callbacks(*callbacks)
{
    JSObjectRef thisRef = toRef(this);
    
    do {
        if (JSInitializeCallback initialize = callbacks->initialize)
            initialize(thisRef);
    } while ((callbacks = callbacks->parentCallbacks));
}

JSCallbackObject::~JSCallbackObject()
{
    JSObjectRef thisRef = toRef(this);
    
    for (JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks)
        if (JSFinalizeCallback finalize = callbacks->finalize)
            finalize(thisRef);
}

UString JSCallbackObject::className() const
{
    JSObjectRef thisRef = toRef(this);
    
    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSCopyDescriptionCallback copyDescriptionCallback = callbacks->copyDescription) {
            JSCharBufferRef descriptionBuf = copyDescriptionCallback(thisRef);
            UString description(toJS(descriptionBuf));
            JSCharBufferRelease(descriptionBuf);
            return description;
        }
    }
    
    return JSObject::className();
}

bool JSCallbackObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSObjectRef thisRef = toRef(this);
    JSCharBufferRef propertyNameRef = toRef(propertyName.ustring().rep());

    // optional optimization for cases when we only need to know if the property exists, not its value
    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSHasPropertyCallback hasPropertyCallback = callbacks->hasProperty) {
            if (hasPropertyCallback(thisRef, propertyNameRef)) {
                slot.setCustom(0, callbackGetter);
                return true;
            }
        }
    }
    
    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSGetPropertyCallback getPropertyCallback = callbacks->getProperty) {
            JSValueRef returnValue;
            if (getPropertyCallback(thisRef, propertyNameRef, &returnValue)) {
                slot.setCustom(reinterpret_cast<JSObject*>(returnValue), cachedValueGetter); // cache the value so we don't have to compute it again
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
    JSObjectRef thisRef = toRef(this);
    JSCharBufferRef propertyNameRef = toRef(propertyName.ustring().rep());

    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSSetPropertyCallback setPropertyCallback = callbacks->setProperty) {
            if (setPropertyCallback(thisRef, propertyNameRef, value))
                return;
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
    JSObjectRef thisRef = toRef(this);
    JSCharBufferRef propertyNameRef = toRef(propertyName.ustring().rep());
    
    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSDeletePropertyCallback deletePropertyCallback = callbacks->deleteProperty) {
            if (deletePropertyCallback(thisRef, propertyNameRef))
                return true;
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
    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks)
        if (callbacks->callAsConstructor)
            return true;

    return false;
}

JSObject* JSCallbackObject::construct(ExecState* exec, const List& args)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    
    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSCallAsConstructorCallback callAsConstructorCallback = callbacks->callAsConstructor) {
            size_t argc = args.size();
            JSValueRef argv[argc];
            for (size_t i = 0; i < argc; i++)
                argv[i] = args[i];
            return toJS(callAsConstructorCallback(execRef, thisRef, argc, argv));
        }
    }
    
    ASSERT(false);
    return 0;
}

bool JSCallbackObject::implementsCall() const
{
    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks)
        if (callbacks->callAsFunction)
            return true;
    
    return false;
}

JSValue* JSCallbackObject::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    JSObjectRef thisObjRef = toRef(thisObj);

    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSCallAsFunctionCallback callAsFunctionCallback = callbacks->callAsFunction) {
            size_t argc = args.size();
            JSValueRef argv[argc];
            for (size_t i = 0; i < argc; i++)
                argv[i] = args[i];
            return toJS(callAsFunctionCallback(execRef, thisRef, thisObjRef, argc, argv));
        }
    }

    ASSERT(false);
    return 0;
}

void JSCallbackObject::getPropertyList(ExecState* exec, ReferenceList& propertyList, bool recursive)
{
    JSObjectRef thisRef = toRef(this);

    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks)
        if (JSGetPropertyListCallback getPropertyListCallback = callbacks->getPropertyList)
            getPropertyListCallback(thisRef, toRef(&propertyList));

    JSObject::getPropertyList(exec, propertyList, recursive);
}

bool JSCallbackObject::toBoolean(ExecState* exec) const
{
    JSObjectRef thisRef = toRef(this);

    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSConvertToTypeCallback convertToTypeCallback = callbacks->convertToType) {
            JSValueRef returnValue;
            if (convertToTypeCallback(thisRef, kJSTypeBoolean, &returnValue))
                return toJS(returnValue)->getBoolean();
        }
    }
    return JSObject::toBoolean(exec);
}

double JSCallbackObject::toNumber(ExecState* exec) const
{
    JSObjectRef thisRef = toRef(this);

    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSConvertToTypeCallback convertToTypeCallback = callbacks->convertToType) {
            JSValueRef returnValue;
            if (convertToTypeCallback(thisRef, kJSTypeNumber, &returnValue))
                return toJS(returnValue)->getNumber();
        }
    }
    return JSObject::toNumber(exec);
}

UString JSCallbackObject::toString(ExecState* exec) const
{
    JSObjectRef thisRef = toRef(this);

    for (const JSObjectCallbacks* callbacks = &m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSConvertToTypeCallback convertToTypeCallback = callbacks->convertToType) {
            JSValueRef returnValue;
            if (convertToTypeCallback(thisRef, kJSTypeString, &returnValue))
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

JSValue* JSCallbackObject::cachedValueGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot)
{
    JSValue* v = slot.slotBase();
    ASSERT(v);
    return v;
}

JSValue* JSCallbackObject::callbackGetter(ExecState*, JSObject* originalObject, const Identifier& propertyName, const PropertySlot&)
{
    ASSERT(originalObject->inherits(&JSCallbackObject::info));
    JSObjectRef thisRef = toRef(originalObject);
    JSCharBufferRef propertyNameRef= toRef(propertyName.ustring().rep());

    for (const JSObjectCallbacks* callbacks = &static_cast<JSCallbackObject*>(originalObject)->m_callbacks; callbacks; callbacks = callbacks->parentCallbacks) {
        if (JSGetPropertyCallback getPropertyCallback = callbacks->getProperty) {
            JSValueRef returnValue;
            if (getPropertyCallback(thisRef, propertyNameRef, &returnValue)) {
                return toJS(returnValue);
            }
        }
    }

    return jsUndefined();
}

} // namespace KJS
