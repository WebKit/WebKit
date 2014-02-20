/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef FTLOutput_h
#define FTLOutput_h

#if ENABLE(FTL_JIT)

#include "DFGCommon.h"
#include "FTLAbbreviations.h"
#include "FTLAbstractHeapRepository.h"
#include "FTLCommonValues.h"
#include "FTLIntrinsicRepository.h"
#include "FTLSwitchCase.h"
#include "FTLTypedPointer.h"
#include "FTLWeight.h"
#include "FTLWeightedTarget.h"
#include <wtf/StringPrintStream.h>

namespace JSC { namespace FTL {

// Idiomatic LLVM IR builder specifically designed for FTL. This uses our own lowering
// terminology, and has some of its own notions:
//
// We say that a "reference" is what LLVM considers to be a "pointer". That is, it has
// an element type and can be passed directly to memory access instructions. Note that
// broadly speaking the users of FTL::Output should only use references for alloca'd
// slots for mutable local variables.
//
// We say that a "pointer" is what LLVM considers to be a pointer-width integer.
//
// We say that a "typed pointer" is a pointer that carries TBAA meta-data (i.e. an
// AbstractHeap). These should usually not have further computation performed on them
// prior to access, though there are exceptions (like offsetting into the payload of
// a typed pointer to a JSValue).
//
// We say that "get" and "set" are what LLVM considers to be "load" and "store". Get
// and set take references.
//
// We say that "load" and "store" are operations that take a typed pointer. These
// operations translate the pointer into a reference (or, a pointer in LLVM-speak),
// emit get or set on the reference (or, load and store in LLVM-speak), and apply the
// TBAA meta-data to the get or set.

enum Scale { ScaleOne, ScaleTwo, ScaleFour, ScaleEight, ScalePtr };

class Output : public IntrinsicRepository {
public:
    Output(LContext);
    ~Output();
    
    void initialize(LModule, LValue, AbstractHeapRepository&);
    
    LBasicBlock insertNewBlocksBefore(LBasicBlock nextBlock)
    {
        LBasicBlock lastNextBlock = m_nextBlock;
        m_nextBlock = nextBlock;
        return lastNextBlock;
    }
    
    LBasicBlock appendTo(LBasicBlock, LBasicBlock nextBlock);
    
    void appendTo(LBasicBlock);
    
    LBasicBlock newBlock(const char* name = "");
    
    LValue param(unsigned index) { return getParam(m_function, index); }
    LValue constBool(bool value) { return constInt(boolean, value); }
    LValue constInt8(int8_t value) { return constInt(int8, value); }
    LValue constInt32(int32_t value) { return constInt(int32, value); }
    template<typename T>
    LValue constIntPtr(T* value) { return constInt(intPtr, bitwise_cast<intptr_t>(value)); }
    template<typename T>
    LValue constIntPtr(T value) { return constInt(intPtr, static_cast<intptr_t>(value)); }
    LValue constInt64(int64_t value) { return constInt(int64, value); }
    LValue constDouble(double value) { return constReal(doubleType, value); }
    
    LValue phi(LType type) { return buildPhi(m_builder, type); }
    LValue phi(LType type, ValueFromBlock value1)
    {
        return buildPhi(m_builder, type, value1);
    }
    LValue phi(LType type, ValueFromBlock value1, ValueFromBlock value2)
    {
        return buildPhi(m_builder, type, value1, value2);
    }
    template<typename VectorType>
    LValue phi(LType type, const VectorType& vector)
    {
        LValue result = phi(type);
        for (unsigned i = 0; i < vector.size(); ++i)
            addIncoming(result, vector[i]);
        return result;
    }
    
    LValue add(LValue left, LValue right) { return buildAdd(m_builder, left, right); }
    LValue sub(LValue left, LValue right) { return buildSub(m_builder, left, right); }
    LValue mul(LValue left, LValue right) { return buildMul(m_builder, left, right); }
    LValue div(LValue left, LValue right) { return buildDiv(m_builder, left, right); }
    LValue rem(LValue left, LValue right) { return buildRem(m_builder, left, right); }
    LValue neg(LValue value) { return buildNeg(m_builder, value); }

