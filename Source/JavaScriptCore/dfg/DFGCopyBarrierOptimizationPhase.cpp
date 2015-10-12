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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DFGCopyBarrierOptimizationPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGClobberize.h"
#include "DFGDoesGC.h"
#include "DFGGraph.h"
#include "DFGPhase.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

namespace {

bool verbose = false;

class CopyBarrierOptimizationPhase : public Phase {
public:
    CopyBarrierOptimizationPhase(Graph& graph)
        : Phase(graph, "copy barrier optimization")
    {
    }
    
    bool run()
    {
        if (verbose) {
            dataLog("Starting copy barrier optimization:\n");
            m_graph.dump();
        }

        // First convert all GetButterfly nodes into GetButterflyReadOnly.
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                if (node->op() == GetButterfly)
                    node->setOp(GetButterflyReadOnly);
            }
        }

        // Anytime we use a GetButterflyReadOnly in a setting that may write to the heap, or if we're in a
        // new epoch, convert it to a GetButterfly. The epoch gets incremented at basic block boundaries,
        // anytime we GC, and anytime a barrier on the butterfly may be executed. We traverse the program
        // in pre-order so that we always see uses after defs. Note that this is a fixpoint because if we
        // turn a GetButterflyReadOnly into a GetButterfly, then we've introduced a butterfly reallocation.
        bool changed = true;
        Epoch currentEpoch = Epoch::first();
        m_graph.clearEpochs();
        while (changed) {
            changed = false;
            for (BasicBlock* block : m_graph.blocksInPreOrder()) {
                currentEpoch.bump();
                for (Node* node : *block) {
                    bool writesToHeap = writesOverlap(m_graph, node, Heap);

                    bool reallocatesButterfly = false;
                    if (doesGC(m_graph, node) || writesOverlap(m_graph, node, JSObject_butterfly))
                        reallocatesButterfly = true;
                    else {
                        // This is not an exhaustive list of things that will execute copy barriers. Most
                        // things that execute copy barriers also do GC or have writes that overlap the
                        // butterfly heap, and we catch that above.
                        switch (node->op()) {
                        case GetButterfly:
                        case MultiPutByOffset:
                            reallocatesButterfly = true;
                            break;
                        default:
                            break;
                        }
                    }
                
                    m_graph.doToChildren(
                        node,
                        [&] (Edge edge) {
                            if (edge->op() != GetButterflyReadOnly)
                                return;
                        
                            if (writesToHeap || currentEpoch != edge->epoch()) {
                                changed = true;
                                edge->setOp(GetButterfly);
                            }
                        });

                    if (reallocatesButterfly)
                        currentEpoch.bump();

                    node->setEpoch(currentEpoch);
                }
            }
        }
        
        // This phase always thinks that it changes the graph. That's OK, because it's a late phase.
        return true;
    }
};

} // anonymous namespace

bool performCopyBarrierOptimization(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Copy Barrier Optimization Phase");
    return runPhase<CopyBarrierOptimizationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

