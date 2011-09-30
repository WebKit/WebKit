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


#if OS(DARWIN) || (OS(WINDOWS) && CPU(X86))
#define SYMBOL_STRING(name) "_" #name
#else
#define SYMBOL_STRING(name) #name
#endif

#if CPU(X86_64)
#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, register) \
    asm( \
    ".globl " SYMBOL_STRING(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "mov (%rsp), %" STRINGIZE(register) "\n" \
        "jmp " SYMBOL_STRING(function) "WithReturnAddress" "\n" \
    );
#elif CPU(X86)
#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, register) \
    asm( \
    ".globl " SYMBOL_STRING(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "push (%esp)\n" \
        "jmp " SYMBOL_STRING(function) "WithReturnAddress" "\n" \
    );
#endif
#define FUNCTION_WRAPPER_WITH_ARG2_RETURN_ADDRESS(function) FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, rsi)
#define FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(function) FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, rcx)
#define FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(function) FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, r8)

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
ALWAYS_INLINE static void DFG_OPERATION operationPutByValInternal(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
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

EncodedJSValue DFG_OPERATION operationConvertThis(ExecState* exec, EncodedJSValue encodedOp)
{
    return JSValue::encode(JSValue::decode(encodedOp).toThisObject(exec));
}

EncodedJSValue DFG_OPERATION operationCreateThis(ExecState* exec, EncodedJSValue encodedOp)
{
    JSFunction* constructor = asFunction(exec->callee());
    
#if !ASSERT_DISABLED
    ConstructData constructData;
    ASSERT(constructor->getConstructData(constructData) == ConstructTypeJS);
#endif
    
    JSGlobalData& globalData = exec->globalData();
    
    Structure* structure;
    JSValue proto = JSValue::decode(encodedOp);
    if (proto.isObject())
        structure = asObject(proto)->inheritorID(globalData);
    else
        structure = constructor->scope()->globalObject->emptyObjectStructure();
    
    return JSValue::encode(constructEmptyObject(exec, structure));
}

EncodedJSValue DFG_OPERATION operationNewObject(ExecState* exec)
{
    return JSValue::encode(constructEmptyObject(exec));
}

EncodedJSValue DFG_OPERATION operationValueAdd(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    return JSValue::encode(jsAdd(exec, op1, op2));
}

EncodedJSValue DFG_OPERATION operationValueAddNotNumber(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    ASSERT(!op1.isNumber() || !op2.isNumber());
    
    if (op1.isString()) {
        if (op2.isString())
            return JSValue::encode(jsString(exec, asString(op1), asString(op2)));
        return JSValue::encode(jsString(exec, asString(op1), op2.toPrimitiveString(exec)));
    }

    return JSValue::encode(jsAddSlowCase(exec, op1, op2));
}

EncodedJSValue DFG_OPERATION operationArithAdd(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    double num1 = JSValue::decode(encodedOp1).uncheckedGetNumber();
    double num2 = JSValue::decode(encodedOp2).uncheckedGetNumber();
    return JSValue::encode(jsNumber(num1 + num2));
}

EncodedJSValue DFG_OPERATION operationArithSub(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    double num1 = JSValue::decode(encodedOp1).uncheckedGetNumber();
    double num2 = JSValue::decode(encodedOp2).uncheckedGetNumber();
    return JSValue::encode(jsNumber(num1 - num2));
}

EncodedJSValue DFG_OPERATION operationArithMul(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    double num1 = JSValue::decode(encodedOp1).uncheckedGetNumber();
    double num2 = JSValue::decode(encodedOp2).uncheckedGetNumber();
    return JSValue::encode(jsNumber(num1 * num2));
}

EncodedJSValue DFG_OPERATION operationArithDiv(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    double num1 = JSValue::decode(encodedOp1).uncheckedGetNumber();
    double num2 = JSValue::decode(encodedOp2).uncheckedGetNumber();
    return JSValue::encode(jsNumber(num1 / num2));
}

EncodedJSValue DFG_OPERATION operationArithMod(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
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

EncodedJSValue DFG_OPERATION operationGetByVal(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty)
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
            if (JSValue result = base->fastGetOwnProperty(exec, asString(property)->value(exec)))
                return JSValue::encode(result);
        }
    }

    Identifier ident(exec, property.toString(exec));
    return JSValue::encode(baseValue.get(exec, ident));
}

EncodedJSValue DFG_OPERATION operationGetById(ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName)
{
    JSValue baseValue = JSValue::decode(encodedBase);
    PropertySlot slot(baseValue);
    return JSValue::encode(baseValue.get(exec, *propertyName, slot));
}

#if CPU(X86_64)
EncodedJSValue DFG_OPERATION operationGetMethodOptimizeWithReturnAddress(ExecState*, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetMethodOptimize);
EncodedJSValue DFG_OPERATION operationGetMethodOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
#elif CPU(X86)
EncodedJSValue DFG_OPERATION operationGetMethodOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetMethodOptimize);
EncodedJSValue DFG_OPERATION operationGetMethodOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName)
#endif
{
    JSValue baseValue = JSValue::decode(encodedBase);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, *propertyName, slot);

    MethodCallLinkInfo& methodInfo = exec->codeBlock()->getMethodCallLinkInfo(returnAddress);
    if (methodInfo.seenOnce())
        dfgRepatchGetMethod(exec, baseValue, *propertyName, slot, methodInfo);
    else
        methodInfo.setSeen();

    return JSValue::encode(result);
}

