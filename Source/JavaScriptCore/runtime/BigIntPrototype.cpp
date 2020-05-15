/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BigIntPrototype.h"

#include "BigIntObject.h"
#include "Error.h"
#include "IntegrityInlines.h"
#include "IntlNumberFormat.h"
#include "JSBigInt.h"
#include "JSCast.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "NumberPrototype.h"
#include <wtf/Assertions.h>

namespace JSC {

static EncodedJSValue JSC_HOST_CALL bigIntProtoFuncToString(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL bigIntProtoFuncToLocaleString(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL bigIntProtoFuncValueOf(JSGlobalObject*, CallFrame*);

}

#include "BigIntPrototype.lut.h"

namespace JSC {

const ClassInfo BigIntPrototype::s_info = { "BigInt", &Base::s_info, &bigIntPrototypeTable, nullptr, CREATE_METHOD_TABLE(BigIntPrototype) };

/* Source for BigIntPrototype.lut.h
@begin bigIntPrototypeTable
  toString          bigIntProtoFuncToString         DontEnum|Function 0
  toLocaleString    bigIntProtoFuncToLocaleString   DontEnum|Function 0
  valueOf           bigIntProtoFuncValueOf          DontEnum|Function 0
@end
*/

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(BigIntPrototype);

BigIntPrototype::BigIntPrototype(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void BigIntPrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// ------------------------------ Functions ---------------------------

static ALWAYS_INLINE JSBigInt* toThisBigIntValue(VM& vm, JSValue thisValue)
{
#if USE(BIGINT32)
    // FIXME: heap-allocating all big ints is inneficient, but re-implementing toString for small BigInts is enough work that I'm deferring it to a later patch.
    if (thisValue.isBigInt32())
        return JSBigInt::createFrom(vm, thisValue.bigInt32AsInt32());
#endif

    if (thisValue.isCell()) {
        if (JSBigInt* bigInt = jsDynamicCast<JSBigInt*>(vm, thisValue.asCell()))
            return bigInt;

        if (BigIntObject* bigIntObject = jsDynamicCast<BigIntObject*>(vm, thisValue.asCell())) {
            JSValue bigInt = bigIntObject->internalValue();
#if USE(BIGINT32)
            if (bigInt.isBigInt32())
                return JSBigInt::createFrom(vm, bigInt.bigInt32AsInt32());
#endif
            return bigInt.asHeapBigInt();
        }
    }

    return nullptr;
}

EncodedJSValue JSC_HOST_CALL bigIntProtoFuncToString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSBigInt* value = toThisBigIntValue(vm, callFrame->thisValue());
    if (!value)
        return throwVMTypeError(globalObject, scope, "'this' value must be a BigInt or BigIntObject"_s);

    ASSERT(value);

    Integrity::auditStructureID(vm, value->structureID());
    int32_t radix = extractToStringRadixArgument(globalObject, callFrame->argument(0), scope);
    RETURN_IF_EXCEPTION(scope, { });

    String resultString = value->toString(globalObject, radix);
    RETURN_IF_EXCEPTION(scope, { });
    scope.release();
    if (resultString.length() == 1)
        return JSValue::encode(vm.smallStrings.singleCharacterString(resultString[0]));

    return JSValue::encode(jsNontrivialString(vm, resultString));
}

// FIXME: this function should introduce the right separators for thousands and similar things.
EncodedJSValue JSC_HOST_CALL bigIntProtoFuncToLocaleString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSBigInt* value = toThisBigIntValue(vm, callFrame->thisValue());
    if (!value)
        return throwVMTypeError(globalObject, scope, "'this' value must be a BigInt or BigIntObject"_s);

    auto* numberFormat = IntlNumberFormat::create(vm, globalObject->numberFormatStructure());
    numberFormat->initializeNumberFormat(globalObject, callFrame->argument(0), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(numberFormat->format(globalObject, value)));
}

EncodedJSValue JSC_HOST_CALL bigIntProtoFuncValueOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSBigInt* value = toThisBigIntValue(vm, callFrame->thisValue());
    if (!value)
        return throwVMTypeError(globalObject, scope, "'this' value must be a BigInt or BigIntObject"_s);

    Integrity::auditStructureID(vm, value->structureID());
    return JSValue::encode(value);
}

} // namespace JSC
