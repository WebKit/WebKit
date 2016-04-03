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
#include "FTLOutput.h"

#if ENABLE(FTL_JIT)

#include "B3MathExtras.h"
#include "B3StackmapGenerationParams.h"
#include "SuperSampler.h"

namespace JSC { namespace FTL {

using namespace B3;

Output::Output(State& state)
    : m_proc(*state.proc)
{
}

Output::~Output()
{
}

void Output::initialize(AbstractHeapRepository& heaps)
{
    m_heaps = &heaps;
}

LBasicBlock Output::newBlock()
{
    LBasicBlock result = m_proc.addBlock(m_frequency);

    if (!m_nextBlock)
        m_blockOrder.append(result);
    else
        m_blockOrder.insertBefore(m_nextBlock, result);

    return result;
}

void Output::applyBlockOrder()
{
    m_proc.setBlockOrder(m_blockOrder);
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

SlotBaseValue* Output::lockedStackSlot(size_t bytes)
{
    return m_block->appendNew<SlotBaseValue>(m_proc, origin(), m_proc.addStackSlot(bytes));
}

LValue Output::neg(LValue value)
{
    return m_block->appendNew<Value>(m_proc, B3::Neg, origin(), value);
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
    m_heaps->decorateMemory(pointer.heap(), load);
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

LValue Output::doubleTrunc(LValue value)
{
    if (MacroAssembler::supportsFloatingPointRounding()) {
        PatchpointValue* result = patchpoint(Double);
        result->append(value, ValueRep::SomeRegister);
        result->setGenerator(
            [] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                jit.roundTowardZeroDouble(params[1].fpr(), params[0].fpr());
            });
        result->effects = Effects::none();
        return result;
    }
    double (*truncDouble)(double) = trunc;
    return callWithoutSideEffects(Double, truncDouble, value);
}

LValue Output::unsignedToDouble(LValue value)
{
    return intToDouble(zeroExt(value, Int64));
}

LValue Output::load8SignExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<MemoryValue>(m_proc, Load8S, Int32, origin(), pointer.value());
    m_heaps->decorateMemory(pointer.heap(), load);
    return load;
}

LValue Output::load8ZeroExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), pointer.value());
    m_heaps->decorateMemory(pointer.heap(), load);
    return load;
}

LValue Output::load16SignExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<MemoryValue>(m_proc, Load16S, Int32, origin(), pointer.value());
    m_heaps->decorateMemory(pointer.heap(), load);
    return load;
}

LValue Output::load16ZeroExt32(TypedPointer pointer)
{
    LValue load = m_block->appendNew<MemoryValue>(m_proc, Load16Z, Int32, origin(), pointer.value());
    m_heaps->decorateMemory(pointer.heap(), load);
    return load;
}

void Output::store(LValue value, TypedPointer pointer)
{
    LValue store = m_block->appendNew<MemoryValue>(m_proc, Store, origin(), value, pointer.value());
    m_heaps->decorateMemory(pointer.heap(), store);
}

void Output::store32As8(LValue value, TypedPointer pointer)
{
    LValue store = m_block->appendNew<MemoryValue>(m_proc, Store8, origin(), value, pointer.value());
    m_heaps->decorateMemory(pointer.heap(), store);
}

void Output::store32As16(LValue value, TypedPointer pointer)
{
    LValue store = m_block->appendNew<MemoryValue>(m_proc, Store16, origin(), value, pointer.value());
    m_heaps->decorateMemory(pointer.heap(), store);
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
    LBasicBlock continuation = newBlock();
    branch(condition, taken, WeightedTarget(continuation, notTakenWeight));
    appendTo(continuation);
}

void Output::check(LValue condition, WeightedTarget taken)
{
    check(condition, taken, taken.weight().inverse());
}

LValue Output::load(TypedPointer pointer, LoadType type)
{
    switch (type) {
    case Load8SignExt32:
        return load8SignExt32(pointer);
    case Load8ZeroExt32:
        return load8ZeroExt32(pointer);
    case Load16SignExt32:
        return load8SignExt32(pointer);
    case Load16ZeroExt32:
        return load8ZeroExt32(pointer);
    case Load32:
        return load32(pointer);
    case Load64:
        return load64(pointer);
    case LoadPtr:
        return loadPtr(pointer);
    case LoadFloat:
        return loadFloat(pointer);
    case LoadDouble:
        return loadDouble(pointer);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

void Output::store(LValue value, TypedPointer pointer, StoreType type)
{
    switch (type) {
    case Store32As8:
        store32As8(value, pointer);
        return;
    case Store32As16:
        store32As16(value, pointer);
        return;
    case Store32:
        store32(value, pointer);
        return;
    case Store64:
        store64(value, pointer);
        return;
    case StorePtr:
        storePtr(value, pointer);
        return;
    case StoreFloat:
        storeFloat(value, pointer);
        return;
    case StoreDouble:
        storeDouble(value, pointer);
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

TypedPointer Output::absolute(void* address)
{
    return TypedPointer(m_heaps->absolute[address], constIntPtr(address));
}

void Output::incrementSuperSamplerCount()
{
    TypedPointer counter = absolute(bitwise_cast<void*>(&g_superSamplerCount));
    store32(add(load32(counter), int32One), counter);
}

void Output::decrementSuperSamplerCount()
{
    TypedPointer counter = absolute(bitwise_cast<void*>(&g_superSamplerCount));
    store32(sub(load32(counter), int32One), counter);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

