/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "PreciseJumpTargets.h"

#include "InterpreterInlines.h"
#include "JSCInlines.h"
#include "PreciseJumpTargetsInlines.h"

namespace JSC {

template <size_t vectorSize, typename Block>
static void getJumpTargetsForInstruction(Block* codeBlock, const InstructionStream::Ref& instruction, Vector<InstructionStream::Offset, vectorSize>& out)
{
    extractStoredJumpTargetsForInstruction(codeBlock, instruction, [&](int32_t relativeOffset) {
        out.append(instruction.offset() + relativeOffset);
    });
    OpcodeID opcodeID = instruction->opcodeID();
    // op_loop_hint does not have jump target stored in bytecode instructions.
    if (opcodeID == op_loop_hint)
        out.append(instruction.offset());
    else if (opcodeID == op_enter && codeBlock->hasTailCalls() && Options::optimizeRecursiveTailCalls()) {
        // We need to insert a jump after op_enter, so recursive tail calls have somewhere to jump to.
        // But we only want to pay that price for functions that have at least one tail call.
        out.append(instruction.next().offset());
    }
}

enum class ComputePreciseJumpTargetsMode {
    FollowCodeBlockClaim,
    ForceCompute,
};

template<ComputePreciseJumpTargetsMode Mode, typename Block, size_t vectorSize>
void computePreciseJumpTargetsInternal(Block* codeBlock, const InstructionStream& instructions, Vector<InstructionStream::Offset, vectorSize>& out)
{
    ASSERT(out.isEmpty());

    // The code block has a superset of the jump targets. So if it claims to have none, we are done.
    if (Mode == ComputePreciseJumpTargetsMode::FollowCodeBlockClaim && !codeBlock->numberOfJumpTargets())
        return;
    
    for (unsigned i = codeBlock->numberOfExceptionHandlers(); i--;) {
        out.append(codeBlock->exceptionHandler(i).target);
        out.append(codeBlock->exceptionHandler(i).start);
        out.append(codeBlock->exceptionHandler(i).end);
    }

    for (const auto& instruction : instructions) {
        getJumpTargetsForInstruction(codeBlock, instruction, out);
    }
    
    std::sort(out.begin(), out.end());
    
    // We will have duplicates, and we must remove them.
    unsigned toIndex = 0;
    unsigned fromIndex = 0;
    unsigned lastValue = UINT_MAX;
    while (fromIndex < out.size()) {
        unsigned value = out[fromIndex++];
        if (value == lastValue)
            continue;
        out[toIndex++] = value;
        lastValue = value;
    }
    out.shrinkCapacity(toIndex);
}

void computePreciseJumpTargets(CodeBlock* codeBlock, Vector<InstructionStream::Offset, 32>& out)
{
    computePreciseJumpTargetsInternal<ComputePreciseJumpTargetsMode::FollowCodeBlockClaim>(codeBlock, codeBlock->instructions(), out);
}

void computePreciseJumpTargets(CodeBlock* codeBlock, const InstructionStream& instructions, Vector<InstructionStream::Offset, 32>& out)
{
    computePreciseJumpTargetsInternal<ComputePreciseJumpTargetsMode::FollowCodeBlockClaim>(codeBlock, instructions, out);
}

void computePreciseJumpTargets(UnlinkedCodeBlock* codeBlock, const InstructionStream& instructions, Vector<InstructionStream::Offset, 32>& out)
{
    computePreciseJumpTargetsInternal<ComputePreciseJumpTargetsMode::FollowCodeBlockClaim>(codeBlock, instructions, out);
}

void recomputePreciseJumpTargets(UnlinkedCodeBlock* codeBlock, const InstructionStream& instructions, Vector<InstructionStream::Offset>& out)
{
    computePreciseJumpTargetsInternal<ComputePreciseJumpTargetsMode::ForceCompute>(codeBlock, instructions, out);
}

void findJumpTargetsForInstruction(CodeBlock* codeBlock, const InstructionStream::Ref& instruction, Vector<InstructionStream::Offset, 1>& out)
{
    getJumpTargetsForInstruction(codeBlock, instruction, out);
}

void findJumpTargetsForInstruction(UnlinkedCodeBlock* codeBlock, const InstructionStream::Ref& instruction, Vector<InstructionStream::Offset, 1>& out)
{
    getJumpTargetsForInstruction(codeBlock, instruction, out);
}

} // namespace JSC

