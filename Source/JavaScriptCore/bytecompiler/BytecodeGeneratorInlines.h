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

#include "BytecodeGenerator.h"

#include "Fits.h"
#include "RegisterID.h"
#include "StackAlignment.h"

namespace JSC {

template<typename T>
static inline void shrinkToFit(T& segmentedVector)
{
    while (segmentedVector.size() && !segmentedVector.last().refCount())
        segmentedVector.removeLast();
}

inline Ref<Label> BytecodeGenerator::newLabel()
{
    shrinkToFit(m_labels);

    // Allocate new label ID.
    m_labels.append();
    return m_labels.last();
}

inline Ref<Label> BytecodeGenerator::newEmittedLabel()
{
    auto label = newLabel();
    emitLabel(label.get());
    return label;
}

inline void BytecodeGenerator::reclaimFreeRegisters()
{
    shrinkToFit(m_calleeLocals);
}

inline void BytecodeGenerator::emitLabel(Label& label)
{
    label.setLocation(*this, m_writer.position());
    m_lastOpcodeID = op_debug;
}

inline void BytecodeGenerator::recordOpcode(OpcodeID opcodeID)
{
    ASSERT(m_lastOpcodeID == op_debug || (m_lastOpcodeID == m_lastInstruction->opcodeID() && m_writer.position() == m_lastInstruction.offset() + m_lastInstruction->size()));
    m_lastInstruction = m_writer.ref();
    m_lastOpcodeID = opcodeID;
}

template<OpcodeSize size, typename... Ops>
void BytecodeGenerator::writeOpcode(OpcodeID opcodeID, Ops... ops)
{
    constexpr auto opcodeIDSize = OpcodeIDWidthBySize<JSOpcodeTraits, size>::opcodeIDSize;
    if constexpr (size == OpcodeSize::Wide16) {
        m_writer.write(
            Fits<OpcodeID, OpcodeSize::Narrow>::convert(op_wide16),
            Fits<OpcodeID, opcodeIDSize>::convert(opcodeID),
            Fits<Ops, size>::convert(ops)...);
    } else if constexpr (size == OpcodeSize::Wide32) {
        m_writer.write(
            Fits<OpcodeID, OpcodeSize::Narrow>::convert(op_wide32),
            Fits<OpcodeID, opcodeIDSize>::convert(opcodeID),
            Fits<Ops, size>::convert(ops)...);
    } else {
        m_writer.write(
            Fits<OpcodeID, opcodeIDSize>::convert(opcodeID),
            Fits<Ops, size>::convert(ops)...);
    }
}

inline RegisterID* BytecodeGenerator::newRegister()
{
    m_calleeLocals.append(virtualRegisterForLocal(m_calleeLocals.size()));
    size_t numCalleeLocals = std::max<size_t>(m_codeBlock->numCalleeLocals(), m_calleeLocals.size());
    numCalleeLocals = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), numCalleeLocals);
    m_codeBlock->setNumCalleeLocals(static_cast<unsigned>(numCalleeLocals));
    RELEASE_ASSERT(numCalleeLocals == m_codeBlock->numCalleeLocals());
    return &m_calleeLocals.last();
}

inline RegisterID* BytecodeGenerator::newTemporary()
{
    reclaimFreeRegisters();

    RegisterID* result = newRegister();
    result->setTemporary();
    return result;
}

template<typename Functor>
void BytecodeGenerator::newTemporaries(size_t count, NOESCAPE const Functor& func)
{
    reclaimFreeRegisters();
    for (size_t index = 0; index < count; ++index) {
        RegisterID* result = newRegister();
        result->setTemporary();
        func(result);
    }
}

// Adds an anonymous local var slot. To give this slot a name, add it to symbolTable().
inline RegisterID* BytecodeGenerator::addVar()
{
    int numVars = m_codeBlock->numVars();
    m_codeBlock->setNumVars(numVars + 1);
    RegisterID* result = newRegister();
    ASSERT(VirtualRegister(result->index()).toLocal() == numVars);
    result->ref(); // We should never free this slot.
    return result;
}

} // namespace JSC
