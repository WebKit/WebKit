/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "Instruction.h"
#include <wtf/Forward.h>

namespace JSC {

void computeUsesForBytecodeIndexImpl(const JSInstruction*, Checkpoint, const ScopedLambda<void(VirtualRegister)>&, const ScopedLambda<const BitVector&(unsigned)>&);
void computeDefsForBytecodeIndexImpl(unsigned, const JSInstruction*, Checkpoint, const ScopedLambda<void(VirtualRegister)>&, const ScopedLambda<const BitVector&(unsigned)>&);

template<typename Block, typename Functor>
void computeUsesForBytecodeIndex(Block* codeBlock, const JSInstruction* instruction, Checkpoint checkpoint, const Functor& functor)
{
    OpcodeID opcodeID = instruction->opcodeID();
    if (opcodeID != op_enter && codeBlock->wasCompiledWithDebuggingOpcodes() && codeBlock->scopeRegister().isValid())
        functor(codeBlock->scopeRegister());

    auto bitVector = [&](unsigned index) -> const BitVector& {
        return codeBlock->bitVector(index);
    };
    computeUsesForBytecodeIndexImpl(instruction, checkpoint, functor, bitVector);
}

template<typename Block, typename Functor>
void computeDefsForBytecodeIndex(Block* codeBlock, const JSInstruction* instruction, Checkpoint checkpoint, const Functor& functor)
{
    auto bitVector = [&](unsigned index) -> const BitVector& {
        return codeBlock->bitVector(index);
    };
    computeDefsForBytecodeIndexImpl(codeBlock->numVars(), instruction, checkpoint, functor, bitVector);
}

#undef CALL_FUNCTOR
#undef USES_OR_DEFS
#undef USES
#undef DEFS
} // namespace JSC
