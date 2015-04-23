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
#include "PreciseJumpTargets.h"

namespace JSC {

BytecodeLivenessAnalysis::BytecodeLivenessAnalysis(CodeBlock* codeBlock)
    : m_codeBlock(codeBlock)
{
    ASSERT(m_codeBlock);
    compute();
}

static bool isValidRegisterForLiveness(CodeBlock* codeBlock, int operand)
{
    if (codeBlock->isConstantRegisterIndex(operand))
        return false;
    
    VirtualRegister virtualReg(operand);
    return virtualReg.isLocal();
}

static unsigned getLeaderOffsetForBasicBlock(RefPtr<BytecodeBasicBlock>* basicBlock)
{
    return (*basicBlock)->leaderBytecodeOffset();
}

static BytecodeBasicBlock* findBasicBlockWithLeaderOffset(Vector<RefPtr<BytecodeBasicBlock> >& basicBlocks, unsigned leaderOffset)
{
    return (*tryBinarySearch<RefPtr<BytecodeBasicBlock>, unsigned>(basicBlocks, basicBlocks.size(), leaderOffset, getLeaderOffsetForBasicBlock)).get();
}

static bool blockContainsBytecodeOffset(BytecodeBasicBlock* block, unsigned bytecodeOffset)
{
    unsigned leaderOffset = block->leaderBytecodeOffset();
    return bytecodeOffset >= leaderOffset && bytecodeOffset < leaderOffset + block->totalBytecodeLength();
}

static BytecodeBasicBlock* findBasicBlockForBytecodeOffset(Vector<RefPtr<BytecodeBasicBlock> >& basicBlocks, unsigned bytecodeOffset)
{
/*
    for (unsigned i = 0; i < basicBlocks.size(); i++) {
        if (blockContainsBytecodeOffset(basicBlocks[i].get(), bytecodeOffset))
            return basicBlocks[i].get();
    }
    return 0;
*/
    RefPtr<BytecodeBasicBlock>* basicBlock = approximateBinarySearch<RefPtr<BytecodeBasicBlock>, unsigned>(
        basicBlocks, basicBlocks.size(), bytecodeOffset, getLeaderOffsetForBasicBlock);
    // We found the block we were looking for.
    if (blockContainsBytecodeOffset((*basicBlock).get(), bytecodeOffset))
        return (*basicBlock).get();

    // Basic block is to the left of the returned block.
    if (bytecodeOffset < (*basicBlock)->leaderBytecodeOffset()) {
        ASSERT(basicBlock - 1 >= basicBlocks.data());
        ASSERT(blockContainsBytecodeOffset(basicBlock[-1].get(), bytecodeOffset));
        return basicBlock[-1].get();
    }

    // Basic block is to the right of the returned block.
    ASSERT(&basicBlock[1] <= &basicBlocks.last());
    ASSERT(blockContainsBytecodeOffset(basicBlock[1].get(), bytecodeOffset));
    return basicBlock[1].get();
}

// Simplified interface to bytecode use/def, which determines defs first and then uses, and includes
// exception handlers in the uses.
template<typename UseFunctor, typename DefFunctor>
static void stepOverInstruction(CodeBlock* codeBlock, Vector<RefPtr<BytecodeBasicBlock>>& basicBlocks, unsigned bytecodeOffset, const UseFunctor& use, const DefFunctor& def)
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
    
    computeDefsForBytecodeOffset(
        codeBlock, bytecodeOffset,
        [&] (CodeBlock* codeBlock, Instruction*, OpcodeID, int operand) {
            if (isValidRegisterForLiveness(codeBlock, operand))
                def(VirtualRegister(operand).toLocal());
        });

    computeUsesForBytecodeOffset(
        codeBlock, bytecodeOffset,
        [&] (CodeBlock* codeBlock, Instruction*, OpcodeID, int operand) {
            if (isValidRegisterForLiveness(codeBlock, operand))
                use(VirtualRegister(operand).toLocal());
        });
        
    // If we have an exception handler, we want the live-in variables of the 
    // exception handler block to be included in the live-in of this particular bytecode.
    if (HandlerInfo* handler = codeBlock->handlerForBytecodeOffset(bytecodeOffset)) {
        BytecodeBasicBlock* handlerBlock = findBasicBlockWithLeaderOffset(basicBlocks, handler->target);
        ASSERT(handlerBlock);
        handlerBlock->in().forEachSetBit(use);
    }
}

static void stepOverInstruction(CodeBlock* codeBlock, Vector<RefPtr<BytecodeBasicBlock>>& basicBlocks, unsigned bytecodeOffset, FastBitVector& out)
{
    stepOverInstruction(
        codeBlock, basicBlocks, bytecodeOffset,
        [&] (unsigned bitIndex) {
            // This is the use functor, so we set the bit.
            out.set(bitIndex);
        },
        [&] (unsigned bitIndex) {
            // This is the def functor, so we clear the bit.
            out.clear(bitIndex);
        });
}

static void computeLocalLivenessForBytecodeOffset(CodeBlock* codeBlock, BytecodeBasicBlock* block, Vector<RefPtr<BytecodeBasicBlock> >& basicBlocks, unsigned targetOffset, FastBitVector& result)
{
    ASSERT(!block->isExitBlock());
    ASSERT(!block->isEntryBlock());

    FastBitVector out = block->out();

    for (int i = block->bytecodeOffsets().size() - 1; i >= 0; i--) {
        unsigned bytecodeOffset = block->bytecodeOffsets()[i];
        if (targetOffset > bytecodeOffset)
            break;
        
        stepOverInstruction(codeBlock, basicBlocks, bytecodeOffset, out);
    }

    result.set(out);
}

