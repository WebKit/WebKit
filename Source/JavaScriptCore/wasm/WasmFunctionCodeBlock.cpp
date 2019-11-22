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


#include "config.h"
#include "WasmFunctionCodeBlock.h"

#if ENABLE(WEBASSEMBLY)

#include "BytecodeDumper.h"
#include "BytecodeStructs.h"
#include "InstructionStream.h"
#include "LLIntThunks.h"

namespace JSC { namespace Wasm {

void FunctionCodeBlock::setInstructions(std::unique_ptr<InstructionStream> instructions)
{
    m_instructions = WTFMove(instructions);
    m_instructionsRawPointer = m_instructions->rawPointer();
}

void FunctionCodeBlock::addOutOfLineJumpTarget(InstructionStream::Offset bytecodeOffset, int target)
{
    RELEASE_ASSERT(target);
    m_outOfLineJumpTargets.set(bytecodeOffset, target);
}

InstructionStream::Offset FunctionCodeBlock::outOfLineJumpOffset(InstructionStream::Offset bytecodeOffset)
{
    ASSERT(m_outOfLineJumpTargets.contains(bytecodeOffset));
    return m_outOfLineJumpTargets.get(bytecodeOffset);
}

const Instruction* FunctionCodeBlock::outOfLineJumpTarget(const Instruction* pc)
{
    int offset = bytecodeOffset(pc);
    int target = outOfLineJumpOffset(offset);
    return m_instructions->at(offset + target).ptr();
}

unsigned FunctionCodeBlock::addSignature(const Signature& signature)
{
    unsigned index = m_signatures.size();
    m_signatures.append(&signature);
    return index;
}

const Signature& FunctionCodeBlock::signature(unsigned index) const
{
    return *m_signatures[index];
}

auto FunctionCodeBlock::addJumpTable(size_t numberOfEntries) -> JumpTable&
{
    m_jumpTables.append(JumpTable(numberOfEntries));
    return m_jumpTables.last();
}

auto FunctionCodeBlock::jumpTable(unsigned tableIndex) const -> const JumpTable&
{
    return m_jumpTables[tableIndex];
}

unsigned FunctionCodeBlock::numberOfJumpTables() const
{
    return m_jumpTables.size();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
