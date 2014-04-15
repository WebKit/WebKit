/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include "DFGDCEPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlockInlines.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

class DCEPhase : public Phase {
public:
    DCEPhase(Graph& graph)
        : Phase(graph, "dead code elimination")
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == ThreadedCPS || m_graph.m_form == SSA);
        
        // First reset the counts to 0 for all nodes.
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned indexInBlock = block->size(); indexInBlock--;)
                block->at(indexInBlock)->setRefCount(0);
            for (unsigned phiIndex = block->phis.size(); phiIndex--;)
                block->phis[phiIndex]->setRefCount(0);
        }
    
        // Now find the roots:
        // - Nodes that are must-generate.
        // - Nodes that are reachable from type checks.
        // Set their ref counts to 1 and put them on the worklist.
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned indexInBlock = block->size(); indexInBlock--;) {
                Node* node = block->at(indexInBlock);
                DFG_NODE_DO_TO_CHILDREN(m_graph, node, findTypeCheckRoot);
                if (!(node->flags() & NodeMustGenerate))
                    continue;
                if (!node->postfixRef())
                    m_worklist.append(node);
            }
        }
        
        while (!m_worklist.isEmpty()) {
            while (!m_worklist.isEmpty()) {
                Node* node = m_worklist.last();
                m_worklist.removeLast();
                ASSERT(node->shouldGenerate()); // It should not be on the worklist unless it's ref'ed.
                DFG_NODE_DO_TO_CHILDREN(m_graph, node, countEdge);
            }
            
            if (m_graph.m_form == SSA) {
                // Find Phi->Upsilon edges, which are represented as meta-data in the
                // Upsilon.
                for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
                    BasicBlock* block = m_graph.block(blockIndex);
                    if (!block)
                        continue;
                    for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                        Node* node = block->at(nodeIndex);
                        if (node->op() != Upsilon)
                            continue;
                        if (node->shouldGenerate())
                            continue;
                        if (node->phi()->shouldGenerate())
                            countNode(node);
                    }
                }
            }
        }
        
        if (m_graph.m_form == SSA) {
            // Need to process the graph in reverse DFS order, so that we get to the uses
            // of a node before we get to the node itself.
            Vector<BasicBlock*> depthFirst;
            m_graph.getBlocksInDepthFirstOrder(depthFirst);
            for (unsigned i = depthFirst.size(); i--;)
                fixupBlock(depthFirst[i]);
        } else {
            RELEASE_ASSERT(m_graph.m_form == ThreadedCPS);
            
            for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex)
                fixupBlock(m_graph.block(blockIndex));
            
            cleanVariables(m_graph.m_arguments);
        }
        
        m_graph.m_refCountState = ExactRefCount;
        
        return true;
    }

