/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2002-2018 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "CallFrame.h"
#include "ExceptionHelpers.h"
#include "JSBigInt.h"
#include "JSCJSValue.h"
#include <wtf/Variant.h>

namespace JSC {

#define InvalidPrototypeChain (std::numeric_limits<size_t>::max())

NEVER_INLINE JSValue jsAddSlowCase(CallFrame*, JSValue, JSValue);
JSValue jsTypeStringForValue(CallFrame*, JSValue);
JSValue jsTypeStringForValue(VM&, JSGlobalObject*, JSValue);
bool jsIsObjectTypeOrNull(CallFrame*, JSValue);
size_t normalizePrototypeChain(CallFrame*, JSCell*, bool& sawPolyProto);

ALWAYS_INLINE JSString* jsString(ExecState* exec, JSString* s1, JSString* s2)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length1 = s1->length();
    if (!length1)
        return s2;
    unsigned length2 = s2->length();
    if (!length2)
        return s1;
    static_assert(JSString::MaxLength == std::numeric_limits<int32_t>::max(), "");
    if (sumOverflows<int32_t>(length1, length2)) {
        throwOutOfMemoryError(exec, scope);
        return nullptr;
    }

    return JSRopeString::create(vm, s1, s2);
}

ALWAYS_INLINE JSString* jsString(ExecState* exec, JSString* s1, JSString* s2, JSString* s3)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length1 = s1->length();
    if (!length1)
        RELEASE_AND_RETURN(scope, jsString(exec, s2, s3));

    unsigned length2 = s2->length();
    if (!length2)
        RELEASE_AND_RETURN(scope, jsString(exec, s1, s3));

    unsigned length3 = s3->length();
    if (!length3)
        RELEASE_AND_RETURN(scope, jsString(exec, s1, s2));

    static_assert(JSString::MaxLength == std::numeric_limits<int32_t>::max(), "");
    if (sumOverflows<int32_t>(length1, length2, length3)) {
        throwOutOfMemoryError(exec, scope);
        return nullptr;
    }
    return JSRopeString::create(vm, s1, s2, s3);
}

ALWAYS_INLINE JSString* jsString(ExecState* exec, const String& u1, const String& u2, const String& u3)
{
    VM* vm = &exec->vm();
    auto scope = DECLARE_THROW_SCOPE(*vm);

    unsigned length1 = u1.length();
    unsigned length2 = u2.length();
    unsigned length3 = u3.length();
    ASSERT(length1 <= JSString::MaxLength);
    ASSERT(length2 <= JSString::MaxLength);
    ASSERT(length3 <= JSString::MaxLength);

    if (!length1)
        RELEASE_AND_RETURN(scope, jsString(exec, jsString(vm, u2), jsString(vm, u3)));

    if (!length2)
        RELEASE_AND_RETURN(scope, jsString(exec, jsString(vm, u1), jsString(vm, u3)));

    if (!length3)
        RELEASE_AND_RETURN(scope, jsString(exec, jsString(vm, u1), jsString(vm, u2)));

    static_assert(JSString::MaxLength == std::numeric_limits<int32_t>::max(), "");
    if (sumOverflows<int32_t>(length1, length2, length3)) {
        throwOutOfMemoryError(exec, scope);
        return nullptr;
    }

    return JSRopeString::create(*vm, jsString(vm, u1), jsString(vm, u2), jsString(vm, u3));
}

ALWAYS_INLINE JSValue jsStringFromRegisterArray(ExecState* exec, Register* strings, unsigned count)
{
    VM* vm = &exec->vm();
    auto scope = DECLARE_THROW_SCOPE(*vm);
    JSRopeString::RopeBuilder<RecordOverflow> ropeBuilder(*vm);

    for (unsigned i = 0; i < count; ++i) {
        JSValue v = strings[-static_cast<int>(i)].jsValue();
        JSString* string = v.toString(exec);
        RETURN_IF_EXCEPTION(scope, { });
        if (!ropeBuilder.append(string))
            return throwOutOfMemoryError(exec, scope);
    }

    return ropeBuilder.release();
}

ALWAYS_INLINE JSValue jsStringFromArguments(ExecState* exec, JSValue thisValue)
{
    VM* vm = &exec->vm();
    auto scope = DECLARE_THROW_SCOPE(*vm);
    JSRopeString::RopeBuilder<RecordOverflow> ropeBuilder(*vm);
    JSString* str = thisValue.toString(exec);
    RETURN_IF_EXCEPTION(scope, { });
    ropeBuilder.append(str);

    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        JSValue v = exec->argument(i);
        JSString* str = v.toString(exec);
        RETURN_IF_EXCEPTION(scope, { });
        if (UNLIKELY(!ropeBuilder.append(str)))
            return throwOutOfMemoryError(exec, scope);
    }

    return ropeBuilder.release();
}