    LValue doubleAdd(LValue left, LValue right) { return buildFAdd(m_builder, left, right); }
    LValue doubleSub(LValue left, LValue right) { return buildFSub(m_builder, left, right); }
    LValue doubleMul(LValue left, LValue right) { return buildFMul(m_builder, left, right); }
    LValue doubleDiv(LValue left, LValue right) { return buildFDiv(m_builder, left, right); }
    LValue doubleRem(LValue left, LValue right) { return buildFRem(m_builder, left, right); }
    LValue doubleNeg(LValue value) { return buildFNeg(m_builder, value); }

    LValue bitAnd(LValue left, LValue right) { return buildAnd(m_builder, left, right); }
    LValue bitOr(LValue left, LValue right) { return buildOr(m_builder, left, right); }
    LValue bitXor(LValue left, LValue right) { return buildXor(m_builder, left, right); }
    LValue shl(LValue left, LValue right) { return buildShl(m_builder, left, right); }
    LValue aShr(LValue left, LValue right) { return buildAShr(m_builder, left, right); }
    LValue lShr(LValue left, LValue right) { return buildLShr(m_builder, left, right); }
    LValue bitNot(LValue value) { return buildNot(m_builder, value); }
    
    LValue insertElement(LValue vector, LValue element, LValue index) { return buildInsertElement(m_builder, vector, element, index); }
    
    LValue addWithOverflow32(LValue left, LValue right)
    {
        return call(addWithOverflow32Intrinsic(), left, right);
    }
    LValue subWithOverflow32(LValue left, LValue right)
    {
        return call(subWithOverflow32Intrinsic(), left, right);
    }
    LValue mulWithOverflow32(LValue left, LValue right)
    {
        return call(mulWithOverflow32Intrinsic(), left, right);
    }
    LValue addWithOverflow64(LValue left, LValue right)
    {
        return call(addWithOverflow64Intrinsic(), left, right);
    }
    LValue subWithOverflow64(LValue left, LValue right)
    {
        return call(subWithOverflow64Intrinsic(), left, right);
    }
    LValue mulWithOverflow64(LValue left, LValue right)
    {
        return call(mulWithOverflow64Intrinsic(), left, right);
    }
    LValue doubleAbs(LValue value)
    {
        return call(doubleAbsIntrinsic(), value);
    }
    LValue doubleSin(LValue value)
    {
        return call(doubleSinIntrinsic(), value);
    }

    LValue doubleCos(LValue value)
    {
        return call(doubleCosIntrinsic(), value);
    }

    LValue doubleSqrt(LValue value)
    {
        return call(doubleSqrtIntrinsic(), value);
    }

    static bool hasSensibleDoubleToInt() { return isX86(); }
    LValue sensibleDoubleToInt(LValue);
    
    LValue signExt(LValue value, LType type) { return buildSExt(m_builder, value, type); }
    LValue zeroExt(LValue value, LType type) { return buildZExt(m_builder, value, type); }
    LValue fpToInt(LValue value, LType type) { return buildFPToSI(m_builder, value, type); }
    LValue fpToUInt(LValue value, LType type) { return buildFPToUI(m_builder, value, type); }
    LValue fpToInt32(LValue value) { return fpToInt(value, int32); }
    LValue fpToUInt32(LValue value) { return fpToUInt(value, int32); }
    LValue intToFP(LValue value, LType type) { return buildSIToFP(m_builder, value, type); }
    LValue intToDouble(LValue value) { return intToFP(value, doubleType); }
    LValue unsignedToFP(LValue value, LType type) { return buildUIToFP(m_builder, value, type); }
    LValue unsignedToDouble(LValue value) { return unsignedToFP(value, doubleType); }
    LValue intCast(LValue value, LType type) { return buildIntCast(m_builder, value, type); }
    LValue castToInt32(LValue value) { return intCast(value, int32); }
    LValue fpCast(LValue value, LType type) { return buildFPCast(m_builder, value, type); }
    LValue intToPtr(LValue value, LType type) { return buildIntToPtr(m_builder, value, type); }
    LValue ptrToInt(LValue value, LType type) { return buildPtrToInt(m_builder, value, type); }
    LValue bitCast(LValue value, LType type) { return buildBitCast(m_builder, value, type); }
    
