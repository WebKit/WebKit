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
        
        Vector<BasicBlock*> depthFirst;
        m_graph.getBlocksInDepthFirstOrder(depthFirst);
        
        for (unsigned i = 0; i < depthFirst.size(); ++i) {
            BasicBlock* block = depthFirst[i];
            block->ssa->availabilityAtHead.fill(0);
            block->ssa->availabilityAtTail.fill(0);
        }
        
        for (unsigned i = 0; i < depthFirst.size(); ++i) {
            BasicBlock* block = depthFirst[i];
            
            // We edit availabilityAtTail in-place, but first initialize it to
            // availabilityAtHead.
            Operands<Node*>& availability = block->ssa->availabilityAtTail;
            availability = block->ssa->availabilityAtHead;
            
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                switch (node->op()) {
                case SetLocal:
                case MovHint:
                case MovHintAndCheck: {
                    availability.operand(node->local()) = node->child1().node();
                    break;
                }
                    
                case ZombieHint: {
                    availability.operand(node->local()) = 0;
                    break;
                }
                    
                case GetArgument: {
                    availability.operand(node->local()) = node;
                    break;
                }
                    
                default:
                    break;
                }
            }
            
            for (unsigned j = block->numSuccessors(); j--;) {
                BasicBlock* successor = block->successor(j);
                Operands<Node*>& successorAvailability = successor->ssa->availabilityAtHead;
                for (unsigned k = availability.size(); k--;) {
                    Node* myNode = availability[k];
                    if (!myNode)
                        continue;
                    
                    if (!successor->ssa->liveAtHead.contains(myNode))
                        continue;
                    
                    // Note that this may overwrite availability with a bogus node
                    // at merge points. This is fine, since merge points have
                    // MovHint(Phi)'s to work around this. The outcome of this is
                    // you might have a program in which a node happens to remain
                    // live into some block B, and separately (due to copy
                    // propagation) just one of the predecessors of B issued a
                    // MovHint putting that node into some local. Then in B we might
                    // think that that node is a valid value for that local. Of
                    // course if that local was actually live in B, B would have a
                    // Phi for it. So essentially we'll have OSR exit dropping this
                    // node's value into the local when we otherwise (in the DFG)
                    // would have dropped undefined into the local. This seems
                    // harmless.
                    
                    successorAvailability[k] = myNode;
                }
            }
        }
        
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