#if CPU(X86_64)
EncodedJSValue DFG_OPERATION operationGetByIdBuildListWithReturnAddress(ExecState*, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetByIdBuildList);
EncodedJSValue DFG_OPERATION operationGetByIdBuildListWithReturnAddress(ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
#elif CPU(X86)
EncodedJSValue DFG_OPERATION operationGetByIdBuildListWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetByIdBuildList);
EncodedJSValue DFG_OPERATION operationGetByIdBuildListWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName)
#endif
{
    JSValue baseValue = JSValue::decode(encodedBase);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, *propertyName, slot);

    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    dfgBuildGetByIDList(exec, baseValue, *propertyName, slot, stubInfo);

    return JSValue::encode(result);
}

#if CPU(X86_64)
EncodedJSValue DFG_OPERATION operationGetByIdProtoBuildListWithReturnAddress(ExecState*, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetByIdProtoBuildList);
EncodedJSValue DFG_OPERATION operationGetByIdProtoBuildListWithReturnAddress(ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
#elif CPU(X86)
EncodedJSValue DFG_OPERATION operationGetByIdProtoBuildListWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetByIdProtoBuildList);
EncodedJSValue DFG_OPERATION operationGetByIdProtoBuildListWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName)
#endif
{
    JSValue baseValue = JSValue::decode(encodedBase);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, *propertyName, slot);

    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    dfgBuildGetByIDProtoList(exec, baseValue, *propertyName, slot, stubInfo);

    return JSValue::encode(result);
}

#if CPU(X86_64)
EncodedJSValue DFG_OPERATION operationGetByIdOptimizeWithReturnAddress(ExecState*, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetByIdOptimize);
EncodedJSValue DFG_OPERATION operationGetByIdOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
#elif CPU(X86)
EncodedJSValue DFG_OPERATION operationGetByIdOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetByIdOptimize);
EncodedJSValue DFG_OPERATION operationGetByIdOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName)
#endif
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

void DFG_OPERATION operationPutByValStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    operationPutByValInternal<true>(exec, encodedBase, encodedProperty, encodedValue);
}

void DFG_OPERATION operationPutByValNonStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    operationPutByValInternal<false>(exec, encodedBase, encodedProperty, encodedValue);
}

