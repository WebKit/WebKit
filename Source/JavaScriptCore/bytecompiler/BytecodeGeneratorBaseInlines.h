/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#include "BytecodeGeneratorBase.h"

#include "RegisterID.h"
#include "StackAlignment.h"

namespace JSC {

template<typename T>
static inline void shrinkToFit(T& segmentedVector)
{
    while (segmentedVector.size() && !segmentedVector.last().refCount())
        segmentedVector.removeLast();
}

template<typename Traits>
BytecodeGeneratorBase<Traits>::BytecodeGeneratorBase(typename Traits::CodeBlock codeBlock, uint32_t virtualRegisterCountForCalleeSaves)
    : m_codeBlock(WTFMove(codeBlock))
{
    allocateCalleeSaveSpace(virtualRegisterCountForCalleeSaves);
}

template<typename Traits>
Ref<GenericLabel<Traits>> BytecodeGeneratorBase<Traits>::newLabel()
{
    shrinkToFit(m_labels);

    // Allocate new label ID.
    m_labels.append();
    return m_labels.last();
}

template<typename Traits>
Ref<GenericLabel<Traits>> BytecodeGeneratorBase<Traits>::newEmittedLabel()
{
    auto label = newLabel();
    emitLabel(label.get());
    return label;
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::reclaimFreeRegisters()
{
    shrinkToFit(m_calleeLocals);
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::emitLabel(GenericLabel<Traits>& label)
{
    unsigned newLabelIndex = m_writer.position();
    label.setLocation(*this, newLabelIndex);

    if (m_codeBlock->numberOfJumpTargets()) {
        unsigned lastLabelIndex = m_codeBlock->lastJumpTarget();
        ASSERT(lastLabelIndex <= newLabelIndex);
        if (newLabelIndex == lastLabelIndex) {
            // Peephole optimizations have already been disabled by emitting the last label
            return;
        }
    }

    m_codeBlock->addJumpTarget(newLabelIndex);

    m_lastOpcodeID = Traits::opcodeForDisablingOptimizations;
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::recordOpcode(typename Traits::OpcodeID opcodeID)
{
    ASSERT(m_lastOpcodeID == Traits::opcodeForDisablingOptimizations || (m_lastOpcodeID == m_lastInstruction->opcodeID<typename Traits::OpcodeTraits>() && m_writer.position() == m_lastInstruction.offset() + m_lastInstruction->size<typename Traits::OpcodeTraits>()));
    m_lastInstruction = m_writer.ref();
    m_lastOpcodeID = opcodeID;
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::alignWideOpcode16()
{
#if CPU(NEEDS_ALIGNED_ACCESS)
    size_t opcodeSize = 1;
    size_t prefixAndOpcodeSize = opcodeSize + PaddingBySize<OpcodeSize::Wide16>::value;
    while ((m_writer.position() + prefixAndOpcodeSize) % OpcodeSize::Wide16)
        Traits::OpNop::template emit<OpcodeSize::Narrow>(this);
#endif
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::alignWideOpcode32()
{
#if CPU(NEEDS_ALIGNED_ACCESS)
    size_t opcodeSize = 1;
    size_t prefixAndOpcodeSize = opcodeSize + PaddingBySize<OpcodeSize::Wide16>::value;
    while ((m_writer.position() + prefixAndOpcodeSize) % OpcodeSize::Wide32)
        Traits::OpNop::template emit<OpcodeSize::Narrow>(this);
#endif
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::write(uint8_t b)
{
    m_writer.write(b);
}


template<typename Traits>
void BytecodeGeneratorBase<Traits>::write(uint16_t h)
{
    m_writer.write(h);
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::write(uint32_t i)
{
    m_writer.write(i);
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::write(int8_t b)
{
    m_writer.write(static_cast<uint8_t>(b));
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::write(int16_t h)
{
    m_writer.write(static_cast<uint16_t>(h));
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::write(int32_t i)
{
    m_writer.write(static_cast<uint32_t>(i));
}

template<typename Traits>
RegisterID* BytecodeGeneratorBase<Traits>::newRegister()
{
    m_calleeLocals.append(virtualRegisterForLocal(m_calleeLocals.size()));
    int numCalleeLocals = std::max<int>(m_codeBlock->numCalleeLocals(), m_calleeLocals.size());
    numCalleeLocals = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), numCalleeLocals);
    m_codeBlock->setNumCalleeLocals(numCalleeLocals);
    return &m_calleeLocals.last();
}

template<typename Traits>
RegisterID* BytecodeGeneratorBase<Traits>::newTemporary()
{
    reclaimFreeRegisters();

    RegisterID* result = newRegister();
    result->setTemporary();
    return result;
}

// Adds an anonymous local var slot. To give this slot a name, add it to symbolTable().
template<typename Traits>
RegisterID* BytecodeGeneratorBase<Traits>::addVar()
{
    int numVars = m_codeBlock->numVars();
    m_codeBlock->setNumVars(numVars + 1);
    RegisterID* result = newRegister();
    ASSERT(VirtualRegister(result->index()).toLocal() == numVars);
    result->ref(); // We should never free this slot.
    return result;
}

template<typename Traits>
void BytecodeGeneratorBase<Traits>::allocateCalleeSaveSpace(uint32_t virtualRegisterCountForCalleeSaves)
{
    for (size_t i = 0; i < virtualRegisterCountForCalleeSaves; i++)
        addVar();
}

} // namespace JSC
