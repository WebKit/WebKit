/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "B3BlockInsertionSet.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3InsertionSet.h"
#include "B3ProcedureInlines.h"
#include <wtf/BubbleSort.h>

namespace JSC { namespace B3 {

BlockInsertionSet::BlockInsertionSet(Procedure &proc)
    : m_proc(proc)
{
}

BlockInsertionSet::~BlockInsertionSet() { }

void BlockInsertionSet::insert(BlockInsertion&& insertion)
{
    m_insertions.append(WTFMove(insertion));
}

BasicBlock* BlockInsertionSet::insert(unsigned index, double frequency)
{
    std::unique_ptr<BasicBlock> block(new BasicBlock(UINT_MAX, frequency));
    BasicBlock* result = block.get();
    insert(BlockInsertion(index, WTFMove(block)));
    return result;
}

BasicBlock* BlockInsertionSet::insertBefore(BasicBlock* before, double frequency)
{
    return insert(before->index(), frequency == frequency ? frequency : before->frequency());
}

BasicBlock* BlockInsertionSet::insertAfter(BasicBlock* after, double frequency)
{
    return insert(after->index() + 1, frequency == frequency ? frequency : after->frequency());
}

BasicBlock* BlockInsertionSet::splitForward(
    BasicBlock* block, unsigned& valueIndex, InsertionSet* insertionSet, double frequency)
{
    Value* value = block->at(valueIndex);

    // Create a new block that will go just before 'block', and make it contain everything prior
    // to 'valueIndex'.
    BasicBlock* result = insertBefore(block, frequency);
    result->m_values.resize(valueIndex + 1);
    for (unsigned i = valueIndex; i--;)
        result->m_values[i] = block->m_values[i];

    // Make the new block jump to 'block'.
    result->m_values[valueIndex] =
        m_proc.add<ControlValue>(Jump, value->origin(), FrequentedBlock(block));

    // If we had inserted things into 'block' before this, execute those insertions now.
    if (insertionSet)
        insertionSet->execute(result);

    // Remove everything prior to 'valueIndex' from 'block', since those things are now in the
    // new block.
    block->m_values.remove(0, valueIndex);

    // This is being used in a forward loop over 'block'. Update the index of the loop so that
    // it can continue to the next block.
    valueIndex = 0;

    // Fixup the predecessors of 'block'. They now must jump to the new block.
    result->predecessors() = WTFMove(block->predecessors());
    block->addPredecessor(result);
    for (BasicBlock* predecessor : result->predecessors())
        predecessor->replaceSuccessor(block, result);

    return result;
}

bool BlockInsertionSet::execute()
{
    if (m_insertions.isEmpty())
        return false;
    
    // We allow insertions to be given to us in any order. So, we need to sort them before
    // running WTF::executeInsertions. We strongly prefer a stable sort and we want it to be
    // fast, so we use bubble sort.
    bubbleSort(m_insertions.begin(), m_insertions.end());

    executeInsertions(m_proc.m_blocks, m_insertions);
    
    // Prune out empty entries. This isn't strictly necessary but it's
    // healthy to keep the block list from growing.
    m_proc.m_blocks.removeAllMatching(
        [&] (std::unique_ptr<BasicBlock>& blockPtr) -> bool {
            return !blockPtr;
        });
    
    // Make sure that the blocks know their new indices.
    for (unsigned i = 0; i < m_proc.m_blocks.size(); ++i)
        m_proc.m_blocks[i]->m_index = i;
    
    return true;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

