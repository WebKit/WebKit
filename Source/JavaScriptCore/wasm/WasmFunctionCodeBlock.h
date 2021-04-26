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

#if ENABLE(WEBASSEMBLY)

#include "BytecodeConventions.h"
#include "InstructionStream.h"
#include "MacroAssemblerCodeRef.h"
#include "WasmLLIntTierUpCounter.h"
#include "WasmOps.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace JSC {

class JITCode;
class LLIntOffsetsExtractor;

template<typename Traits>
class BytecodeGeneratorBase;

namespace Wasm {

class Signature;
struct GeneratorTraits;

// FIXME: Consider merging this with LLIntCallee
// https://bugs.webkit.org/show_bug.cgi?id=203691
class FunctionCodeBlock {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(FunctionCodeBlock);

    friend BytecodeGeneratorBase<GeneratorTraits>;
    friend LLIntOffsetsExtractor;
    friend class LLIntGenerator;

public:
    FunctionCodeBlock(uint32_t functionIndex)
        : m_functionIndex(functionIndex)
    {
    }

    uint32_t functionIndex() const { return m_functionIndex; }
    unsigned numVars() const { return m_numVars; }
    unsigned numCalleeLocals() const { return m_numCalleeLocals; }
    uint32_t numArguments() const { return m_numArguments; }
    const Vector<Type>& constantTypes() const { return m_constantTypes; }
    const Vector<uint64_t>& constants() const { return m_constants; }
    const InstructionStream& instructions() const { return *m_instructions; }

    void setNumVars(unsigned numVars) { m_numVars = numVars; }
    void setNumCalleeLocals(unsigned numCalleeLocals) { m_numCalleeLocals = numCalleeLocals; }

    ALWAYS_INLINE uint64_t getConstant(VirtualRegister reg) const { return m_constants[reg.toConstantIndex()]; }
    ALWAYS_INLINE Type getConstantType(VirtualRegister reg) const
    {
        ASSERT(Options::dumpGeneratedWasmBytecodes());
        return m_constantTypes[reg.toConstantIndex()];
    }

    void setInstructions(std::unique_ptr<InstructionStream>);
    void addJumpTarget(InstructionStream::Offset jumpTarget) { m_jumpTargets.append(jumpTarget); }
    InstructionStream::Offset numberOfJumpTargets() { return m_jumpTargets.size(); }
    InstructionStream::Offset lastJumpTarget() { return m_jumpTargets.last(); }

    void addOutOfLineJumpTarget(InstructionStream::Offset, int target);
    const Instruction* outOfLineJumpTarget(const Instruction*);
    InstructionStream::Offset outOfLineJumpOffset(InstructionStream::Offset);
    InstructionStream::Offset outOfLineJumpOffset(const InstructionStream::Ref& instruction)
    {
        return outOfLineJumpOffset(instruction.offset());
    }

    inline InstructionStream::Offset bytecodeOffset(const Instruction* returnAddress)
    {
        const auto* instructionsBegin = m_instructions->at(0).ptr();
        const auto* instructionsEnd = reinterpret_cast<const Instruction*>(reinterpret_cast<uintptr_t>(instructionsBegin) + m_instructions->size());
        RELEASE_ASSERT(returnAddress >= instructionsBegin && returnAddress < instructionsEnd);
        return returnAddress - instructionsBegin;
    }

    LLIntTierUpCounter& tierUpCounter() { return m_tierUpCounter; }

    unsigned addSignature(const Signature&);
    const Signature& signature(unsigned index) const;

    struct JumpTableEntry {
        int target { 0 };
        unsigned startOffset;
        unsigned dropCount;
        unsigned keepCount;
    };

    using JumpTable = Vector<JumpTableEntry>;
    JumpTable& addJumpTable(size_t numberOfEntries);
    const JumpTable& jumpTable(unsigned tableIndex) const;
    unsigned numberOfJumpTables() const;

private:
    using OutOfLineJumpTargets = HashMap<InstructionStream::Offset, int>;

    uint32_t m_functionIndex;

    // Used for the number of WebAssembly locals, as in https://webassembly.github.io/spec/core/syntax/modules.html#syntax-local
    unsigned m_numVars { 0 };
    // Number of VirtualRegister. The naming is unfortunate, but has to match UnlinkedCodeBlock
    unsigned m_numCalleeLocals { 0 };
    uint32_t m_numArguments { 0 };
    Vector<Type> m_constantTypes;
    Vector<uint64_t> m_constants;
    std::unique_ptr<InstructionStream> m_instructions;
    const void* m_instructionsRawPointer { nullptr };
    Vector<InstructionStream::Offset> m_jumpTargets;
    Vector<const Signature*> m_signatures;
    OutOfLineJumpTargets m_outOfLineJumpTargets;
    LLIntTierUpCounter m_tierUpCounter;
    Vector<JumpTable> m_jumpTables;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