void DFG_OPERATION operationPutByValBeyondArrayBounds(ExecState* exec, JSArray* array, int32_t index, EncodedJSValue encodedValue)
{
    // We should only get here if index is outside the existing vector.
    ASSERT(!array->canSetIndex(index));
    array->JSArray::put(exec, index, JSValue::decode(encodedValue));
}

void DFG_OPERATION operationPutByIdStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
{
    PutPropertySlot slot(true);
    JSValue::decode(encodedBase).put(exec, *propertyName, JSValue::decode(encodedValue), slot);
}

void DFG_OPERATION operationPutByIdNonStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
{
    PutPropertySlot slot(false);
    JSValue::decode(encodedBase).put(exec, *propertyName, JSValue::decode(encodedValue), slot);
}

void DFG_OPERATION operationPutByIdDirectStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
{
    PutPropertySlot slot(true);
    JSValue::decode(encodedBase).putDirect(exec, *propertyName, JSValue::decode(encodedValue), slot);
}

void DFG_OPERATION operationPutByIdDirectNonStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
{
    PutPropertySlot slot(false);
    JSValue::decode(encodedBase).putDirect(exec, *propertyName, JSValue::decode(encodedValue), slot);
}

#if CPU(X86_64)
void DFG_OPERATION operationPutByIdStrictOptimizeWithReturnAddress(ExecState*, EncodedJSValue, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdStrictOptimize);
void DFG_OPERATION operationPutByIdStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
#elif CPU(X86)
void DFG_OPERATION operationPutByIdStrictOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdStrictOptimize);
void DFG_OPERATION operationPutByIdStrictOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
#endif
{
    JSValue value = JSValue::decode(encodedValue);
    JSValue base = JSValue::decode(encodedBase);
    PutPropertySlot slot(true);
    
    base.put(exec, *propertyName, value, slot);
    
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    if (stubInfo.seen)
        dfgRepatchPutByID(exec, base, *propertyName, slot, stubInfo, NotDirect);
    else
        stubInfo.seen = true;
}

#if CPU(X86_64)
void DFG_OPERATION operationPutByIdNonStrictOptimizeWithReturnAddress(ExecState*, EncodedJSValue, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdNonStrictOptimize);
void DFG_OPERATION operationPutByIdNonStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
#elif CPU(X86)
void DFG_OPERATION operationPutByIdNonStrictOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdNonStrictOptimize);
void DFG_OPERATION operationPutByIdNonStrictOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
#endif
{
    JSValue value = JSValue::decode(encodedValue);
    JSValue base = JSValue::decode(encodedBase);
    PutPropertySlot slot(false);
    
    base.put(exec, *propertyName, value, slot);
    
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    if (stubInfo.seen)
        dfgRepatchPutByID(exec, base, *propertyName, slot, stubInfo, NotDirect);
    else
        stubInfo.seen = true;
}

#if CPU(X86_64)
void DFG_OPERATION operationPutByIdDirectStrictOptimizeWithReturnAddress(ExecState*, EncodedJSValue, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdDirectStrictOptimize);
void DFG_OPERATION operationPutByIdDirectStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
#elif CPU(X86)
void DFG_OPERATION operationPutByIdDirectStrictOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdDirectStrictOptimize);
void DFG_OPERATION operationPutByIdDirectStrictOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
#endif
{
    JSValue value = JSValue::decode(encodedValue);
    JSValue base = JSValue::decode(encodedBase);
    PutPropertySlot slot(true);
    
    base.putDirect(exec, *propertyName, value, slot);
    
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    if (stubInfo.seen)
        dfgRepatchPutByID(exec, base, *propertyName, slot, stubInfo, Direct);
    else
        stubInfo.seen = true;
}

