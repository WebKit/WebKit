/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "JSValueRef.h"

#include <wtf/Platform.h>
#include "APICast.h"
#include "JSCallbackObject.h"

#include <runtime/JSGlobalObject.h>
#include <runtime/JSString.h>
#include <runtime/Operations.h>
#include <runtime/Protect.h>
#include <runtime/UString.h>
#include <runtime/JSValue.h>

#include <wtf/Assertions.h>

#include <algorithm> // for std::min

JSType JSValueGetType(JSContextRef, JSValueRef value)
{
    JSC::JSValue* jsValue = toJS(value);
    if (jsValue->isUndefined())
        return kJSTypeUndefined;
    if (jsValue->isNull())
        return kJSTypeNull;
    if (jsValue->isBoolean())
        return kJSTypeBoolean;
    if (jsValue->isNumber())
        return kJSTypeNumber;
    if (jsValue->isString())
        return kJSTypeString;
    ASSERT(jsValue->isObject());
    return kJSTypeObject;
}

using namespace JSC; // placed here to avoid conflict between JSC::JSType and JSType, above.

bool JSValueIsUndefined(JSContextRef, JSValueRef value)
{
    JSValue* jsValue = toJS(value);
    return jsValue->isUndefined();
}

bool JSValueIsNull(JSContextRef, JSValueRef value)
{
    JSValue* jsValue = toJS(value);
    return jsValue->isNull();
}

bool JSValueIsBoolean(JSContextRef, JSValueRef value)
{
    JSValue* jsValue = toJS(value);
    return jsValue->isBoolean();
}

bool JSValueIsNumber(JSContextRef, JSValueRef value)
{
    JSValue* jsValue = toJS(value);
    return jsValue->isNumber();
}

bool JSValueIsString(JSContextRef, JSValueRef value)
{
    JSValue* jsValue = toJS(value);
    return jsValue->isString();
}

bool JSValueIsObject(JSContextRef, JSValueRef value)
{
    JSValue* jsValue = toJS(value);
    return jsValue->isObject();
}

bool JSValueIsObjectOfClass(JSContextRef, JSValueRef value, JSClassRef jsClass)
{
    JSValue* jsValue = toJS(value);
    
    if (JSObject* o = jsValue->getObject()) {
        if (o->inherits(&JSCallbackObject<JSGlobalObject>::info))
            return static_cast<JSCallbackObject<JSGlobalObject>*>(o)->inherits(jsClass);
        else if (o->inherits(&JSCallbackObject<JSObject>::info))
            return static_cast<JSCallbackObject<JSObject>*>(o)->inherits(jsClass);
    }
    return false;
}

bool JSValueIsEqual(JSContextRef ctx, JSValueRef a, JSValueRef b, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSValue* jsA = toJS(a);
    JSValue* jsB = toJS(b);

    bool result = equal(exec, jsA, jsB); // false if an exception is thrown
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
    return result;
}

bool JSValueIsStrictEqual(JSContextRef, JSValueRef a, JSValueRef b)
{
    JSValue* jsA = toJS(a);
    JSValue* jsB = toJS(b);
    
    bool result = strictEqual(jsA, jsB);
    return result;
}

bool JSValueIsInstanceOfConstructor(JSContextRef ctx, JSValueRef value, JSObjectRef constructor, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSValue* jsValue = toJS(value);
    JSObject* jsConstructor = toJS(constructor);
    if (!jsConstructor->structure()->typeInfo().implementsHasInstance())
        return false;
    bool result = jsConstructor->hasInstance(exec, jsValue, jsConstructor->get(exec, exec->propertyNames().prototype)); // false if an exception is thrown
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
    return result;
}

JSValueRef JSValueMakeUndefined(JSContextRef)
{
    return toRef(jsUndefined());
}

JSValueRef JSValueMakeNull(JSContextRef)
{
    return toRef(jsNull());
}

JSValueRef JSValueMakeBoolean(JSContextRef, bool value)
{
    return toRef(jsBoolean(value));
}

JSValueRef JSValueMakeNumber(JSContextRef ctx, double value)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    return toRef(jsNumber(exec, value));
}

JSValueRef JSValueMakeString(JSContextRef ctx, JSStringRef string)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    return toRef(jsString(exec, string->ustring()));
}

bool JSValueToBoolean(JSContextRef ctx, JSValueRef value)
{
    ExecState* exec = toJS(ctx);
    JSValue* jsValue = toJS(value);
    return jsValue->toBoolean(exec);
}

double JSValueToNumber(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSValue* jsValue = toJS(value);

    double number = jsValue->toNumber(exec);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
        number = NaN;
    }
    return number;
}

JSStringRef JSValueToStringCopy(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSValue* jsValue = toJS(value);
    
    RefPtr<OpaqueJSString> stringRef(OpaqueJSString::create(jsValue->toString(exec)));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
        stringRef.clear();
    }
    return stringRef.release().releaseRef();
}

JSObjectRef JSValueToObject(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSValue* jsValue = toJS(value);
    
    JSObjectRef objectRef = toRef(jsValue->toObject(exec));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
        objectRef = 0;
    }
    return objectRef;
}    

void JSValueProtect(JSContextRef ctx, JSValueRef value)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSValue* jsValue = toJS(value);
    gcProtect(jsValue);
}

void JSValueUnprotect(JSContextRef ctx, JSValueRef value)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap.registerThread();
    JSLock lock(exec);

    JSValue* jsValue = toJS(value);
    gcUnprotect(jsValue);
}