ALWAYS_INLINE bool bigIntCompareResult(JSBigInt::ComparisonResult comparisonResult, JSBigInt::ComparisonMode comparisonMode)
{
    if (comparisonMode == JSBigInt::ComparisonMode::LessThan)
        return comparisonResult == JSBigInt::ComparisonResult::LessThan;

    ASSERT(comparisonMode == JSBigInt::ComparisonMode::LessThanOrEqual);
    return comparisonResult == JSBigInt::ComparisonResult::LessThan || comparisonResult == JSBigInt::ComparisonResult::Equal;
}

ALWAYS_INLINE bool bigIntCompare(CallFrame* callFrame, JSValue v1, JSValue v2, JSBigInt::ComparisonMode comparisonMode)
{
    ASSERT(v1.isBigInt() || v2.isBigInt());
    ASSERT(v1.isPrimitive() && v2.isPrimitive());

    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (v1.isBigInt() && v2.isBigInt())
        return bigIntCompareResult(JSBigInt::compare(asBigInt(v1), asBigInt(v2)), comparisonMode);
    
    if (v1.isBigInt()) {
        JSValue primValue = v2;
        if (primValue.isString()) {
            JSBigInt* bigIntValue = JSBigInt::stringToBigInt(callFrame, asString(primValue)->value(callFrame));
            RETURN_IF_EXCEPTION(scope, false);
            if (!bigIntValue)
                return false;

            return bigIntCompareResult(JSBigInt::compare(asBigInt(v1), bigIntValue), comparisonMode);
        }

        if (primValue.isBigInt())
            return bigIntCompareResult(JSBigInt::compare(asBigInt(v1), asBigInt(primValue)), comparisonMode);

        double numberValue = primValue.toNumber(callFrame);
        RETURN_IF_EXCEPTION(scope, false);
        return bigIntCompareResult(JSBigInt::compareToDouble(asBigInt(v1), numberValue), comparisonMode);
    }
    
    JSValue primValue = v1;
    if (primValue.isString()) {
        JSBigInt* bigIntValue = JSBigInt::stringToBigInt(callFrame, asString(primValue)->value(callFrame));
        RETURN_IF_EXCEPTION(scope, false);
        if (!bigIntValue)
            return false;

        return bigIntCompareResult(JSBigInt::compare(bigIntValue, asBigInt(v2)), comparisonMode);
    }
    
    if (primValue.isBigInt())
        return bigIntCompareResult(JSBigInt::compare(asBigInt(primValue), asBigInt(v2)), comparisonMode);
    
    double numberValue = primValue.toNumber(callFrame);
    RETURN_IF_EXCEPTION(scope, false);

    // Here we check inverted because BigInt is the v2
    JSBigInt::ComparisonResult comparisonResult = JSBigInt::compareToDouble(asBigInt(v2), numberValue);
    if (comparisonMode == JSBigInt::ComparisonMode::LessThan)
        return comparisonResult == JSBigInt::ComparisonResult::GreaterThan;

    return comparisonResult == JSBigInt::ComparisonResult::GreaterThan || comparisonResult == JSBigInt::ComparisonResult::Equal;
}

ALWAYS_INLINE bool toPrimitiveNumeric(CallFrame* callFrame, JSValue v, JSValue& p, double& n)
{
    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    p = v.toPrimitive(callFrame, PreferNumber);
    RETURN_IF_EXCEPTION(scope, false);
    if (p.isBigInt())
        return true;
    
    n = p.toNumber(callFrame);
    RETURN_IF_EXCEPTION(scope, false);
    return !p.isString();
}

// See ES5 11.8.1/11.8.2/11.8.5 for definition of leftFirst, this value ensures correct
// evaluation ordering for argument conversions for '<' and '>'. For '<' pass the value
// true, for leftFirst, for '>' pass the value false (and reverse operand order).
template<bool leftFirst>
ALWAYS_INLINE bool jsLess(CallFrame* callFrame, JSValue v1, JSValue v2)
{
    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (v1.isInt32() && v2.isInt32())
        return v1.asInt32() < v2.asInt32();

    if (v1.isNumber() && v2.isNumber())
        return v1.asNumber() < v2.asNumber();

    if (isJSString(v1) && isJSString(v2))
        return codePointCompareLessThan(asString(v1)->value(callFrame), asString(v2)->value(callFrame));

    double n1;
    double n2;
    JSValue p1;
    JSValue p2;
    bool wasNotString1;
    bool wasNotString2;
    if (leftFirst) {
        wasNotString1 = toPrimitiveNumeric(callFrame, v1, p1, n1);
        RETURN_IF_EXCEPTION(scope, false);
        wasNotString2 = toPrimitiveNumeric(callFrame, v2, p2, n2);
    } else {
        wasNotString2 = toPrimitiveNumeric(callFrame, v2, p2, n2);
        RETURN_IF_EXCEPTION(scope, false);
        wasNotString1 = toPrimitiveNumeric(callFrame, v1, p1, n1);
    }
    RETURN_IF_EXCEPTION(scope, false);

    if (wasNotString1 | wasNotString2) {
        if (p1.isBigInt() || p2.isBigInt())
            RELEASE_AND_RETURN(scope, bigIntCompare(callFrame, p1, p2, JSBigInt::ComparisonMode::LessThan));

        return n1 < n2;
    }

    return codePointCompareLessThan(asString(p1)->value(callFrame), asString(p2)->value(callFrame));
}

