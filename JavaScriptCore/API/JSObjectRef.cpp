/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Kelvin W Sherlock (ksherlock@gmail.com)
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
#include "JSObjectRef.h"

#include "APICast.h"
#include "DateConstructor.h"
#include "ErrorConstructor.h"
#include "FunctionConstructor.h"
#include "Identifier.h"
#include "InitializeThreading.h"
#include "JSArray.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"
#include "JSClassRef.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include "JSRetainPtr.h"
#include "JSString.h"
#include "JSValueRef.h"
#include "ObjectPrototype.h"
#include "PropertyNameArray.h"
#include "RegExpConstructor.h"
#include <wtf/Platform.h>

using namespace JSC;

JSClassRef JSClassCreate(const JSClassDefinition* definition)
{
    initializeThreading();
    RefPtr<OpaqueJSClass> jsClass = (definition->attributes & kJSClassAttributeNoAutomaticPrototype)
        ? OpaqueJSClass::createNoAutomaticPrototype(definition)
        : OpaqueJSClass::create(definition);
    
    return jsClass.release().releaseRef();
}

JSClassRef JSClassRetain(JSClassRef jsClass)
{
    jsClass->ref();
    return jsClass;
}

void JSClassRelease(JSClassRef jsClass)
{
    jsClass->deref();
}

JSObjectRef JSObjectMake(JSContextRef ctx, JSClassRef jsClass, void* data)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    if (!jsClass)
        return toRef(new (exec) JSObject(exec->lexicalGlobalObject()->emptyObjectStructure())); // slightly more efficient

    JSCallbackObject<JSObject>* object = new (exec) JSCallbackObject<JSObject>(exec, exec->lexicalGlobalObject()->callbackObjectStructure(), jsClass, data);
    if (JSObject* prototype = jsClass->prototype(exec))
        object->setPrototype(prototype);

    return toRef(object);
}

JSObjectRef JSObjectMakeFunctionWithCallback(JSContextRef ctx, JSStringRef name, JSObjectCallAsFunctionCallback callAsFunction)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    Identifier nameID = name ? name->identifier(&exec->globalData()) : Identifier(exec, "anonymous");
    
    return toRef(new (exec) JSCallbackFunction(exec, callAsFunction, nameID));
}

JSObjectRef JSObjectMakeConstructor(JSContextRef ctx, JSClassRef jsClass, JSObjectCallAsConstructorCallback callAsConstructor)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSValue jsPrototype = jsClass ? jsClass->prototype(exec) : 0;
    if (!jsPrototype)
        jsPrototype = exec->lexicalGlobalObject()->objectPrototype();

    JSCallbackConstructor* constructor = new (exec) JSCallbackConstructor(exec->lexicalGlobalObject()->callbackConstructorStructure(), jsClass, callAsConstructor);
    constructor->putDirect(exec->propertyNames().prototype, jsPrototype, DontEnum | DontDelete | ReadOnly);
    return toRef(constructor);
}

JSObjectRef JSObjectMakeFunction(JSContextRef ctx, JSStringRef name, unsigned parameterCount, const JSStringRef parameterNames[], JSStringRef body, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    Identifier nameID = name ? name->identifier(&exec->globalData()) : Identifier(exec, "anonymous");
    
    MarkedArgumentBuffer args;
    for (unsigned i = 0; i < parameterCount; i++)
        args.append(jsString(exec, parameterNames[i]->ustring()));
    args.append(jsString(exec, body->ustring()));

    JSObject* result = constructFunction(exec, args, nameID, sourceURL->ustring(), startingLineNumber);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
        result = 0;
    }
    return toRef(result);
}

JSObjectRef JSObjectMakeArray(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[],  JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* result;
    if (argumentCount) {
        MarkedArgumentBuffer argList;
        for (size_t i = 0; i < argumentCount; ++i)
            argList.append(toJS(exec, arguments[i]));

        result = constructArray(exec, argList);
    } else
        result = constructEmptyArray(exec);

    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
        result = 0;
    }

    return toRef(result);
}

JSObjectRef JSObjectMakeDate(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[],  JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    MarkedArgumentBuffer argList;
    for (size_t i = 0; i < argumentCount; ++i)
        argList.append(toJS(exec, arguments[i]));

    JSObject* result = constructDate(exec, argList);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
        result = 0;
    }

    return toRef(result);
}

JSObjectRef JSObjectMakeError(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[],  JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    MarkedArgumentBuffer argList;
    for (size_t i = 0; i < argumentCount; ++i)
        argList.append(toJS(exec, arguments[i]));

    JSObject* result = constructError(exec, argList);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
        result = 0;
    }

    return toRef(result);
}

JSObjectRef JSObjectMakeRegExp(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[],  JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    MarkedArgumentBuffer argList;
    for (size_t i = 0; i < argumentCount; ++i)
        argList.append(toJS(exec, arguments[i]));

    JSObject* result = constructRegExp(exec, argList);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
        result = 0;
    }
    
    return toRef(result);
}

JSValueRef JSObjectGetPrototype(JSContextRef ctx, JSObjectRef object)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* jsObject = toJS(object);
    return toRef(exec, jsObject->prototype());
}

void JSObjectSetPrototype(JSContextRef ctx, JSObjectRef object, JSValueRef value)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* jsObject = toJS(object);
    JSValue jsValue = toJS(exec, value);

    jsObject->setPrototype(jsValue.isObject() ? jsValue : jsNull());
}

bool JSObjectHasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* jsObject = toJS(object);
    
    return jsObject->hasProperty(exec, propertyName->identifier(&exec->globalData()));
}

