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

static EncodedJSValue JSC_HOST_CALL bigIntConstructorFuncAsUintN(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL bigIntConstructorFuncAsIntN(JSGlobalObject*, CallFrame*);

} // namespace JSC

#include "BigIntConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(BigIntConstructor);

const ClassInfo BigIntConstructor::s_info = { "Function", &Base::s_info, &bigIntConstructorTable, nullptr, CREATE_METHOD_TABLE(BigIntConstructor) };

/* Source for BigIntConstructor.lut.h
@begin bigIntConstructorTable
  asUintN   bigIntConstructorFuncAsUintN   DontEnum|Function 2
  asIntN    bigIntConstructorFuncAsIntN    DontEnum|Function 2
@end
*/

static EncodedJSValue JSC_HOST_CALL callBigIntConstructor(JSGlobalObject*, CallFrame*);

BigIntConstructor::BigIntConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callBigIntConstructor, nullptr)
{
}

void BigIntConstructor::finishCreation(VM& vm, BigIntPrototype* bigIntPrototype)
{
    Base::finishCreation(vm, "BigInt"_s, NameAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(vm, info()));

    putDirectWithoutTransition(vm, vm.propertyNames->prototype, bigIntPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

// ------------------------------ Functions ---------------------------

JSValue toBigInt(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue primitive = argument.toPrimitive(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    
    if (primitive.isBigInt())
        return primitive;

    if (primitive.isBoolean()) {
#if USE(BIGINT32)
        return jsBigInt32(primitive.asBoolean());
#else
        RELEASE_AND_RETURN(scope, JSBigInt::createFrom(globalObject, primitive.asBoolean()));
#endif
    }

    if (primitive.isString()) {
        scope.release();
        return toStringView(globalObject, primitive, [&] (StringView view) {
            return JSBigInt::parseInt(globalObject, view);
        });
    }

    ASSERT(primitive.isUndefinedOrNull() || primitive.isNumber() || primitive.isSymbol());
    throwTypeError(globalObject, scope, "Invalid argument type in ToBigInt operation"_s);
    return jsUndefined();
}

static EncodedJSValue JSC_HOST_CALL callBigIntConstructor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue value = callFrame->argument(0);
    JSValue primitive = value.toPrimitive(globalObject);
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

    RELEASE_AND_RETURN(scope, JSValue::encode(toBigInt(globalObject, primitive)));
}

EncodedJSValue JSC_HOST_CALL bigIntConstructorFuncAsUintN(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto numberOfBits = callFrame->argument(0).toIndex(globalObject, "number of bits");
    RETURN_IF_EXCEPTION(scope, { });

    JSValue bigInt = toBigInt(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

#if USE(BIGINT32)
    if (bigInt.isBigInt32())
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::asUintN(globalObject, numberOfBits, bigInt.bigInt32AsInt32())));
#endif

    ASSERT(bigInt.isHeapBigInt());
    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::asUintN(globalObject, numberOfBits, bigInt.asHeapBigInt())));
}

EncodedJSValue JSC_HOST_CALL bigIntConstructorFuncAsIntN(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto numberOfBits = callFrame->argument(0).toIndex(globalObject, "number of bits");
    RETURN_IF_EXCEPTION(scope, { });

    JSValue bigInt = toBigInt(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

#if USE(BIGINT32)
    if (bigInt.isBigInt32())
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::asIntN(globalObject, numberOfBits, bigInt.bigInt32AsInt32())));
#endif

    ASSERT(bigInt.isHeapBigInt());
    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::asIntN(globalObject, numberOfBits, bigInt.asHeapBigInt())));
}

} // namespace JSC