#if CPU(X86_64)
void DFG_OPERATION operationPutByIdDirectNonStrictOptimizeWithReturnAddress(ExecState*, EncodedJSValue, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdDirectNonStrictOptimize);
void DFG_OPERATION operationPutByIdDirectNonStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
#elif CPU(X86)
void DFG_OPERATION operationPutByIdDirectNonStrictOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdDirectNonStrictOptimize);
void DFG_OPERATION operationPutByIdDirectNonStrictOptimizeWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName)
#endif
{
    JSValue value = JSValue::decode(encodedValue);
    JSValue base = JSValue::decode(encodedBase);
    PutPropertySlot slot(false);
    
    base.putDirect(exec, *propertyName, value, slot);
    
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    if (stubInfo.seen)
        dfgRepatchPutByID(exec, base, *propertyName, slot, stubInfo, Direct);
    else
        stubInfo.seen = true;
}

RegisterSizedBoolean DFG_OPERATION operationCompareLess(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLess<true>(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

RegisterSizedBoolean DFG_OPERATION operationCompareLessEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLessEq<true>(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

RegisterSizedBoolean DFG_OPERATION operationCompareGreater(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLess<false>(exec, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1));
}

RegisterSizedBoolean DFG_OPERATION operationCompareGreaterEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLessEq<false>(exec, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1));
}

RegisterSizedBoolean DFG_OPERATION operationCompareEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return JSValue::equalSlowCaseInline(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

RegisterSizedBoolean DFG_OPERATION operationCompareStrictEqCell(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    ASSERT(op1.isCell());
    ASSERT(op2.isCell());
    
    return JSValue::strictEqualSlowCaseInline(exec, op1, op2);
}

RegisterSizedBoolean DFG_OPERATION operationCompareStrictEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return JSValue::strictEqual(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

EncodedJSValue DFG_OPERATION getHostCallReturnValue();
EncodedJSValue DFG_OPERATION getHostCallReturnValueWithExecState(ExecState*);

#if CPU(X86_64)
asm (
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "mov -40(%r13), %r13\n"
    "mov %r13, %rdi\n"
    "jmp " SYMBOL_STRING(getHostCallReturnValueWithExecState) "\n"
);
#elif CPU(X86)
asm (
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "mov -40(%edi), %edi\n"
    "mov (%esp), %ecx\n"
    "mov %edi, (%esp)\n"
    "lea -4(%esp), %esp\n"
    "mov %ecx, (%esp)\n"
    "jmp " SYMBOL_STRING(getHostCallReturnValueWithExecState) "\n"
);
#endif

EncodedJSValue DFG_OPERATION getHostCallReturnValueWithExecState(ExecState* exec)
{
    return JSValue::encode(exec->globalData().hostCallReturnValue);
}

static void* handleHostCall(ExecState* execCallee, JSValue callee, CodeSpecializationKind kind)
{
    ExecState* exec = execCallee->callerFrame();
    JSGlobalData* globalData = &exec->globalData();
    if (kind == CodeForCall) {
        CallData callData;
        CallType callType = getCallData(callee, callData);
    
        ASSERT(callType != CallTypeJS);
    
        if (callType == CallTypeHost) {
            if (!globalData->interpreter->registerFile().grow(execCallee->registers())) {
                globalData->exception = createStackOverflowError(exec);
                return 0;
            }
        
            execCallee->setScopeChain(exec->scopeChain());
        
            globalData->hostCallReturnValue = JSValue::decode(callData.native.function(execCallee));
        
            if (globalData->exception)
                return 0;
            return reinterpret_cast<void*>(getHostCallReturnValue);
        }
    
        ASSERT(callType == CallTypeNone);
        exec->globalData().exception = createNotAFunctionError(exec, callee);
        return 0;
    }

    ASSERT(kind == CodeForConstruct);
    
    ConstructData constructData;
    ConstructType constructType = getConstructData(callee, constructData);
    
    ASSERT(constructType != ConstructTypeJS);
    
    if (constructType == ConstructTypeHost) {
        if (!globalData->interpreter->registerFile().grow(execCallee->registers())) {
            globalData->exception = createStackOverflowError(exec);
            return 0;
        }
        
        execCallee->setScopeChain(exec->scopeChain());
        
        globalData->hostCallReturnValue = JSValue::decode(constructData.native.function(execCallee));
        
        if (globalData->exception)
            return 0;
        return reinterpret_cast<void*>(getHostCallReturnValue);
    }
    
    ASSERT(constructType == ConstructTypeNone);
    exec->globalData().exception = createNotAConstructorError(exec, callee);
    return 0;
}

inline void* linkFor(ExecState* execCallee, ReturnAddressPtr returnAddress, CodeSpecializationKind kind)
{
    ExecState* exec = execCallee->callerFrame();
    JSGlobalData* globalData = &exec->globalData();
    JSValue calleeAsValue = execCallee->calleeAsValue();
    JSCell* calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (!calleeAsFunctionCell)
        return handleHostCall(execCallee, calleeAsValue, kind);
    JSFunction* callee = asFunction(calleeAsFunctionCell);
    ExecutableBase* executable = callee->executable();
    
    MacroAssemblerCodePtr codePtr;
    CodeBlock* codeBlock = 0;
    if (executable->isHostFunction())
        codePtr = executable->generatedJITCodeFor(kind).addressForCall();
    else {
        execCallee->setScopeChain(callee->scope());
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);
        JSObject* error = functionExecutable->compileFor(execCallee, callee->scope(), kind);
        if (error) {
            globalData->exception = createStackOverflowError(exec);
            return 0;
        }
        codeBlock = &functionExecutable->generatedBytecodeFor(kind);
        if (execCallee->argumentCountIncludingThis() == static_cast<size_t>(codeBlock->m_numParameters))
            codePtr = functionExecutable->generatedJITCodeFor(kind).addressForCall();
        else
            codePtr = functionExecutable->generatedJITCodeWithArityCheckFor(kind);
    }
    CallLinkInfo& callLinkInfo = exec->codeBlock()->getCallLinkInfo(returnAddress);
    if (!callLinkInfo.seenOnce())
        callLinkInfo.setSeen();
    else
        dfgLinkFor(execCallee, callLinkInfo, codeBlock, callee, codePtr, kind);
    return codePtr.executableAddress();
}

#if CPU(X86_64)
void* DFG_OPERATION operationLinkCallWithReturnAddress(ExecState*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG2_RETURN_ADDRESS(operationLinkCall);
void* DFG_OPERATION operationLinkCallWithReturnAddress(ExecState* execCallee, ReturnAddressPtr returnAddress)
#elif CPU(X86)
void* DFG_OPERATION operationLinkCallWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* execCallee);
FUNCTION_WRAPPER_WITH_ARG2_RETURN_ADDRESS(operationLinkCall);
void* DFG_OPERATION operationLinkCallWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* execCallee)
#endif
{
    return linkFor(execCallee, returnAddress, CodeForCall);
}

