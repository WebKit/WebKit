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
#include "function.h"
#include "nodes.h"
#include "internal.h"
#include "object.h"
#include "reference_list.h"

using namespace KJS;

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

JSObjectRef JSFunctionMake(JSContextRef context, JSCallAsFunctionCallback callAsFunction)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    return toRef(new JSCallbackFunction(exec, callAsFunction));
}

JSObjectRef JSConstructorMake(JSContextRef context, JSCallAsConstructorCallback callAsConstructor)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    return toRef(new JSCallbackConstructor(exec, callAsConstructor));
}

JSObjectRef JSFunctionMakeWithBody(JSContextRef context, JSStringBufferRef body, JSStringBufferRef sourceURL, int startingLineNumber)
{
    JSLock lock;
    
    ExecState* exec = toJS(context);
    UString::Rep* bodyRep = toJS(body);
    UString::Rep* sourceURLRep = toJS(sourceURL);
    if (!bodyRep)
        bodyRep = &UString::Rep::null;
    RefPtr<FunctionBodyNode> bodyNode = Parser::parse(UString(sourceURLRep), startingLineNumber, bodyRep->data(), bodyRep->size(), NULL, NULL, NULL);
    if (!bodyNode)
        return NULL;

    ScopeChain scopeChain;
    scopeChain.push(exec->dynamicInterpreter()->globalObject());
    return toRef(static_cast<JSObject*>(new DeclaredFunctionImp(exec, "anonymous", bodyNode.get(), scopeChain)));
}

JSStringBufferRef JSObjectGetDescription(JSObjectRef object)
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

bool JSObjectHasProperty(JSContextRef context, JSObjectRef object, JSStringBufferRef propertyName)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);
    
    return jsObject->hasProperty(exec, Identifier(nameRep));
}

bool JSObjectGetProperty(JSContextRef context, JSObjectRef object, JSStringBufferRef propertyName, JSValueRef* value)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);

    *value = toRef(jsObject->get(exec, Identifier(nameRep)));
    return !JSValueIsUndefined(*value);
}

bool JSObjectSetProperty(JSContextRef context, JSObjectRef object, JSStringBufferRef propertyName, JSValueRef value, JSPropertyAttributes attributes)
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

bool JSObjectDeleteProperty(JSContextRef context, JSObjectRef object, JSStringBufferRef propertyName)
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

JSValueRef JSObjectCallAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    JSObject* jsThisObject = toJS(thisObject);

    List argList;
    for (size_t i = 0; i < argc; i++)
        argList.append(toJS(argv[i]));
    
    JSValueRef result = toRef(jsObject->call(exec, jsThisObject, argList));
    if (exec->hadException()) {
        if (exception)
            *exception = exec->exception();
        result = NULL;
        exec->clearException();
    }
    return result;
}

bool JSObjectIsConstructor(JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    return jsObject->implementsConstruct();
}

JSObjectRef JSObjectCallAsConstructor(JSContextRef context, JSObjectRef object, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(context);
    JSObject* jsObject = toJS(object);
    
    List argList;
    for (size_t i = 0; i < argc; i++)
        argList.append(toJS(argv[i]));
    
    JSObjectRef result = toRef(jsObject->construct(exec, argList));
    if (exec->hadException()) {
        if (exception)
            *exception = exec->exception();
        result = NULL;
        exec->clearException();
    }
    return result;
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

JSStringBufferRef JSPropertyEnumeratorGetNext(JSContextRef context, JSPropertyEnumeratorRef enumerator)
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

void JSPropertyListAdd(JSPropertyListRef propertyList, JSObjectRef thisObject, JSStringBufferRef propertyName)
{
    JSLock lock;
    ReferenceList* jsPropertyList = toJS(propertyList);
    JSObject* jsObject = toJS(thisObject);
    UString::Rep* rep = toJS(propertyName);
    
    jsPropertyList->append(Reference(jsObject, Identifier(rep)));
}
