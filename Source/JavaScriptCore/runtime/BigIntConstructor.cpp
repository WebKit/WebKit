/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "BigIntConstructor.h"

#include "BigIntPrototype.h"
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "ParseInt.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(bigIntConstructorFuncAsUintN);
static JSC_DECLARE_HOST_FUNCTION(bigIntConstructorFuncAsIntN);

} // namespace JSC

#include "BigIntConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(BigIntConstructor);

const ClassInfo BigIntConstructor::s_info = { "Function"_s, &Base::s_info, &bigIntConstructorTable, nullptr, CREATE_METHOD_TABLE(BigIntConstructor) };

/* Source for BigIntConstructor.lut.h
@begin bigIntConstructorTable
  asUintN   bigIntConstructorFuncAsUintN   DontEnum|Function 2
  asIntN    bigIntConstructorFuncAsIntN    DontEnum|Function 2
@end
*/

static JSC_DECLARE_HOST_FUNCTION(callBigIntConstructor);
static JSC_DECLARE_HOST_FUNCTION(constructBigIntConstructor);
static JSC_DECLARE_HOST_FUNCTION(bigIntConstructorAbs);
static JSC_DECLARE_HOST_FUNCTION(bigIntConstructorCbrt);
static JSC_DECLARE_HOST_FUNCTION(bigIntConstructorMax);
static JSC_DECLARE_HOST_FUNCTION(bigIntConstructorMin);
static JSC_DECLARE_HOST_FUNCTION(bigIntConstructorPow);
static JSC_DECLARE_HOST_FUNCTION(bigIntConstructorSign);
static JSC_DECLARE_HOST_FUNCTION(bigIntConstructorSqrt);

BigIntConstructor::BigIntConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callBigIntConstructor, constructBigIntConstructor)
{
}

void BigIntConstructor::finishCreation(VM& vm, BigIntPrototype* bigIntPrototype)
{
    Base::finishCreation(vm, 1, "BigInt"_s, PropertyAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(info()));

    putDirectWithoutTransition(vm, vm.propertyNames->prototype, bigIntPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);

    if (Options::useBigIntMathMethods()) {
        JSGlobalObject* globalObject = this->globalObject();
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("abs"_s, bigIntConstructorAbs, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("cbrt"_s, bigIntConstructorCbrt, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("max"_s, bigIntConstructorMax, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("min"_s, bigIntConstructorMin, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("pow"_s, bigIntConstructorPow, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("sign"_s, bigIntConstructorSign, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("sqrt"_s, bigIntConstructorSqrt, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    }
}

// ------------------------------ Functions ---------------------------

JSC_DEFINE_HOST_FUNCTION(callBigIntConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue value = callFrame->argument(0);
    JSValue primitive = value.toPrimitive(globalObject, PreferNumber);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (primitive.isInt32()) {
#if USE(BIGINT32)
        return JSValue::encode(jsBigInt32(primitive.asInt32()));
#else
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::createFrom(globalObject, primitive.asInt32())));
#endif
    }

    if (primitive.isDouble()) {
        double number = primitive.asDouble();
        if (!isInteger(number))
            return throwVMError(globalObject, scope, createRangeError(globalObject, "Not an integer"_s));
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::makeHeapBigIntOrBigInt32(globalObject, primitive.asDouble())));
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(primitive.toBigInt(globalObject)));
}

JSC_DEFINE_HOST_FUNCTION(constructBigIntConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMError(globalObject, scope, createNotAConstructorError(globalObject, callFrame->jsCallee()));
}

JSC_DEFINE_HOST_FUNCTION(bigIntConstructorFuncAsUintN, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    uint64_t numberOfBits = callFrame->argument(0).toIndex(globalObject, "number of bits"_s);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue bigInt = callFrame->argument(1).toBigInt(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

#if USE(BIGINT32)
    if (bigInt.isBigInt32())
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::asUintN(globalObject, numberOfBits, bigInt.bigInt32AsInt32())));
#endif

    ASSERT(bigInt.isHeapBigInt());
    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::asUintN(globalObject, numberOfBits, bigInt.asHeapBigInt())));
}

JSC_DEFINE_HOST_FUNCTION(bigIntConstructorFuncAsIntN, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    uint64_t numberOfBits = callFrame->argument(0).toIndex(globalObject, "number of bits"_s);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue bigInt = callFrame->argument(1).toBigInt(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

#if USE(BIGINT32)
    if (bigInt.isBigInt32())
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::asIntN(globalObject, numberOfBits, bigInt.bigInt32AsInt32())));
#endif

    ASSERT(bigInt.isHeapBigInt());
    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::asIntN(globalObject, numberOfBits, bigInt.asHeapBigInt())));
}

// https://tc39.es/proposal-bigint-math/#sec-bigint.abs
JSC_DEFINE_HOST_FUNCTION(bigIntConstructorAbs, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue bigIntValue = callFrame->argument(0);
    if (!bigIntValue.isBigInt()) {
        throwTypeError(globalObject, scope, "BigInt.abs requires the argument to be a BigInt"_s);
        return { };
    }

#if USE(BIGINT32)
    if (bigIntValue.isBigInt32())
        return JSValue::encode(jsBigInt32(std::abs(bigIntValue.bigInt32AsInt32())));
#endif

    JSBigInt* bigInt = bigIntValue.asHeapBigInt();
    if (bigInt->sign())
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::unaryMinus(globalObject, bigInt)));
    return JSValue::encode(bigInt);
}

