/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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
#include "JSValueRef.h"

#include "APICast.h"
#include "APIUtils.h"
#include "DateInstance.h"
#include "Exception.h"
#include "JSAPIWrapperObject.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSCallbackObject.h"
#include "JSGlobalObject.h"
#include "JSONObject.h"
#include "JSObjectRefPrivate.h"
#include "JSString.h"
#include "LiteralParser.h"
#include "Protect.h"
#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#include <mach-o/dyld.h>
#endif

#if ENABLE(REMOTE_INSPECTOR)
#include "JSGlobalObjectInspectorController.h"
#endif

using namespace JSC;

::JSType JSValueGetType(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return kJSTypeUndefined;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    JSValue jsValue = toJS(globalObject, value);

    if (jsValue.isUndefined())
        return kJSTypeUndefined;
    if (jsValue.isNull())
        return kJSTypeNull;
    if (jsValue.isBoolean())
        return kJSTypeBoolean;
    if (jsValue.isNumber())
        return kJSTypeNumber;
    if (jsValue.isString())
        return kJSTypeString;
    if (jsValue.isSymbol())
        return kJSTypeSymbol;
    ASSERT(jsValue.isObject());
    return kJSTypeObject;
}

bool JSValueIsUndefined(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).isUndefined();
}

bool JSValueIsNull(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).isNull();
}

bool JSValueIsBoolean(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).isBoolean();
}

bool JSValueIsNumber(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).isNumber();
}

bool JSValueIsString(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).isString();
}

bool JSValueIsObject(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).isObject();
}

bool JSValueIsSymbol(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).isSymbol();
}

bool JSValueIsArray(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).inherits<JSArray>(vm);
}

bool JSValueIsDate(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).inherits<DateInstance>(vm);
}

bool JSValueIsObjectOfClass(JSContextRef ctx, JSValueRef value, JSClassRef jsClass)
{
    if (!ctx || !jsClass) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(globalObject);

    JSValue jsValue = toJS(globalObject, value);
    
    if (JSObject* o = jsValue.getObject()) {
        if (o->inherits<JSProxy>(vm))
            o = jsCast<JSProxy*>(o)->target();

        if (o->inherits<JSCallbackObject<JSGlobalObject>>(vm))
            return jsCast<JSCallbackObject<JSGlobalObject>*>(o)->inherits(jsClass);
        if (o->inherits<JSCallbackObject<JSNonFinalObject>>(vm))
            return jsCast<JSCallbackObject<JSNonFinalObject>*>(o)->inherits(jsClass);
#if JSC_OBJC_API_ENABLED
        if (o->inherits<JSCallbackObject<JSAPIWrapperObject>>(vm))
            return jsCast<JSCallbackObject<JSAPIWrapperObject>*>(o)->inherits(jsClass);
#endif
    }
    return false;
}

bool JSValueIsEqual(JSContextRef ctx, JSValueRef a, JSValueRef b, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsA = toJS(globalObject, a);
    JSValue jsB = toJS(globalObject, b);

    bool result = JSValue::equal(globalObject, jsA, jsB); // false if an exception is thrown
    handleExceptionIfNeeded(scope, ctx, exception);
    
    return result;
}

bool JSValueIsStrictEqual(JSContextRef ctx, JSValueRef a, JSValueRef b)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    JSValue jsA = toJS(globalObject, a);
    JSValue jsB = toJS(globalObject, b);

    return JSValue::strictEqual(globalObject, jsA, jsB);
}

bool JSValueIsInstanceOfConstructor(JSContextRef ctx, JSValueRef value, JSObjectRef constructor, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsValue = toJS(globalObject, value);

    JSObject* jsConstructor = toJS(constructor);
    if (!jsConstructor->structure(vm)->typeInfo().implementsHasInstance())
        return false;
    bool result = jsConstructor->hasInstance(globalObject, jsValue); // false if an exception is thrown
    handleExceptionIfNeeded(scope, ctx, exception);
    return result;
}

JSValueRef JSValueMakeUndefined(JSContextRef ctx)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toRef(globalObject, jsUndefined());
}

JSValueRef JSValueMakeNull(JSContextRef ctx)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toRef(globalObject, jsNull());
}

JSValueRef JSValueMakeBoolean(JSContextRef ctx, bool value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toRef(globalObject, jsBoolean(value));
}

JSValueRef JSValueMakeNumber(JSContextRef ctx, double value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toRef(globalObject, jsNumber(purifyNaN(value)));
}

JSValueRef JSValueMakeSymbol(JSContextRef ctx, JSStringRef description)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(globalObject);

    if (!description)
        return toRef(globalObject, Symbol::create(vm));
    return toRef(globalObject, Symbol::createWithDescription(vm, description->string()));
}

JSValueRef JSValueMakeString(JSContextRef ctx, JSStringRef string)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);

    return toRef(globalObject, jsString(vm, string ? string->string() : String()));
}

JSValueRef JSValueMakeFromJSONString(JSContextRef ctx, JSStringRef string)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    String str = string->string();
    unsigned length = str.length();
    if (!length || str.is8Bit()) {
        LiteralParser<LChar> parser(globalObject, str.characters8(), length, StrictJSON);
        return toRef(globalObject, parser.tryLiteralParse());
    }
    LiteralParser<UChar> parser(globalObject, str.characters16(), length, StrictJSON);
    return toRef(globalObject, parser.tryLiteralParse());
}

JSStringRef JSValueCreateJSONString(JSContextRef ctx, JSValueRef apiValue, unsigned indent, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue value = toJS(globalObject, apiValue);
    String result = JSONStringify(globalObject, value, indent);
    if (exception)
        *exception = 0;
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return 0;
    return OpaqueJSString::tryCreate(result).leakRef();
}

bool JSValueToBoolean(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    JSValue jsValue = toJS(globalObject, value);
    return jsValue.toBoolean(globalObject);
}

double JSValueToNumber(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return PNaN;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsValue = toJS(globalObject, value);

    double number = jsValue.toNumber(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        number = PNaN;
    return number;
}

JSStringRef JSValueToStringCopy(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsValue = toJS(globalObject, value);
    
    auto stringRef(OpaqueJSString::tryCreate(jsValue.toWTFString(globalObject)));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        stringRef = nullptr;
    return stringRef.leakRef();
}

JSObjectRef JSValueToObject(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsValue = toJS(globalObject, value);
    
    JSObjectRef objectRef = toRef(jsValue.toObject(globalObject));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        objectRef = 0;
    return objectRef;
}

void JSValueProtect(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    JSValue jsValue = toJSForGC(globalObject, value);
    gcProtect(jsValue);
}

void JSValueUnprotect(JSContextRef ctx, JSValueRef value)
{
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    JSValue jsValue = toJSForGC(globalObject, value);
    gcUnprotect(jsValue);
}
