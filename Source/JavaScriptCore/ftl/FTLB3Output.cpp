/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#include "FTLB3Output.h"

#if ENABLE(FTL_JIT)
#if FTL_USES_B3

#include "B3MathExtras.h"
#include "B3StackmapGenerationParams.h"

namespace JSC { namespace FTL {

using namespace B3;

Output::Output(State& state)
    : CommonValues(state.context)
    , m_proc(*state.proc)
{
}

Output::~Output()
{
}

void Output::initialize(AbstractHeapRepository& heaps)
{
    m_heaps = &heaps;
}

LBasicBlock Output::appendTo(LBasicBlock block, LBasicBlock nextBlock)
{
    appendTo(block);
    return insertNewBlocksBefore(nextBlock);
}

void Output::appendTo(LBasicBlock block)
{
    m_block = block;
}

StackSlotValue* Output::lockedStackSlot(size_t bytes)
{
    return m_block->appendNew<StackSlotValue>(m_proc, origin(), bytes, StackSlotKind::Locked);
}

LValue Output::bitNot(LValue value)
{
    return m_block->appendNew<B3::Value>(m_proc, B3::BitXor, origin(),
        value,
        m_block->appendIntConstant(m_proc, origin(), value->type(), -1));
}

LValue Output::logicalNot(LValue value)
{
    return m_block->appendNew<B3::Value>(m_proc, B3::Equal, origin(), value, int32Zero);
}

LValue Output::load(TypedPointer pointer, LType type)
{
    LValue load = m_block->appendNew<MemoryValue>(m_proc, Load, type, origin(), pointer.value());
    pointer.heap().decorateInstruction(load, *m_heaps);
    return load;
}

LValue Output::doublePowi(LValue x, LValue y)
{
    // FIXME: powDoubleInt32() should be inlined here since Output knows about block layout and
    // should be involved in any operation that creates blocks.
    // https://bugs.webkit.org/show_bug.cgi?id=152223
    auto result = powDoubleInt32(m_proc, m_block, origin(), x, y);
    m_block = result.first;
    return result.second;
}

bool Output::hasSensibleDoubleToInt()
{
    return optimizeForX86();
}

LValue Output::doubleToInt(LValue value)
{
    PatchpointValue* result = patchpoint(Int32);
    result->append(value, ValueRep::SomeRegister);
    result->setGenerator(
        [] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
        });
    result->effects = Effects::none();
    return result;
}

LValue Output::doubleToUInt(LValue value)
{
    PatchpointValue* result = patchpoint(Int32);
    result->append(value, ValueRep::SomeRegister);
    result->setGenerator(
        [] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
        });
    result->effects = Effects::none();
    return result;
}

LValue Output::unsignedToDouble(LValue value)
{
    return intToDouble(zeroExt(value, Int64));
}

LValue Output::load8SignExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<MemoryValue>(m_proc, Load8S, Int32, origin(), pointer.value());
    pointer.heap().decorateInstruction(load, *m_heaps);
    return load;
}

LValue Output::load8ZeroExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), pointer.value());
    pointer.heap().decorateInstruction(load, *m_heaps);
    return load;
}

LValue Output::load16SignExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<MemoryValue>(m_proc, Load16S, Int32, origin(), pointer.value());
    pointer.heap().decorateInstruction(load, *m_heaps);
    return load;
}

LValue Output::load16ZeroExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<MemoryValue>(m_proc, Load16Z, Int32, origin(), pointer.value());
    pointer.heap().decorateInstruction(load, *m_heaps);
    return load;
}

void Output::store(LValue value, TypedPointer pointer)
{
    LValue store = m_block->appendNew<MemoryValue>(m_proc, Store, origin(), value, pointer.value());
    pointer.heap().decorateInstruction(store, *m_heaps);
}

void Output::store32As8(LValue value, TypedPointer pointer)
{
    LValue store = m_block->appendNew<MemoryValue>(m_proc, Store8, origin(), value, pointer.value());
    pointer.heap().decorateInstruction(store, *m_heaps);
}

void Output::store32As16(LValue value, TypedPointer pointer)
{
    LValue store = m_block->appendNew<MemoryValue>(m_proc, Store16, origin(), value, pointer.value());
    pointer.heap().decorateInstruction(store, *m_heaps);
}

LValue Output::baseIndex(LValue base, LValue index, Scale scale, ptrdiff_t offset)
{
    LValue accumulatedOffset;
        
    switch (scale) {
    case ScaleOne:
        accumulatedOffset = index;
        break;
    case ScaleTwo:
        accumulatedOffset = shl(index, intPtrOne);
        break;
    case ScaleFour:
        accumulatedOffset = shl(index, intPtrTwo);
        break;
    case ScaleEight:
    case ScalePtr:
        accumulatedOffset = shl(index, intPtrThree);
        break;
    }
        
    if (offset)
        accumulatedOffset = add(accumulatedOffset, constIntPtr(offset));
        
    return add(base, accumulatedOffset);
}

void Output::branch(LValue condition, LBasicBlock taken, Weight takenWeight, LBasicBlock notTaken, Weight notTakenWeight)
{
    m_block->appendNew<ControlValue>(
        m_proc, B3::Branch, origin(), condition,
        FrequentedBlock(taken, takenWeight.frequencyClass()),
        FrequentedBlock(notTaken, notTakenWeight.frequencyClass()));
}

void Output::check(LValue condition, WeightedTarget taken, Weight notTakenWeight)
{
    LBasicBlock continuation = FTL_NEW_BLOCK(*this, ("Output::check continuation"));
    branch(condition, taken, WeightedTarget(continuation, notTakenWeight));
    appendTo(continuation);
}

void Output::check(LValue condition, WeightedTarget taken)
{
    check(condition, taken, taken.weight().inverse());
}

} } // namespace JSC::FTL

#endif // FTL_USES_B3
#endif // ENABLE(FTL_JIT)