    LValue alloca(LType type) { return buildAlloca(m_builder, type); }
    LValue get(LValue reference) { return buildLoad(m_builder, reference); }
    LValue set(LValue value, LValue reference) { return buildStore(m_builder, value, reference); }
    
    LValue load(TypedPointer, LType refType);
    void store(LValue, TypedPointer, LType refType);
    
    LValue load8(TypedPointer pointer) { return load(pointer, ref8); }
    LValue load16(TypedPointer pointer) { return load(pointer, ref16); }
    LValue load32(TypedPointer pointer) { return load(pointer, ref32); }
    LValue load64(TypedPointer pointer) { return load(pointer, ref64); }
    LValue loadPtr(TypedPointer pointer) { return load(pointer, refPtr); }
    LValue loadFloat(TypedPointer pointer) { return load(pointer, refFloat); }
    LValue loadDouble(TypedPointer pointer) { return load(pointer, refDouble); }
    void store8(LValue value, TypedPointer pointer) { store(value, pointer, ref8); }
    void store16(LValue value, TypedPointer pointer) { store(value, pointer, ref16); }
    void store32(LValue value, TypedPointer pointer) { store(value, pointer, ref32); }
    void store64(LValue value, TypedPointer pointer) { store(value, pointer, ref64); }
    void storePtr(LValue value, TypedPointer pointer) { store(value, pointer, refPtr); }
    void storeFloat(LValue value, TypedPointer pointer) { store(value, pointer, refFloat); }
    void storeDouble(LValue value, TypedPointer pointer) { store(value, pointer, refDouble); }

    LValue addPtr(LValue value, ptrdiff_t immediate = 0)
    {
        if (!immediate)
            return value;
        return add(value, constIntPtr(immediate));
    }
    
    // Construct an address by offsetting base by the requested amount and ascribing
    // the requested abstract heap to it.
    TypedPointer address(const AbstractHeap& heap, LValue base, ptrdiff_t offset = 0)
    {
        return TypedPointer(heap, addPtr(base, offset));
    }
    // Construct an address by offsetting base by the amount specified by the field,
    // and optionally an additional amount (use this with care), and then creating
    // a TypedPointer with the given field as the heap.
    TypedPointer address(LValue base, const AbstractField& field, ptrdiff_t offset = 0)
    {
        return address(field, base, offset + field.offset());
    }
    
    LValue baseIndex(LValue base, LValue index, Scale, ptrdiff_t offset = 0);

    TypedPointer baseIndex(const AbstractHeap& heap, LValue base, LValue index, Scale scale, ptrdiff_t offset = 0)
    {
        return TypedPointer(heap, baseIndex(base, index, scale, offset));
    }
    TypedPointer baseIndex(IndexedAbstractHeap& heap, LValue base, LValue index, JSValue indexAsConstant = JSValue(), ptrdiff_t offset = 0)
    {
        return heap.baseIndex(*this, base, index, indexAsConstant, offset);
    }
    
    TypedPointer absolute(void* address)
    {
        return TypedPointer(m_heaps->absolute[address], constIntPtr(address));
    }
    
    LValue load8(LValue base, const AbstractField& field) { return load8(address(base, field)); }
    LValue load16(LValue base, const AbstractField& field) { return load16(address(base, field)); }
    LValue load32(LValue base, const AbstractField& field) { return load32(address(base, field)); }
    LValue load64(LValue base, const AbstractField& field) { return load64(address(base, field)); }
    LValue loadPtr(LValue base, const AbstractField& field) { return loadPtr(address(base, field)); }
    LValue loadDouble(LValue base, const AbstractField& field) { return loadDouble(address(base, field)); }
    void store32(LValue value, LValue base, const AbstractField& field) { store32(value, address(base, field)); }
    void store64(LValue value, LValue base, const AbstractField& field) { store64(value, address(base, field)); }
    void storePtr(LValue value, LValue base, const AbstractField& field) { storePtr(value, address(base, field)); }
    void storeDouble(LValue value, LValue base, const AbstractField& field) { storeDouble(value, address(base, field)); }
    
    void ascribeRange(LValue loadInstruction, const ValueRange& range)
    {
        range.decorateInstruction(m_context, loadInstruction, rangeKind);
    }
    
    LValue nonNegative32(LValue loadInstruction)
    {
        ascribeRange(loadInstruction, nonNegativeInt32);
        return loadInstruction;
    }
    
