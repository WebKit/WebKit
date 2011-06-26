/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DFGOperations.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGRepatch.h"
#include "Interpreter.h"
#include "JSByteArray.h"
#include "JSGlobalData.h"
#include "Operations.h"

#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, register) \
    asm( \
    ".globl _" STRINGIZE(function) "\n" \
    "_" STRINGIZE(function) ":" "\n" \
        "mov (%rsp), %" STRINGIZE(register) "\n" \
        "jmp _" STRINGIZE(function) "WithReturnAddress" "\n" \
    );
#define FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(function) FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, rcx)

namespace JSC { namespace DFG {

static inline void putByVal(ExecState* exec, JSValue baseValue, uint32_t index, JSValue value)
{
    JSGlobalData* globalData = &exec->globalData();

    if (isJSArray(globalData, baseValue)) {
        JSArray* array = asArray(baseValue);
        if (array->canSetIndex(index)) {
            array->setIndex(*globalData, index, value);
            return;
        }

        array->JSArray::put(exec, index, value);
        return;
    }

    if (isJSByteArray(globalData, baseValue) && asByteArray(baseValue)->canAccessIndex(index)) {
        JSByteArray* byteArray = asByteArray(baseValue);
        // FIXME: the JITstub used to relink this to an optimized form!
        if (value.isInt32()) {
            byteArray->setIndex(index, value.asInt32());
            return;
        }

        double dValue = 0;
        if (value.getNumber(dValue)) {
            byteArray->setIndex(index, dValue);
            return;
        }
    }

    baseValue.put(exec, index, value);
}

template<bool strict>
ALWAYS_INLINE static void operationPutByValInternal(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue property = JSValue::decode(encodedProperty);
    JSValue value = JSValue::decode(encodedValue);

    if (LIKELY(property.isUInt32())) {
        putByVal(exec, baseValue, property.asUInt32(), value);
        return;
    }

    if (property.isDouble()) {
        double propertyAsDouble = property.asDouble();
        uint32_t propertyAsUInt32 = static_cast<uint32_t>(propertyAsDouble);
        if (propertyAsDouble == propertyAsUInt32) {
            putByVal(exec, baseValue, propertyAsUInt32, value);
            return;
        }
    }

    JSGlobalData* globalData = &exec->globalData();

    // Don't put to an object if toString throws an exception.
    Identifier ident(exec, property.toString(exec));
    if (!globalData->exception) {
        PutPropertySlot slot(strict);
        baseValue.put(exec, ident, value, slot);
    }
}

extern "C" {

EncodedJSValue operationConvertThis(ExecState* exec, EncodedJSValue encodedOp)
{
    return JSValue::encode(JSValue::decode(encodedOp).toThisObject(exec));
}

EncodedJSValue operationValueAdd(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    if (op1.isInt32() && op2.isInt32()) {
        int64_t result64 = static_cast<int64_t>(op1.asInt32()) + static_cast<int64_t>(op2.asInt32());
        int32_t result32 = static_cast<int32_t>(result64);
        if (LIKELY(result32 == result64))
            return JSValue::encode(jsNumber(result32));
        return JSValue::encode(jsNumber((double)result64));
    }
    
    double number1;
    double number2;
    if (op1.getNumber(number1) && op2.getNumber(number2))
        return JSValue::encode(jsNumber(number1 + number2));

    return JSValue::encode(jsAddSlowCase(exec, op1, op2));
}

EncodedJSValue operationArithAdd(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    double num1 = JSValue::decode(encodedOp1).uncheckedGetNumber();
    double num2 = JSValue::decode(encodedOp2).uncheckedGetNumber();
    return JSValue::encode(jsNumber(num1 + num2));
}

EncodedJSValue operationArithSub(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    double num1 = JSValue::decode(encodedOp1).uncheckedGetNumber();
    double num2 = JSValue::decode(encodedOp2).uncheckedGetNumber();
    return JSValue::encode(jsNumber(num1 - num2));
}

EncodedJSValue operationArithMul(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    double num1 = JSValue::decode(encodedOp1).uncheckedGetNumber();
    double num2 = JSValue::decode(encodedOp2).uncheckedGetNumber();
    return JSValue::encode(jsNumber(num1 * num2));
}

EncodedJSValue operationArithDiv(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    double num1 = JSValue::decode(encodedOp1).uncheckedGetNumber();
    double num2 = JSValue::decode(encodedOp2).uncheckedGetNumber();
    return JSValue::encode(jsNumber(num1 / num2));
}

EncodedJSValue operationArithMod(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    double num1 = JSValue::decode(encodedOp1).uncheckedGetNumber();
    double num2 = JSValue::decode(encodedOp2).uncheckedGetNumber();
    return JSValue::encode(jsNumber(fmod(num1, num2)));
}

static inline EncodedJSValue getByVal(ExecState* exec, JSCell* base, uint32_t index)
{
    JSGlobalData* globalData = &exec->globalData();

    // FIXME: the JIT used to handle these in compiled code!
    if (isJSArray(globalData, base) && asArray(base)->canGetIndex(index))
        return JSValue::encode(asArray(base)->getIndex(index));

    // FIXME: the JITstub used to relink this to an optimized form!
    if (isJSString(globalData, base) && asString(base)->canGetIndex(index))
        return JSValue::encode(asString(base)->getIndex(exec, index));

    // FIXME: the JITstub used to relink this to an optimized form!
    if (isJSByteArray(globalData, base) && asByteArray(base)->canAccessIndex(index))
        return JSValue::encode(asByteArray(base)->getIndex(exec, index));

    return JSValue::encode(JSValue(base).get(exec, index));
}

EncodedJSValue operationGetByVal(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty)
{
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue property = JSValue::decode(encodedProperty);

    if (LIKELY(baseValue.isCell())) {
        JSCell* base = baseValue.asCell();

        if (property.isUInt32()) {
            return getByVal(exec, base, property.asUInt32());
        } else if (property.isDouble()) {
            double propertyAsDouble = property.asDouble();
            uint32_t propertyAsUInt32 = static_cast<uint32_t>(propertyAsDouble);
            if (propertyAsUInt32 == propertyAsDouble)
                return getByVal(exec, base, propertyAsUInt32);
        } else if (property.isString()) {
            Identifier propertyName(exec, asString(property)->value(exec));
            PropertySlot slot(base);
            if (base->fastGetOwnPropertySlot(exec, propertyName, slot))
                return JSValue::encode(slot.getValue(exec, propertyName));
        }
    }

    Identifier ident(exec, property.toString(exec));
    return JSValue::encode(baseValue.get(exec, ident));
}

EncodedJSValue operationGetById(ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName)
{
    JSValue baseValue = JSValue::decode(encodedBase);
    PropertySlot slot(baseValue);
    return JSValue::encode(baseValue.get(exec, *propertyName, slot));
}

EncodedJSValue operationGetByIdOptimizeWithReturnAddress(ExecState*, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetByIdOptimize);
EncodedJSValue operationGetByIdOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue baseValue = JSValue::decode(encodedBase);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, *propertyName, slot);

    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    if (stubInfo.seen)
        dfgRepatchGetByID(exec, baseValue, *propertyName, slot, stubInfo);
    else
        stubInfo.seen = true;

