/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#ifndef FTLB3Output_h
#define FTLB3Output_h

#include "DFGCommon.h"

#if ENABLE(FTL_JIT)
#if FTL_USES_B3

#include "B3BasicBlock.h"
#include "B3Procedure.h"
#include "FTLAbbreviatedTypes.h"
#include "FTLAbstractHeapRepository.h"
#include "FTLCommonValues.h"
#include "FTLState.h"
#include "FTLSwitchCase.h"
#include "FTLTypedPointer.h"
#include "FTLValueFromBlock.h"
#include "FTLWeight.h"
#include "FTLWeightedTarget.h"
#include <wtf/StringPrintStream.h>

// FIXME: remove this once everything can be generated through B3.
#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif // COMPILER(CLANG)

namespace JSC { namespace FTL {

enum Scale { ScaleOne, ScaleTwo, ScaleFour, ScaleEight, ScalePtr };

class Output : public CommonValues {
public:
    Output(State&);
    ~Output() { CRASH(); }

    LBasicBlock newBlock(const char* name = "")
    {
        UNUSED_PARAM(name);
        return m_procedure.addBlock();
    }

    LBasicBlock insertNewBlocksBefore(LBasicBlock nextBlock)
    {
        LBasicBlock lastNextBlock = m_nextBlock;
        m_nextBlock = nextBlock;
        return lastNextBlock;
    }

    LBasicBlock appendTo(LBasicBlock, LBasicBlock nextBlock) { CRASH(); }
    void appendTo(LBasicBlock) { CRASH(); }

    LValue param(unsigned index) { CRASH(); }
    LValue constBool(bool value) { CRASH(); }
    LValue constInt32(int32_t value) { CRASH(); }
    template<typename T>
    LValue constIntPtr(T* value) { CRASH(); }
    template<typename T>
    LValue constIntPtr(T value) { CRASH(); }
    LValue constInt64(int64_t value) { CRASH(); }
    LValue constDouble(double value) { CRASH(); }

    LValue phi(LType type) { CRASH(); }
    template<typename... Params>
    LValue phi(LType type, ValueFromBlock value, Params... theRest) { CRASH(); }
    template<typename VectorType>
    LValue phi(LType type, const VectorType& vector) { CRASH(); }
    void addIncomingToPhi(LValue phi, ValueFromBlock value) { CRASH(); }

    LValue add(LValue left, LValue right) { CRASH(); }
    LValue sub(LValue left, LValue right) { CRASH(); }
    LValue mul(LValue left, LValue right) { CRASH(); }
    LValue div(LValue left, LValue right) { CRASH(); }
    LValue rem(LValue left, LValue right) { CRASH(); }
    LValue neg(LValue value) { CRASH(); }

    LValue doubleAdd(LValue left, LValue right) { CRASH(); }
    LValue doubleSub(LValue left, LValue right) { CRASH(); }
    LValue doubleMul(LValue left, LValue right) { CRASH(); }
    LValue doubleDiv(LValue left, LValue right) { CRASH(); }
    LValue doubleRem(LValue left, LValue right) { CRASH(); }
    LValue doubleNeg(LValue value) { CRASH(); }

    LValue bitAnd(LValue left, LValue right) { CRASH(); }
    LValue bitOr(LValue left, LValue right) { CRASH(); }
    LValue bitXor(LValue left, LValue right) { CRASH(); }
    LValue shl(LValue left, LValue right) { CRASH(); }
    LValue aShr(LValue left, LValue right) { CRASH(); }
    LValue lShr(LValue left, LValue right) { CRASH(); }
    LValue bitNot(LValue value) { CRASH(); }

    LValue insertElement(LValue vector, LValue element, LValue index) { CRASH(); }

    LValue ceil64(LValue operand) { CRASH(); }
    LValue ctlz32(LValue xOperand, LValue yOperand) { CRASH(); }
    LValue addWithOverflow32(LValue left, LValue right) { CRASH(); }
    LValue subWithOverflow32(LValue left, LValue right) { CRASH(); }
    LValue mulWithOverflow32(LValue left, LValue right) { CRASH(); }
    LValue addWithOverflow64(LValue left, LValue right) { CRASH(); }
    LValue subWithOverflow64(LValue left, LValue right) { CRASH(); }
    LValue mulWithOverflow64(LValue left, LValue right) { CRASH(); }
    LValue doubleAbs(LValue value) { CRASH(); }

