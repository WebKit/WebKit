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

#include "config.h"
#include "FTLB3Output.h"

#if ENABLE(FTL_JIT)
#if FTL_USES_B3

#include "B3MathExtras.h"

namespace JSC { namespace FTL {

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

LValue Output::lockedStackSlot(size_t bytes)
{
    return m_block->appendNew<B3::StackSlotValue>(m_proc, origin(), bytes, B3::StackSlotKind::Locked);
}

LValue Output::load(TypedPointer pointer, LType type)
{
    LValue load = m_block->appendNew<B3::MemoryValue>(m_proc, B3::Load, type, origin(), pointer.value());
    pointer.heap().decorateInstruction(load, *m_heaps);
    return load;
}

LValue Output::doublePowi(LValue x, LValue y)
{
    auto result = powDoubleInt32(m_proc, m_block, origin(), x, y);
    m_block = result.first;
    return result.second;
}

LValue Output::load8SignExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<B3::MemoryValue>(m_proc, B3::Load8S, B3::Int32, origin(), pointer.value());
    pointer.heap().decorateInstruction(load, *m_heaps);
    return load;
}

LValue Output::load8ZeroExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<B3::MemoryValue>(m_proc, B3::Load8Z, B3::Int32, origin(), pointer.value());
    pointer.heap().decorateInstruction(load, *m_heaps);
    return load;
}

LValue Output::load16SignExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<B3::MemoryValue>(m_proc, B3::Load16S, B3::Int32, origin(), pointer.value());
    pointer.heap().decorateInstruction(load, *m_heaps);
    return load;
}

LValue Output::load16ZeroExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<B3::MemoryValue>(m_proc, B3::Load16Z, B3::Int32, origin(), pointer.value());
    pointer.heap().decorateInstruction(load, *m_heaps);
    return load;
}

void Output::store(LValue value, TypedPointer pointer)
{
    LValue store = m_block->appendNew<B3::MemoryValue>(m_proc, B3::Store, origin(), value, pointer.value());
    pointer.heap().decorateInstruction(store, *m_heaps);
}

void Output::store32As8(LValue value, TypedPointer pointer)
{
    LValue store = m_block->appendNew<B3::MemoryValue>(m_proc, B3::Store8, origin(), value, pointer.value());
    pointer.heap().decorateInstruction(store, *m_heaps);
}

void Output::store32As16(LValue value, TypedPointer pointer)
{
    LValue store = m_block->appendNew<B3::MemoryValue>(m_proc, B3::Store16, origin(), value, pointer.value());
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
    m_block->appendNew<B3::ControlValue>(
        m_proc, B3::Branch, origin(), condition,
        B3::FrequentedBlock(taken, takenWeight.frequencyClass()),
        B3::FrequentedBlock(notTaken, notTakenWeight.frequencyClass()));
}

} } // namespace JSC::FTL

#endif // FTL_USES_B3
#endif // ENABLE(FTL_JIT)

