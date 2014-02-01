/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DFGDominators.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"

namespace JSC { namespace DFG {

Dominators::Dominators()
{
}

Dominators::~Dominators()
{
}

void Dominators::compute(Graph& graph)
{
    // This implements a naive dominator solver.
    
    ASSERT(graph.block(0)->predecessors.isEmpty());
    
    unsigned numBlocks = graph.numBlocks();
    
    // Allocate storage for the dense dominance matrix. 
    if (numBlocks > m_results.size()) {
        m_results.grow(numBlocks);
        for (unsigned i = numBlocks; i--;)
            m_results[i].resize(numBlocks);
        m_scratch.resize(numBlocks);
    }

    // We know that the entry block is only dominated by itself.
    m_results[0].clearAll();
    m_results[0].set(0);

    // Find all of the valid blocks.
    m_scratch.clearAll();
    for (unsigned i = numBlocks; i--;) {
        if (!graph.block(i))
            continue;
        m_scratch.set(i);
    }
    
    // Mark all nodes as dominated by everything.
    for (unsigned i = numBlocks; i-- > 1;) {
        if (!graph.block(i) || graph.block(i)->predecessors.isEmpty())
            m_results[i].clearAll();
        else
            m_results[i].set(m_scratch);
    }

    // Iteratively eliminate nodes that are not dominator.
    bool changed;
    do {
        changed = false;
        // Prune dominators in all non entry blocks: forward scan.
        for (unsigned i = 1; i < numBlocks; ++i)
            changed |= pruneDominators(graph, i);

        if (!changed)
            break;

        // Prune dominators in all non entry blocks: backward scan.
        changed = false;
        for (unsigned i = numBlocks; i-- > 1;)
            changed |= pruneDominators(graph, i);
    } while (changed);
}

bool Dominators::pruneDominators(Graph& graph, BlockIndex idx)
{
    BasicBlock* block = graph.block(idx);

    if (!block || block->predecessors.isEmpty())
        return false;

    // Find the intersection of dom(preds).
    m_scratch.set(m_results[block->predecessors[0]->index]);
    for (unsigned j = block->predecessors.size(); j-- > 1;)
        m_scratch.filter(m_results[block->predecessors[j]->index]);

    // The block is also dominated by itself.
    m_scratch.set(idx);

    return m_results[idx].setAndCheck(m_scratch);
}

void Dominators::dump(Graph& graph, PrintStream& out) const
{
    for (BlockIndex blockIndex = 0; blockIndex < graph.numBlocks(); ++blockIndex) {
        BasicBlock* block = graph.block(blockIndex);
        if (!block)
            continue;
        out.print("    Block ", *block, ":");
        for (BlockIndex otherIndex = 0; otherIndex < graph.numBlocks(); ++otherIndex) {
            if (!dominates(block->index, otherIndex))
                continue;
            out.print(" #", otherIndex);
        }
        out.print("\n");
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

