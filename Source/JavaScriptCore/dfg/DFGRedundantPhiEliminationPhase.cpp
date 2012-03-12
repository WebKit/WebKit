/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "DFGRedundantPhiEliminationPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"

namespace JSC { namespace DFG {

class RedundantPhiEliminationPhase : public Phase {
public:
    RedundantPhiEliminationPhase(Graph& graph)
        : Phase(graph, "redundant phi elimination")
    {
    }
    
    void run()
    {
        bool changed = false;
        do {
            changed = fixupPhis();
        } while (changed);

        updateBlockVariableInformation();

        // Update the Phi references from non-Phi nodes, e.g., the GetLocals.
        for (NodeIndex index = 0; index < m_graph.size(); ++index) {
            Node& node = m_graph[index];

            if (!node.shouldGenerate())
                continue;

            switch (node.op()) {
            case GetLocal:
                replacePhiChild(node, 0);
                break;
            default:
                break;
            }
        }

    }

private:
    NodeIndex getRedundantReplacement(NodeIndex phi)
    {
        NodeIndex child1 = m_graph[phi].child1().indexUnchecked();
        NodeIndex candidate = child1 == phi ? NoNode : child1;

        NodeIndex child2 = m_graph[phi].child2().indexUnchecked();
        if (candidate != NoNode) {
            if (child2 != NoNode && child2 != candidate && child2 != phi)
                return NoNode;
        } else if (child2 != phi)
            candidate = child2;

        NodeIndex child3 = m_graph[phi].child3().indexUnchecked();
        if (candidate != NoNode) {
            if (child3 != NoNode && child3 != candidate && child3 != phi)
                return NoNode;
        } else if (child3 != phi)
            candidate = child3;
        
        return candidate;
    }

    bool replacePhiChild(Node& node, unsigned childIndex)
    {
        ASSERT(childIndex < 3);

        bool replaced = false;
        NodeIndex child = node.children.child(childIndex).indexUnchecked();
        if (child != NoNode && m_graph[child].op() == Phi) {
            NodeIndex childReplacement = getRedundantReplacement(child);
            if (childReplacement != NoNode) {
                node.children.child(childIndex).setIndex(childReplacement);
                replaced = true;
                if (node.refCount()) {
                    m_graph[childReplacement].ref();
                    m_graph.deref(child);
                }
            }
        }
        return replaced;
    }

    bool fixupPhis()
    {
        bool changed = false;

        for (BlockIndex block = 0; block < m_graph.m_blocks.size(); ++block) {
            Vector<NodeIndex>& phis = m_graph.m_blocks[block]->phis;

            for (size_t i = 0; i < phis.size(); ++i) {
                NodeIndex phi = phis[i];
                Node& phiNode = m_graph[phi];

                changed |= (replacePhiChild(phiNode, 0) && phiNode.refCount());
                changed |= (replacePhiChild(phiNode, 1) && phiNode.refCount());
                changed |= (replacePhiChild(phiNode, 2) && phiNode.refCount());
            }
        }

        return changed;
    }

    void updateBlockVariableInformation()
    {
        // Redundant Phi nodes are eliminated, we need to update
        // the variable information if it references them.
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
            BasicBlock* basicBlock = m_graph.m_blocks[blockIndex].get();

            for (size_t arg = 0; arg < basicBlock->variablesAtHead.numberOfArguments(); ++arg) {
                NodeIndex nodeIndex = basicBlock->variablesAtHead.argument(arg);
                if (nodeIndex != NoNode && m_graph[nodeIndex].op() == Phi && !m_graph[nodeIndex].refCount()) {
                    NodeIndex replacement = getRedundantReplacement(nodeIndex);
                    if (replacement != NoNode) {
                        // This argument must be unused in this block.
                        ASSERT(basicBlock->variablesAtTail.argument(arg) == nodeIndex);
                        basicBlock->variablesAtHead.argument(arg) = replacement;
                        basicBlock->variablesAtTail.argument(arg) = replacement;
                    }
                }
            }

            for (size_t local = 0; local < basicBlock->variablesAtHead.numberOfLocals(); ++local) {
                NodeIndex nodeIndex = basicBlock->variablesAtHead.local(local);
                if (nodeIndex != NoNode && m_graph[nodeIndex].op() == Phi && !m_graph[nodeIndex].refCount()) {
                    NodeIndex replacement = getRedundantReplacement(nodeIndex);
                    if (replacement != NoNode) {
                        // This local variable must be unused in this block.
                        ASSERT(basicBlock->variablesAtTail.local(local) == nodeIndex);
                        basicBlock->variablesAtHead.local(local) = replacement;
                        basicBlock->variablesAtTail.local(local) = replacement;
                    }
                }
            }
        }
    }

};

void performRedundantPhiElimination(Graph& graph)
{
    runPhase<RedundantPhiEliminationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
