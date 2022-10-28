/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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
#include "JSAPIWrapperObject.h"
#include "JSCInlines.h"
#include "JSCallbackObject.h"
#include "JSONObject.h"
#include "LiteralParser.h"
#include "Protect.h"
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>
#include <wtf/ForkExtras.h>
#if PLATFORM(MAC)
#include <mach-o/dyld.h>
#endif

#if ENABLE(REMOTE_INSPECTOR)
#include "JSGlobalObjectInspectorController.h"
#endif

using namespace JSC;

::JSType JSValueGetType(JSContextRef ctx, JSValueRef value)
{

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    JSValue jsValue = toJS(globalObject, value);
#else
    UNUSED_PARAM(ctx);
    JSValue jsValue = toJS(value);
#endif

    if (jsValue.isUndefined())
        return kJSTypeUndefined;
    if (!jsValue || jsValue.isNull())
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

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toJS(globalObject, value).isUndefined();
#else
    UNUSED_PARAM(ctx);
    return toJS(value).isUndefined();
#endif
}

bool JSValueIsNull(JSContextRef ctx, JSValueRef value)
{


#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toJS(globalObject, value).isNull();
#else
    UNUSED_PARAM(ctx);
    return !value || toJS(value).isNull();
#endif
}

bool JSValueIsBoolean(JSContextRef ctx, JSValueRef value)
{

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toJS(globalObject, value).isBoolean();
#else
    UNUSED_PARAM(ctx);
    return toJS(value).isBoolean();
#endif
}

bool JSValueIsNumber(JSContextRef ctx, JSValueRef value)
{

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toJS(globalObject, value).isNumber();
#else
    UNUSED_PARAM(ctx);
    return toJS(value).isNumber();
#endif
}

bool JSValueIsString(JSContextRef ctx, JSValueRef value)
{

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toJS(globalObject, value).isString();
#else
    UNUSED_PARAM(ctx);
    return value && toJS(value).isString();
#endif
}

bool JSValueIsObject(JSContextRef ctx, JSValueRef value)
{

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toJS(globalObject, value).isObject();
#else
    UNUSED_PARAM(ctx);
    return value && toJS(value).isObject();
#endif
}

bool JSValueIsSymbol(JSContextRef ctx, JSValueRef value)
{

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toJS(globalObject, value).isSymbol();
#else
    UNUSED_PARAM(ctx);
    return value && toJS(value).isSymbol();
#endif
}

bool JSValueIsArray(JSContextRef ctx, JSValueRef value)
{

    JSGlobalObject* globalObject = toJS(ctx);
    return toJS(globalObject, value).inherits<JSArray>();
}

bool JSValueIsDate(JSContextRef ctx, JSValueRef value)
{

    JSGlobalObject* globalObject = toJS(ctx);
    return toJS(globalObject, value).inherits<DateInstance>();
}

bool JSValueIsObjectOfClass(JSContextRef ctx, JSValueRef value, JSClassRef jsClass)
{

    JSGlobalObject* globalObject = toJS(ctx);

    JSValue jsValue = toJS(globalObject, value);
    
    if (JSObject* o = jsValue.getObject()) {
        if (o->inherits<JSProxy>())
            o = jsCast<JSProxy*>(o)->target();

        if (o->inherits<JSCallbackObject<JSGlobalObject>>())
            return jsCast<JSCallbackObject<JSGlobalObject>*>(o)->inherits(jsClass);
        if (o->inherits<JSCallbackObject<JSNonFinalObject>>())
            return jsCast<JSCallbackObject<JSNonFinalObject>*>(o)->inherits(jsClass);
#if JSC_OBJC_API_ENABLED
        if (o->inherits<JSCallbackObject<JSAPIWrapperObject>>())
            return jsCast<JSCallbackObject<JSAPIWrapperObject>*>(o)->inherits(jsClass);
#endif
    }
    return false;
}

bool JSValueIsEqual(JSContextRef ctx, JSValueRef a, JSValueRef b, JSValueRef* exception)
{

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsA = toJS(globalObject, a);
    JSValue jsB = toJS(globalObject, b);

    bool result = JSValue::equal(globalObject, jsA, jsB); // false if an exception is thrown
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return false;
    
    return result;
}

bool JSValueIsStrictEqual(JSContextRef ctx, JSValueRef a, JSValueRef b)
{

    JSGlobalObject* globalObject = toJS(ctx);


    JSValue jsA = toJS(globalObject, a);
    JSValue jsB = toJS(globalObject, b);

    return JSValue::strictEqual(globalObject, jsA, jsB);
}

