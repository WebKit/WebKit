/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "AirOptimizeBlockOrder.h"

#if ENABLE(B3_JIT)

#include "AirBlockWorklist.h"
#include "AirCode.h"
#include "AirPhaseScope.h"
#include <wtf/BubbleSort.h>
#include <wtf/Deque.h>

namespace JSC { namespace B3 { namespace Air {

namespace {

class ChainWorklist {
public:
    ChainWorklist()
        : startNewChain(false)
    {
    }

    bool isEmpty() const
    {
        return blocks.isEmpty();
    }

    size_t size() const
    {
        return blocks.size();
    }

    BasicBlock* pop(IndexSet<BasicBlock*>& done)
    {
        if (startNewChain) {
            startNewChain = false;
            return popNewChain(done);
        }

        return popChain(done);
    }

    void markStartNewChain()
    {
        startNewChain = true;
    }

    void append(BasicBlock* block)
    {
        blocks.append(block);
    }

    BasicBlock* popChain(IndexSet<BasicBlock*>& done)
    {
        // Take the last added successor to continue the chain.
        // This transforms a jump into a fallthrough.
        while (!blocks.isEmpty()) {
            BasicBlock* block = blocks.takeLast();
            if (done.contains(block))
                continue;

            return block;
        }
        return nullptr;
    }

    BasicBlock* popNewChain(IndexSet<BasicBlock*>& done)
    {
        // Take the oldest added successor to start a new chain.
        // We prefer this,
        // - because that keeps earlier blocks earlier
        // - earlier blocks can still have longer chains
        // - better locality, instead of iterating further and
        //   having to backtrack these left-over early blocks
        while (!blocks.isEmpty()) {
            BasicBlock* block = blocks.takeFirst();
            if (done.contains(block))
                continue;

            return block;
        }
        return nullptr;
    }

private:

    bool startNewChain;
    Deque<BasicBlock*, 16> blocks;
};


class SortedSuccessors {
public:
    SortedSuccessors()
    {
    }

    void append(BasicBlock* block)
    {
        m_successors.append(block);
    }

    void process(BlockWorklist& worklist)
    {
        sort();

        // Pushing the successors in ascending order of frequency ensures that the very next block we visit
        // is our highest-frequency successor (unless that successor has already been visited).
        for (unsigned i = 0; i < m_successors.size(); ++i)
            worklist.push(m_successors[i]);

        m_successors.shrink(0);
    }

    void process(ChainWorklist& worklist)
    {
        sort();

        // Pushing the successors in ascending order of frequency ensures that the very next block we visit
        // is our highest-frequency successor (unless that successor has already been visited).
        for (unsigned i = 0; i < m_successors.size(); ++i)
            worklist.append(m_successors[i]);

        m_successors.shrink(0);
    }

private:
    void sort()
    {
        // We prefer a stable sort, and we don't want it to go off the rails if we see NaN. Also, the number
        // of successors is bounded. In fact, it currently cannot be more than 2. :-)
        bubbleSort(
            m_successors.mutableSpan(),
            [] (BasicBlock* left, BasicBlock* right) {
                return left->frequency() < right->frequency();
            });
    }

