/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include "WasmFunctionCodeBlockGenerator.h"

#if ENABLE(WEBASSEMBLY)

#include "InstructionStream.h"
#include "VirtualRegister.h"
#include <wtf/FixedVector.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC { namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FunctionCodeBlockGenerator);

void FunctionCodeBlockGenerator::setInstructions(std::unique_ptr<WasmInstructionStream> instructions)
{
    m_instructions = WTFMove(instructions);
    m_instructionsRawPointer = m_instructions->rawPointer();
}

void FunctionCodeBlockGenerator::addOutOfLineJumpTarget(WasmInstructionStream::Offset bytecodeOffset, int target)
{
    RELEASE_ASSERT(target);
    m_outOfLineJumpTargets.set(bytecodeOffset, target);
}

WasmInstructionStream::Offset FunctionCodeBlockGenerator::outOfLineJumpOffset(WasmInstructionStream::Offset bytecodeOffset)
{
    ASSERT(m_outOfLineJumpTargets.contains(bytecodeOffset));
    return m_outOfLineJumpTargets.get(bytecodeOffset);
}

unsigned FunctionCodeBlockGenerator::addSignature(const TypeDefinition& signature)
{
    unsigned index = m_signatures.size();
    m_signatures.append(&signature);
    return index;
}

auto FunctionCodeBlockGenerator::addJumpTable(size_t numberOfEntries) -> JumpTable&
{
    m_jumpTables.append(JumpTable(numberOfEntries));
    return m_jumpTables.last();
}

unsigned FunctionCodeBlockGenerator::numberOfJumpTables() const
{
    return m_jumpTables.size();
}

void FunctionCodeBlockGenerator::setTailCall(uint32_t functionIndex, bool isImportedFunctionFromFunctionIndexSpace)
{
    m_tailCallSuccessors.set(functionIndex);
    setTailCallClobbersInstance(isImportedFunctionFromFunctionIndexSpace);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
