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
#include "DFGOSRAvailabilityAnalysisPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlockInlines.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "Operations.h"

namespace JSC { namespace DFG {

class OSRAvailabilityAnalysisPhase : public Phase {
public:
    OSRAvailabilityAnalysisPhase(Graph& graph)
        : Phase(graph, "OSR availability analysis")
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            block->ssa->availabilityAtHead.fill(Availability());
            block->ssa->availabilityAtTail.fill(Availability());
        }
        
        BasicBlock* root = m_graph.block(0);
        for (unsigned argument = root->ssa->availabilityAtHead.numberOfArguments(); argument--;) {
            root->ssa->availabilityAtHead.argument(argument) =
                Availability::unavailable().withFlush(
                    FlushedAt(FlushedJSValue, virtualRegisterForArgument(argument)));
        }

        if (m_graph.m_plan.mode == FTLForOSREntryMode) {
            for (unsigned local = m_graph.m_profiledBlock->m_numCalleeRegisters; local--;)
                root->ssa->availabilityAtHead.local(local) = Availability::unavailable();
        } else {
            for (unsigned local = root->ssa->availabilityAtHead.numberOfLocals(); local--;)
                root->ssa->availabilityAtHead.local(local) = Availability::unavailable();
        }
        
        // This could be made more efficient by processing blocks in reverse postorder.
        Operands<Availability> availability;
        bool changed;
        do {
            changed = false;
            
            for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
                BasicBlock* block = m_graph.block(blockIndex);
                if (!block)
                    continue;
                
                availability = block->ssa->availabilityAtHead;
                
                for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                    Node* node = block->at(nodeIndex);
                    
                    switch (node->op()) {
                    case SetLocal: {
                        VariableAccessData* variable = node->variableAccessData();
                        availability.operand(variable->local()) =
                            Availability(node->child1().node(), variable->flushedAt());
                        break;
                    }
                        
                    case GetArgument: {
                        VariableAccessData* variable = node->variableAccessData();
                        availability.operand(variable->local()) =
                            Availability(node, variable->flushedAt());
                        break;
                    }
                        
                    case MovHint: {
                        availability.operand(node->unlinkedLocal()) =
                            Availability(node->child1().node());
                        break;
                    }
                        
                    case ZombieHint: {
                        availability.operand(node->unlinkedLocal()) =
                            Availability::unavailable();
                        break;
                    }
                        
                    default:
                        break;
                    }
                }
                
                if (availability == block->ssa->availabilityAtTail)
                    continue;
                
                block->ssa->availabilityAtTail = availability;
                changed = true;
                
                for (unsigned successorIndex = block->numSuccessors(); successorIndex--;) {
                    BasicBlock* successor = block->successor(successorIndex);
                    for (unsigned i = availability.size(); i--;) {
                        successor->ssa->availabilityAtHead[i] = availability[i].merge(
                            successor->ssa->availabilityAtHead[i]);
                    }
                }
            }
        } while (changed);
        
        return true;
    }
};

bool performOSRAvailabilityAnalysis(Graph& graph)
{
    SamplingRegion samplingRegion("DFG OSR Availability Analysis Phase");
    return runPhase<OSRAvailabilityAnalysisPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

