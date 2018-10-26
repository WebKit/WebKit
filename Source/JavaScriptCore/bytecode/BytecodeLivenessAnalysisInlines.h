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

#include "BytecodeGraph.h"
#include "BytecodeLivenessAnalysis.h"
#include "CodeBlock.h"
#include "InterpreterInlines.h"
#include "Operations.h"

namespace JSC {

inline bool operandIsAlwaysLive(int operand)
{
    return !VirtualRegister(operand).isLocal();
}

inline bool operandThatIsNotAlwaysLiveIsLive(const FastBitVector& out, int operand)
{
    unsigned local = VirtualRegister(operand).toLocal();
    if (local >= out.numBits())
        return false;
    return out[local];
}

inline bool operandIsLive(const FastBitVector& out, int operand)
{
    return operandIsAlwaysLive(operand) || operandThatIsNotAlwaysLiveIsLive(out, operand);
}

inline bool isValidRegisterForLiveness(VirtualRegister operand)
{
    if (operand.isConstant())
        return false;
    return operand.isLocal();
}

// Simplified interface to bytecode use/def, which determines defs first and then uses, and includes
// exception handlers in the uses.
template<typename CodeBlockType, typename UseFunctor, typename DefFunctor>
inline void BytecodeLivenessPropagation::stepOverInstruction(CodeBlockType* codeBlock, const InstructionStream& instructions, BytecodeGraph& graph, InstructionStream::Offset bytecodeOffset, const UseFunctor& use, const DefFunctor& def)
{
    // This abstractly execute the instruction in reverse. Instructions logically first use operands and
    // then define operands. This logical ordering is necessary for operations that use and def the same
    // operand, like:
    //
    //     op_add loc1, loc1, loc2
    //
    // The use of loc1 happens before the def of loc1. That's a semantic requirement since the add
    // operation cannot travel forward in time to read the value that it will produce after reading that
    // value. Since we are executing in reverse, this means that we must do defs before uses (reverse of
    // uses before defs).
    //
    // Since this is a liveness analysis, this ordering ends up being particularly important: if we did
    // uses before defs, then the add operation above would appear to not have loc1 live, since we'd
    // first add it to the out set (the use), and then we'd remove it (the def).

    auto* instruction = instructions.at(bytecodeOffset).ptr();
    OpcodeID opcodeID = instruction->opcodeID();

    computeDefsForBytecodeOffset(
        codeBlock, opcodeID, instruction,
        [&] (VirtualRegister operand) {
            if (isValidRegisterForLiveness(operand))
                def(operand.toLocal());
        });

    computeUsesForBytecodeOffset(
        codeBlock, opcodeID, instruction,
        [&] (VirtualRegister operand) {
            if (isValidRegisterForLiveness(operand))
                use(operand.toLocal());
        });

    // If we have an exception handler, we want the live-in variables of the 
    // exception handler block to be included in the live-in of this particular bytecode.
    if (auto* handler = codeBlock->handlerForBytecodeOffset(bytecodeOffset)) {
        BytecodeBasicBlock* handlerBlock = graph.findBasicBlockWithLeaderOffset(handler->target);
        ASSERT(handlerBlock);
        handlerBlock->in().forEachSetBit(use);
    }
}

template<typename CodeBlockType>
inline void BytecodeLivenessPropagation::stepOverInstruction(CodeBlockType* codeBlock, const InstructionStream& instructions, BytecodeGraph& graph, InstructionStream::Offset bytecodeOffset, FastBitVector& out)
{
    stepOverInstruction(
        codeBlock, instructions, graph, bytecodeOffset,
        [&] (unsigned bitIndex) {
            // This is the use functor, so we set the bit.
            out[bitIndex] = true;
        },
        [&] (unsigned bitIndex) {
            // This is the def functor, so we clear the bit.
            out[bitIndex] = false;
        });
}

template<typename CodeBlockType, typename Instructions>
inline bool BytecodeLivenessPropagation::computeLocalLivenessForBytecodeOffset(CodeBlockType* codeBlock, const Instructions& instructions, BytecodeGraph& graph, BytecodeBasicBlock* block, unsigned targetOffset, FastBitVector& result)
{
    ASSERT(!block->isExitBlock());
    ASSERT(!block->isEntryBlock());

    FastBitVector out = block->out();

    for (int i = block->offsets().size() - 1; i >= 0; i--) {
        unsigned bytecodeOffset = block->offsets()[i];
        if (targetOffset > bytecodeOffset)
            break;
        stepOverInstruction(codeBlock, instructions, graph, bytecodeOffset, out);
    }

    return result.setAndCheck(out);
}

template<typename CodeBlockType, typename Instructions>
inline bool BytecodeLivenessPropagation::computeLocalLivenessForBlock(CodeBlockType* codeBlock, const Instructions& instructions, BytecodeGraph& graph, BytecodeBasicBlock* block)
{
    if (block->isExitBlock() || block->isEntryBlock())
        return false;
    return computeLocalLivenessForBytecodeOffset(codeBlock, instructions, graph, block, block->leaderOffset(), block->in());
}

template<typename CodeBlockType, typename Instructions>
inline FastBitVector BytecodeLivenessPropagation::getLivenessInfoAtBytecodeOffset(CodeBlockType* codeBlock, const Instructions& instructions, BytecodeGraph& graph, unsigned bytecodeOffset)
{
    BytecodeBasicBlock* block = graph.findBasicBlockForBytecodeOffset(bytecodeOffset);
    ASSERT(block);
    ASSERT(!block->isEntryBlock());
    ASSERT(!block->isExitBlock());
    FastBitVector out;
    out.resize(block->out().numBits());
    computeLocalLivenessForBytecodeOffset(codeBlock, instructions, graph, block, bytecodeOffset, out);
    return out;
}

template<typename CodeBlockType, typename Instructions>
inline void BytecodeLivenessPropagation::runLivenessFixpoint(CodeBlockType* codeBlock, const Instructions& instructions, BytecodeGraph& graph)
{
    unsigned numberOfVariables = codeBlock->numCalleeLocals();
    for (BytecodeBasicBlock* block : graph) {
        block->in().resize(numberOfVariables);
        block->out().resize(numberOfVariables);
        block->in().clearAll();
        block->out().clearAll();
    }

    bool changed;
    BytecodeBasicBlock* lastBlock = graph.last();
    lastBlock->in().clearAll();
    lastBlock->out().clearAll();
    FastBitVector newOut;
    newOut.resize(lastBlock->out().numBits());
    do {
        changed = false;
        for (std::unique_ptr<BytecodeBasicBlock>& block : graph.basicBlocksInReverseOrder()) {
            newOut.clearAll();
            for (BytecodeBasicBlock* successor : block->successors())
                newOut |= successor->in();
            block->out() = newOut;
            changed |= computeLocalLivenessForBlock(codeBlock, instructions, graph, block.get());
        }
    } while (changed);
}

} // namespace JSC
