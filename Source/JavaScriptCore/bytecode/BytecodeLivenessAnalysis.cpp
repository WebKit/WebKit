/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "BytecodeLivenessAnalysis.h"

#include "BytecodeKills.h"
#include "BytecodeLivenessAnalysisInlines.h"
#include "BytecodeUseDef.h"
#include "CodeBlock.h"
#include "FullBytecodeLiveness.h"
#include "InterpreterInlines.h"
#include "PreciseJumpTargets.h"

namespace JSC {

BytecodeLivenessAnalysis::BytecodeLivenessAnalysis(CodeBlock* codeBlock)
    : m_graph(codeBlock, codeBlock->instructions())
{
    compute();
}

template<typename Functor>
void BytecodeLivenessAnalysis::computeDefsForBytecodeOffset(CodeBlock* codeBlock, OpcodeID opcodeID, Instruction* instruction, FastBitVector&, const Functor& functor)
{
    JSC::computeDefsForBytecodeOffset(codeBlock, opcodeID, instruction, functor);
}

template<typename Functor>
void BytecodeLivenessAnalysis::computeUsesForBytecodeOffset(CodeBlock* codeBlock, OpcodeID opcodeID, Instruction* instruction, FastBitVector&, const Functor& functor)
{
    JSC::computeUsesForBytecodeOffset(codeBlock, opcodeID, instruction, functor);
}

void BytecodeLivenessAnalysis::getLivenessInfoAtBytecodeOffset(unsigned bytecodeOffset, FastBitVector& result)
{
    BytecodeBasicBlock* block = m_graph.findBasicBlockForBytecodeOffset(bytecodeOffset);
    ASSERT(block);
    ASSERT(!block->isEntryBlock());
    ASSERT(!block->isExitBlock());
    result.resize(block->out().numBits());
    computeLocalLivenessForBytecodeOffset(m_graph, block, bytecodeOffset, result);
}

bool BytecodeLivenessAnalysis::operandIsLiveAtBytecodeOffset(int operand, unsigned bytecodeOffset)
{
    if (operandIsAlwaysLive(operand))
        return true;
    FastBitVector result;
    getLivenessInfoAtBytecodeOffset(bytecodeOffset, result);
    return operandThatIsNotAlwaysLiveIsLive(result, operand);
}

FastBitVector BytecodeLivenessAnalysis::getLivenessInfoAtBytecodeOffset(unsigned bytecodeOffset)
{
    FastBitVector out;
    getLivenessInfoAtBytecodeOffset(bytecodeOffset, out);
    return out;
}

void BytecodeLivenessAnalysis::computeFullLiveness(FullBytecodeLiveness& result)
{
    FastBitVector out;
    CodeBlock* codeBlock = m_graph.codeBlock();
    
    result.m_map.resize(codeBlock->instructions().size());
    
    for (std::unique_ptr<BytecodeBasicBlock>& block : m_graph.basicBlocksInReverseOrder()) {
        if (block->isEntryBlock() || block->isExitBlock())
            continue;
        
        out = block->out();
        
        for (unsigned i = block->offsets().size(); i--;) {
            unsigned bytecodeOffset = block->offsets()[i];
            stepOverInstruction(m_graph, bytecodeOffset, out);
            result.m_map[bytecodeOffset] = out;
        }
    }
}

void BytecodeLivenessAnalysis::computeKills(BytecodeKills& result)
{
    FastBitVector out;
    
    CodeBlock* codeBlock = m_graph.codeBlock();
    result.m_codeBlock = codeBlock;
    result.m_killSets = std::make_unique<BytecodeKills::KillSet[]>(codeBlock->instructions().size());
    
    for (std::unique_ptr<BytecodeBasicBlock>& block : m_graph.basicBlocksInReverseOrder()) {
        if (block->isEntryBlock() || block->isExitBlock())
            continue;
        
        out = block->out();
        
        for (unsigned i = block->offsets().size(); i--;) {
            unsigned bytecodeOffset = block->offsets()[i];
            stepOverInstruction(
                m_graph, bytecodeOffset, out,
                [&] (unsigned index) {
                    // This is for uses.
                    if (out.get(index))
                        return;
                    result.m_killSets[bytecodeOffset].add(index);
                    out.set(index);
                },
                [&] (unsigned index) {
                    // This is for defs.
                    out.clear(index);
                });
        }
    }
}

void BytecodeLivenessAnalysis::dumpResults()
{
    CodeBlock* codeBlock = m_graph.codeBlock();
    dataLog("\nDumping bytecode liveness for ", *codeBlock, ":\n");
    Interpreter* interpreter = codeBlock->vm()->interpreter;
    Instruction* instructionsBegin = codeBlock->instructions().begin();
    unsigned i = 0;
    for (BytecodeBasicBlock* block : m_graph) {
        dataLogF("\nBytecode basic block %u: %p (offset: %u, length: %u)\n", i++, block, block->leaderOffset(), block->totalLength());
        dataLogF("Successors: ");
        for (unsigned j = 0; j < block->successors().size(); j++) {
            BytecodeBasicBlock* successor = block->successors()[j];
            dataLogF("%p ", successor);
        }
        dataLogF("\n");
        if (block->isEntryBlock()) {
            dataLogF("Entry block %p\n", block);
            continue;
        }
        if (block->isExitBlock()) {
            dataLogF("Exit block: %p\n", block);
            continue;
        }
        for (unsigned bytecodeOffset = block->leaderOffset(); bytecodeOffset < block->leaderOffset() + block->totalLength();) {
            const Instruction* currentInstruction = &instructionsBegin[bytecodeOffset];

            dataLogF("Live variables: ");
            FastBitVector liveBefore = getLivenessInfoAtBytecodeOffset(bytecodeOffset);
            for (unsigned j = 0; j < liveBefore.numBits(); j++) {
                if (liveBefore.get(j))
                    dataLogF("%u ", j);
            }
            dataLogF("\n");
            codeBlock->dumpBytecode(WTF::dataFile(), codeBlock->globalObject()->globalExec(), instructionsBegin, currentInstruction);

            OpcodeID opcodeID = interpreter->getOpcodeID(instructionsBegin[bytecodeOffset].u.opcode);
            unsigned opcodeLength = opcodeLengths[opcodeID];
            bytecodeOffset += opcodeLength;
        }

        dataLogF("Live variables: ");
        FastBitVector liveAfter = block->out();
        for (unsigned j = 0; j < liveAfter.numBits(); j++) {
            if (liveAfter.get(j))
                dataLogF("%u ", j);
        }
        dataLogF("\n");
    }
}

void BytecodeLivenessAnalysis::compute()
{
    runLivenessFixpoint(m_graph);

    if (Options::dumpBytecodeLivenessResults())
        dumpResults();
}

} // namespace JSC
