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
#include "DFGFlushLivenessAnalysisPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlockInlines.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "OperandsInlines.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

class FlushLivenessAnalysisPhase : public Phase {
public:
    FlushLivenessAnalysisPhase(Graph& graph)
        : Phase(graph, "flush-liveness analysis")
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        
        // Liveness is a backwards analysis; the roots are the blocks that
        // end in a terminal (Return/Unreachable). For now, we
        // use a fixpoint formulation since liveness is a rapid analysis with
        // convergence guaranteed after O(connectivity).
        
        // Start by assuming that everything is dead.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            block->ssa->flushAtHead.fill(FlushedAt());
            block->ssa->flushAtTail.fill(FlushedAt());
        }
        
        do {
            m_changed = false;
            for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;)
                process(blockIndex);
        } while (m_changed);
        
        Operands<FlushedAt>& root = m_graph.block(0)->ssa->flushAtHead;
        for (unsigned i = root.size(); i--;) {
            if (root.isArgument(i)) {
                if (!root[i]
                    || root[i] == FlushedAt(FlushedJSValue, VirtualRegister(root.operandForIndex(i))))
                    continue;
            } else {
                if (!root[i])
                    continue;
            }
            dataLog(
                "Bad flush liveness analysis result: bad flush liveness at root: ",
                root, "\n");
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
        
        m_live = block->ssa->flushAtTail;
        
        for (unsigned nodeIndex = block->size(); nodeIndex--;) {
            Node* node = block->at(nodeIndex);
            
            switch (node->op()) {
            case SetLocal: {
                VariableAccessData* variable = node->variableAccessData();
                FlushedAt& current = m_live.operand(variable->local());
                if (!!current && current != variable->flushedAt())
                    reportError(node);
                current = FlushedAt();
                break;
            }
                
            case GetArgument: {
                VariableAccessData* variable = node->variableAccessData();
                ASSERT(variable->local() == variable->machineLocal());
                ASSERT(variable->local().isArgument());
                FlushedAt& current = m_live.operand(variable->local());
                if (!!current && current != variable->flushedAt())
                    reportError(node);
                current = FlushedAt(FlushedJSValue, node->local());
                break;
            }
                
            case Flush:
            case GetLocal: {
                VariableAccessData* variable = node->variableAccessData();
                FlushedAt& current = m_live.operand(variable->local());
                if (!!current && current != variable->flushedAt())
                    reportError(node);
                current = variable->flushedAt();
                break;
            }
                
            default:
                break;
            }
        }
        
        if (m_live == block->ssa->flushAtHead)
            return;
        
        m_changed = true;
        block->ssa->flushAtHead = m_live;
        for (unsigned i = block->predecessors.size(); i--;) {
            BasicBlock* predecessor = block->predecessors[i];
            for (unsigned j = m_live.size(); j--;) {
                FlushedAt& predecessorFlush = predecessor->ssa->flushAtTail[j];
                FlushedAt myFlush = m_live[j];
                
                // Three possibilities:
                // 1) Predecessor format is Dead, in which case it acquires our format.
                // 2) Predecessor format is not Dead but our format is dead, in which
                //    case we acquire the predecessor format.
                // 3) Predecessor format is identical to our format, in which case we
                //    do nothing.
                // 4) Predecessor format is different from our format and it's not Dead,
                //    in which case we have an erroneous set of Flushes and SetLocals.

                if (!predecessorFlush) {
                    predecessorFlush = myFlush;
                    continue;
                }

                if (!myFlush) {
                    m_live[j] = predecessorFlush;
                    continue;
                }

                if (predecessorFlush == myFlush)
                    continue;
                
                dataLog(
                    "Bad Flush merge at edge ", *predecessor, " -> ", *block,
                    ", local variable r", m_live.operandForIndex(j), ": ", *predecessor,
                    " has ", predecessorFlush, " and ", *block, " has ", myFlush, ".\n");
                dataLog("IR at time of error:\n");
                m_graph.dump();
                CRASH();
            }
        }
    }
    
    NO_RETURN_DUE_TO_CRASH void reportError(Node* node)
    {
        dataLog(
            "Bad flush merge at node ", node, ", r", node->local(), ": node claims ",
            node->variableAccessData()->flushedAt(), " but backwards flow claims ",
            m_live.operand(node->local()), ".\n");
        dataLog("IR at time of error:\n");
        m_graph.dump();
        CRASH();
    }
    
    bool m_changed;
    Operands<FlushedAt> m_live;
};

bool performFlushLivenessAnalysis(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Flush-Liveness Analysis Phase");
    return runPhase<FlushLivenessAnalysisPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

