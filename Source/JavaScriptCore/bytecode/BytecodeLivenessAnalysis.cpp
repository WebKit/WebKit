/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
    if (!virtualReg.isLocal())
        return false;
    
    if (codeBlock->captureCount()
        && operand <= codeBlock->captureStart()
        && operand > codeBlock->captureEnd())
        return false;
    
    return true;
}

static void setForOperand(CodeBlock* codeBlock, FastBitVector& bits, int operand)
{
    ASSERT(isValidRegisterForLiveness(codeBlock, operand));
    VirtualRegister virtualReg(operand);
    if (virtualReg.offset() > codeBlock->captureStart())
        bits.set(virtualReg.toLocal());
    else
        bits.set(virtualReg.toLocal() - codeBlock->captureCount());
}

namespace {

class SetBit {
public:
    SetBit(FastBitVector& bits)
        : m_bits(bits)
    {
    }
    
    void operator()(CodeBlock* codeBlock, Instruction*, OpcodeID, int operand)
    {
        if (isValidRegisterForLiveness(codeBlock, operand))
            setForOperand(codeBlock, m_bits, operand);
    }
    
private:
    FastBitVector& m_bits;
};

} // anonymous namespace

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

static void stepOverInstruction(CodeBlock* codeBlock, Vector<RefPtr<BytecodeBasicBlock>>& basicBlocks, unsigned bytecodeOffset, FastBitVector& uses, FastBitVector& defs, FastBitVector& out)
{
    uses.clearAll();
    defs.clearAll();
    
    SetBit setUses(uses);
    SetBit setDefs(defs);
    computeUsesForBytecodeOffset(codeBlock, bytecodeOffset, setUses);
    computeDefsForBytecodeOffset(codeBlock, bytecodeOffset, setDefs);
    
    out.exclude(defs);
    out.merge(uses);
    
    // If we have an exception handler, we want the live-in variables of the 
    // exception handler block to be included in the live-in of this particular bytecode.
    if (HandlerInfo* handler = codeBlock->handlerForBytecodeOffset(bytecodeOffset)) {
        BytecodeBasicBlock* handlerBlock = findBasicBlockWithLeaderOffset(basicBlocks, handler->target);
        ASSERT(handlerBlock);
        out.merge(handlerBlock->in());
    }
}

static void computeLocalLivenessForBytecodeOffset(CodeBlock* codeBlock, BytecodeBasicBlock* block, Vector<RefPtr<BytecodeBasicBlock> >& basicBlocks, unsigned targetOffset, FastBitVector& result)
{
    ASSERT(!block->isExitBlock());
    ASSERT(!block->isEntryBlock());

    FastBitVector out = block->out();

    FastBitVector uses;
    FastBitVector defs;
    uses.resize(out.numBits());
    defs.resize(out.numBits());

    for (int i = block->bytecodeOffsets().size() - 1; i >= 0; i--) {
        unsigned bytecodeOffset = block->bytecodeOffsets()[i];
        if (targetOffset > bytecodeOffset)
            break;
        
        stepOverInstruction(codeBlock, basicBlocks, bytecodeOffset, uses, defs, out);
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
    unsigned numberOfVariables =
        unlinkedCodeBlock->m_numCalleeRegisters - m_codeBlock->captureCount();

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
        for (int i = m_basicBlocks.size() - 2; i >= 0; i--) {
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

void BytecodeLivenessAnalysis::getLivenessInfoForNonCapturedVarsAtBytecodeOffset(unsigned bytecodeOffset, FastBitVector& result)
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
    if (operandIsAlwaysLive(m_codeBlock, operand))
        return true;
    FastBitVector result;
    getLivenessInfoForNonCapturedVarsAtBytecodeOffset(bytecodeOffset, result);
    return operandThatIsNotAlwaysLiveIsLive(m_codeBlock, result, operand);
}

FastBitVector getLivenessInfo(CodeBlock* codeBlock, const FastBitVector& out)
{
    FastBitVector result;

    unsigned numCapturedVars = codeBlock->captureCount();
    if (numCapturedVars) {
        int firstCapturedLocal = VirtualRegister(codeBlock->captureStart()).toLocal();
        result.resize(out.numBits() + numCapturedVars);
        for (unsigned i = 0; i < numCapturedVars; ++i)
            result.set(firstCapturedLocal + i);
    } else
        result.resize(out.numBits());

    int outLength = out.numBits();
    ASSERT(outLength >= 0);
    for (int i = 0; i < outLength; i++) {
        if (!out.get(i))
            continue;

        if (!numCapturedVars) {
            result.set(i);
            continue;
        }

        if (virtualRegisterForLocal(i).offset() > codeBlock->captureStart())
            result.set(i);
        else 
            result.set(numCapturedVars + i);
    }
    return result;
}

FastBitVector BytecodeLivenessAnalysis::getLivenessInfoAtBytecodeOffset(unsigned bytecodeOffset)
{
    FastBitVector out;
    getLivenessInfoForNonCapturedVarsAtBytecodeOffset(bytecodeOffset, out);
    return getLivenessInfo(m_codeBlock, out);
}

void BytecodeLivenessAnalysis::computeFullLiveness(FullBytecodeLiveness& result)
{
    FastBitVector out;
    FastBitVector uses;
    FastBitVector defs;
    
    result.m_codeBlock = m_codeBlock;
    result.m_map.clear();
    
    for (unsigned i = m_basicBlocks.size(); i--;) {
        BytecodeBasicBlock* block = m_basicBlocks[i].get();
        if (block->isEntryBlock() || block->isExitBlock())
            continue;
        
        out = block->out();
        uses.resize(out.numBits());
        defs.resize(out.numBits());
        
        for (unsigned i = block->bytecodeOffsets().size(); i--;) {
            unsigned bytecodeOffset = block->bytecodeOffsets()[i];
            stepOverInstruction(m_codeBlock, m_basicBlocks, bytecodeOffset, uses, defs, out);
            result.m_map.add(bytecodeOffset, out);
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
