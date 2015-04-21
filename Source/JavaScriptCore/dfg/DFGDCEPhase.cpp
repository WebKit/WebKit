/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
        
        m_graph.computeRefCounts();
        
        if (m_graph.m_form == SSA) {
            for (BasicBlock* block : m_graph.blocksInPreOrder())
                fixupBlock(block);
            
            // This is like cleanVariables, but has a much simpler approach to GetLocal.
            for (unsigned i = m_graph.m_arguments.size(); i--;) {
                Node* node = m_graph.m_arguments[i];
                if (!node)
                    continue;
                if (node->op() != Phantom && node->op() != Check && node->shouldGenerate())
                    continue;
                m_graph.m_arguments[i] = nullptr;
            }
        } else {
            RELEASE_ASSERT(m_graph.m_form == ThreadedCPS);
            
            for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex)
                fixupBlock(m_graph.block(blockIndex));
            cleanVariables(m_graph.m_arguments);
        }
        
        // Just do a basic HardPhantom/Phantom/Check clean-up.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            unsigned sourceIndex = 0;
            unsigned targetIndex = 0;
            while (sourceIndex < block->size()) {
                Node* node = block->at(sourceIndex++);
                switch (node->op()) {
                case Check:
                case HardPhantom:
                case Phantom:
                    if (node->children.isEmpty())
                        continue;
                    break;
                default:
                    break;
                }
                block->at(targetIndex++) = node;
            }
            block->resize(targetIndex);
        }
        
        m_graph.m_refCountState = ExactRefCount;
        
        return true;
    }

private:
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

        // This has to be a forward loop because we are using the insertion set.
        for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
            Node* node = block->at(indexInBlock);
            if (node->shouldGenerate())
                continue;
                
            switch (node->op()) {
            case MovHint:
            case ZombieHint:
                // These are not killable. (They once were.)
                RELEASE_ASSERT_NOT_REACHED();
                
            default: {
                if (node->flags() & NodeHasVarArgs) {
                    for (unsigned childIdx = node->firstChild(); childIdx < node->firstChild() + node->numChildren(); childIdx++) {
                        Edge edge = m_graph.m_varArgChildren[childIdx];

                        if (!edge || edge.isProved() || edge.willNotHaveCheck())
                            continue;

                        m_insertionSet.insertNode(indexInBlock, SpecNone, Check, node->origin, edge);
                    }

                    node->convertToPhantom();
                    node->children.reset();
                    node->setRefCount(1);
                    break;
                }

                node->convertToCheck();
                for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
                    Edge edge = node->children.child(i);
                    if (!edge)
                        continue;
                    if (edge.isProved() || edge.willNotHaveCheck())
                        node->children.removeEdge(i--);
                }
                node->setRefCount(1);
                break;
            } }
        }

        m_insertionSet.execute(block);
    }
    
    template<typename VariablesVectorType>
    void cleanVariables(VariablesVectorType& variables)
    {
        for (unsigned i = variables.size(); i--;) {
            Node* node = variables[i];
            if (!node)
                continue;
            if (node->op() != Phantom && node->op() != Check && node->shouldGenerate())
                continue;
            if (node->op() == GetLocal) {
                node = node->child1().node();
                
                ASSERT(node->op() == Phi || node->op() == SetArgument);
                
                if (node->shouldGenerate()) {
                    variables[i] = node;
                    continue;
                }
            }
            variables[i] = 0;
        }
    }
    
    InsertionSet m_insertionSet;
};

bool performDCE(Graph& graph)
{
    SamplingRegion samplingRegion("DFG DCE Phase");
    return runPhase<DCEPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

