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
#include "JSValueRef.h"
#include "JSObjectRef.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"

#include "identifier.h"
#include "internal.h"
#include "object.h"
#include "reference_list.h"

using namespace KJS;

JSObjectRef JSValueToObject(JSContextRef context, JSValueRef value)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSValue* jsValue = toJS(value);

    JSObjectRef objectRef = toRef(jsValue->toObject(exec));
    // FIXME: What should we do with this exception?
    if (exec->hadException())
        exec->clearException();
    return objectRef;
}    

JSObjectRef JSObjectMake(JSContextRef context, JSClassRef jsClass, JSObjectRef prototype)
{
    JSLock lock;

    ExecState* exec = toJS(context);
    JSObject* jsPrototype = toJS(prototype);

    if (!prototype)
        jsPrototype = exec->lexicalInterpreter()->builtinObjectPrototype();

    if (!jsClass)
        return toRef(new JSObject(jsPrototype)); // slightly more efficient
    else
        return toRef(new JSCallbackObject(jsClass, jsPrototype));
}

JSObjectRef JSFunctionMake(JSContextRef context, JSCallAsFunctionCallback callback)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    return toRef(new JSCallbackFunction(exec, callback));
}

JSObjectRef JSConstructorMake(JSContextRef context, JSCallAsConstructorCallback callback)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    return toRef(new JSCallbackConstructor(exec, callback));
}

JSCharBufferRef JSObjectGetDescription(JSObjectRef object)
{
    JSLock lock;
    JSObject* jsObject = toJS(object);
    return toRef(jsObject->className().rep());
}

JSValueRef JSObjectGetPrototype(JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    return toRef(jsObject->prototype());
}

void JSObjectSetPrototype(JSObjectRef object, JSValueRef value)
{
    JSObject* jsObject = toJS(object);
    JSValue* jsValue = toJS(value);

    jsObject->setPrototype(jsValue);
}

bool JSObjectHasProperty(JSContextRef context, JSObjectRef object, JSCharBufferRef propertyName)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);
    
    return jsObject->hasProperty(exec, Identifier(nameRep));
}

bool JSObjectGetProperty(JSContextRef context, JSObjectRef object, JSCharBufferRef propertyName, JSValueRef* value)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);

    *value = toRef(jsObject->get(exec, Identifier(nameRep)));
    return !JSValueIsUndefined(*value);
}

bool JSObjectSetProperty(JSContextRef context, JSObjectRef object, JSCharBufferRef propertyName, JSValueRef value, JSPropertyAttributes attributes)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);
    JSValue* jsValue = toJS(value);
    
    Identifier jsNameID = Identifier(nameRep);
    if (jsObject->canPut(exec, jsNameID)) {
        jsObject->put(exec, jsNameID, jsValue, attributes);
        return true;
    } else
        return false;
}

bool JSObjectDeleteProperty(JSContextRef context, JSObjectRef object, JSCharBufferRef propertyName)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);

    return jsObject->deleteProperty(exec, Identifier(nameRep));
}

void* JSObjectGetPrivate(JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    
    if (!jsObject->inherits(&JSCallbackObject::info))
        return 0;
    
    return static_cast<JSCallbackObject*>(jsObject)->getPrivate();
}

bool JSObjectSetPrivate(JSObjectRef object, void* data)
{
    JSObject* jsObject = toJS(object);
    
    if (!jsObject->inherits(&JSCallbackObject::info))
        return false;
    
    static_cast<JSCallbackObject*>(jsObject)->setPrivate(data);
    return true;
}

bool JSObjectIsFunction(JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    return jsObject->implementsCall();
}

bool JSObjectCallAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argc, JSValueRef argv[], JSValueRef* returnValue)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    JSObject* jsThisObject = toJS(thisObject);

    List argList;
    for (size_t i = 0; i < argc; i++)
        argList.append(toJS(argv[i]));
    
    *returnValue = jsObject->call(exec, jsThisObject, argList);
    if (exec->hadException()) {
        exec->clearException();
        return false;
    }
    return true;
}

bool JSObjectIsConstructor(JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    return jsObject->implementsConstruct();
}

bool JSObjectCallAsConstructor(JSContextRef context, JSObjectRef object, size_t argc, JSValueRef argv[], JSValueRef* returnValue)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    
    List argList;
    for (size_t i = 0; i < argc; i++)
        argList.append(toJS(argv[i]));
    
    *returnValue = jsObject->construct(exec, argList);
    if (exec->hadException()) {
        exec->clearException();
        return false;
    }
    return true;
}

struct __JSPropertyEnumerator
{
    __JSPropertyEnumerator() : refCount(0), iterator(list.end())
    {
    }
    
    unsigned refCount;
    ReferenceList list;
    ReferenceListIterator iterator;
};

JSPropertyEnumeratorRef JSObjectCreatePropertyEnumerator(JSContextRef context, JSObjectRef object)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    
    JSPropertyEnumeratorRef enumerator = new __JSPropertyEnumerator();
    jsObject->getPropertyList(exec, enumerator->list);
    enumerator->iterator = enumerator->list.begin();
    
    return JSPropertyEnumeratorRetain(enumerator);
}

JSCharBufferRef JSPropertyEnumeratorGetNext(JSContextRef context, JSPropertyEnumeratorRef enumerator)
{
    ExecState* exec = toJS(context);
    ReferenceListIterator& iterator = enumerator->iterator;
    if (iterator != enumerator->list.end()) {
        iterator++;
        return toRef(iterator->getPropertyName(exec).ustring().rep());
    }
    return 0;
}

JSPropertyEnumeratorRef JSPropertyEnumeratorRetain(JSPropertyEnumeratorRef enumerator)
{
    ++enumerator->refCount;
    return enumerator;
}

void JSPropertyEnumeratorRelease(JSPropertyEnumeratorRef enumerator)
{
    if (--enumerator->refCount == 0)
        delete enumerator;
}

void JSPropertyListAdd(JSPropertyListRef propertyList, JSObjectRef thisObject, JSCharBufferRef propertyName)
{
    JSLock lock;
    ReferenceList* jsPropertyList = toJS(propertyList);
    JSObject* jsObject = toJS(thisObject);
    UString::Rep* rep = toJS(propertyName);
    
    jsPropertyList->append(Reference(jsObject, Identifier(rep)));
}
