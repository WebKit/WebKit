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
#include "DFGCriticalEdgeBreakingPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlockInlines.h"
#include "DFGBlockInsertionSet.h"
#include "DFGGraph.h"
#include "DFGPhase.h"
#include "JSCJSValueInlines.h"

namespace JSC { namespace DFG {

class CriticalEdgeBreakingPhase : public Phase {
public:
    CriticalEdgeBreakingPhase(Graph& graph)
        : Phase(graph, "critical edge breaking"_s)
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        return breakCriticalEdge(m_graph, m_insertionSet, nullptr);
    }

    static bool breakCriticalEdge(Graph& graph, BlockInsertionSet& insertionSet, PadSetupFunc functor)
    {
        ASSERT(insertionSet.isEmpty());
        for (BlockIndex blockIndex = 0; blockIndex < graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = graph.block(blockIndex);
            if (!block)
                continue;
            
            // An edge A->B is critical if A has multiple successor and B has multiple
            // predecessors. Thus we fail early if we don't have multiple successors.
            
            if (block->numSuccessors() <= 1)
                continue;

            // Break critical edges by inserting a "Jump" pad block in place of each
            // unique A->B critical edge.
            UncheckedKeyHashMap<BasicBlock*, BasicBlock*> successorPads;

            for (unsigned i = block->numSuccessors(); i--;) {
                BasicBlock** successor = &block->successor(i);
                if ((*successor)->predecessors.size() <= 1)
                    continue;

                BasicBlock* pad = nullptr;
                auto iter = successorPads.find(*successor);

                if (iter == successorPads.end()) {
                    pad = insertionSet.insertBefore(*successor, (*successor)->executionCount);
                    pad->appendNode(
                        graph, SpecNone, Jump, (*successor)->at(0)->origin, OpInfo(*successor));
                    pad->predecessors.append(block);
                    (*successor)->replacePredecessor(block, pad);
                    successorPads.set(*successor, pad);
                    if (functor)
                        functor(graph, pad, *successor);
                } else
                    pad = iter->value;

                *successor = pad;
            }
        }
        
        return insertionSet.execute();
    }

private:
    BlockInsertionSet m_insertionSet;
};

bool performCriticalEdgeBreaking(Graph& graph)
{
    return runPhase<CriticalEdgeBreakingPhase>(graph);
}

bool performCriticalEdgeBreaking(Graph& graph, BlockInsertionSet& insertionSet, PadSetupFunc functor)
{
    return CriticalEdgeBreakingPhase::breakCriticalEdge(graph, insertionSet, functor);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