JSValueRef JSObjectGetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* jsObject = toJS(object);

    JSValue jsValue = jsObject->get(exec, propertyName->identifier(&exec->globalData()));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
    }
    return toRef(exec, jsValue);
}

void JSObjectSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* jsObject = toJS(object);
    Identifier name(propertyName->identifier(&exec->globalData()));
    JSValue jsValue = toJS(exec, value);

    if (attributes && !jsObject->hasProperty(exec, name))
        jsObject->putWithAttributes(exec, name, jsValue, attributes);
    else {
        PutPropertySlot slot;
        jsObject->put(exec, name, jsValue, slot);
    }

    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
    }
}

JSValueRef JSObjectGetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* jsObject = toJS(object);

    JSValue jsValue = jsObject->get(exec, propertyIndex);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
    }
    return toRef(exec, jsValue);
}


void JSObjectSetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef value, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* jsObject = toJS(object);
    JSValue jsValue = toJS(exec, value);
    
    jsObject->put(exec, propertyIndex, jsValue);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
    }
}

bool JSObjectDeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* jsObject = toJS(object);

    bool result = jsObject->deleteProperty(exec, propertyName->identifier(&exec->globalData()));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
    }
    return result;
}

void* JSObjectGetPrivate(JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    
    if (jsObject->inherits(&JSCallbackObject<JSGlobalObject>::info))
        return static_cast<JSCallbackObject<JSGlobalObject>*>(jsObject)->getPrivate();
    else if (jsObject->inherits(&JSCallbackObject<JSObject>::info))
        return static_cast<JSCallbackObject<JSObject>*>(jsObject)->getPrivate();
    
    return 0;
}

bool JSObjectSetPrivate(JSObjectRef object, void* data)
{
    JSObject* jsObject = toJS(object);
    
    if (jsObject->inherits(&JSCallbackObject<JSGlobalObject>::info)) {
        static_cast<JSCallbackObject<JSGlobalObject>*>(jsObject)->setPrivate(data);
        return true;
    } else if (jsObject->inherits(&JSCallbackObject<JSObject>::info)) {
        static_cast<JSCallbackObject<JSObject>*>(jsObject)->setPrivate(data);
        return true;
    }
        
    return false;
}

bool JSObjectIsFunction(JSContextRef, JSObjectRef object)
{
    CallData callData;
    return toJS(object)->getCallData(callData) != CallTypeNone;
}

JSValueRef JSObjectCallAsFunction(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* jsObject = toJS(object);
    JSObject* jsThisObject = toJS(thisObject);

    if (!jsThisObject)
        jsThisObject = exec->globalThisValue();

    MarkedArgumentBuffer argList;
    for (size_t i = 0; i < argumentCount; i++)
        argList.append(toJS(exec, arguments[i]));

    CallData callData;
    CallType callType = jsObject->getCallData(callData);
    if (callType == CallTypeNone)
        return 0;

    JSValueRef result = toRef(exec, call(exec, jsObject, callType, callData, jsThisObject, argList));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
        result = 0;
    }
    return result;
}

bool JSObjectIsConstructor(JSContextRef, JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    ConstructData constructData;
    return jsObject->getConstructData(constructData) != ConstructTypeNone;
}

JSObjectRef JSObjectCallAsConstructor(JSContextRef ctx, JSObjectRef object, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSObject* jsObject = toJS(object);

    ConstructData constructData;
    ConstructType constructType = jsObject->getConstructData(constructData);
    if (constructType == ConstructTypeNone)
        return 0;

    MarkedArgumentBuffer argList;
    for (size_t i = 0; i < argumentCount; i++)
        argList.append(toJS(exec, arguments[i]));
    JSObjectRef result = toRef(construct(exec, jsObject, constructType, constructData, argList));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
        result = 0;
    }
    return result;
}

struct OpaqueJSPropertyNameArray {
    OpaqueJSPropertyNameArray(JSGlobalData* globalData)
        : refCount(0)
        , globalData(globalData)
    {
    }
    
    unsigned refCount;
    JSGlobalData* globalData;
    Vector<JSRetainPtr<JSStringRef> > array;
};

JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef ctx, JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSGlobalData* globalData = &exec->globalData();

    JSPropertyNameArrayRef propertyNames = new OpaqueJSPropertyNameArray(globalData);
    PropertyNameArray array(globalData);
    jsObject->getPropertyNames(exec, array);

    size_t size = array.size();
    propertyNames->array.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i)
        propertyNames->array.append(JSRetainPtr<JSStringRef>(Adopt, OpaqueJSString::create(array[i].ustring()).releaseRef()));
    
    return JSPropertyNameArrayRetain(propertyNames);
}

JSPropertyNameArrayRef JSPropertyNameArrayRetain(JSPropertyNameArrayRef array)
{
    ++array->refCount;
    return array;
}

void JSPropertyNameArrayRelease(JSPropertyNameArrayRef array)
{
    if (--array->refCount == 0) {
        JSLock lock(array->globalData->isSharedInstance);
        delete array;
    }
}

size_t JSPropertyNameArrayGetCount(JSPropertyNameArrayRef array)
{
    return array->array.size();
}

JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef array, size_t index)
{
    return array->array[static_cast<unsigned>(index)].get();
}

void JSPropertyNameAccumulatorAddName(JSPropertyNameAccumulatorRef array, JSStringRef propertyName)
{
    PropertyNameArray* propertyNames = toJS(array);

    propertyNames->globalData()->heap.registerThread();
    JSLock lock(propertyNames->globalData()->isSharedInstance);

    propertyNames->add(propertyName->identifier(propertyNames->globalData()));
}
