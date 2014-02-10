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

#if ENABLE(DFG_JIT)

#include "DFGCFAPhase.h"

#include "DFGAbstractInterpreterInlines.h"
#include "DFGGraph.h"
#include "DFGInPlaceAbstractState.h"
#include "DFGPhase.h"
#include "DFGSafeToExecute.h"
#include "OperandsInlines.h"
#include "Operations.h"

namespace JSC { namespace DFG {

class CFAPhase : public Phase {
public:
    CFAPhase(Graph& graph)
        : Phase(graph, "control flow analysis")
        , m_state(graph)
        , m_interpreter(graph, m_state)
        , m_verbose(Options::verboseCFA())
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == ThreadedCPS || m_graph.m_form == SSA);
        ASSERT(m_graph.m_unificationState == GloballyUnified);
        ASSERT(m_graph.m_refCountState == EverythingIsLive);
        
        m_count = 0;
        
        if (m_verbose && !shouldDumpGraphAtEachPhase()) {
            dataLog("Graph before CFA:\n");
            m_graph.dump();
        }
        
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
        
        m_state.initialize();
        
        do {
            m_changed = false;
            performForwardCFA();
        } while (m_changed);
        
        return true;
    }
    
private:
    void performBlockCFA(BasicBlock* block)
    {
        if (!block)
            return;
        if (!block->cfaShouldRevisit)
            return;
        if (m_verbose)
            dataLog("   Block ", *block, ":\n");
        m_state.beginBasicBlock(block);
        if (m_verbose)
            dataLog("      head vars: ", block->valuesAtHead, "\n");
        for (unsigned i = 0; i < block->size(); ++i) {
            if (m_verbose) {
                Node* node = block->at(i);
                dataLogF("      %s @%u: ", Graph::opName(node->op()), node->index());
                
                if (!safeToExecute(m_state, m_graph, node))
                    dataLog("(UNSAFE) ");
                
                m_interpreter.dump(WTF::dataFile());
                
                if (m_state.haveStructures())
                    dataLog(" (Have Structures)");
                dataLogF("\n");
            }
            if (!m_interpreter.execute(i)) {
                if (m_verbose)
                    dataLogF("         Expect OSR exit.\n");
                break;
            }
        }
        if (m_verbose) {
            dataLogF("      tail regs: ");
            m_interpreter.dump(WTF::dataFile());
            dataLogF("\n");
        }
        m_changed |= m_state.endBasicBlock(MergeToSuccessors);
        
        if (m_verbose)
            dataLog("      tail vars: ", block->valuesAtTail, "\n");
    }
    
    void performForwardCFA()
    {
        ++m_count;
        if (m_verbose)
            dataLogF("CFA [%u]\n", ++m_count);
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex)
            performBlockCFA(m_graph.block(blockIndex));
    }

private:
    InPlaceAbstractState m_state;
    AbstractInterpreter<InPlaceAbstractState> m_interpreter;
    
    bool m_verbose;
    
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
