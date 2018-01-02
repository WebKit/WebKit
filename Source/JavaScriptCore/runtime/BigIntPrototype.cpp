/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Error.h"
#include "JSBigInt.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSCell.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include <wtf/Assertions.h>

namespace JSC {

static EncodedJSValue JSC_HOST_CALL bigIntProtoFuncToString(ExecState*);
static EncodedJSValue JSC_HOST_CALL bigIntProtoFuncToLocaleString(ExecState*);
static EncodedJSValue JSC_HOST_CALL bigIntProtoFuncValueOf(ExecState*);

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
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsString(&vm, "BigInt"), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
}

// ------------------------------ Functions ---------------------------

static ALWAYS_INLINE JSBigInt* toThisBigIntValue(VM& vm, JSValue thisValue)
{
    if (thisValue.isCell()) {
        if (JSBigInt* bigInt = jsDynamicCast<JSBigInt*>(vm, thisValue.asCell()))
            return bigInt;
        
        if (BigIntObject* bigIntObject = jsDynamicCast<BigIntObject*>(vm, thisValue.asCell()))
            return bigIntObject->internalValue();
    }

    return nullptr;
}

EncodedJSValue JSC_HOST_CALL bigIntProtoFuncToString(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSBigInt* value = toThisBigIntValue(vm, state->thisValue());
    if (!value)
        return throwVMTypeError(state, scope, ASCIILiteral("'this' value must be a BigInt or BigIntObject"));
    
    ASSERT(value);

    int64_t radix;
    JSValue radixValue = state->argument(0);
    if (radixValue.isInt32())
        radix = radixValue.asInt32();
    else if (radixValue.isUndefined())
        radix = 10;
    else {
        radix = static_cast<int64_t>(radixValue.toInteger(state));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    if (radix < 2 || radix > 36)
        return throwVMError(state, scope, createRangeError(state, ASCIILiteral("toString() radix argument must be between 2 and 36")));

    String resultString = value->toString(*state, static_cast<int32_t>(radix));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (resultString.length() == 1)
        return JSValue::encode(vm.smallStrings.singleCharacterString(resultString[0]));

    scope.release();
    return JSValue::encode(jsNontrivialString(&vm, resultString));
}

EncodedJSValue JSC_HOST_CALL bigIntProtoFuncToLocaleString(ExecState* state)
{
    return bigIntProtoFuncToString(state);
}

EncodedJSValue JSC_HOST_CALL bigIntProtoFuncValueOf(ExecState* state)
{
    VM& vm = state->vm();
    if (JSBigInt* value = toThisBigIntValue(vm, state->thisValue()))
        return JSValue::encode(value);
    
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(state, scope, ASCIILiteral("'this' value must be a BigInt or BigIntObject"));
}

} // namespace JSC