bool JSValueIsInstanceOfConstructor(JSContextRef ctx, JSValueRef value, JSObjectRef constructor, JSValueRef* exception)
{

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsValue = toJS(globalObject, value);

    JSObject* jsConstructor = toJS(constructor);
    if (!jsConstructor->structure()->typeInfo().implementsHasInstance())
        return false;
    bool result = jsConstructor->hasInstance(globalObject, jsValue); // false if an exception is thrown
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return false;
    return result;
}

JSValueRef JSValueMakeUndefined(JSContextRef ctx)
{

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toRef(globalObject, jsUndefined());
#else
    UNUSED_PARAM(ctx);
    return toRef(jsUndefined());
#endif
}

JSValueRef JSValueMakeNull(JSContextRef ctx)
{

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toRef(globalObject, jsNull());
#else
    UNUSED_PARAM(ctx);
    return toRef(jsNull());
#endif
}

JSValueRef JSValueMakeBoolean(JSContextRef ctx, bool value)
{

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toRef(globalObject, jsBoolean(value));
#else
    UNUSED_PARAM(ctx);
    return toRef(jsBoolean(value));
#endif
}

JSValueRef JSValueMakeNumber(JSContextRef ctx, double value)
{

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);

    return toRef(globalObject, jsNumber(purifyNaN(value)));
#else
    UNUSED_PARAM(ctx);
    return toRef(jsNumber(purifyNaN(value)));
#endif
}

JSValueRef JSValueMakeSymbol(JSContextRef ctx, JSStringRef description)
{

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();


    if (!description)
        return toRef(globalObject, Symbol::create(vm));
    return toRef(globalObject, Symbol::createWithDescription(vm, description->string()));
}

JSValueRef JSValueMakeString(JSContextRef ctx, JSStringRef string)
{

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    return toRef(globalObject, string->isExternal() ? jsOwnedString(vm, string->string()) : jsString(vm, string->string()));
}

JSValueRef JSValueMakeFromJSONString(JSContextRef ctx, JSStringRef string)
{

    JSGlobalObject* globalObject = toJS(ctx);

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

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue value = toJS(globalObject, apiValue);
    String result = JSONStringify(globalObject, value, indent);
    if (exception)
        *exception = nullptr;
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;
    return OpaqueJSString::tryCreate(result).leakRef();
}

JSStringRef JSValueToStringCopy(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsValue = toJS(globalObject, value);
    
    auto stringRef(OpaqueJSString::tryCreate(jsValue.toWTFString(globalObject)));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        stringRef = nullptr;
    return stringRef.leakRef();
}

bool JSValueToBoolean(JSContextRef ctx, JSValueRef value)
{

    JSGlobalObject* globalObject = toJS(ctx);


    JSValue jsValue = toJS(globalObject, value);
    return jsValue.toBoolean(globalObject);
}

double JSValueToNumber(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsValue = toJS(globalObject, value);

    double number = jsValue.toNumber(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        number = PNaN;
    return number;
}

JSObjectRef JSValueToObject(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsValue = toJS(globalObject, value);
    
    JSObjectRef objectRef = toRef(jsValue.toObject(globalObject));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        objectRef = nullptr;
    return objectRef;
}

void JSValueProtect(JSContextRef ctx, JSValueRef value)
{
    JSGlobalObject* globalObject = toJS(ctx);
    JSValue jsValue = toJSForGC(globalObject, value);
    gcProtect(jsValue);
}

void JSValueUnprotect(JSContextRef ctx, JSValueRef value)
{
    JSGlobalObject* globalObject = toJS(ctx);
    JSValue jsValue = toJSForGC(globalObject, value);
    gcUnprotect(jsValue);
}

bool JSValueIsEqualString(JSContextRef ctx, JSValueRef value, const char* ptr, size_t length, JSValueRef* exception) {
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsValue = toJS(globalObject, value);


    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow || !isJSString(jsValue))
        return false;


    return static_cast<JSString*>(jsValue.asCell())->equal(globalObject, ptr, length);
}

// void* JSValueGetCell(JSContextRef ctx, JSValueRef value) {
//     UNUSED_PARAM(ctx);
//     JSC::JSValue result = bitwise_cast<JSC::JSValue>(value);
//     if (!result.isCell()) {
//         return nullptr;
//     }
//     JSCell* cell = result.asCell();
//     gcProtect(cell);

//     return cell;
// }

// void JSValueReleaseCell(JSC::JSCell *cell) {
//     gcUnprotect(cell);
// }

// size_t JSStringLength(JSC::JSString *string) {
//     return string->length();
// }

// void JSStringIterate(JSC::JSString *string, jsstring_iterator *iter) {
//     string->value(iter);
// }
