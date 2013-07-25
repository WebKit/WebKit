/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
#include "DFGCFAPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGAbstractState.h"
#include "DFGGraph.h"
#include "DFGPhase.h"
#include "Operations.h"

namespace JSC { namespace DFG {

class CFAPhase : public Phase {
public:
#if DFG_ENABLE(DFG_PROPAGATION_VERBOSE)
    static const bool verbose = true;
#else
    static const bool verbose = false;
#endif

    CFAPhase(Graph& graph)
        : Phase(graph, "control flow analysis")
        , m_state(graph)
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == ThreadedCPS);
        ASSERT(m_graph.m_unificationState == GloballyUnified);
        ASSERT(m_graph.m_refCountState == EverythingIsLive);
        
        m_count = 0;
        
        // This implements a pseudo-worklist-based forward CFA, except that the visit order
        // of blocks is the bytecode program order (which is nearly topological), and
        // instead of a worklist we just walk all basic blocks checking if cfaShouldRevisit
        // is set to true. This is likely to balance the efficiency properties of both
        // worklist-based and forward fixpoint-based approaches. Like a worklist-based
        // approach, it won't visit code if it's meaningless to do so (nothing changed at
        // the head of the block or the predecessors have not been visited). Like a forward
        // fixpoint-based approach, it has a high probability of only visiting a block
        // after all predecessors have been visited. Only loops will cause this analysis to
        // revisit blocks, and the amount of revisiting is proportional to loop depth.
        
        AbstractState::initialize(m_graph);
        
        do {
            m_changed = false;
            performForwardCFA();
        } while (m_changed);
        
        return true;
    }
    
private:
    void performBlockCFA(BlockIndex blockIndex)
    {
        BasicBlock* block = m_graph.m_blocks[blockIndex].get();
        if (!block)
            return;
        if (!block->cfaShouldRevisit)
            return;
        if (verbose)
            dataLogF("   Block #%u (bc#%u):\n", blockIndex, block->bytecodeBegin);
        m_state.beginBasicBlock(block);
        if (verbose) {
            dataLogF("      head vars: ");
            dumpOperands(block->valuesAtHead, WTF::dataFile());
            dataLogF("\n");
        }
        for (unsigned i = 0; i < block->size(); ++i) {
            if (verbose) {
                Node* node = block->at(i);
                dataLogF("      %s @%u: ", Graph::opName(node->op()), node->index());
                m_state.dump(WTF::dataFile());
                dataLogF("\n");
            }
            if (!m_state.execute(i, AbstractState::StillConverging)) {
                if (verbose)
                    dataLogF("         Expect OSR exit.\n");
                break;
            }
        }
        if (verbose) {
            dataLogF("      tail regs: ");
            m_state.dump(WTF::dataFile());
            dataLogF("\n");
        }
        m_changed |= m_state.endBasicBlock(AbstractState::MergeToSuccessors);
        
        if (verbose) {
            dataLogF("      tail vars: ");
            dumpOperands(block->valuesAtTail, WTF::dataFile());
            dataLogF("\n");
        }
    }
    
    void performForwardCFA()
    {
        ++m_count;
        if (verbose)
            dataLogF("CFA [%u]\n", ++m_count);
        
        for (BlockIndex block = 0; block < m_graph.m_blocks.size(); ++block)
            performBlockCFA(block);
    }

private:
    AbstractState m_state;
    
    bool m_changed;
    unsigned m_count;
};

bool performCFA(Graph& graph)
{
    SamplingRegion samplingRegion("DFG CFA Phase");
    return runPhase<CFAPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
