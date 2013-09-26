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
#include "Operations.h"

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
            block->ssa->flushFormatAtHead.fill(DeadFlush);
            block->ssa->flushFormatAtTail.fill(DeadFlush);
        }
        
        do {
            m_changed = false;
            for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;)
                process(blockIndex);
        } while (m_changed);
        
        Operands<FlushFormat>& root = m_graph.block(0)->ssa->flushFormatAtHead;
        for (unsigned i = root.size(); i--;) {
            if (root.isArgument(i)) {
                if (root[i] == DeadFlush || root[i] == FlushedJSValue)
                    continue;
            } else {
                if (root[i] == DeadFlush)
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
        
        m_live = block->ssa->flushFormatAtTail;
        
        for (unsigned nodeIndex = block->size(); nodeIndex--;) {
            Node* node = block->at(nodeIndex);
            
            switch (node->op()) {
            case SetLocal: {
                VariableAccessData* variable = node->variableAccessData();
                setForNode(node, variable->local(), variable->flushFormat(), DeadFlush);
                break;
            }
                
            case GetArgument: {
                VariableAccessData* variable = node->variableAccessData();
                setForNode(node, variable->local(), variable->flushFormat(), FlushedJSValue);
                break;
            }
                
            case Flush:
            case GetLocal: {
                VariableAccessData* variable = node->variableAccessData();
                FlushFormat format = variable->flushFormat();
                setForNode(node, variable->local(), format, format);
                break;
            }
                
            default:
                break;
            }
        }
        
        if (m_live == block->ssa->flushFormatAtHead)
            return;
        
        m_changed = true;
        block->ssa->flushFormatAtHead = m_live;
        for (unsigned i = block->predecessors.size(); i--;) {
            BasicBlock* predecessor = block->predecessors[i];
            for (unsigned j = m_live.size(); j--;) {
                FlushFormat& predecessorFormat = predecessor->ssa->flushFormatAtTail[j];
                FlushFormat myFormat = m_live[j];
                
                // Three possibilities:
                // 1) Predecessor format is Dead, in which case it acquires our format.
                // 2) Predecessor format is identical to our format, in which case we
                //    do nothing.
                // 3) Predecessor format is different from our format and it's not Dead,
                //    in which case we have an erroneous set of Flushes and SetLocals.
                
                // FIXME: What if the predecessor was already processed by the fixpoint
                // and says "not Dead" and the current block says "Dead"? We may want to
                // revisit this, and say that this is is acceptable.
                
                if (predecessorFormat == DeadFlush) {
                    predecessorFormat = myFormat;
                    continue;
                }
                
                if (predecessorFormat == myFormat)
                    continue;
                
                dataLog(
                    "Bad Flush merge at edge ", *predecessor, " -> ", *block,
                    ", local variable r", m_live.operandForIndex(j), ": ", *predecessor,
                    " has ", predecessorFormat, " and ", *block, " has ", myFormat, ".\n");
                dataLog("IR at time of error:\n");
                m_graph.dump();
                CRASH();
            }
        }
    }
    
    void setForNode(Node* node, int operand, FlushFormat nodeFormat, FlushFormat newFormat)
    {
        FlushFormat& currentFormat = m_live.operand(operand);
        
        // Do some useful verification here. It's OK if we think that the
        // flush format is dead at the time that we see the node. But it's
        // not OK if both the node and m_live see a FlushFormat and they do
        // not agree on it.
        
        if (currentFormat == DeadFlush || currentFormat == nodeFormat) {
            currentFormat = newFormat;
            return;
        }
        
        dataLog(
            "Bad Flush merge at node ", node, ", r", operand, ": node claims ", nodeFormat,
            " but backwards flow claims ", currentFormat, ".\n");
        dataLog("IR at time of error:\n");
        m_graph.dump();
        CRASH();
    }
    
    FlushFormat flushFormat(Node* node)
    {
        return node->variableAccessData()->flushFormat();
    }
    
    bool m_changed;
    Operands<FlushFormat> m_live;
};

bool performFlushLivenessAnalysis(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Flush-Liveness Analysis Phase");
    return runPhase<FlushLivenessAnalysisPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

