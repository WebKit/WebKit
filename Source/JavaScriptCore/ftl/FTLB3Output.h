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

#include "B3ArgumentRegValue.h"
#include "B3BasicBlockInlines.h"
#include "B3CCallValue.h"
#include "B3Compilation.h"
#include "B3Const32Value.h"
#include "B3ConstPtrValue.h"
#include "B3ControlValue.h"
#include "B3MemoryValue.h"
#include "B3Procedure.h"
#include "B3StackSlotValue.h"
#include "B3SwitchValue.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"
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
#if COMPILER(GCC_OR_CLANG)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-noreturn"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif // COMPILER(GCC_OR_CLANG)

namespace JSC {

namespace DFG { struct Node; }

namespace FTL {

enum Scale { ScaleOne, ScaleTwo, ScaleFour, ScaleEight, ScalePtr };

class Output : public CommonValues {
public:
    Output(State&);
    ~Output();

    void initialize(AbstractHeapRepository&);

    void setFrequency(double value)
    {
        m_frequency = value;
    }

    LBasicBlock newBlock(const char* name = "")
    {
        UNUSED_PARAM(name);
        return m_proc.addBlock(m_frequency);
    }

    LBasicBlock insertNewBlocksBefore(LBasicBlock nextBlock)
    {
        LBasicBlock lastNextBlock = m_nextBlock;
        m_nextBlock = nextBlock;
        return lastNextBlock;
    }

    LBasicBlock appendTo(LBasicBlock, LBasicBlock nextBlock);
    void appendTo(LBasicBlock);

    void setOrigin(DFG::Node* node) { m_origin = node; }
    B3::Origin origin() { return B3::Origin(m_origin); }

    LValue framePointer() { return m_block->appendNew<B3::Value>(m_proc, B3::FramePointer, origin()); }

    B3::StackSlotValue* lockedStackSlot(size_t bytes);

    LValue constBool(bool value) { return m_block->appendNew<B3::Const32Value>(m_proc, origin(), value); }
    LValue constInt32(int32_t value) { return m_block->appendNew<B3::Const32Value>(m_proc, origin(), value); }
    template<typename T>
    LValue constIntPtr(T* value) { return m_block->appendNew<B3::ConstPtrValue>(m_proc, origin(), value); }
    template<typename T>
    LValue constIntPtr(T value) { return m_block->appendNew<B3::ConstPtrValue>(m_proc, origin(), value); }
    LValue constInt64(int64_t value) { return m_block->appendNew<B3::Const64Value>(m_proc, origin(), value); }
    LValue constDouble(double value) { return m_block->appendNew<B3::ConstDoubleValue>(m_proc, origin(), value); }

    LValue phi(LType type) { return m_block->appendNew<B3::Value>(m_proc, B3::Phi, type, origin()); }
    template<typename... Params>
    LValue phi(LType, ValueFromBlock, Params... theRest);
    template<typename VectorType>
    LValue phi(LType, const VectorType&);
    void addIncomingToPhi(LValue phi, ValueFromBlock);
    template<typename... Params>
    void addIncomingToPhi(LValue phi, ValueFromBlock, Params... theRest);

