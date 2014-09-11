/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "DFGPhantomRemovalPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGVariableAccessDataDump.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

class PhantomRemovalPhase : public Phase {
public:
    PhantomRemovalPhase(Graph& graph)
        : Phase(graph, "phantom removal")
    {
    }
    
    bool run()
    {
        bool changed = false;
        
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            // All Phis need to already be marked as relevant to OSR.
            if (!ASSERT_DISABLED) {
                for (unsigned i = 0; i < block->phis.size(); ++i)
                    ASSERT(block->phis[i]->flags() & NodeRelevantToOSR);
            }
            
            for (unsigned i = block->size(); i--;) {
                Node* node = block->at(i);
                
                switch (node->op()) {
                case SetLocal:
                case GetLocal: // FIXME: The GetLocal case is only necessary until we do https://bugs.webkit.org/show_bug.cgi?id=106707.
                    node->mergeFlags(NodeRelevantToOSR);
                    break;
                default:
                    node->clearFlags(NodeRelevantToOSR);
                    break;
                }
            }
        }
        
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            for (unsigned i = block->size(); i--;) {
                Node* node = block->at(i);
                if (node->op() == MovHint)
                    node->child1()->mergeFlags(NodeRelevantToOSR);
            }
        }
        
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            unsigned sourceIndex = 0;
            unsigned targetIndex = 0;
            while (sourceIndex < block->size()) {
                Node* node = block->at(sourceIndex++);
                switch (node->op()) {
                case Phantom: {
                    Node* lastNode = nullptr;
                    if (sourceIndex > 1) {
                        lastNode = block->at(sourceIndex - 2);
                        if (lastNode->op() != Phantom
                            || lastNode->origin.forExit != node->origin.forExit)
                            lastNode = nullptr;
                    }
                    for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
                        Edge edge = node->children.child(i);
                        if (!edge)
                            break;
                        if (edge.willHaveCheck())
                            continue; // Keep the type check.
                        if (edge->flags() & NodeRelevantToOSR) {
                            bool found = false;
                            if (lastNode) {
                                for (unsigned j = 0; j < AdjacencyList::Size; ++j) {
                                    if (lastNode->children.child(j).node() == edge.node()) {
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            if (!found)
                                continue;
                        }
                        
                        node->children.removeEdge(i--);
                        changed = true;
                    }
                    
                    if (node->children.isEmpty()) {
                        m_graph.m_allocator.free(node);
                        changed = true;
                        continue;
                    }
                    break;
                }
                    
                case Check: {
                    for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
                        Edge edge = node->children.child(i);
                        if (!edge)
                            break;
                        if (edge.willHaveCheck())
                            continue;
                        node->children.removeEdge(i--);
                        changed = true;
                    }
                    if (node->children.isEmpty()) {
                        m_graph.m_allocator.free(node);
                        changed = true;
                        continue;
                    }
                    break;
                }
                    
                case HardPhantom: {
                    if (node->children.isEmpty()) {
                        m_graph.m_allocator.free(node);
                        continue;
                    }
                    break;
                }
                    
                default:
                    break;
                }
                
                block->at(targetIndex++) = node;
            }
            block->resize(targetIndex);
        }
        
        return changed;
    }
};
    
bool performPhantomRemoval(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Phantom Removal Phase");
    return runPhase<PhantomRemovalPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

