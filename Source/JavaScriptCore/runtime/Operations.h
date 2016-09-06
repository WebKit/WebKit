/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2002, 2005-2009, 2013-2014, 2016 Apple Inc. All rights reserved.
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

#ifndef Operations_h
#define Operations_h

#include "CallFrame.h"
#include "ExceptionHelpers.h"
#include "JSCJSValue.h"

namespace JSC {

#define InvalidPrototypeChain (std::numeric_limits<size_t>::max())

NEVER_INLINE JSValue jsAddSlowCase(CallFrame*, JSValue, JSValue);
JSValue jsTypeStringForValue(CallFrame*, JSValue);
JSValue jsTypeStringForValue(VM&, JSGlobalObject*, JSValue);
bool jsIsObjectTypeOrNull(CallFrame*, JSValue);
bool jsIsFunctionType(JSValue);
size_t normalizePrototypeChain(CallFrame*, Structure*);

ALWAYS_INLINE JSValue jsString(ExecState* exec, JSString* s1, JSString* s2)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    int32_t length1 = s1->length();
    if (!length1)
        return s2;
    int32_t length2 = s2->length();
    if (!length2)
        return s1;
    if (sumOverflows<int32_t>(length1, length2))
        return throwOutOfMemoryError(exec, scope);

    return JSRopeString::create(vm, s1, s2);
}

ALWAYS_INLINE JSValue jsString(ExecState* exec, const String& u1, const String& u2, const String& u3)
{
    VM* vm = &exec->vm();
    auto scope = DECLARE_THROW_SCOPE(*vm);

    int32_t length1 = u1.length();
    int32_t length2 = u2.length();
    int32_t length3 = u3.length();
    
    if (length1 < 0 || length2 < 0 || length3 < 0)
        return throwOutOfMemoryError(exec, scope);
    
    if (!length1)
        return jsString(exec, jsString(vm, u2), jsString(vm, u3));
    if (!length2)
        return jsString(exec, jsString(vm, u1), jsString(vm, u3));
    if (!length3)
        return jsString(exec, jsString(vm, u1), jsString(vm, u2));

    if (sumOverflows<int32_t>(length1, length2, length3))
        return throwOutOfMemoryError(exec, scope);

    return JSRopeString::create(exec->vm(), jsString(vm, u1), jsString(vm, u2), jsString(vm, u3));
}

ALWAYS_INLINE JSValue jsStringFromRegisterArray(ExecState* exec, Register* strings, unsigned count)
{
    VM* vm = &exec->vm();
    auto scope = DECLARE_THROW_SCOPE(*vm);
    JSRopeString::RopeBuilder ropeBuilder(*vm);

    for (unsigned i = 0; i < count; ++i) {
        JSValue v = strings[-static_cast<int>(i)].jsValue();
        if (!ropeBuilder.append(v.toString(exec)))
            return throwOutOfMemoryError(exec, scope);
    }

    return ropeBuilder.release();
}

ALWAYS_INLINE JSValue jsStringFromArguments(ExecState* exec, JSValue thisValue)
{
    VM* vm = &exec->vm();
    auto scope = DECLARE_THROW_SCOPE(*vm);
    JSRopeString::RopeBuilder ropeBuilder(*vm);
    ropeBuilder.append(thisValue.toString(exec));

    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        JSValue v = exec->argument(i);
        if (!ropeBuilder.append(v.toString(exec)))
            return throwOutOfMemoryError(exec, scope);
    }

    return ropeBuilder.release();
}

// See ES5 11.8.1/11.8.2/11.8.5 for definition of leftFirst, this value ensures correct
// evaluation ordering for argument conversions for '<' and '>'. For '<' pass the value
// true, for leftFirst, for '>' pass the value false (and reverse operand order).
template<bool leftFirst>
ALWAYS_INLINE bool jsLess(CallFrame* callFrame, JSValue v1, JSValue v2)
{
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
        wasNotString1 = v1.getPrimitiveNumber(callFrame, n1, p1);
        wasNotString2 = v2.getPrimitiveNumber(callFrame, n2, p2);
    } else {
        wasNotString2 = v2.getPrimitiveNumber(callFrame, n2, p2);
        wasNotString1 = v1.getPrimitiveNumber(callFrame, n1, p1);
    }

    if (wasNotString1 | wasNotString2)
        return n1 < n2;
    return codePointCompareLessThan(asString(p1)->value(callFrame), asString(p2)->value(callFrame));
}

// See ES5 11.8.3/11.8.4/11.8.5 for definition of leftFirst, this value ensures correct
// evaluation ordering for argument conversions for '<=' and '=>'. For '<=' pass the
// value true, for leftFirst, for '=>' pass the value false (and reverse operand order).
template<bool leftFirst>
ALWAYS_INLINE bool jsLessEq(CallFrame* callFrame, JSValue v1, JSValue v2)
{
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
        wasNotString1 = v1.getPrimitiveNumber(callFrame, n1, p1);
        wasNotString2 = v2.getPrimitiveNumber(callFrame, n2, p2);
    } else {
        wasNotString2 = v2.getPrimitiveNumber(callFrame, n2, p2);
        wasNotString1 = v1.getPrimitiveNumber(callFrame, n1, p1);
    }

    if (wasNotString1 | wasNotString2)
        return n1 <= n2;
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

inline bool scribbleFreeCells()
{
    return !ASSERT_DISABLED || Options::scribbleFreeCells();
}

inline void scribble(void* base, size_t size)
{
    for (size_t i = size / sizeof(EncodedJSValue); i--;) {
        // Use a 16-byte aligned value to ensure that it passes the cell check.
        static_cast<EncodedJSValue*>(base)[i] = JSValue::encode(
            bitwise_cast<JSCell*>(static_cast<intptr_t>(0xbadbeef0)));
    }
}

} // namespace JSC

#endif // Operations_h