    LValue doubleSin(LValue value) { CRASH(); }
    LValue doubleCos(LValue value) { CRASH(); }

    LValue doublePow(LValue xOperand, LValue yOperand) { CRASH(); }

    LValue doublePowi(LValue xOperand, LValue yOperand) { CRASH(); }

    LValue doubleSqrt(LValue value) { CRASH(); }

    LValue doubleLog(LValue value) { CRASH(); }

    static bool hasSensibleDoubleToInt() { CRASH(); }
    LValue sensibleDoubleToInt(LValue) { CRASH(); }

    LValue signExt(LValue value, LType type) { CRASH(); }
    LValue zeroExt(LValue value, LType type) { CRASH(); }
    LValue zeroExtPtr(LValue value) { CRASH(); }
    LValue fpToInt(LValue value, LType type) { CRASH(); }
    LValue fpToUInt(LValue value, LType type) { CRASH(); }
    LValue fpToInt32(LValue value) { CRASH(); }
    LValue fpToUInt32(LValue value) { CRASH(); }
    LValue intToFP(LValue value, LType type) { CRASH(); }
    LValue intToDouble(LValue value) { CRASH(); }
    LValue unsignedToFP(LValue value, LType type) { CRASH(); }
    LValue unsignedToDouble(LValue value) { CRASH(); }
    LValue intCast(LValue value, LType type) { CRASH(); }
    LValue castToInt32(LValue value) { CRASH(); }
    LValue fpCast(LValue value, LType type) { CRASH(); }
    LValue intToPtr(LValue value, LType type) { CRASH(); }
    LValue ptrToInt(LValue value, LType type) { CRASH(); }
    LValue bitCast(LValue value, LType type) { CRASH(); }

    LValue fround(LValue doubleValue) { CRASH(); }

    // Hilariously, the #define machinery in the stdlib means that this method is actually called
    // __builtin_alloca. So far this appears benign. :-|
    LValue alloca(LType type) { CRASH(); }

    // Access the value of an alloca. Also used as a low-level implementation primitive for
    // load(). Never use this to load from "pointers" in the FTL sense, since FTL pointers
    // are actually integers. This requires an LLVM pointer. Broadly speaking, you don't
    // have any LLVM pointers even if you really think you do. A TypedPointer is not an
    // LLVM pointer. See comment block at top of this file to understand the distinction
    // between LLVM pointers, FTL pointers, and FTL references.
    LValue get(LValue reference) { CRASH(); }
    // Similar to get() but for storing to the value in an alloca.
    LValue set(LValue value, LValue reference) { CRASH(); }

    LValue load(TypedPointer, LType refType) { CRASH(); }
    void store(LValue, TypedPointer, LType refType) { CRASH(); }

    LValue load8SignExt32(TypedPointer) { CRASH(); }
    LValue load8ZeroExt32(TypedPointer) { CRASH(); }
    LValue load16SignExt32(TypedPointer) { CRASH(); }
    LValue load16ZeroExt32(TypedPointer) { CRASH(); }
    LValue load32(TypedPointer pointer) { CRASH(); }
    LValue load64(TypedPointer pointer) { CRASH(); }
    LValue loadPtr(TypedPointer pointer) { CRASH(); }
    LValue loadFloatToDouble(TypedPointer pointer) { CRASH(); }
    LValue loadDouble(TypedPointer pointer) { CRASH(); }
    void store16(LValue value, TypedPointer pointer) { CRASH(); }
    void store32(LValue value, TypedPointer pointer) { CRASH(); }
    void store64(LValue value, TypedPointer pointer) { CRASH(); }
    void storePtr(LValue value, TypedPointer pointer) { CRASH(); }
    void storeDouble(LValue value, TypedPointer pointer) { CRASH(); }