static void computeLocalLivenessForBlock(CodeBlock* codeBlock, BytecodeBasicBlock* block, Vector<RefPtr<BytecodeBasicBlock> >& basicBlocks)
{
    if (block->isExitBlock() || block->isEntryBlock())
        return;
    computeLocalLivenessForBytecodeOffset(codeBlock, block, basicBlocks, block->leaderBytecodeOffset(), block->in());
}

void BytecodeLivenessAnalysis::runLivenessFixpoint()
{
    UnlinkedCodeBlock* unlinkedCodeBlock = m_codeBlock->unlinkedCodeBlock();
    unsigned numberOfVariables = unlinkedCodeBlock->m_numCalleeRegisters;

    for (unsigned i = 0; i < m_basicBlocks.size(); i++) {
        BytecodeBasicBlock* block = m_basicBlocks[i].get();
        block->in().resize(numberOfVariables);
        block->out().resize(numberOfVariables);
    }

    bool changed;
    m_basicBlocks.last()->in().clearAll();
    m_basicBlocks.last()->out().clearAll();
    FastBitVector newOut;
    newOut.resize(m_basicBlocks.last()->out().numBits());
    do {
        changed = false;
        for (unsigned i = m_basicBlocks.size() - 1; i--;) {
            BytecodeBasicBlock* block = m_basicBlocks[i].get();
            newOut.clearAll();
            for (unsigned j = 0; j < block->successors().size(); j++)
                newOut.merge(block->successors()[j]->in());
            bool outDidChange = block->out().setAndCheck(newOut);
            computeLocalLivenessForBlock(m_codeBlock, block, m_basicBlocks);
            changed |= outDidChange;
        }
    } while (changed);
}

void BytecodeLivenessAnalysis::getLivenessInfoAtBytecodeOffset(unsigned bytecodeOffset, FastBitVector& result)
{
    BytecodeBasicBlock* block = findBasicBlockForBytecodeOffset(m_basicBlocks, bytecodeOffset);
    ASSERT(block);
    ASSERT(!block->isEntryBlock());
    ASSERT(!block->isExitBlock());
    result.resize(block->out().numBits());
    computeLocalLivenessForBytecodeOffset(m_codeBlock, block, m_basicBlocks, bytecodeOffset, result);
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
    
    result.m_map.resize(m_codeBlock->instructions().size());
    
    for (unsigned i = m_basicBlocks.size(); i--;) {
        BytecodeBasicBlock* block = m_basicBlocks[i].get();
        if (block->isEntryBlock() || block->isExitBlock())
            continue;
        
        out = block->out();
        
        for (unsigned i = block->bytecodeOffsets().size(); i--;) {
            unsigned bytecodeOffset = block->bytecodeOffsets()[i];
            stepOverInstruction(m_codeBlock, m_basicBlocks, bytecodeOffset, out);
            result.m_map[bytecodeOffset] = out;
        }
    }
}

void BytecodeLivenessAnalysis::computeKills(BytecodeKills& result)
{
    FastBitVector out;
    
    result.m_codeBlock = m_codeBlock;
    result.m_killSets = std::make_unique<BytecodeKills::KillSet[]>(m_codeBlock->instructions().size());
    
    for (unsigned i = m_basicBlocks.size(); i--;) {
        BytecodeBasicBlock* block = m_basicBlocks[i].get();
        if (block->isEntryBlock() || block->isExitBlock())
            continue;
        
        out = block->out();
        
        for (unsigned i = block->bytecodeOffsets().size(); i--;) {
            unsigned bytecodeOffset = block->bytecodeOffsets()[i];
            stepOverInstruction(
                m_codeBlock, m_basicBlocks, bytecodeOffset,
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
    Interpreter* interpreter = m_codeBlock->vm()->interpreter;
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();
    for (unsigned i = 0; i < m_basicBlocks.size(); i++) {
        BytecodeBasicBlock* block = m_basicBlocks[i].get();
        dataLogF("\nBytecode basic block %u: %p (offset: %u, length: %u)\n", i, block, block->leaderBytecodeOffset(), block->totalBytecodeLength());
        dataLogF("Predecessors: ");
        for (unsigned j = 0; j < block->predecessors().size(); j++) {
            BytecodeBasicBlock* predecessor = block->predecessors()[j];
            dataLogF("%p ", predecessor);
        }
        dataLogF("\n");
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
        for (unsigned bytecodeOffset = block->leaderBytecodeOffset(); bytecodeOffset < block->leaderBytecodeOffset() + block->totalBytecodeLength();) {
            const Instruction* currentInstruction = &instructionsBegin[bytecodeOffset];

            dataLogF("Live variables: ");
            FastBitVector liveBefore = getLivenessInfoAtBytecodeOffset(bytecodeOffset);
            for (unsigned j = 0; j < liveBefore.numBits(); j++) {
                if (liveBefore.get(j))
                    dataLogF("%u ", j);
            }
            dataLogF("\n");
            m_codeBlock->dumpBytecode(WTF::dataFile(), m_codeBlock->globalObject()->globalExec(), instructionsBegin, currentInstruction);

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
    computeBytecodeBasicBlocks(m_codeBlock, m_basicBlocks);
    ASSERT(m_basicBlocks.size());
    runLivenessFixpoint();

    if (Options::dumpBytecodeLivenessResults())
        dumpResults();
}

} // namespace JSC
