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
#include "DFGOSRExit.h"
#include "DFGRepatch.h"
#include "InlineASM.h"
#include "Interpreter.h"
#include "JSByteArray.h"
#include "JSGlobalData.h"
#include "Operations.h"

#if CPU(X86_64)

#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, register) \
    asm( \
    ".globl " SYMBOL_STRING(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "mov (%rsp), %" STRINGIZE(register) "\n" \
        "jmp " SYMBOL_STRING_RELOCATION(function##WithReturnAddress) "\n" \
    );
#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)    FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, rsi)
#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function)  FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, rcx)
#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(function) FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, r8)

#elif CPU(X86)

#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, offset) \
    asm( \
    ".globl " SYMBOL_STRING(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "mov (%esp), %eax\n" \
        "mov %eax, " STRINGIZE(offset) "(%esp)\n" \
        "jmp " SYMBOL_STRING_RELOCATION(function##WithReturnAddress) "\n" \
    );
#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)    FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, 8)
#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function)  FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, 16)
#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(function) FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, 24)

#elif COMPILER(GCC) && CPU(ARM_THUMB2)

#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    asm ( \
    ".text" "\n" \
    ".align 2" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    ".thumb" "\n" \
    ".thumb_func " THUMB_FUNC_PARAM(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "cpy a2, lr" "\n" \
        "b " SYMBOL_STRING_RELOCATION(function) "WithReturnAddress" "\n" \
    );

#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function) \
    asm ( \
    ".text" "\n" \
    ".align 2" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    ".thumb" "\n" \
    ".thumb_func " THUMB_FUNC_PARAM(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "cpy a4, lr" "\n" \
        "b " SYMBOL_STRING_RELOCATION(function) "WithReturnAddress" "\n" \
    );

#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(function) \
    asm ( \
    ".text" "\n" \
    ".align 2" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    ".thumb" "\n" \
    ".thumb_func " THUMB_FUNC_PARAM(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "str lr, [sp, #4]" "\n" \
        "b " SYMBOL_STRING_RELOCATION(function) "WithReturnAddress" "\n" \
    );

#endif

#define P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
void* DFG_OPERATION function##WithReturnAddress(ExecState*, ReturnAddressPtr); \
FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)

#define J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function) \
EncodedJSValue DFG_OPERATION function##WithReturnAddress(ExecState*, JSCell*, Identifier*, ReturnAddressPtr); \
FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function)

#define V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(function) \
void DFG_OPERATION function##WithReturnAddress(ExecState*, EncodedJSValue, JSCell*, Identifier*, ReturnAddressPtr); \
FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(function)

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

        JSArray::putByIndex(array, exec, index, value);
        return;
    }

    if (isJSByteArray(globalData, baseValue) && asByteArray(baseValue)->canAccessIndex(index)) {
        JSByteArray* byteArray = asByteArray(baseValue);
        // FIXME: the JITstub used to relink this to an optimized form!
        if (value.isInt32()) {
            byteArray->setIndex(index, value.asInt32());
            return;
        }

        if (value.isNumber()) {
            byteArray->setIndex(index, value.asNumber());
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

inline JSCell* createThis(ExecState* exec, JSCell* prototype, JSFunction* constructor)
{
#if !ASSERT_DISABLED
    ConstructData constructData;
    ASSERT(constructor->methodTable()->getConstructData(constructor, constructData) == ConstructTypeJS);
#endif
    
    JSGlobalData& globalData = exec->globalData();
    
    Structure* structure;
    if (prototype->isObject())
        structure = asObject(prototype)->inheritorID(globalData);
    else
        structure = constructor->scope()->globalObject->emptyObjectStructure();
    
    return constructEmptyObject(exec, structure);
}

JSCell* DFG_OPERATION operationCreateThis(ExecState* exec, JSCell* prototype)
{
    return createThis(exec, prototype, asFunction(exec->callee()));
}

JSCell* DFG_OPERATION operationCreateThisInlined(ExecState* exec, JSCell* prototype, JSCell* constructor)
{
    return createThis(exec, prototype, static_cast<JSFunction*>(constructor));
}

JSCell* DFG_OPERATION operationNewObject(ExecState* exec)
{
    return constructEmptyObject(exec);
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

EncodedJSValue DFG_OPERATION operationGetByValCell(ExecState* exec, JSCell* base, EncodedJSValue encodedProperty)
{
    JSValue property = JSValue::decode(encodedProperty);

    if (property.isUInt32())
        return getByVal(exec, base, property.asUInt32());
    if (property.isDouble()) {
        double propertyAsDouble = property.asDouble();
        uint32_t propertyAsUInt32 = static_cast<uint32_t>(propertyAsDouble);
        if (propertyAsUInt32 == propertyAsDouble)
            return getByVal(exec, base, propertyAsUInt32);
    } else if (property.isString()) {
        if (JSValue result = base->fastGetOwnProperty(exec, asString(property)->value(exec)))
            return JSValue::encode(result);
    }

    Identifier ident(exec, property.toString(exec));
    return JSValue::encode(JSValue(base).get(exec, ident));
}

EncodedJSValue DFG_OPERATION operationGetById(ExecState* exec, JSCell* base, Identifier* propertyName)
{
    JSValue baseValue(base);
    PropertySlot slot(baseValue);
    return JSValue::encode(baseValue.get(exec, *propertyName, slot));
}

J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(operationGetMethodOptimize);
EncodedJSValue DFG_OPERATION operationGetMethodOptimizeWithReturnAddress(ExecState* exec, JSCell* base, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue baseValue(base);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, *propertyName, slot);
    
    CodeBlock* codeBlock = exec->codeBlock();
    MethodCallLinkInfo& methodInfo = codeBlock->getMethodCallLinkInfo(returnAddress);
    if (methodInfo.seenOnce())
        dfgRepatchGetMethod(exec, baseValue, *propertyName, slot, methodInfo);
    else
        methodInfo.setSeen();

    return JSValue::encode(result);
}

J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(operationGetByIdBuildList);
EncodedJSValue DFG_OPERATION operationGetByIdBuildListWithReturnAddress(ExecState* exec, JSCell* base, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue baseValue(base);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, *propertyName, slot);

    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    dfgBuildGetByIDList(exec, baseValue, *propertyName, slot, stubInfo);

    return JSValue::encode(result);
}

J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(operationGetByIdProtoBuildList);
EncodedJSValue DFG_OPERATION operationGetByIdProtoBuildListWithReturnAddress(ExecState* exec, JSCell* base, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue baseValue(base);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, *propertyName, slot);

    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    dfgBuildGetByIDProtoList(exec, baseValue, *propertyName, slot, stubInfo);

    return JSValue::encode(result);
}

J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(operationGetByIdOptimize);
EncodedJSValue DFG_OPERATION operationGetByIdOptimizeWithReturnAddress(ExecState* exec, JSCell* base, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue baseValue(base);
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

void DFG_OPERATION operationPutByValCellStrict(ExecState* exec, JSCell* cell, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    operationPutByValInternal<true>(exec, JSValue::encode(cell), encodedProperty, encodedValue);
}

void DFG_OPERATION operationPutByValCellNonStrict(ExecState* exec, JSCell* cell, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    operationPutByValInternal<false>(exec, JSValue::encode(cell), encodedProperty, encodedValue);
}

void DFG_OPERATION operationPutByValBeyondArrayBounds(ExecState* exec, JSArray* array, int32_t index, EncodedJSValue encodedValue)
{
    // We should only get here if index is outside the existing vector.
    ASSERT(!array->canSetIndex(index));
    JSArray::putByIndex(array, exec, index, JSValue::decode(encodedValue));
}

EncodedJSValue DFG_OPERATION operationArrayPush(ExecState* exec, EncodedJSValue encodedValue, JSArray* array)
{
    array->push(exec, JSValue::decode(encodedValue));
    return JSValue::encode(jsNumber(array->length()));
}
        
EncodedJSValue DFG_OPERATION operationArrayPop(ExecState*, JSArray* array)
{
    return JSValue::encode(array->pop());
}
        
void DFG_OPERATION operationPutByIdStrict(ExecState* exec, EncodedJSValue encodedValue, JSCell* base, Identifier* propertyName)
{
    PutPropertySlot slot(true);
    base->methodTable()->put(base, exec, *propertyName, JSValue::decode(encodedValue), slot);
}

void DFG_OPERATION operationPutByIdNonStrict(ExecState* exec, EncodedJSValue encodedValue, JSCell* base, Identifier* propertyName)
{
    PutPropertySlot slot(false);
    base->methodTable()->put(base, exec, *propertyName, JSValue::decode(encodedValue), slot);
}

void DFG_OPERATION operationPutByIdDirectStrict(ExecState* exec, EncodedJSValue encodedValue, JSCell* base, Identifier* propertyName)
{
    PutPropertySlot slot(true);
    JSValue(base).putDirect(exec, *propertyName, JSValue::decode(encodedValue), slot);
}

void DFG_OPERATION operationPutByIdDirectNonStrict(ExecState* exec, EncodedJSValue encodedValue, JSCell* base, Identifier* propertyName)
{
    PutPropertySlot slot(false);
    JSValue(base).putDirect(exec, *propertyName, JSValue::decode(encodedValue), slot);
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(operationPutByIdStrictOptimize);
void DFG_OPERATION operationPutByIdStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, JSCell* base, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue(base);
    PutPropertySlot slot(true);
    
    baseValue.put(exec, *propertyName, value, slot);
    
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    if (stubInfo.seen)
        dfgRepatchPutByID(exec, baseValue, *propertyName, slot, stubInfo, NotDirect);
    else
        stubInfo.seen = true;
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(operationPutByIdNonStrictOptimize);
void DFG_OPERATION operationPutByIdNonStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, JSCell* base, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue(base);
    PutPropertySlot slot(false);
    
    baseValue.put(exec, *propertyName, value, slot);
    
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    if (stubInfo.seen)
        dfgRepatchPutByID(exec, baseValue, *propertyName, slot, stubInfo, NotDirect);
    else
        stubInfo.seen = true;
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(operationPutByIdDirectStrictOptimize);
void DFG_OPERATION operationPutByIdDirectStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, JSCell* base, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue(base);
    PutPropertySlot slot(true);
    
    baseValue.putDirect(exec, *propertyName, value, slot);
    
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    if (stubInfo.seen)
        dfgRepatchPutByID(exec, baseValue, *propertyName, slot, stubInfo, Direct);
    else
        stubInfo.seen = true;
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(operationPutByIdDirectNonStrictOptimize);
void DFG_OPERATION operationPutByIdDirectNonStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, JSCell* base, Identifier* propertyName, ReturnAddressPtr returnAddress)
{
    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue(base);
    PutPropertySlot slot(false);
    
    baseValue.putDirect(exec, *propertyName, value, slot);
    
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    if (stubInfo.seen)
        dfgRepatchPutByID(exec, baseValue, *propertyName, slot, stubInfo, Direct);
    else
        stubInfo.seen = true;
}

size_t DFG_OPERATION operationCompareLess(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLess<true>(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

size_t DFG_OPERATION operationCompareLessEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLessEq<true>(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

size_t DFG_OPERATION operationCompareGreater(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLess<false>(exec, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1));
}

size_t DFG_OPERATION operationCompareGreaterEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return jsLessEq<false>(exec, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1));
}

size_t DFG_OPERATION operationCompareEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    return JSValue::equalSlowCaseInline(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

size_t DFG_OPERATION operationCompareStrictEqCell(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    ASSERT(op1.isCell());
    ASSERT(op2.isCell());
    
    return JSValue::strictEqualSlowCaseInline(exec, op1, op2);
}

size_t DFG_OPERATION operationCompareStrictEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
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
    "jmp " SYMBOL_STRING_RELOCATION(getHostCallReturnValueWithExecState) "\n"
);
#elif CPU(X86)
asm (
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "mov -40(%edi), %edi\n"
    "mov %edi, 4(%esp)\n"
    "jmp " SYMBOL_STRING_RELOCATION(getHostCallReturnValueWithExecState) "\n"
);
#elif CPU(ARM_THUMB2)
asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "ldr r5, [r5, #-40]" "\n"
    "cpy r0, r5" "\n"
    "b " SYMBOL_STRING_RELOCATION(getHostCallReturnValueWithExecState) "\n"
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

    execCallee->setScopeChain(exec->scopeChain());
    execCallee->setCodeBlock(0);

    if (kind == CodeForCall) {
        CallData callData;
        CallType callType = getCallData(callee, callData);
    
        ASSERT(callType != CallTypeJS);
    
        if (callType == CallTypeHost) {
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
    execCallee->setScopeChain(callee->scopeUnchecked());
    ExecutableBase* executable = callee->executable();

    MacroAssemblerCodePtr codePtr;
    CodeBlock* codeBlock = 0;
    if (executable->isHostFunction())
        codePtr = executable->generatedJITCodeFor(kind).addressForCall();
    else {
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

P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(operationLinkCall);
void* DFG_OPERATION operationLinkCallWithReturnAddress(ExecState* execCallee, ReturnAddressPtr returnAddress)
{
    return linkFor(execCallee, returnAddress, CodeForCall);
}

P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(operationLinkConstruct);
void* DFG_OPERATION operationLinkConstructWithReturnAddress(ExecState* execCallee, ReturnAddressPtr returnAddress)
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
    return JSValue::encode(constructArray(exec, static_cast<JSValue*>(start), size));
}

EncodedJSValue DFG_OPERATION operationNewArrayBuffer(ExecState* exec, size_t start, size_t size)
{
    return JSValue::encode(constructArray(exec, exec->codeBlock()->constantBuffer(start), size));
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

size_t DFG_OPERATION dfgConvertJSValueToInt32(ExecState* exec, EncodedJSValue value)
{
    // toInt32/toUInt32 return the same value; we want the value zero extended to fill the register.
    return JSValue::decode(value).toUInt32(exec);
}

size_t DFG_OPERATION dfgConvertJSValueToBoolean(ExecState* exec, EncodedJSValue encodedOp)
{
    return JSValue::decode(encodedOp).toBoolean(exec);
}

#if DFG_ENABLE(VERBOSE_SPECULATION_FAILURE)
void DFG_OPERATION debugOperationPrintSpeculationFailure(ExecState*, void* debugInfoRaw)
{
    SpeculationFailureDebugInfo* debugInfo = static_cast<SpeculationFailureDebugInfo*>(debugInfoRaw);
    CodeBlock* codeBlock = debugInfo->codeBlock;
    CodeBlock* alternative = codeBlock->alternative();
    printf("Speculation failure in %p at @%u with executeCounter = %d, reoptimizationRetryCounter = %u, optimizationDelayCounter = %u, success/fail %u/%u\n", codeBlock, debugInfo->nodeIndex, alternative ? alternative->executeCounter() : 0, alternative ? alternative->reoptimizationRetryCounter() : 0, alternative ? alternative->optimizationDelayCounter() : 0, codeBlock->speculativeSuccessCounter(), codeBlock->speculativeFailCounter());
}
#endif

} // extern "C"
} } // namespace JSC::DFG

#endif