#if CPU(X86_64)
void* DFG_OPERATION operationLinkConstructWithReturnAddress(ExecState*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG2_RETURN_ADDRESS(operationLinkConstruct);
void* DFG_OPERATION operationLinkConstructWithReturnAddress(ExecState* execCallee, ReturnAddressPtr returnAddress)
#elif CPU(X86)
void* DFG_OPERATION operationLinkConstructWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* execCallee);
FUNCTION_WRAPPER_WITH_ARG2_RETURN_ADDRESS(operationLinkConstruct);
void* DFG_OPERATION operationLinkConstructWithReturnAddress(ReturnAddressPtr returnAddress, ExecState* execCallee)
#endif
{
    return linkFor(execCallee, returnAddress, CodeForConstruct);
}

inline void* virtualFor(ExecState* execCallee, CodeSpecializationKind kind)
{
    ExecState* exec = execCallee->callerFrame();
    JSValue calleeAsValue = execCallee->calleeAsValue();
    JSCell* calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (UNLIKELY(!calleeAsFunctionCell))
        return handleHostCall(execCallee, calleeAsValue, kind);
    
    JSFunction* function = asFunction(calleeAsFunctionCell);
    execCallee->setScopeChain(function->scopeUnchecked());
    ExecutableBase* executable = function->executable();
    if (UNLIKELY(!executable->hasJITCodeFor(kind))) {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);
        JSObject* error = functionExecutable->compileFor(execCallee, function->scope(), kind);
        if (error) {
            exec->globalData().exception = error;
            return 0;
        }
    }
    return executable->generatedJITCodeWithArityCheckFor(kind).executableAddress();
}