    LValue load32NonNegative(TypedPointer pointer) { return nonNegative32(load32(pointer)); }
    LValue load32NonNegative(LValue base, const AbstractField& field) { return nonNegative32(load32(base, field)); }
    
    LValue icmp(LIntPredicate cond, LValue left, LValue right) { return buildICmp(m_builder, cond, left, right); }
    LValue equal(LValue left, LValue right) { return icmp(LLVMIntEQ, left, right); }
    LValue notEqual(LValue left, LValue right) { return icmp(LLVMIntNE, left, right); }
    LValue above(LValue left, LValue right) { return icmp(LLVMIntUGT, left, right); }
    LValue aboveOrEqual(LValue left, LValue right) { return icmp(LLVMIntUGE, left, right); }
    LValue below(LValue left, LValue right) { return icmp(LLVMIntULT, left, right); }
    LValue belowOrEqual(LValue left, LValue right) { return icmp(LLVMIntULE, left, right); }
    LValue greaterThan(LValue left, LValue right) { return icmp(LLVMIntSGT, left, right); }
    LValue greaterThanOrEqual(LValue left, LValue right) { return icmp(LLVMIntSGE, left, right); }
    LValue lessThan(LValue left, LValue right) { return icmp(LLVMIntSLT, left, right); }
    LValue lessThanOrEqual(LValue left, LValue right) { return icmp(LLVMIntSLE, left, right); }
    
    LValue fcmp(LRealPredicate cond, LValue left, LValue right) { return buildFCmp(m_builder, cond, left, right); }
    LValue doubleEqual(LValue left, LValue right) { return fcmp(LLVMRealOEQ, left, right); }
    LValue doubleNotEqualOrUnordered(LValue left, LValue right) { return fcmp(LLVMRealUNE, left, right); }
    LValue doubleLessThan(LValue left, LValue right) { return fcmp(LLVMRealOLT, left, right); }
    LValue doubleLessThanOrEqual(LValue left, LValue right) { return fcmp(LLVMRealOLE, left, right); }
    LValue doubleGreaterThan(LValue left, LValue right) { return fcmp(LLVMRealOGT, left, right); }
    LValue doubleGreaterThanOrEqual(LValue left, LValue right) { return fcmp(LLVMRealOGE, left, right); }
    LValue doubleEqualOrUnordered(LValue left, LValue right) { return fcmp(LLVMRealUEQ, left, right); }
    LValue doubleNotEqual(LValue left, LValue right) { return fcmp(LLVMRealONE, left, right); }
    LValue doubleLessThanOrUnordered(LValue left, LValue right) { return fcmp(LLVMRealULT, left, right); }
    LValue doubleLessThanOrEqualOrUnordered(LValue left, LValue right) { return fcmp(LLVMRealULE, left, right); }
    LValue doubleGreaterThanOrUnordered(LValue left, LValue right) { return fcmp(LLVMRealUGT, left, right); }
    LValue doubleGreaterThanOrEqualOrUnordered(LValue left, LValue right) { return fcmp(LLVMRealUGE, left, right); }
    
    LValue isZero8(LValue value) { return equal(value, int8Zero); }
    LValue notZero8(LValue value) { return notEqual(value, int8Zero); }
    LValue isZero32(LValue value) { return equal(value, int32Zero); }
    LValue notZero32(LValue value) { return notEqual(value, int32Zero); }
    LValue isZero64(LValue value) { return equal(value, int64Zero); }
    LValue notZero64(LValue value) { return notEqual(value, int64Zero); }
    LValue isNull(LValue value) { return equal(value, intPtrZero); }
    LValue notNull(LValue value) { return notEqual(value, intPtrZero); }
    
    LValue testIsZero8(LValue value, LValue mask) { return isZero8(bitAnd(value, mask)); }
    LValue testNonZero8(LValue value, LValue mask) { return notZero8(bitAnd(value, mask)); }
    LValue testIsZero32(LValue value, LValue mask) { return isZero32(bitAnd(value, mask)); }
    LValue testNonZero32(LValue value, LValue mask) { return notZero32(bitAnd(value, mask)); }
    LValue testIsZero64(LValue value, LValue mask) { return isZero64(bitAnd(value, mask)); }
    LValue testNonZero64(LValue value, LValue mask) { return notZero64(bitAnd(value, mask)); }
    
