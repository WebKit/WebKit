/*
 * Copyright (C) 2006-2024 Apple Inc. All rights reserved.
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
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    JSValue jsValue = toJS(globalObject, value);
#else
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
    if (jsValue.isBigInt())
        return kJSTypeBigInt;
    ASSERT(jsValue.isObject());
    return kJSTypeObject;
}

bool JSValueIsUndefined(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toJS(globalObject, value).isUndefined();
#else
    return toJS(value).isUndefined();
#endif
}

bool JSValueIsNull(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }

#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toJS(globalObject, value).isNull();
#else
    return !value || toJS(value).isNull();
#endif
}

bool JSValueIsBoolean(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toJS(globalObject, value).isBoolean();
#else
    return toJS(value).isBoolean();
#endif
}

bool JSValueIsNumber(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toJS(globalObject, value).isNumber();
#else
    return toJS(value).isNumber();
#endif
}

bool JSValueIsString(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toJS(globalObject, value).isString();
#else
    return value && toJS(value).isString();
#endif
}

bool JSValueIsObject(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toJS(globalObject, value).isObject();
#else
    return value && toJS(value).isObject();
#endif
}

bool JSValueIsSymbol(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toJS(globalObject, value).isSymbol();
#else
    return value && toJS(value).isSymbol();
#endif
}

bool JSValueIsBigInt(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toJS(globalObject, value).isBigInt();
#else
    return value && toJS(value).isBigInt();
#endif
}

bool JSValueIsArray(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).inherits<JSArray>();
}

bool JSValueIsDate(JSContextRef ctx, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    return toJS(globalObject, value).inherits<DateInstance>();
}

bool JSValueIsObjectOfClass(JSContextRef ctx, JSValueRef value, JSClassRef jsClass)
{
    if (!ctx || !jsClass) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);

    JSValue jsValue = toJS(globalObject, value);
    
    if (JSObject* o = jsValue.getObject()) {
        if (o->inherits<JSGlobalProxy>())
            o = jsCast<JSGlobalProxy*>(o)->target();

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
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return false;
    
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
    if (!jsConstructor->structure()->typeInfo().implementsHasInstance())
        return false;
    bool result = jsConstructor->hasInstance(globalObject, jsValue); // false if an exception is thrown
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return false;
    return result;
}

JSValueRef JSValueMakeUndefined(JSContextRef ctx)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toRef(globalObject, jsUndefined());
#else
    return toRef(jsUndefined());
#endif
}

JSValueRef JSValueMakeNull(JSContextRef ctx)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toRef(globalObject, jsNull());
#else
    return toRef(jsNull());
#endif
}

JSValueRef JSValueMakeBoolean(JSContextRef ctx, bool value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toRef(globalObject, jsBoolean(value));
#else
    return toRef(jsBoolean(value));
#endif
}

JSValueRef JSValueMakeNumber(JSContextRef ctx, double value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
#if !CPU(ADDRESS64)
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    return toRef(globalObject, jsNumber(purifyNaN(value)));
#else
    return toRef(jsNumber(purifyNaN(value)));
#endif
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

JSValueRef JSBigIntCreateWithDouble(JSContextRef ctx, double value, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (!isInteger(value)) {
        setException(ctx, exception, createRangeError(globalObject, "Not an integer"_s));
        return nullptr;
    }

    JSValue result = JSBigInt::makeHeapBigIntOrBigInt32(globalObject, value);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;

    return toRef(globalObject, result);
}

JSValueRef JSBigIntCreateWithUInt64(JSContextRef ctx, uint64_t integer, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue result = JSBigInt::makeHeapBigIntOrBigInt32(globalObject, integer);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;

    return toRef(globalObject, result);
}

JSValueRef JSBigIntCreateWithInt64(JSContextRef ctx, int64_t integer, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue result = JSBigInt::makeHeapBigIntOrBigInt32(globalObject, integer);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;

    return toRef(globalObject, result);
}

JSValueRef JSBigIntCreateWithString(JSContextRef ctx, JSStringRef string, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue result = JSBigInt::parseInt(globalObject, string->string(), JSBigInt::ErrorParseMode::ThrowExceptions);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;

    return toRef(globalObject, result);
}

uint64_t JSValueToUInt64(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    JSValue numeric = toJS(globalObject, value).toNumeric(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return 0;

    if (numeric.isBigInt())
        return JSBigInt::toBigUInt64(numeric);

    ASSERT(numeric.isNumber());
    return JSC::toUInt64(numeric.asNumber());
}

int64_t JSValueToInt64(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    JSValue numeric = toJS(globalObject, value).toNumeric(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return 0;

    if (numeric.isBigInt())
        return JSBigInt::toBigInt64(numeric);

    ASSERT(numeric.isNumber());
    return JSC::toInt64(numeric.asNumber());
}

uint32_t JSValueToUInt32(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    JSValue numeric = toJS(globalObject, value).toNumeric(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return 0;

    if (numeric.isBigInt())
        return static_cast<uint32_t>(JSBigInt::toBigUInt64(numeric));

    ASSERT(numeric.isNumber());
    return JSC::toUInt32(numeric.asNumber());
}

int32_t JSValueToInt32(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    JSValue numeric = toJS(globalObject, value).toNumeric(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return 0;

    if (numeric.isBigInt())
        return static_cast<int32_t>(JSBigInt::toBigInt64(numeric));

    ASSERT(numeric.isNumber());
    return JSC::toInt32(numeric.asNumber());
}

ALWAYS_INLINE JSRelationCondition toJSRelationCondition(JSC::JSBigInt::ComparisonResult);
JSRelationCondition toJSRelationCondition(JSC::JSBigInt::ComparisonResult result)
{
    switch (result) {
    case JSC::JSBigInt::ComparisonResult::Equal:
        return JSRelationCondition::kJSRelationConditionEqual;
    case JSC::JSBigInt::ComparisonResult::Undefined:
        return JSRelationCondition::kJSRelationConditionUndefined;
    case JSC::JSBigInt::ComparisonResult::GreaterThan:
        return JSRelationCondition::kJSRelationConditionGreaterThan;
    case JSC::JSBigInt::ComparisonResult::LessThan:
        return JSRelationCondition::kJSRelationConditionLessThan;
    default:
        ASSERT_NOT_REACHED();
        return JSRelationCondition::kJSRelationConditionUndefined;
    }
}

JSRelationCondition JSValueCompareUInt64(JSContextRef ctx, JSValueRef left, uint64_t right, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return kJSRelationConditionUndefined;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    JSValue leftNumeric = toJS(globalObject, left).toNumeric(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return kJSRelationConditionUndefined;

    if (leftNumeric.isBigInt())
        return toJSRelationCondition(JSBigInt::compare(leftNumeric, right));
    ASSERT(leftNumeric.isNumber());
    return toJSRelationCondition(JSBigInt::compareToDouble(leftNumeric.asNumber(), right));
}

JSRelationCondition JSValueCompareInt64(JSContextRef ctx, JSValueRef left, int64_t right, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return kJSRelationConditionUndefined;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    JSValue leftNumeric = toJS(globalObject, left).toNumeric(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return kJSRelationConditionUndefined;

    if (leftNumeric.isBigInt())
        return toJSRelationCondition(JSBigInt::compare(leftNumeric, right));
    ASSERT(leftNumeric.isNumber());
    return toJSRelationCondition(JSBigInt::compareToDouble(leftNumeric.asNumber(), right));
}

JSRelationCondition JSValueCompareDouble(JSContextRef ctx, JSValueRef left, double right, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return kJSRelationConditionUndefined;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    JSValue leftNumeric = toJS(globalObject, left).toNumeric(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return kJSRelationConditionUndefined;

    if (leftNumeric.isBigInt())
        return toJSRelationCondition(JSBigInt::compareToDouble(leftNumeric, right));

    ASSERT(leftNumeric.isNumber());
    double leftDouble = leftNumeric.asNumber();
    if (std::isnan(leftDouble) || std::isnan(right))
        return kJSRelationConditionUndefined;
    if (leftDouble == right)
        return kJSRelationConditionEqual;
    if (leftDouble < right)
        return kJSRelationConditionLessThan;
    return kJSRelationConditionGreaterThan;
}

JSRelationCondition JSValueCompare(JSContextRef ctx, JSValueRef left, JSValueRef right, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return kJSRelationConditionUndefined;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    JSValue leftValue = toJS(globalObject, left);
    JSValue rightValue = toJS(globalObject, right);

    bool isEqual = JSValue::equal(globalObject, leftValue, rightValue);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return kJSRelationConditionUndefined;
    if (isEqual)
        return kJSRelationConditionEqual;

    bool isLessThan = jsLess<true>(globalObject, leftValue, rightValue);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return kJSRelationConditionUndefined;

    bool isGreaterThan = jsLess<true>(globalObject, rightValue, leftValue);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return kJSRelationConditionUndefined;

    ASSERT(!isLessThan || !isGreaterThan);

    // This means either the left or right has a NaN result of toPrimitiveNumeric.
    if (!isLessThan && !isGreaterThan)
        return kJSRelationConditionUndefined;

    if (isLessThan)
        return kJSRelationConditionLessThan;
    return kJSRelationConditionGreaterThan;
}

JSValueRef JSValueMakeString(JSContextRef ctx, JSStringRef string)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
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
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    JSLockHolder locker(globalObject);
    String str = string->string();
    if (str.is8Bit()) {
        LiteralParser parser(globalObject, str.span8(), StrictJSON);
        return toRef(globalObject, parser.tryLiteralParse());
    }
    LiteralParser parser(globalObject, str.span16(), StrictJSON);
    return toRef(globalObject, parser.tryLiteralParse());
}

JSStringRef JSValueCreateJSONString(JSContextRef ctx, JSValueRef apiValue, unsigned indent, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue value = toJS(globalObject, apiValue);
    String result = JSONStringify(globalObject, value, indent);
    if (exception)
        *exception = nullptr;
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;
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
    JSValue numeric = toJS(globalObject, value).toNumeric(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return PNaN;

    if (numeric.isBigInt())
        return JSBigInt::toNumber(numeric).asNumber();

    ASSERT(numeric.isNumber());
    return numeric.asNumber();
}

JSStringRef JSValueToStringCopy(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
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
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue jsValue = toJS(globalObject, value);
    
    JSObjectRef objectRef = toRef(jsValue.toObject(globalObject));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        objectRef = nullptr;
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