    Vector<BasicBlock*, 2> m_successors;
};

} // anonymous namespace

static bool detectTriangleStructure(BasicBlock* blockA, ChainWorklist& worklist, IndexSet<BasicBlock*>& done)
{
    // A*
    // |-----.
    // |      |
    // |      C
    // |      |
    // |-----'
    // B

    // Since we don't have actual frequencies,
    // it is better to schedule C before B

    if (blockA->numSuccessors() != 2)
        return false;

    if (blockA->successor(0).isRare())
        return false;

    if (blockA->successor(1).isRare())
        return false;

    auto attemptToDetect = [&](BasicBlock* blockB, BasicBlock* blockC) {
        if ((blockC->numSuccessors() >= 1 && blockC->successor(0).block() == blockB)
            || (blockC->numSuccessors() >= 2 && blockC->successor(1).block() == blockB)) {
            if (!done.contains(blockB))
                worklist.append(blockB);
            if (!done.contains(blockC))
                worklist.append(blockC);
            return true;
        }
        return false;
    };

    auto* block0 = blockA->successor(0).block();
    auto* block1 = blockA->successor(1).block();
    return attemptToDetect(block0, block1)
        || attemptToDetect(block1, block0);
}

static bool detectDiamondStructure(BasicBlock* blockB, ChainWorklist& worklist, IndexSet<BasicBlock*>& done)
{
    //     A
    //  .--'--.
    // |       |
    // B*      C
    // |       |
    //  '--.--'
    //     D

    // B* is the block we are currently looking at.

    // Since we don't have actual frequencies,
    // it is better to not decide which branch (B,C) is best and
    // assume both have equal chance.
    // With a small penalty we better organize it as:
    // A B C D
    // That way we have one small jump for case B and for case C,
    // instead of having no jumps for B and two long jumps for C.

    if (blockB->numSuccessors() != 1)
        return false;

    if (blockB->numPredecessors() != 1)
        return false;

    if (blockB->successor(0).isRare())
        return false;

    BasicBlock* blockD = blockB->successor(0).block();
    BasicBlock* blockA = blockB->predecessor(0);

    if (blockA->numSuccessors() != 2)
        return false;

    BasicBlock* blockC;
    if (blockA->successor(0).block() == blockB) {
        if (blockA->successor(1).isRare())
            return false;
        blockC = blockA->successor(1).block();
    } else if (blockA->successor(1).block() == blockB) {
        if (blockA->successor(0).isRare())
            return false;
        blockC = blockA->successor(0).block();
    } else
        return false;

    if (blockC->numSuccessors() != 1)
        return false;

    if (blockC->numPredecessors() != 1)
        return false;

    if (blockC->successor(0).block() != blockD)
        return false;

    if (!done.contains(blockD))
        worklist.append(blockD);
    if (!done.contains(blockC))
        worklist.append(blockC);
    return true;
}

static bool detectExclusiveSuccessor(BasicBlock* blockA, ChainWorklist& worklist, IndexSet<BasicBlock*>& done)
{
    //     A*       D
    //  .--'--.     |
    // |       |.---'
    // B       C

    // A* is the block we are currently looking at.

    // It's better to use successor B as the fallthrough block,
    // because C can still become the fallthrough block from the other predecessors.

    if (blockA->numSuccessors() != 2)
        return false;
    if (blockA->successor(0).isRare())
        return false;
    if (blockA->successor(1).isRare())
        return false;
    if (blockA->successor(0).block()->frequency() != blockA->successor(1).block()->frequency())
        return false;

    BasicBlock* blockB = blockA->successor(0).block();
    BasicBlock* blockC = blockA->successor(1).block();
    if (blockB->numPredecessors() == 1
        && blockC->numPredecessors() > 1) {
        // Same frequency, with succ[0] having only one predecessor.
        // and succ[1] having multiple predecessors.
        // It is better to add succ[0] as last to get a fallthrough.
        // Since except here there is no chance succ[0] can fallthrough, but succ[1] still can.
        if (!done.contains(blockC))
            worklist.append(blockC);
        if (!done.contains(blockB))
            worklist.append(blockB);
        return true;
    }

    return false;
}

Vector<BasicBlock*> blocksInOptimizedOrder(Code& code)
{
    Vector<BasicBlock*> blocksInOrder;

    SortedSuccessors sortedSlowSuccessors;
    SortedSuccessors sortedSuccessors;
    ChainWorklist chainWorklist;
    IndexSet<BasicBlock*> done;

    // We expect entrypoint lowering to have already happened.
    RELEASE_ASSERT(code.numEntrypoints());

    auto appendSuccessor = [&] (const FrequentedBlock& block) {
        if (block.isRare())
            sortedSlowSuccessors.append(block.block());
        else
            sortedSuccessors.append(block.block());
    };
    
    // For everything but the first entrypoint, we push them in order of frequency and frequency
    // class.
    for (unsigned i = 1; i < code.numEntrypoints(); ++i)
        appendSuccessor(code.entrypoint(i));
    
    // Always push the primary successor last so that it gets highest priority.
    chainWorklist.append(code.entrypoint(0).block());

    while (BasicBlock* block = chainWorklist.pop(done)) {
        ASSERT(!done.contains(block));

        done.add(block);
        blocksInOrder.append(block);

        size_t size = chainWorklist.size();

        if (!detectTriangleStructure(block, chainWorklist, done)
            && !detectDiamondStructure(block, chainWorklist, done)
            && !detectExclusiveSuccessor(block, chainWorklist, done)) {

            for (FrequentedBlock& successor : block->successors()) {
                if (!done.contains(successor.block()))
                    appendSuccessor(successor);
            }
        }
        sortedSuccessors.process(chainWorklist);

        // Detect if we added a successor. If not decide for a good candidate.
        if (size == chainWorklist.size())
            chainWorklist.markStartNewChain();
    }

    BlockWorklist slowWorklist;
    sortedSlowSuccessors.process(slowWorklist);

    while (BasicBlock* block = slowWorklist.pop()) {
        // We might have already processed this block.
        if (done.contains(block))
            continue;

        done.add(block);

        blocksInOrder.append(block);
        for (BasicBlock* successor : block->successorBlocks())
            sortedSlowSuccessors.append(successor);
        sortedSlowSuccessors.process(slowWorklist);
    }

    ASSERT(chainWorklist.isEmpty());
    ASSERT(slowWorklist.isEmpty());

    return blocksInOrder;
}

void optimizeBlockOrder(Code& code)
{
    PhaseScope phaseScope(code, "optimizeBlockOrder"_s);

    Vector<BasicBlock*> blocksInOrder = blocksInOptimizedOrder(code);
    
    // Place blocks into Code's block list according to the ordering in blocksInOrder. We do this by leaking
    // all of the blocks and then readopting them.
    for (auto& entry : code.blockList())
        entry.release();

    code.blockList().shrink(0);

    for (unsigned i = 0; i < blocksInOrder.size(); ++i) {
        BasicBlock* block = blocksInOrder[i];
        block->setIndex(i);
        code.blockList().append(std::unique_ptr<BasicBlock>(block));
    }

    // Finally, flip any branches that we recognize. It's most optimal if the taken successor does not point
    // at the next block.
    for (BasicBlock* block : code) {
        Inst& branch = block->last();

        // It's somewhat tempting to just say that if the block has two successors and the first arg is
        // invertible, then we can do the optimization. But that's wagging the dog. The fact that an
        // instruction happens to have an argument that is invertible doesn't mean it's a branch, even though
        // it is true that currently only branches have invertible arguments. It's also tempting to say that
        // the /branch flag in AirOpcode.opcodes tells us that something is a branch - except that there,
        // /branch also means Jump. The approach taken here means that if you add new branch instructions and
        // forget about this phase, then at worst your new instructions won't opt into the inversion
        // optimization.  You'll probably realize that as soon as you look at the disassembly, and it
        // certainly won't cause any correctness issues.
        
        switch (branch.kind.opcode) {
        case BranchOnFlags:
        case Branch8:
        case Branch32:
        case Branch64:
        case BranchTest8:
        case BranchTest32:
        case BranchTest64:
        case BranchFloat:
        case BranchDouble:
        case BranchAdd32:
        case BranchAdd64:
        case BranchMul32:
        case BranchMul64:
        case BranchSub32:
        case BranchSub64:
        case BranchNeg32:
        case BranchNeg64:
        case BranchAtomicStrongCAS8:
        case BranchAtomicStrongCAS16:
        case BranchAtomicStrongCAS32:
        case BranchAtomicStrongCAS64:
            if (code.findNextBlock(block) == block->successorBlock(0) && branch.args[0].isInvertible()) {
                std::swap(block->successor(0), block->successor(1));
                branch.args[0] = branch.args[0].inverted();
            }
            break;

        default:
            break;
        }
    }
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
