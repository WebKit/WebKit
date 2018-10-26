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

#include "config.h"
#include "BytecodeLivenessAnalysis.h"

#include "BytecodeKills.h"
#include "BytecodeLivenessAnalysisInlines.h"
#include "BytecodeUseDef.h"
#include "CodeBlock.h"
#include "FullBytecodeLiveness.h"
#include "HeapInlines.h"
#include "InterpreterInlines.h"
#include "PreciseJumpTargets.h"

namespace JSC {

BytecodeLivenessAnalysis::BytecodeLivenessAnalysis(CodeBlock* codeBlock)
    : m_graph(codeBlock, codeBlock->instructions())
{
    runLivenessFixpoint(codeBlock, codeBlock->instructions(), m_graph);

    if (Options::dumpBytecodeLivenessResults())
        dumpResults(codeBlock);
}

void BytecodeLivenessAnalysis::getLivenessInfoAtBytecodeOffset(CodeBlock* codeBlock, unsigned bytecodeOffset, FastBitVector& result)
{
    BytecodeBasicBlock* block = m_graph.findBasicBlockForBytecodeOffset(bytecodeOffset);
    ASSERT(block);
    ASSERT(!block->isEntryBlock());
    ASSERT(!block->isExitBlock());
    result.resize(block->out().numBits());
    computeLocalLivenessForBytecodeOffset(codeBlock, codeBlock->instructions(), m_graph, block, bytecodeOffset, result);
}

FastBitVector BytecodeLivenessAnalysis::getLivenessInfoAtBytecodeOffset(CodeBlock* codeBlock, unsigned bytecodeOffset)
{
    FastBitVector out;
    getLivenessInfoAtBytecodeOffset(codeBlock, bytecodeOffset, out);
    return out;
}

void BytecodeLivenessAnalysis::computeFullLiveness(CodeBlock* codeBlock, FullBytecodeLiveness& result)
{
    FastBitVector out;

    result.m_map.resize(codeBlock->instructions().size());
    
    for (std::unique_ptr<BytecodeBasicBlock>& block : m_graph.basicBlocksInReverseOrder()) {
        if (block->isEntryBlock() || block->isExitBlock())
            continue;
        
        out = block->out();
        
        for (unsigned i = block->offsets().size(); i--;) {
            unsigned bytecodeOffset = block->offsets()[i];
            stepOverInstruction(codeBlock, codeBlock->instructions(), m_graph, bytecodeOffset, out);
            result.m_map[bytecodeOffset] = out;
        }
    }
}

void BytecodeLivenessAnalysis::computeKills(CodeBlock* codeBlock, BytecodeKills& result)
{
    UNUSED_PARAM(result);
    FastBitVector out;

    result.m_codeBlock = codeBlock;
    result.m_killSets = makeUniqueArray<BytecodeKills::KillSet>(codeBlock->instructions().size());
    
    for (std::unique_ptr<BytecodeBasicBlock>& block : m_graph.basicBlocksInReverseOrder()) {
        if (block->isEntryBlock() || block->isExitBlock())
            continue;
        
        out = block->out();
        
        for (unsigned i = block->offsets().size(); i--;) {
            unsigned bytecodeOffset = block->offsets()[i];
            stepOverInstruction(
                codeBlock, codeBlock->instructions(), m_graph, bytecodeOffset,
                [&] (unsigned index) {
                    // This is for uses.
                    if (out[index])
                        return;
                    result.m_killSets[bytecodeOffset].add(index);
                    out[index] = true;
                },
                [&] (unsigned index) {
                    // This is for defs.
                    out[index] = false;
                });
        }
    }
}

void BytecodeLivenessAnalysis::dumpResults(CodeBlock* codeBlock)
{
    dataLog("\nDumping bytecode liveness for ", *codeBlock, ":\n");
    const auto& instructions = codeBlock->instructions();
    unsigned i = 0;

    unsigned numberOfBlocks = m_graph.size();
    Vector<FastBitVector> predecessors(numberOfBlocks);
    for (BytecodeBasicBlock* block : m_graph)
        predecessors[block->index()].resize(numberOfBlocks);
    for (BytecodeBasicBlock* block : m_graph) {
        for (unsigned j = 0; j < block->successors().size(); j++) {
            unsigned blockIndex = block->index();
            unsigned successorIndex = block->successors()[j]->index();
            predecessors[successorIndex][blockIndex] = true;
        }
    }

    auto dumpBitVector = [] (FastBitVector& bits) {
        for (unsigned j = 0; j < bits.numBits(); j++) {
            if (bits[j])
                dataLogF(" %u", j);
        }
    };

    for (BytecodeBasicBlock* block : m_graph) {
        dataLogF("\nBytecode basic block %u: %p (offset: %u, length: %u)\n", i++, block, block->leaderOffset(), block->totalLength());

        dataLogF("Predecessors:");
        dumpBitVector(predecessors[block->index()]);
        dataLogF("\n");

        dataLogF("Successors:");
        FastBitVector successors;
        successors.resize(numberOfBlocks);
        for (unsigned j = 0; j < block->successors().size(); j++) {
            BytecodeBasicBlock* successor = block->successors()[j];
            successors[successor->index()] = true;
        }
        dumpBitVector(successors); // Dump in sorted order.
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
            const auto currentInstruction = instructions.at(bytecodeOffset);

            dataLogF("Live variables:");
            FastBitVector liveBefore = getLivenessInfoAtBytecodeOffset(codeBlock, bytecodeOffset);
            dumpBitVector(liveBefore);
            dataLogF("\n");
            codeBlock->dumpBytecode(WTF::dataFile(), currentInstruction);

            bytecodeOffset += currentInstruction->size();
        }

        dataLogF("Live variables:");
        FastBitVector liveAfter = block->out();
        dumpBitVector(liveAfter);
        dataLogF("\n");
    }
}

} // namespace JSC