void* DFG_OPERATION operationVirtualCall(ExecState* execCallee)
{
    return virtualFor(execCallee, CodeForCall);
}

void* DFG_OPERATION operationVirtualConstruct(ExecState* execCallee)
{
    return virtualFor(execCallee, CodeForConstruct);
}

EncodedJSValue DFG_OPERATION operationInstanceOf(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, EncodedJSValue encodedPrototype)
{
    JSValue value = JSValue::decode(encodedValue);
    JSValue base = JSValue::decode(encodedBase);
    JSValue prototype = JSValue::decode(encodedPrototype);

    // Otherwise CheckHasInstance should have failed.
    ASSERT(base.isCell());
    // At least one of these checks must have failed to get to the slow case.
    ASSERT(!value.isCell()
        || !prototype.isCell()
        || !prototype.isObject()
        || !base.asCell()->structure()->typeInfo().implementsDefaultHasInstance());


    // ECMA-262 15.3.5.3:
    // Throw an exception either if base is not an object, or if it does not implement 'HasInstance' (i.e. is a function).
    TypeInfo typeInfo(UnspecifiedType);
    if (!base.isObject() || !(typeInfo = asObject(base)->structure()->typeInfo()).implementsHasInstance()) {
        throwError(exec, createInvalidParamError(exec, "instanceof", base));
        return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(jsBoolean(asObject(base)->hasInstance(exec, value, prototype)));
}

EncodedJSValue DFG_OPERATION operationResolve(ExecState* exec, Identifier* propertyName)
{
    ScopeChainNode* scopeChain = exec->scopeChain();
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    do {
        JSObject* record = iter->get();
        PropertySlot slot(record);
        if (record->getPropertySlot(exec, *propertyName, slot))
            return JSValue::encode(slot.getValue(exec, *propertyName));
    } while (++iter != end);

    return throwVMError(exec, createUndefinedVariableError(exec, *propertyName));
}

EncodedJSValue DFG_OPERATION operationResolveBase(ExecState* exec, Identifier* propertyName)
{
    return JSValue::encode(resolveBase(exec, *propertyName, exec->scopeChain(), false));
}

EncodedJSValue DFG_OPERATION operationResolveBaseStrictPut(ExecState* exec, Identifier* propertyName)
{
    JSValue base = resolveBase(exec, *propertyName, exec->scopeChain(), true);
    if (!base)
        throwError(exec, createErrorForInvalidGlobalAssignment(exec, propertyName->ustring()));
    return JSValue::encode(base);
}

EncodedJSValue DFG_OPERATION operationResolveGlobal(ExecState* exec, GlobalResolveInfo* resolveInfo, Identifier* propertyName)
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();

    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(exec, *propertyName, slot)) {
        JSValue result = slot.getValue(exec, *propertyName);

        if (slot.isCacheableValue() && !globalObject->structure()->isUncacheableDictionary() && slot.slotBase() == globalObject) {
            resolveInfo->structure.set(exec->globalData(), exec->codeBlock()->ownerExecutable(), globalObject->structure());
            resolveInfo->offset = slot.cachedOffset();
        }

        return JSValue::encode(result);
    }

    return throwVMError(exec, createUndefinedVariableError(exec, *propertyName));
}

EncodedJSValue DFG_OPERATION operationToPrimitive(ExecState* exec, EncodedJSValue value)
{
    return JSValue::encode(JSValue::decode(value).toPrimitive(exec));
}