// https://tc39.es/proposal-bigint-math/#sec-bigint.cbrt
JSC_DEFINE_HOST_FUNCTION(bigIntConstructorCbrt, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue bigIntValue = callFrame->argument(0);
    if (!bigIntValue.isBigInt()) {
        throwTypeError(globalObject, scope, "BigInt.cbrt requires the argument to be a BigInt"_s);
        return { };
    }

#if USE(BIGINT32)
    if (bigIntValue.isBigInt32())
        return JSValue::encode(jsBigInt32(static_cast<int32_t>(std::cbrt(static_cast<double>(bigIntValue.bigInt32AsInt32())))));
#endif

    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::cbrt(globalObject, bigIntValue.asHeapBigInt())));
}

// https://tc39.es/proposal-bigint-math/#sec-bigint.max
JSC_DEFINE_HOST_FUNCTION(bigIntConstructorMax, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue result = callFrame->argument(0);
    if (!result.isBigInt()) {
        throwTypeError(globalObject, scope, "BigInt.max requires every argument to be a BigInt"_s);
        return { };
    }

    for (unsigned i = 1, count = callFrame->argumentCount(); i < count; ++i) {
        JSValue value = callFrame->uncheckedArgument(i);
        if (!value.isBigInt()) {
            throwTypeError(globalObject, scope, "BigInt.max requires every argument to be a BigInt"_s);
            return { };
        }

        if (JSBigInt::compare(value, result) == JSBigInt::ComparisonResult::GreaterThan)
            result = value;
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(result));
}

// https://tc39.es/proposal-bigint-math/#sec-bigint.min
JSC_DEFINE_HOST_FUNCTION(bigIntConstructorMin, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue result = callFrame->argument(0);
    if (!result.isBigInt()) {
        throwTypeError(globalObject, scope, "BigInt.min requires every argument to be a BigInt"_s);
        return { };
    }

    for (unsigned i = 1, count = callFrame->argumentCount(); i < count; ++i) {
        JSValue value = callFrame->uncheckedArgument(i);
        if (!value.isBigInt()) {
            throwTypeError(globalObject, scope, "BigInt.min requires every argument to be a BigInt"_s);
            return { };
        }

        if (JSBigInt::compare(value, result) == JSBigInt::ComparisonResult::LessThan)
            result = value;
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(result));
}

// https://tc39.es/proposal-bigint-math/#sec-bigint.pow
JSC_DEFINE_HOST_FUNCTION(bigIntConstructorPow, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = callFrame->argument(0);
    if (!base.isBigInt()) {
        throwTypeError(globalObject, scope, "BigInt.pow requires the first argument to be a BigInt"_s);
        return { };
    }

    JSValue exponent = callFrame->argument(1);
    if (!exponent.isBigInt()) {
        throwTypeError(globalObject, scope, "BigInt.pow requires the second argument to be a BigInt"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(jsPow(globalObject, base, exponent)));
}

// https://tc39.es/proposal-bigint-math/#sec-bigint.sign
JSC_DEFINE_HOST_FUNCTION(bigIntConstructorSign, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue bigIntValue = callFrame->argument(0);
    if (!bigIntValue.isBigInt()) {
        throwTypeError(globalObject, scope, "BigInt.sign requires the argument to be a BigInt"_s);
        return { };
    }

#if USE(BIGINT32)
    if (bigIntValue.isBigInt32()) {
        int32_t value = bigIntValue.bigInt32AsInt32();
        if (!value)
            return JSValue::encode(bigIntValue);
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::makeHeapBigIntOrBigInt32(globalObject, static_cast<int64_t>(value < 0 ? -1 : 1))));
    }
#endif

    JSBigInt* bigInt = bigIntValue.asHeapBigInt();
    if (bigInt->isZero())
        return JSValue::encode(bigIntValue);
    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::makeHeapBigIntOrBigInt32(globalObject, static_cast<int64_t>(bigInt->sign() ? -1 : 1))));
}

// https://tc39.es/proposal-bigint-math/#sec-bigint.sqrt
JSC_DEFINE_HOST_FUNCTION(bigIntConstructorSqrt, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue bigIntValue = callFrame->argument(0);
    if (!bigIntValue.isBigInt()) {
        throwTypeError(globalObject, scope, "BigInt.sqrt requires the argument to be a positive BigInt"_s);
        return { };
    }

#if USE(BIGINT32)
    if (bigIntValue.isBigInt32()) {
        int32_t value = bigIntValue.bigInt32AsInt32();
        if (value < 0) {
            throwRangeError(globalObject, scope, "BigInt.sqrt requires the argument to be a positive BigInt"_s);
            return { };
        }
        return JSValue::encode(jsBigInt32(static_cast<int32_t>(std::sqrt(static_cast<double>(value)))));
    }
#endif

    JSBigInt* bigInt = bigIntValue.asHeapBigInt();
    if (bigInt->sign()) {
        throwRangeError(globalObject, scope, "BigInt.sqrt requires the argument to be a positive BigInt"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::sqrt(globalObject, bigInt)));
}

} // namespace JSC
