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
            if (JSValue result = base->fastGetOwnProperty(exec, asString(property)->value(exec)))
                return JSValue::encode(result);
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

EncodedJSValue operationGetMethodOptimizeWithReturnAddress(ExecState*, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetMethodOptimize);
EncodedJSValue operationGetMethodOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
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

EncodedJSValue operationGetByIdBuildListWithReturnAddress(ExecState*, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetByIdBuildList);
EncodedJSValue operationGetByIdBuildListWithReturnAddress(ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue baseValue = JSValue::decode(encodedBase);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, *propertyName, slot);

    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    dfgBuildGetByIDList(exec, baseValue, *propertyName, slot, stubInfo);

    return JSValue::encode(result);
}

EncodedJSValue operationGetByIdProtoBuildListWithReturnAddress(ExecState*, EncodedJSValue, Identifier*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG4_RETURN_ADDRESS(operationGetByIdProtoBuildList);
EncodedJSValue operationGetByIdProtoBuildListWithReturnAddress(ExecState* exec, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue baseValue = JSValue::decode(encodedBase);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, *propertyName, slot);

    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    dfgBuildGetByIDProtoList(exec, baseValue, *propertyName, slot, stubInfo);

    return JSValue::encode(result);
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

void operationPutByIdStrictOptimizeWithReturnAddress(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdStrictOptimize);
void operationPutByIdStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
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

void operationPutByIdNonStrictOptimizeWithReturnAddress(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdNonStrictOptimize);
void operationPutByIdNonStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
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

void operationPutByIdDirectStrictOptimizeWithReturnAddress(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdDirectStrictOptimize);
void operationPutByIdDirectStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
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

void operationPutByIdDirectNonStrictOptimizeWithReturnAddress(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG5_RETURN_ADDRESS(operationPutByIdDirectNonStrictOptimize);
void operationPutByIdDirectNonStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier* propertyName, ReturnAddressPtr returnAddress)
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

RegisterSizedBoolean operationCompareLess(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLess<true>(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

RegisterSizedBoolean operationCompareLessEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLessEq<true>(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

RegisterSizedBoolean operationCompareGreater(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLess<false>(exec, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1));
}

RegisterSizedBoolean operationCompareGreaterEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLessEq<false>(exec, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1));
}

RegisterSizedBoolean operationCompareEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return JSValue::equalSlowCaseInline(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

RegisterSizedBoolean operationCompareStrictEqCell(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    ASSERT(op1.isCell());
    ASSERT(op2.isCell());
    
    return JSValue::strictEqualSlowCaseInline(exec, op1, op2);
}

RegisterSizedBoolean operationCompareStrictEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return JSValue::strictEqual(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

EncodedJSValue getHostCallReturnValue();
EncodedJSValue getHostCallReturnValueWithExecState(ExecState*);

asm (
".globl _" STRINGIZE(getHostCallReturnValue) "\n"
"_" STRINGIZE(getHostCallReturnValue) ":" "\n"
    "mov -40(%r13), %r13\n"
    "mov %r13, %rdi\n"
    "jmp _" STRINGIZE(getHostCallReturnValueWithExecState) "\n"
);

EncodedJSValue getHostCallReturnValueWithExecState(ExecState* exec)
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
    JSCell* calleeAsFunctionCell = getJSFunction(*globalData, calleeAsValue);
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

void* operationLinkCallWithReturnAddress(ExecState*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG2_RETURN_ADDRESS(operationLinkCall);
void* operationLinkCallWithReturnAddress(ExecState* execCallee, ReturnAddressPtr returnAddress)
{
    return linkFor(execCallee, returnAddress, CodeForCall);
}

void* operationLinkConstructWithReturnAddress(ExecState*, ReturnAddressPtr);
FUNCTION_WRAPPER_WITH_ARG2_RETURN_ADDRESS(operationLinkConstruct);
void* operationLinkConstructWithReturnAddress(ExecState* execCallee, ReturnAddressPtr returnAddress)
{
    return linkFor(execCallee, returnAddress, CodeForConstruct);
}

inline void* virtualFor(ExecState* execCallee, CodeSpecializationKind kind)
{
    ExecState* exec = execCallee->callerFrame();
    JSGlobalData* globalData = &exec->globalData();
    JSValue calleeAsValue = execCallee->calleeAsValue();
    JSCell* calleeAsFunctionCell = getJSFunction(*globalData, calleeAsValue);
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

void* operationVirtualCall(ExecState* execCallee)
{
    return virtualFor(execCallee, CodeForCall);
}

void* operationVirtualConstruct(ExecState* execCallee)
{
    return virtualFor(execCallee, CodeForConstruct);
}

EncodedJSValue operationInstanceOf(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, EncodedJSValue encodedPrototype)
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
        || (base.asCell()->structure()->typeInfo().flags() & (ImplementsHasInstance | OverridesHasInstance)) != ImplementsHasInstance);


    // ECMA-262 15.3.5.3:
    // Throw an exception either if base is not an object, or if it does not implement 'HasInstance' (i.e. is a function).
    TypeInfo typeInfo(UnspecifiedType);
    if (!base.isObject() || !(typeInfo = asObject(base)->structure()->typeInfo()).implementsHasInstance()) {
        throwError(exec, createInvalidParamError(exec, "instanceof", base));
        return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(jsBoolean(asObject(base)->hasInstance(exec, value, prototype)));
}

EncodedJSValue operationResolve(ExecState* exec, Identifier* propertyName)
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

EncodedJSValue operationResolveBase(ExecState* exec, Identifier* propertyName)
{
    return JSValue::encode(resolveBase(exec, *propertyName, exec->scopeChain(), false));
}

EncodedJSValue operationResolveBaseStrictPut(ExecState* exec, Identifier* propertyName)
{
    JSValue base = resolveBase(exec, *propertyName, exec->scopeChain(), true);
    if (!base)
        throwError(exec, createErrorForInvalidGlobalAssignment(exec, propertyName->ustring()));
    return JSValue::encode(base);
}

void operationThrowHasInstanceError(ExecState* exec, EncodedJSValue encodedBase)
{
    JSValue base = JSValue::decode(encodedBase);

    // We should only call this function if base is not an object, or if it does not implement 'HasInstance'.
    ASSERT(!base.isObject() || !asObject(base)->structure()->typeInfo().implementsHasInstance());

    throwError(exec, createInvalidParamError(exec, "instanceof", base));
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

RegisterSizedBoolean dfgConvertJSValueToBoolean(ExecState* exec, EncodedJSValue encodedOp)
{
    return JSValue::decode(encodedOp).toBoolean(exec);
}

} // extern "C"
} } // namespace JSC::DFG

#endif
