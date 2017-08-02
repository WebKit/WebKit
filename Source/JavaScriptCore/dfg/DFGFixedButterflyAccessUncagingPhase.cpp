/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "DFGFixedButterflyAccessUncagingPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGClobberize.h"
#include "DFGGraph.h"
#include "DFGPhase.h"
#include "JSCInlines.h"
#include <wtf/IndexSet.h>

namespace JSC { namespace DFG {

namespace {

class FixedButterflyAccessUncagingPhase : public Phase {
public:
    FixedButterflyAccessUncagingPhase(Graph& graph)
        : Phase(graph, "fixed butterfly access uncaging")
    {
    }
    
    bool run()
    {
        IndexSet<Node*> needCaging;
        
        bool changed = true;
        while (changed) {
            changed = false;
            for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
                for (Node* node : *block) {
                    switch (node->op()) {
                    // FIXME: Check again how badly we need this. It might not be worth it.
                    // https://bugs.webkit.org/show_bug.cgi?id=175044
                    case GetByOffset:
                    case PutByOffset:
                    case GetGetterSetterByOffset:
                    case GetArrayLength:
                    case GetVectorLength:
                        break;
                        
                    case Upsilon:
                        if (needCaging.contains(node->phi()))
                            changed |= needCaging.add(node->child1().node());
                        break;
                        
                    default:
                        // FIXME: We could possibly make this more precise. We really only care about whether
                        // this can read/write butterfly contents.
                        // https://bugs.webkit.org/show_bug.cgi?id=174926
                        if (!accessesOverlap(m_graph, node, Heap))
                            break;
                    
                        m_graph.doToChildren(
                            node,
                            [&] (Edge& edge) {
                                changed |= needCaging.add(edge.node());
                            });
                        break;
                    }
                }
            }
        }
        
        bool didOptimize = false;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                if (node->op() == GetButterfly && !needCaging.contains(node)) {
                    node->setOp(GetButterflyWithoutCaging);
                    didOptimize = true;
                }
            }
        }
        
        return didOptimize;
    }
};

} // anonymous namespace

bool performFixedButterflyAccessUncaging(Graph& graph)
{
    return runPhase<FixedButterflyAccessUncagingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