// See ES5 11.8.3/11.8.4/11.8.5 for definition of leftFirst, this value ensures correct
// evaluation ordering for argument conversions for '<=' and '=>'. For '<=' pass the
// value true, for leftFirst, for '=>' pass the value false (and reverse operand order).
template<bool leftFirst>
ALWAYS_INLINE bool jsLessEq(CallFrame* callFrame, JSValue v1, JSValue v2)
{
    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (v1.isInt32() && v2.isInt32())
        return v1.asInt32() <= v2.asInt32();

    if (v1.isNumber() && v2.isNumber())
        return v1.asNumber() <= v2.asNumber();

    if (isJSString(v1) && isJSString(v2))
        return !codePointCompareLessThan(asString(v2)->value(callFrame), asString(v1)->value(callFrame));

    double n1;
    double n2;
    JSValue p1;
    JSValue p2;
    bool wasNotString1;
    bool wasNotString2;
    if (leftFirst) {
        wasNotString1 = toPrimitiveNumeric(callFrame, v1, p1, n1);
        RETURN_IF_EXCEPTION(scope, false);
        wasNotString2 = toPrimitiveNumeric(callFrame, v2, p2, n2);
    } else {
        wasNotString2 = toPrimitiveNumeric(callFrame, v2, p2, n2);
        RETURN_IF_EXCEPTION(scope, false);
        wasNotString1 = toPrimitiveNumeric(callFrame, v1, p1, n1);
    }
    RETURN_IF_EXCEPTION(scope, false);

    if (wasNotString1 | wasNotString2) {
        if (p1.isBigInt() || p2.isBigInt())
            RELEASE_AND_RETURN(scope, bigIntCompare(callFrame, p1, p2, JSBigInt::ComparisonMode::LessThanOrEqual));

        return n1 <= n2;
    }
    return !codePointCompareLessThan(asString(p2)->value(callFrame), asString(p1)->value(callFrame));
}

// Fast-path choices here are based on frequency data from SunSpider:
//    <times> Add case: <t1> <t2>
//    ---------------------------
//    5626160 Add case: 3 3 (of these, 3637690 are for immediate values)
//    247412  Add case: 5 5
//    20900   Add case: 5 6
//    13962   Add case: 5 3
//    4000    Add case: 3 5

ALWAYS_INLINE JSValue jsAdd(CallFrame* callFrame, JSValue v1, JSValue v2)
{
    if (v1.isNumber() && v2.isNumber())
        return jsNumber(v1.asNumber() + v2.asNumber());
        
    if (v1.isString() && !v2.isObject())
        return jsString(callFrame, asString(v1), v2.toString(callFrame));

    // All other cases are pretty uncommon
    return jsAddSlowCase(callFrame, v1, v2);
}

ALWAYS_INLINE JSValue jsSub(ExecState* exec, JSValue v1, JSValue v2)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto leftNumeric = v1.toNumeric(exec);
    RETURN_IF_EXCEPTION(scope, { });
    auto rightNumeric = v2.toNumeric(exec);
    RETURN_IF_EXCEPTION(scope, { });

    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric))
            return JSBigInt::sub(vm, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));

        return throwTypeError(exec, scope, "Invalid mix of BigInt and other type in subtraction."_s);
    }

    return jsNumber(WTF::get<double>(leftNumeric) - WTF::get<double>(rightNumeric));
}

ALWAYS_INLINE JSValue jsMul(ExecState* state, JSValue v1, JSValue v2)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Variant<JSBigInt*, double> leftNumeric = v1.toNumeric(state);
    RETURN_IF_EXCEPTION(scope, { });
    Variant<JSBigInt*, double> rightNumeric = v2.toNumeric(state);
    RETURN_IF_EXCEPTION(scope, { });

    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric))
            return JSBigInt::multiply(state, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));

        throwTypeError(state, scope, "Invalid mix of BigInt and other type in multiplication."_s);
        return { };
    }

    double leftValue =  WTF::get<double>(leftNumeric);
    double rightValue =  WTF::get<double>(rightNumeric);
    return jsNumber(leftValue * rightValue);
}

inline bool scribbleFreeCells()
{
    return !ASSERT_DISABLED || Options::scribbleFreeCells();
}

#define SCRIBBLE_WORD static_cast<intptr_t>(0xbadbeef0)

inline bool isScribbledValue(JSValue value)
{
    return JSValue::encode(value) == JSValue::encode(bitwise_cast<JSCell*>(SCRIBBLE_WORD));
}

inline void scribble(void* base, size_t size)
{
    for (size_t i = size / sizeof(EncodedJSValue); i--;) {
        // Use a 16-byte aligned value to ensure that it passes the cell check.
        static_cast<EncodedJSValue*>(base)[i] = JSValue::encode(bitwise_cast<JSCell*>(SCRIBBLE_WORD));
    }
}

} // namespace JSC