    LValue addPtr(LValue value, ptrdiff_t immediate = 0) { CRASH(); }

    // Construct an address by offsetting base by the requested amount and ascribing
    // the requested abstract heap to it.
    TypedPointer address(const AbstractHeap& heap, LValue base, ptrdiff_t offset = 0) { CRASH(); }
    // Construct an address by offsetting base by the amount specified by the field,
    // and optionally an additional amount (use this with care), and then creating
    // a TypedPointer with the given field as the heap.
    TypedPointer address(LValue base, const AbstractField& field, ptrdiff_t offset = 0) { CRASH(); }

    LValue baseIndex(LValue base, LValue index, Scale, ptrdiff_t offset = 0) { CRASH(); }

    TypedPointer baseIndex(const AbstractHeap& heap, LValue base, LValue index, Scale scale, ptrdiff_t offset = 0) { CRASH(); }
    TypedPointer baseIndex(IndexedAbstractHeap& heap, LValue base, LValue index, JSValue indexAsConstant = JSValue(), ptrdiff_t offset = 0) { CRASH(); }

    TypedPointer absolute(void* address) { CRASH(); }

    LValue load8SignExt32(LValue base, const AbstractField& field) { CRASH(); }
    LValue load8ZeroExt32(LValue base, const AbstractField& field) { CRASH(); }
    LValue load16SignExt32(LValue base, const AbstractField& field) { CRASH(); }
    LValue load16ZeroExt32(LValue base, const AbstractField& field) { CRASH(); }
    LValue load32(LValue base, const AbstractField& field) { CRASH(); }
    LValue load64(LValue base, const AbstractField& field) { CRASH(); }
    LValue loadPtr(LValue base, const AbstractField& field) { CRASH(); }
    LValue loadDouble(LValue base, const AbstractField& field) { CRASH(); }
    void store32(LValue value, LValue base, const AbstractField& field) { CRASH(); }
    void store64(LValue value, LValue base, const AbstractField& field) { CRASH(); }
    void storePtr(LValue value, LValue base, const AbstractField& field) { CRASH(); }
    void storeDouble(LValue value, LValue base, const AbstractField& field) { CRASH(); }

    void ascribeRange(LValue loadInstruction, const ValueRange& range) { CRASH(); }

    LValue nonNegative32(LValue loadInstruction) { CRASH(); }

    LValue load32NonNegative(TypedPointer pointer) { CRASH(); }
    LValue load32NonNegative(LValue base, const AbstractField& field) { CRASH(); }

    LValue icmp(LIntPredicate cond, LValue left, LValue right) { CRASH(); }
    LValue equal(LValue left, LValue right) { CRASH(); }
    LValue notEqual(LValue left, LValue right) { CRASH(); }
    LValue above(LValue left, LValue right) { CRASH(); }
    LValue aboveOrEqual(LValue left, LValue right) { CRASH(); }
    LValue below(LValue left, LValue right) { CRASH(); }
    LValue belowOrEqual(LValue left, LValue right) { CRASH(); }
    LValue greaterThan(LValue left, LValue right) { CRASH(); }
    LValue greaterThanOrEqual(LValue left, LValue right) { CRASH(); }
    LValue lessThan(LValue left, LValue right) { CRASH(); }
    LValue lessThanOrEqual(LValue left, LValue right) { CRASH(); }

    LValue fcmp(LRealPredicate cond, LValue left, LValue right) { CRASH(); }
    LValue doubleEqual(LValue left, LValue right) { CRASH(); }
    LValue doubleNotEqualOrUnordered(LValue left, LValue right) { CRASH(); }
    LValue doubleLessThan(LValue left, LValue right) { CRASH(); }
    LValue doubleLessThanOrEqual(LValue left, LValue right) { CRASH(); }
    LValue doubleGreaterThan(LValue left, LValue right) { CRASH(); }
    LValue doubleGreaterThanOrEqual(LValue left, LValue right) { CRASH(); }
    LValue doubleEqualOrUnordered(LValue left, LValue right) { CRASH(); }
    LValue doubleNotEqual(LValue left, LValue right) { CRASH(); }
    LValue doubleLessThanOrUnordered(LValue left, LValue right) { CRASH(); }
    LValue doubleLessThanOrEqualOrUnordered(LValue left, LValue right) { CRASH(); }
    LValue doubleGreaterThanOrUnordered(LValue left, LValue right) { CRASH(); }
    LValue doubleGreaterThanOrEqualOrUnordered(LValue left, LValue right) { CRASH(); }