    LValue add(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Add, origin(), left, right); }
    LValue sub(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Sub, origin(), left, right); }
    LValue mul(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Mul, origin(), left, right); }
    LValue div(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Div, origin(), left, right); }
    LValue chillDiv(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::ChillDiv, origin(), left, right); }
    LValue mod(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Mod, origin(), left, right); }
    LValue chillMod(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::ChillMod, origin(), left, right); }
    LValue neg(LValue value)
    {
        LValue zero = m_block->appendIntConstant(m_proc, origin(), value->type(), 0);
        return sub(zero, value);
    }

    LValue doubleAdd(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Add, origin(), left, right); }
    LValue doubleSub(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Sub, origin(), left, right); }
    LValue doubleMul(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Mul, origin(), left, right); }
    LValue doubleDiv(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Div, origin(), left, right); }
    LValue doubleMod(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Mod, origin(), left, right); }
    LValue doubleNeg(LValue value)
    {
        return sub(doubleZero, value);
    }

    LValue bitAnd(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::BitAnd, origin(), left, right); }
    LValue bitOr(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::BitOr, origin(), left, right); }
    LValue bitXor(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::BitXor, origin(), left, right); }
    LValue shl(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Shl, origin(), left, castToInt32(right)); }
    LValue aShr(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::SShr, origin(), left, castToInt32(right)); }
    LValue lShr(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::ZShr, origin(), left, castToInt32(right)); }
    LValue bitNot(LValue);
    LValue logicalNot(LValue);

    LValue ctlz32(LValue operand) { return m_block->appendNew<B3::Value>(m_proc, B3::Clz, origin(), operand); }
    LValue addWithOverflow32(LValue left, LValue right) { CRASH(); }
    LValue subWithOverflow32(LValue left, LValue right) { CRASH(); }
    LValue mulWithOverflow32(LValue left, LValue right) { CRASH(); }
    LValue addWithOverflow64(LValue left, LValue right) { CRASH(); }
    LValue subWithOverflow64(LValue left, LValue right) { CRASH(); }
    LValue mulWithOverflow64(LValue left, LValue right) { CRASH(); }
    LValue doubleAbs(LValue value) { return m_block->appendNew<B3::Value>(m_proc, B3::Abs, origin(), value); }
    LValue doubleCeil(LValue operand) { return m_block->appendNew<B3::Value>(m_proc, B3::Ceil, origin(), operand); }

    LValue doubleSin(LValue value) { return callWithoutSideEffects(B3::Double, sin, value); }
    LValue doubleCos(LValue value) { return callWithoutSideEffects(B3::Double, cos, value); }

    LValue doublePow(LValue xOperand, LValue yOperand) { return callWithoutSideEffects(B3::Double, pow, xOperand, yOperand); }

    LValue doublePowi(LValue xOperand, LValue yOperand);

    LValue doubleSqrt(LValue value) { return m_block->appendNew<B3::Value>(m_proc, B3::Sqrt, origin(), value); }

    LValue doubleLog(LValue value) { return callWithoutSideEffects(B3::Double, log, value); }

    static bool hasSensibleDoubleToInt();
    LValue doubleToInt(LValue);
    LValue doubleToUInt(LValue);

    LValue signExt32To64(LValue value) { return m_block->appendNew<B3::Value>(m_proc, B3::SExt32, origin(), value); }
    LValue zeroExt(LValue value, LType type)
    {
        if (value->type() == type)
            return value;
        return m_block->appendNew<B3::Value>(m_proc, B3::ZExt32, origin(), value);
    }
    LValue zeroExtPtr(LValue value) { return zeroExt(value, B3::Int64); }
    LValue intToDouble(LValue value) { return m_block->appendNew<B3::Value>(m_proc, B3::IToD, origin(), value); }
    LValue unsignedToDouble(LValue value) { CRASH(); }
    LValue castToInt32(LValue value)
    {
        return value->type() == B3::Int32 ? value :
            m_block->appendNew<B3::Value>(m_proc, B3::Trunc, origin(), value);
    }
    LValue doubleToFloat(LValue value) { return m_block->appendNew<B3::Value>(m_proc, B3::DoubleToFloat, origin(), value); }
    LValue floatToDouble(LValue value) { return m_block->appendNew<B3::Value>(m_proc, B3::FloatToDouble, origin(), value); }
    LValue bitCast(LValue, LType);
    LValue fround(LValue doubleValue);

    LValue load(TypedPointer, LType);
    void store(LValue, TypedPointer);

    LValue load8SignExt32(TypedPointer);
    LValue load8ZeroExt32(TypedPointer);
    LValue load16SignExt32(TypedPointer);
    LValue load16ZeroExt32(TypedPointer);
    LValue load32(TypedPointer pointer) { return load(pointer, B3::Int32); }
    LValue load64(TypedPointer pointer) { return load(pointer, B3::Int64); }
    LValue loadPtr(TypedPointer pointer) { return load(pointer, B3::pointerType()); }
    LValue loadFloat(TypedPointer pointer) { return load(pointer, B3::Float); }
    LValue loadDouble(TypedPointer pointer) { return load(pointer, B3::Double); }
    void store32As8(LValue value, TypedPointer pointer);
    void store32As16(LValue value, TypedPointer pointer);
    void store32(LValue value, TypedPointer pointer)
    {
        ASSERT(value->type() == B3::Int32);
        store(value, pointer);
    }
    void store64(LValue value, TypedPointer pointer)
    {
        ASSERT(value->type() == B3::Int64);
        store(value, pointer);
    }
    void storePtr(LValue value, TypedPointer pointer)
    {
        ASSERT(value->type() == B3::pointerType());
        store(value, pointer);
    }
    void storeFloat(LValue value, TypedPointer pointer)
    {
        ASSERT(value->type() == B3::Float);
        store(value, pointer);
    }
    void storeDouble(LValue value, TypedPointer pointer)
    {
        ASSERT(value->type() == B3::Double);
        store(value, pointer);
    }

    enum LoadType {
        Load8SignExt32,
        Load8ZeroExt32,
        Load16SignExt32,
        Load16ZeroExt32,
        Load32,
        Load64,
        LoadPtr,
        LoadFloat,
        LoadDouble
    };

    LValue load(TypedPointer, LoadType);
    
    enum StoreType {
        Store32As8,
        Store32As16,
        Store32,
        Store64,
        StorePtr,
        StoreFloat,
        StoreDouble
    };

    void store(LValue, TypedPointer, StoreType);

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

    LValue load8SignExt32(LValue base, const AbstractField& field) { return load8SignExt32(address(base, field)); }
    LValue load8ZeroExt32(LValue base, const AbstractField& field) { return load8ZeroExt32(address(base, field)); }
    LValue load16SignExt32(LValue base, const AbstractField& field) { return load16SignExt32(address(base, field)); }
    LValue load16ZeroExt32(LValue base, const AbstractField& field) { return load16ZeroExt32(address(base, field)); }
    LValue load32(LValue base, const AbstractField& field) { return load32(address(base, field)); }
    LValue load64(LValue base, const AbstractField& field) { return load64(address(base, field)); }
    LValue loadPtr(LValue base, const AbstractField& field) { return loadPtr(address(base, field)); }
    LValue loadDouble(LValue base, const AbstractField& field) { return loadDouble(address(base, field)); }
    void store32(LValue value, LValue base, const AbstractField& field) { store32(value, address(base, field)); }
    void store64(LValue value, LValue base, const AbstractField& field) { store64(value, address(base, field)); }
    void storePtr(LValue value, LValue base, const AbstractField& field) { storePtr(value, address(base, field)); }
    void storeDouble(LValue value, LValue base, const AbstractField& field) { storeDouble(value, address(base, field)); }

    // FIXME: Explore adding support for value range constraints to B3. Maybe it could be as simple as having
    // a load instruction that guarantees that its result is non-negative.
    // https://bugs.webkit.org/show_bug.cgi?id=151458
    void ascribeRange(LValue, const ValueRange&) { }
    LValue nonNegative32(LValue loadInstruction) { return loadInstruction; }
    LValue load32NonNegative(TypedPointer pointer) { return load32(pointer); }
    LValue load32NonNegative(LValue base, const AbstractField& field) { return load32(base, field); }

    LValue equal(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Equal, origin(), left, right); }
    LValue notEqual(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::NotEqual, origin(), left, right); }
    LValue above(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Above, origin(), left, right); }
    LValue aboveOrEqual(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::AboveEqual, origin(), left, right); }
    LValue below(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Below, origin(), left, right); }
    LValue belowOrEqual(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::BelowEqual, origin(), left, right); }
    LValue greaterThan(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::GreaterThan, origin(), left, right); }
    LValue greaterThanOrEqual(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::GreaterEqual, origin(), left, right); }
    LValue lessThan(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::LessThan, origin(), left, right); }
    LValue lessThanOrEqual(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::LessEqual, origin(), left, right); }

    LValue doubleEqual(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::Equal, origin(), left, right); }
    LValue doubleEqualOrUnordered(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::EqualOrUnordered, origin(), left, right); }
    LValue doubleNotEqualOrUnordered(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::NotEqual, origin(), left, right); }
    LValue doubleLessThan(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::LessThan, origin(), left, right); }
    LValue doubleLessThanOrEqual(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::LessEqual, origin(), left, right); }
    LValue doubleGreaterThan(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::GreaterThan, origin(), left, right); }
    LValue doubleGreaterThanOrEqual(LValue left, LValue right) { return m_block->appendNew<B3::Value>(m_proc, B3::GreaterEqual, origin(), left, right); }
    LValue doubleNotEqualAndOrdered(LValue left, LValue right) { return logicalNot(doubleEqualOrUnordered(left, right)); }
    LValue doubleLessThanOrUnordered(LValue left, LValue right) { return logicalNot(doubleGreaterThanOrEqual(left, right)); }
    LValue doubleLessThanOrEqualOrUnordered(LValue left, LValue right) { return logicalNot(doubleGreaterThan(left, right)); }
    LValue doubleGreaterThanOrUnordered(LValue left, LValue right) { return logicalNot(doubleLessThanOrEqual(left, right)); }
    LValue doubleGreaterThanOrEqualOrUnordered(LValue left, LValue right) { return logicalNot(doubleLessThan(left, right)); }

    LValue isZero32(LValue value) { return m_block->appendNew<B3::Value>(m_proc, B3::Equal, origin(), value, int32Zero); }
    LValue notZero32(LValue value) { return m_block->appendNew<B3::Value>(m_proc, B3::NotEqual, origin(), value, int32Zero); }
    LValue isZero64(LValue value) { return m_block->appendNew<B3::Value>(m_proc, B3::Equal, origin(), value, int64Zero); }
    LValue notZero64(LValue value) { return m_block->appendNew<B3::Value>(m_proc, B3::NotEqual, origin(), value, int64Zero); }
    LValue isNull(LValue value) { return isZero64(value); }
    LValue notNull(LValue value) { return notZero64(value); }

    LValue testIsZero32(LValue value, LValue mask) { return isZero32(bitAnd(value, mask)); }
    LValue testNonZero32(LValue value, LValue mask) { return notZero32(bitAnd(value, mask)); }
    LValue testIsZero64(LValue value, LValue mask) { return isZero64(bitAnd(value, mask)); }
    LValue testNonZero64(LValue value, LValue mask) { return notZero64(bitAnd(value, mask)); }
    LValue testIsZeroPtr(LValue value, LValue mask) { return isNull(bitAnd(value, mask)); }
    LValue testNonZeroPtr(LValue value, LValue mask) { return notNull(bitAnd(value, mask)); }

    LValue select(LValue value, LValue taken, LValue notTaken) { return m_block->appendNew<B3::Value>(m_proc, B3::Select, origin(), value, taken, notTaken); }
    LValue extractValue(LValue aggVal, unsigned index) { CRASH(); }

    template<typename VectorType>
    LValue call(LType type, LValue function, const VectorType& vector)
    {
        B3::CCallValue* result = m_block->appendNew<B3::CCallValue>(m_proc, type, origin(), function);
        result->children().appendVector(vector);
        return result;
    }
    LValue call(LType type, LValue function) { return m_block->appendNew<B3::CCallValue>(m_proc, type, origin(), function); }
    LValue call(LType type, LValue function, LValue arg1) { return m_block->appendNew<B3::CCallValue>(m_proc, type, origin(), function, arg1); }
    template<typename... Args>
    LValue call(LType type, LValue function, LValue arg1, Args... args) { return m_block->appendNew<B3::CCallValue>(m_proc, type, origin(), function, arg1, args...); }

    template<typename FunctionType>
    LValue operation(FunctionType function) { return constIntPtr(bitwise_cast<void*>(function)); }

    void jump(LBasicBlock destination) { m_block->appendNew<B3::ControlValue>(m_proc, B3::Jump, origin(), B3::FrequentedBlock(destination)); }
    void branch(LValue condition, LBasicBlock taken, Weight takenWeight, LBasicBlock notTaken, Weight notTakenWeight);
    void branch(LValue condition, WeightedTarget taken, WeightedTarget notTaken)
    {
        branch(condition, taken.target(), taken.weight(), notTaken.target(), notTaken.weight());
    }

    // Branches to an already-created handler if true, "falls through" if false. Fall-through is
    // simulated by creating a continuation for you.
    void check(LValue condition, WeightedTarget taken, Weight notTakenWeight) { CRASH(); }

    // Same as check(), but uses Weight::inverse() to compute the notTakenWeight.
    void check(LValue condition, WeightedTarget taken) { CRASH(); }

    template<typename VectorType>
    void switchInstruction(LValue value, const VectorType& cases, LBasicBlock fallThrough, Weight fallThroughWeight)
    {
        B3::SwitchValue* switchValue = m_block->appendNew<B3::SwitchValue>(
            m_proc, origin(), value, B3::FrequentedBlock(fallThrough));
        for (const SwitchCase& switchCase : cases) {
            int64_t value = switchCase.value()->asInt();
            B3::FrequentedBlock target(switchCase.target(), switchCase.weight().frequencyClass());
            switchValue->appendCase(B3::SwitchCase(value, target));
        }
    }

    void ret(LValue value) { m_block->appendNew<B3::ControlValue>(m_proc, B3::Return, origin(), value); }

    void unreachable() { m_block->appendNew<B3::ControlValue>(m_proc, B3::Oops, origin()); }

    template<typename Functor>
    void speculate(LValue value, const StackmapArgumentList& arguments, const Functor& functor)
    {
        B3::CheckValue* check = speculate(value, arguments);
        check->setGenerator(functor);
    }

    B3::CheckValue* speculate(LValue value, const StackmapArgumentList& arguments)
    {
        B3::CheckValue* check = speculate(value);
        for (LValue value : arguments)
            check->append(B3::ConstrainedValue(value));
        return check;
    }

    B3::CheckValue* speculate(LValue value)
    {
        return m_block->appendNew<B3::CheckValue>(m_proc, B3::Check, origin(), value);
    }

    B3::CheckValue* speculateAdd(LValue left, LValue right)
    {
        return m_block->appendNew<B3::CheckValue>(m_proc, B3::CheckAdd, origin(), left, right);
    }

    B3::CheckValue* speculateSub(LValue left, LValue right)
    {
        return m_block->appendNew<B3::CheckValue>(m_proc, B3::CheckSub, origin(), left, right);
    }

    B3::CheckValue* speculateMul(LValue left, LValue right)
    {
        return m_block->appendNew<B3::CheckValue>(m_proc, B3::CheckMul, origin(), left, right);
    }

    B3::PatchpointValue* patchpoint(LType type)
    {
        return m_block->appendNew<B3::PatchpointValue>(m_proc, type, origin());
    }

    void trap()
    {
        m_block->appendNew<B3::ControlValue>(m_proc, B3::Oops, origin());
    }

    ValueFromBlock anchor(LValue value)
    {
        B3::UpsilonValue* upsilon = m_block->appendNew<B3::UpsilonValue>(m_proc, origin(), value);
        return ValueFromBlock(upsilon, m_block);
    }