private:
    void findTypeCheckRoot(Node*, Edge edge)
    {
        // We may have an "unproved" untyped use for code that is unreachable. The CFA
        // will just not have gotten around to it.
        if (edge.willNotHaveCheck())
            return;
        if (!edge->postfixRef())
            m_worklist.append(edge.node());
    }
    
    void countNode(Node* node)
    {
        if (node->postfixRef())
            return;
        m_worklist.append(node);
    }
    
    void countEdge(Node*, Edge edge)
    {
        // Don't count edges that are already counted for their type checks.
        if (edge.willHaveCheck())
            return;
        countNode(edge.node());
    }
    
    void fixupBlock(BasicBlock* block)
    {
        if (!block)
            return;
        
        switch (m_graph.m_form) {
        case SSA:
            break;
            
        case ThreadedCPS: {
            // Clean up variable links for the block. We need to do this before the actual DCE
            // because we need to see GetLocals, so we can bypass them in situations where the
            // vars-at-tail point to a GetLocal, the GetLocal is dead, but the Phi it points
            // to is alive.
            
            for (unsigned phiIndex = 0; phiIndex < block->phis.size(); ++phiIndex) {
                if (!block->phis[phiIndex]->shouldGenerate()) {
                    // FIXME: We could actually free nodes here. Except that it probably
                    // doesn't matter, since we don't add any nodes after this phase.
                    // https://bugs.webkit.org/show_bug.cgi?id=126239
                    block->phis[phiIndex--] = block->phis.last();
                    block->phis.removeLast();
                }
            }
            
            cleanVariables(block->variablesAtHead);
            cleanVariables(block->variablesAtTail);
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }

        for (unsigned indexInBlock = block->size(); indexInBlock--;) {
            Node* node = block->at(indexInBlock);
            if (node->shouldGenerate())
                continue;
                
            switch (node->op()) {
            case MovHint: {
                ASSERT(node->child1().useKind() == node->child1()->defaultUseKind());
                if (!node->child1()->shouldGenerate()) {
                    node->setOpAndDefaultFlags(ZombieHint);
                    node->child1() = Edge();
                    break;
                }
                break;
            }
                
            case ZombieHint: {
                // Currently we assume that DCE runs only once.
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            
            default: {
                if (node->flags() & NodeHasVarArgs) {
                    for (unsigned childIdx = node->firstChild(); childIdx < node->firstChild() + node->numChildren(); childIdx++) {
                        Edge edge = m_graph.m_varArgChildren[childIdx];

                        if (!edge || edge.willNotHaveCheck())
                            continue;

                        m_insertionSet.insertNode(indexInBlock, SpecNone, Phantom, node->origin, edge);
                    }

                    node->convertToPhantomUnchecked();
                    node->children.reset();
                    node->setRefCount(1);
                    break;
                }

                node->convertToPhantom();
                eliminateIrrelevantPhantomChildren(node);
                node->setRefCount(1);
                break;
            } }
        }

        m_insertionSet.execute(block);
    }
    
    void eliminateIrrelevantPhantomChildren(Node* node)
    {
        for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
            Edge edge = node->children.child(i);
            if (!edge)
                continue;
            if (edge.willNotHaveCheck())
                node->children.removeEdge(i--);
        }
    }
    
    template<typename VariablesVectorType>
    void cleanVariables(VariablesVectorType& variables)
    {
        for (unsigned i = variables.size(); i--;) {
            Node* node = variables[i];
            if (!node)
                continue;
            if (node->op() != Phantom && node->shouldGenerate())
                continue;
            if (node->op() == GetLocal) {
                node = node->child1().node();
                
                // FIXME: In the case that the variable is captured, we really want to be able
                // to replace the variable-at-tail with the last use of the variable in the same
                // way that CPS rethreading would do. The child of the GetLocal isn't necessarily
                // the same as what CPS rethreading would do. For example, we may have:
                //
                // a: SetLocal(...) // live
                // b: GetLocal(@a) // live
                // c: GetLocal(@a) // dead
                //
                // When killing @c, the code below will set the variable-at-tail to @a, while CPS
                // rethreading would have set @b. This is a benign bug, since all clients of CPS
                // only use the variable-at-tail of captured variables to get the
                // VariableAccessData and observe that it is in fact captured. But, this feels
                // like it could cause bugs in the future.
                //
                // It's tempting to just dethread and then invoke CPS rethreading, but CPS
                // rethreading fails to preserve exact ref-counts. So we would need a fixpoint.
                // It's probably the case that this fixpoint will be guaranteed to converge after
                // the second iteration (i.e. the second run of DCE will not kill anything and so
                // will not need to dethread), but for now the safest approach is probably just to
                // allow for this tiny bit of sloppiness.
                //
                // Another possible solution would be to simply say that DCE dethreads but then
                // we never rethread before going to the backend. That feels intuitively right
                // because it's unlikely that any of the phases after DCE in the backend rely on
                // ThreadedCPS.
                //
                // https://bugs.webkit.org/show_bug.cgi?id=130115
                ASSERT(
                    node->op() == Phi || node->op() == SetArgument
                    || node->variableAccessData()->isCaptured());
                
                if (node->shouldGenerate()) {
                    variables[i] = node;
                    continue;
                }
            }
            variables[i] = 0;
        }
    }
    
    Vector<Node*, 128> m_worklist;
    InsertionSet m_insertionSet;
};

bool performDCE(Graph& graph)
{
    SamplingRegion samplingRegion("DFG DCE Phase");
    return runPhase<DCEPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