    LValue isZero32(LValue value) { CRASH(); }
    LValue notZero32(LValue value) { CRASH(); }
    LValue isZero64(LValue value) { CRASH(); }
    LValue notZero64(LValue value) { CRASH(); }
    LValue isNull(LValue value) { CRASH(); }
    LValue notNull(LValue value) { CRASH(); }

    LValue testIsZero32(LValue value, LValue mask) { CRASH(); }
    LValue testNonZero32(LValue value, LValue mask) { CRASH(); }
    LValue testIsZero64(LValue value, LValue mask) { CRASH(); }
    LValue testNonZero64(LValue value, LValue mask) { CRASH(); }
    LValue testIsZeroPtr(LValue value, LValue mask) { CRASH(); }
    LValue testNonZeroPtr(LValue value, LValue mask) { CRASH(); }

    LValue select(LValue value, LValue taken, LValue notTaken) { CRASH(); }
    LValue extractValue(LValue aggVal, unsigned index) { CRASH(); }

    LValue fence(LAtomicOrdering ordering = LLVMAtomicOrderingSequentiallyConsistent, SynchronizationScope scope = CrossThread) { CRASH(); }
    LValue fenceAcqRel() { CRASH(); }

    template<typename VectorType>
    LValue call(LValue function, const VectorType& vector) { CRASH(); }
    LValue call(LValue function) { CRASH(); }
    LValue call(LValue function, LValue arg1) { CRASH(); }
    template<typename... Args>
    LValue call(LValue function, LValue arg1, Args... args) { CRASH(); }

    template<typename FunctionType>
    LValue operation(FunctionType function) { CRASH(); }

    void jump(LBasicBlock destination) { CRASH(); }
    void branch(LValue condition, LBasicBlock taken, Weight takenWeight, LBasicBlock notTaken, Weight notTakenWeight) { CRASH(); }
    void branch(LValue condition, WeightedTarget taken, WeightedTarget notTaken) { CRASH(); }

    // Branches to an already-created handler if true, "falls through" if false. Fall-through is
    // simulated by creating a continuation for you.
    void check(LValue condition, WeightedTarget taken, Weight notTakenWeight) { CRASH(); }

    // Same as check(), but uses Weight::inverse() to compute the notTakenWeight.
    void check(LValue condition, WeightedTarget taken) { CRASH(); }

    template<typename VectorType>
    void switchInstruction(LValue value, const VectorType& cases, LBasicBlock fallThrough, Weight fallThroughWeight) { CRASH(); }

    void ret(LValue value) { CRASH(); }

    void unreachable() { CRASH(); }

    void trap() { CRASH(); }

    ValueFromBlock anchor(LValue value) { CRASH(); }

#pragma mark - Intrinsics

    LValue stackmapIntrinsic() { CRASH(); }
    LValue frameAddressIntrinsic() { CRASH(); }
    LValue patchpointInt64Intrinsic() { CRASH(); }
    LValue patchpointVoidIntrinsic() { CRASH(); }

#pragma mark - States
    B3::Procedure& m_procedure;

    LBasicBlock m_block { nullptr };
    LBasicBlock m_nextBlock { nullptr };
};

#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif // COMPILER(CLANG)

#define FTL_NEW_BLOCK(output, nameArguments) \
    (LIKELY(!verboseCompilationEnabled()) \
    ? (output).newBlock() \
    : (output).newBlock((toCString nameArguments).data()))

} } // namespace JSC::FTL

#endif // FTL_USES_B3
#endif // ENABLE(FTL_JIT)

#endif // FTLB3Output_h