#if PLATFORM(COCOA)
#pragma mark - States
#endif
    B3::Procedure& m_proc;

    DFG::Node* m_origin { nullptr };
    LBasicBlock m_block { nullptr };
    LBasicBlock m_nextBlock { nullptr };

    AbstractHeapRepository* m_heaps;

    double m_frequency { 1 };

private:
    template<typename Function, typename... Args>
    LValue callWithoutSideEffects(B3::Type type, Function function, LValue arg1, Args... args)
    {
        return m_block->appendNew<B3::CCallValue>(m_proc, type, origin(), B3::Effects::none(),
            m_block->appendNew<B3::ConstPtrValue>(m_proc, origin(), bitwise_cast<void*>(function)),
            arg1, args...);
    }

};

template<typename... Params>
inline LValue Output::phi(LType type, ValueFromBlock value, Params... theRest)
{
    LValue phiNode = phi(type);
    addIncomingToPhi(phiNode, value, theRest...);
    return phiNode;
}

template<typename VectorType>
inline LValue Output::phi(LType type, const VectorType& vector)
{
    LValue phiNode = phi(type);
    for (const ValueFromBlock& valueFromBlock : vector)
        addIncomingToPhi(phiNode, valueFromBlock);
    return phiNode;
}

inline void Output::addIncomingToPhi(LValue phi, ValueFromBlock value)
{
    value.value()->as<B3::UpsilonValue>()->setPhi(phi);
}

template<typename... Params>
inline void Output::addIncomingToPhi(LValue phi, ValueFromBlock value, Params... theRest)
{
    addIncomingToPhi(phi, value);
    addIncomingToPhi(phi, theRest...);
}

inline LValue Output::bitCast(LValue value, LType type)
{
    ASSERT_UNUSED(type, type == int64 || type == doubleType);
    return m_block->appendNew<B3::Value>(m_proc, B3::BitwiseCast, origin(), value);
}

inline LValue Output::fround(LValue doubleValue)
{
    return floatToDouble(doubleToFloat(doubleValue));
}

#if COMPILER(GCC_OR_CLANG)
#pragma GCC diagnostic pop
#endif // COMPILER(GCC_OR_CLANG)

#define FTL_NEW_BLOCK(output, nameArguments) \
    (LIKELY(!verboseCompilationEnabled()) \
    ? (output).newBlock() \
    : (output).newBlock((toCString nameArguments).data()))

} } // namespace JSC::FTL

#endif // FTL_USES_B3
#endif // ENABLE(FTL_JIT)

#endif // FTLB3Output_h
