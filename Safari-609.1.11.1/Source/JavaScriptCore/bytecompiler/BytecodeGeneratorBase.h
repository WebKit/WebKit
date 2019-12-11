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

#include "InstructionStream.h"
#include <wtf/SegmentedVector.h>

namespace JSC {

class RegisterID;

template<typename BytecodeGenerator>
class GenericBoundLabel;

template<typename BytecodeGenerator>
class GenericLabel;

template<typename Traits>
class BytecodeGeneratorBase {
    template<typename BytecodeGenerator>
    friend class GenericBoundLabel;

    template<typename BytecodeGenerator>
    friend class GenericLabel;

public:
    BytecodeGeneratorBase(typename Traits::CodeBlock, uint32_t virtualRegisterCountForCalleeSaves);

    void allocateCalleeSaveSpace(uint32_t virtualRegisterCountForCalleeSaves);

    Ref<GenericLabel<Traits>> newLabel();
    Ref<GenericLabel<Traits>> newEmittedLabel();
    RegisterID* newRegister();
    RegisterID* addVar();

    // Returns the next available temporary register. Registers returned by
    // newTemporary require a modified form of reference counting: any
    // register with a refcount of 0 is considered "available", meaning that
    // the next instruction may overwrite it.
    RegisterID* newTemporary();

    void emitLabel(GenericLabel<Traits>&);
    void recordOpcode(typename Traits::OpcodeID);
    void alignWideOpcode16();
    void alignWideOpcode32();

    void write(uint8_t);
    void write(uint16_t);
    void write(uint32_t);
    void write(int8_t);
    void write(int16_t);
    void write(int32_t);

protected:
    void reclaimFreeRegisters();

    InstructionStreamWriter m_writer;
    typename Traits::CodeBlock m_codeBlock;

    typename Traits::OpcodeID m_lastOpcodeID = Traits::opcodeForDisablingOptimizations;
    InstructionStream::MutableRef m_lastInstruction { m_writer.ref() };

    SegmentedVector<GenericLabel<Traits>, 32> m_labels;
    SegmentedVector<RegisterID, 32> m_calleeLocals;
};

} // namespace JSC
