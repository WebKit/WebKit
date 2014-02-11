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

#if ENABLE(DFG_JIT)

#include "DFGLivenessAnalysisPhase.h"

#include "DFGBasicBlockInlines.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

class LivenessAnalysisPhase : public Phase {
public:
    LivenessAnalysisPhase(Graph& graph)
        : Phase(graph, "liveness analysis")
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        
        // Liveness is a backwards analysis; the roots are the blocks that
        // end in a terminal (Return/Throw/ThrowReferenceError). For now, we
        // use a fixpoint formulation since liveness is a rapid analysis with
        // convergence guaranteed after O(connectivity).
        
        // Start by assuming that everything is dead.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            block->ssa->liveAtHead.clear();
            block->ssa->liveAtTail.clear();
        }
        
        do {
            m_changed = false;
            for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;)
                process(blockIndex);
        } while (m_changed);
        
        if (!m_graph.block(0)->ssa->liveAtHead.isEmpty()) {
            dataLog(
                "Bad liveness analysis result: live at root is not empty: ",
                nodeListDump(m_graph.block(0)->ssa->liveAtHead), "\n");
            dataLog("IR at time of error:\n");
            m_graph.dump();
            CRASH();
        }
        
        return true;
    }

private:
    void process(BlockIndex blockIndex)
    {
        BasicBlock* block = m_graph.block(blockIndex);
        if (!block)
            return;
        
        // FIXME: It's likely that this can be improved, for static analyses that use
        // HashSets. https://bugs.webkit.org/show_bug.cgi?id=118455
        m_live = block->ssa->liveAtTail;
        
        for (unsigned nodeIndex = block->size(); nodeIndex--;) {
            Node* node = block->at(nodeIndex);
            
            // Given an Upsilon:
            //
            //    n: Upsilon(@x, ^p)
            //
            // We say that it def's @p and @n and uses @x.
            //
            // Given a Phi:
            //
            //    p: Phi()
            //
            // We say nothing. It's neither a use nor a def.
            //
            // Given a node:
            //
            //    n: Thingy(@a, @b, @c)
            //
            // We say that it def's @n and uses @a, @b, @c.
            
            switch (node->op()) {
            case Upsilon: {
                Node* phi = node->phi();
                m_live.remove(phi);
                m_live.remove(node);
                m_live.add(node->child1().node());
                break;
            }
                
            case Phi: {
                break;
            }
                
            default:
                m_live.remove(node);
                DFG_NODE_DO_TO_CHILDREN(m_graph, node, addChildUse);
                break;
            }
        }
        
        if (m_live == block->ssa->liveAtHead)
            return;
        
        m_changed = true;
        block->ssa->liveAtHead = m_live;
        for (unsigned i = block->predecessors.size(); i--;)
            block->predecessors[i]->ssa->liveAtTail.add(m_live.begin(), m_live.end());
    }
    
    void addChildUse(Node*, Edge& edge)
    {
        addChildUse(edge);
    }
    
    void addChildUse(Edge& edge)
    {
        edge.setKillStatus(m_live.add(edge.node()).isNewEntry ? DoesKill : DoesNotKill);
    }
    
    bool m_changed;
    HashSet<Node*> m_live;
};

bool performLivenessAnalysis(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Liveness Analysis Phase");
    return runPhase<LivenessAnalysisPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

