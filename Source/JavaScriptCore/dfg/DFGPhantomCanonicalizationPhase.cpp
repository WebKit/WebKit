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
#include "DFGPhantomCanonicalizationPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGVariableAccessDataDump.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

static const NodeFlags NodeNeedsPhantom = NodeMiscFlag1;
static const NodeFlags NodeNeedsHardPhantom = NodeMiscFlag2;

class PhantomCanonicalizationPhase : public Phase {
public:
    PhantomCanonicalizationPhase(Graph& graph)
        : Phase(graph, "phantom canonicalization")
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        
        m_graph.clearFlagsOnAllNodes(NodeNeedsPhantom | NodeNeedsHardPhantom | NodeRelevantToOSR);
        
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
                if (node->op() == HardPhantom || node->op() == Phantom || node->op() == Check) {
                    for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
                        Edge edge = node->children.child(i);
                        if (!edge)
                            break;
                        if (node->op() == HardPhantom)
                            edge->mergeFlags(NodeNeedsHardPhantom);
                        if ((edge->flags() & NodeRelevantToOSR) && node->op() == Phantom) {
                            // A Phantom on a node that is RelevantToOSR means that we need to keep
                            // a Phantom on this node instead of just having a Check.
                            edge->mergeFlags(NodeNeedsPhantom);
                        }
                        if (edge.willHaveCheck())
                            continue; // Keep the type check.
                        
                        node->children.removeEdge(i--);
                    }
                    
                    if (node->children.isEmpty()) {
                        m_graph.m_allocator.free(node);
                        continue;
                    }
                    
                    node->convertToCheck();
                }
                block->at(targetIndex++) = node;
            }
            block->resize(targetIndex);
        }
        
        InsertionSet insertionSet(m_graph);
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                if (node->flags() & NodeNeedsHardPhantom) {
                    insertionSet.insertNode(
                        nodeIndex + 1, SpecNone, HardPhantom, node->origin, node->defaultEdge());
                } else if (node->flags() & NodeNeedsPhantom) {
                    insertionSet.insertNode(
                        nodeIndex + 1, SpecNone, Phantom, node->origin, node->defaultEdge());
                }
            }
            insertionSet.execute(block);
        }
        
        return true;
    }
};
    
bool performPhantomCanonicalization(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Phantom Canonicalization Phase");
    return runPhase<PhantomCanonicalizationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