    LValue select(LValue value, LValue taken, LValue notTaken) { return buildSelect(m_builder, value, taken, notTaken); }
    LValue extractValue(LValue aggVal, unsigned index) { return buildExtractValue(m_builder, aggVal, index); }
    
    LValue fence(LAtomicOrdering ordering = LLVMAtomicOrderingSequentiallyConsistent, SynchronizationScope scope = CrossThread) { return buildFence(m_builder, ordering, scope); }
    LValue fenceAcqRel() { return fence(LLVMAtomicOrderingAcquireRelease); }
    
    template<typename VectorType>
    LValue call(LValue function, const VectorType& vector) { return buildCall(m_builder, function, vector); }
    LValue call(LValue function) { return buildCall(m_builder, function); }
    LValue call(LValue function, LValue arg1) { return buildCall(m_builder, function, arg1); }
    LValue call(LValue function, LValue arg1, LValue arg2) { return buildCall(m_builder, function, arg1, arg2); }
    LValue call(LValue function, LValue arg1, LValue arg2, LValue arg3) { return buildCall(m_builder, function, arg1, arg2, arg3); }
    LValue call(LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4) { return buildCall(m_builder, function, arg1, arg2, arg3, arg4); }
    LValue call(LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4, LValue arg5) { return buildCall(m_builder, function, arg1, arg2, arg3, arg4, arg5); }
    LValue call(LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4, LValue arg5, LValue arg6) { return buildCall(m_builder, function, arg1, arg2, arg3, arg4, arg5, arg6); }
    LValue call(LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4, LValue arg5, LValue arg6, LValue arg7) { return buildCall(m_builder, function, arg1, arg2, arg3, arg4, arg5, arg6, arg7); }
    LValue call(LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4, LValue arg5, LValue arg6, LValue arg7, LValue arg8) { return buildCall(m_builder, function, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); }
    
    template<typename FunctionType>
    LValue operation(FunctionType function)
    {
        return intToPtr(constIntPtr(function), pointerType(operationType(function)));
    }
    
    void jump(LBasicBlock destination) { buildBr(m_builder, destination); }
    void branch(LValue condition, LBasicBlock taken, Weight takenWeight, LBasicBlock notTaken, Weight notTakenWeight);
    void branch(LValue condition, WeightedTarget taken, WeightedTarget notTaken)
    {
        branch(condition, taken.target(), taken.weight(), notTaken.target(), notTaken.weight());
    }
    
    template<typename VectorType>
    void switchInstruction(LValue value, const VectorType& cases, LBasicBlock fallThrough, Weight fallThroughWeight)
    {
        LValue inst = buildSwitch(m_builder, value, cases, fallThrough);
        
        double total = 0;
        if (!fallThroughWeight)
            return;
        total += fallThroughWeight.value();
        for (unsigned i = cases.size(); i--;) {
            if (!cases[i].weight())
                return;
            total += cases[i].weight().value();
        }
        
        Vector<LValue> mdArgs;
        mdArgs.append(branchWeights);
        mdArgs.append(constInt32(fallThroughWeight.scaleToTotal(total)));
        for (unsigned i = 0; i < cases.size(); ++i)
            mdArgs.append(constInt32(cases[i].weight().scaleToTotal(total)));
        
        setMetadata(inst, profKind, mdNode(m_context, mdArgs));
    }
    
    void ret(LValue value) { buildRet(m_builder, value); }
    
    void unreachable() { buildUnreachable(m_builder); }
    
    void trap()
    {
        call(trapIntrinsic());
    }
    
    void crashNonTerminal();

    void crash()
    {
        crashNonTerminal();
        unreachable();
    }
    
    ValueFromBlock anchor(LValue value)
    {
        return ValueFromBlock(value, m_block);
    }
    
    LValue m_function;
    AbstractHeapRepository* m_heaps;
    LBuilder m_builder;
    LBasicBlock m_block;
    LBasicBlock m_nextBlock;
};

#define FTL_NEW_BLOCK(output, nameArguments) \
    (LIKELY(!::JSC::DFG::verboseCompilationEnabled()) \
    ? (output).newBlock() \
    : (output).newBlock((toCString nameArguments).data()))

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLOutput_h

