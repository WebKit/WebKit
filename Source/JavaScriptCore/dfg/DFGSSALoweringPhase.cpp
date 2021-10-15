/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "DFGSSALoweringPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "JSCJSValueInlines.h"

namespace JSC { namespace DFG {

class SSALoweringPhase : public Phase {
    static constexpr bool verbose = false;
    
public:
    SSALoweringPhase(Graph& graph)
        : Phase(graph, "SSA lowering")
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        RELEASE_ASSERT(m_graph.m_form == SSA);
        
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            m_block = m_graph.block(blockIndex);
            if (!m_block)
                continue;
            for (m_nodeIndex = 0; m_nodeIndex < m_block->size(); ++m_nodeIndex) {
                m_node = m_block->at(m_nodeIndex);
                handleNode();
            }
            m_insertionSet.execute(m_block);
        }

        return true;
    }

private:
    void handleNode()
    {
        switch (m_node->op()) {
        case AtomicsAdd:
        case AtomicsAnd:
        case AtomicsCompareExchange:
        case AtomicsExchange:
        case AtomicsLoad:
        case AtomicsOr:
        case AtomicsStore:
        case AtomicsSub:
        case AtomicsXor: {
            unsigned numExtraArgs = numExtraAtomicsArgs(m_node->op());
            lowerBoundsCheck(m_graph.child(m_node, 0), m_graph.child(m_node, 1), m_graph.child(m_node, 2 + numExtraArgs));
            break;
        }

        case HasIndexedProperty:
            lowerBoundsCheck(m_graph.child(m_node, 0), m_graph.child(m_node, 1), m_graph.child(m_node, 2));
            break;

        case EnumeratorGetByVal:
        case GetByVal: {
            lowerBoundsCheck(m_graph.varArgChild(m_node, 0), m_graph.varArgChild(m_node, 1), m_graph.varArgChild(m_node, 2));
            break;
        }
            
        case PutByVal:
        case PutByValDirect: {
            Edge base = m_graph.varArgChild(m_node, 0);
            Edge index = m_graph.varArgChild(m_node, 1);
            Edge storage = m_graph.varArgChild(m_node, 3);
            if (lowerBoundsCheck(base, index, storage))
                break;
            
            if (m_node->arrayMode().typedArrayType() != NotTypedArray && m_node->arrayMode().isOutOfBounds()) {
                Node* length = m_insertionSet.insertNode(
                    m_nodeIndex, SpecInt32Only, GetArrayLength, m_node->origin,
                    OpInfo(m_node->arrayMode().asWord()), base, storage);
                
                m_graph.varArgChild(m_node, 4) = Edge(length, KnownInt32Use);
                break;
            }
            break;
        }
            
        default:
            break;
        }
    }
    
    bool lowerBoundsCheck(Edge base, Edge index, Edge storage)
    {
        if (!m_node->arrayMode().permitsBoundsCheckLowering())
            return false;
        
        if (!m_node->arrayMode().lengthNeedsStorage())
            storage = Edge();
        
        NodeType op = GetArrayLength;
        switch (m_node->arrayMode().type()) {
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            op = GetVectorLength;
            break;
        case Array::String:
            // When we need to support this, it will require additional code since base's useKind is KnownStringUse.
            DFG_CRASH(m_graph, m_node, "Array::String's base.useKind() is KnownStringUse");
            break;
        default:
            break;
        }

        Node* length = m_insertionSet.insertNode(
            m_nodeIndex, SpecInt32Only, op, m_node->origin,
            OpInfo(m_node->arrayMode().asWord()), Edge(base.node(), KnownCellUse), storage);
        Node* checkInBounds = m_insertionSet.insertNode(
            m_nodeIndex, SpecInt32Only, CheckInBounds, m_node->origin,
            index, Edge(length, KnownInt32Use));

        AdjacencyList adjacencyList = m_graph.copyVarargChildren(m_node);
        m_graph.m_varArgChildren.append(Edge(checkInBounds, UntypedUse));
        adjacencyList.setNumChildren(adjacencyList.numChildren() + 1);
        m_node->children = adjacencyList;
        return true;
    }
    
    InsertionSet m_insertionSet;
    BasicBlock* m_block;
    unsigned m_nodeIndex;
    Node* m_node;
};

bool performSSALowering(Graph& graph)
{
    return runPhase<SSALoweringPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