EncodedJSValue DFG_OPERATION operationStrCat(ExecState* exec, void* start, size_t size)
{
    return JSValue::encode(jsString(exec, static_cast<Register*>(start), size));
}

EncodedJSValue DFG_OPERATION operationNewArray(ExecState* exec, void* start, size_t size)
{
    ArgList argList(static_cast<Register*>(start), size);
    return JSValue::encode(constructArray(exec, argList));
}

EncodedJSValue DFG_OPERATION operationNewArrayBuffer(ExecState* exec, size_t start, size_t size)
{
    ArgList argList(exec->codeBlock()->constantBuffer(start), size);
    return JSValue::encode(constructArray(exec, argList));
}

EncodedJSValue DFG_OPERATION operationNewRegexp(ExecState* exec, void* regexpPtr)
{
    RegExp* regexp = static_cast<RegExp*>(regexpPtr);
    if (!regexp->isValid()) {
        throwError(exec, createSyntaxError(exec, "Invalid flags supplied to RegExp constructor."));
        return JSValue::encode(jsUndefined());
    }
    
    return JSValue::encode(RegExpObject::create(exec->globalData(), exec->lexicalGlobalObject(), exec->lexicalGlobalObject()->regExpStructure(), regexp));
}

void DFG_OPERATION operationThrowHasInstanceError(ExecState* exec, EncodedJSValue encodedBase)
{
    JSValue base = JSValue::decode(encodedBase);

    // We should only call this function if base is not an object, or if it does not implement 'HasInstance'.
    ASSERT(!base.isObject() || !asObject(base)->structure()->typeInfo().implementsHasInstance());

    throwError(exec, createInvalidParamError(exec, "instanceof", base));
}

DFGHandler DFG_OPERATION lookupExceptionHandler(ExecState* exec, ReturnAddressPtr faultLocation)
{
    JSValue exceptionValue = exec->exception();
    ASSERT(exceptionValue);

    unsigned vPCIndex = exec->codeBlock()->bytecodeOffset(faultLocation);
    HandlerInfo* handler = exec->globalData().interpreter->throwException(exec, exceptionValue, vPCIndex);

    void* catchRoutine = handler ? handler->nativeCode.executableAddress() : (void*)ctiOpThrowNotCaught;
    ASSERT(catchRoutine);
    return DFGHandler(exec, catchRoutine);
}

double DFG_OPERATION dfgConvertJSValueToNumber(ExecState* exec, EncodedJSValue value)
{
    return JSValue::decode(value).toNumber(exec);
}

int32_t DFG_OPERATION dfgConvertJSValueToInt32(ExecState* exec, EncodedJSValue value)
{
    return JSValue::decode(value).toInt32(exec);
}

RegisterSizedBoolean DFG_OPERATION dfgConvertJSValueToBoolean(ExecState* exec, EncodedJSValue encodedOp)
{
    return JSValue::decode(encodedOp).toBoolean(exec);
}

#if ENABLE(DFG_VERBOSE_SPECULATION_FAILURE)
void DFG_OPERATION debugOperationPrintSpeculationFailure(ExecState*, void* debugInfoRaw)
{
    SpeculationFailureDebugInfo* debugInfo = static_cast<SpeculationFailureDebugInfo*>(debugInfoRaw);
    CodeBlock* codeBlock = debugInfo->codeBlock;
    printf("Speculation failure in %p at 0x%x with executeCounter = %d, reoptimizationRetryCounter = %u, optimizationDelayCounter = %u, success/fail %u/%u\n", codeBlock, debugInfo->debugOffset, codeBlock->alternative()->executeCounter(), codeBlock->alternative()->reoptimizationRetryCounter(), codeBlock->alternative()->optimizationDelayCounter(), codeBlock->speculativeSuccessCounter(), codeBlock->speculativeFailCounter());
}
#endif

} // extern "C"
} } // namespace JSC::DFG

#endif