    return JSValue::encode(result);
}

void operationPutByValStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    operationPutByValInternal<true>(exec, encodedBase, encodedProperty, encodedValue);
}

void operationPutByValNonStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    operationPutByValInternal<false>(exec, encodedBase, encodedProperty, encodedValue);
}

void operationPutByValBeyondArrayBounds(ExecState* exec, JSArray* array, int32_t index, EncodedJSValue encodedValue)
{
    // We should only get here if index is outside the existing vector.
    ASSERT(!array->canSetIndex(index));
    array->JSArray::put(exec, index, JSValue::decode(encodedValue));
}

void operationPutByIdStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
{
    PutPropertySlot slot(true);
    JSValue::decode(encodedBase).put(exec, *propertyName, JSValue::decode(encodedValue), slot);
}

void operationPutByIdNonStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
{
    PutPropertySlot slot(false);
    JSValue::decode(encodedBase).put(exec, *propertyName, JSValue::decode(encodedValue), slot);
}

void operationPutByIdDirectStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
{
    PutPropertySlot slot(true);
    JSValue::decode(encodedBase).putDirect(exec, *propertyName, JSValue::decode(encodedValue), slot);
}

void operationPutByIdDirectNonStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
{
    PutPropertySlot slot(false);
    JSValue::decode(encodedBase).putDirect(exec, *propertyName, JSValue::decode(encodedValue), slot);
}

bool operationCompareLess(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLess(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

bool operationCompareLessEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLessEq(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

bool operationCompareEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return JSValue::equal(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

bool operationCompareStrictEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return JSValue::strictEqual(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

DFGHandler lookupExceptionHandler(ExecState* exec, ReturnAddressPtr faultLocation)
{
    JSValue exceptionValue = exec->exception();
    ASSERT(exceptionValue);

    unsigned vPCIndex = exec->codeBlock()->bytecodeOffset(faultLocation);
    HandlerInfo* handler = exec->globalData().interpreter->throwException(exec, exceptionValue, vPCIndex);

    void* catchRoutine = handler ? handler->nativeCode.executableAddress() : (void*)ctiOpThrowNotCaught;
    ASSERT(catchRoutine);
    return DFGHandler(exec, catchRoutine);
}

double dfgConvertJSValueToNumber(ExecState* exec, EncodedJSValue value)
{
    return JSValue::decode(value).toNumber(exec);
}

int32_t dfgConvertJSValueToInt32(ExecState* exec, EncodedJSValue value)
{
    return JSValue::decode(value).toInt32(exec);
}

bool dfgConvertJSValueToBoolean(ExecState* exec, EncodedJSValue encodedOp)
{
    return JSValue::decode(encodedOp).toBoolean(exec);
}

} // extern "C"
} } // namespace JSC::DFG

#endif
