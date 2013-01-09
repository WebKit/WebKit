/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
#include "DFGValidate.h"

#if ENABLE(DFG_JIT)

#include "CodeBlockWithJITType.h"
#include <wtf/Assertions.h>
#include <wtf/BitVector.h>

namespace JSC { namespace DFG {

#if DFG_ENABLE(VALIDATION)

class Validate {
public:
    Validate(Graph& graph, GraphDumpMode graphDumpMode)
        : m_graph(graph)
        , m_graphDumpMode(graphDumpMode)
    {
    }
    
    #define VALIDATE(context, assertion) do { \
        if (!(assertion)) { \
            dataLogF("\n\n\nAt "); \
            reportValidationContext context; \
            dataLogF(": validation %s (%s:%d) failed.\n", #assertion, __FILE__, __LINE__); \
            dumpGraphIfAppropriate(); \
            WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #assertion); \
            CRASH(); \
        } \
    } while (0)
    
    #define V_EQUAL(context, left, right) do { \
        if (left != right) { \
            dataLogF("\n\n\nAt "); \
            reportValidationContext context; \
            dataLogF(": validation (%s = ", #left); \
            dumpData(left); \
            dataLogF(") == (%s = ", #right); \
            dumpData(right); \
            dataLogF(") (%s:%d) failed.\n", __FILE__, __LINE__); \
            dumpGraphIfAppropriate(); \
            WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #left " == " #right); \
            CRASH(); \
        } \
    } while (0)

    #define notSet (static_cast<size_t>(-1))

    void validate()
    {
        // NB. This code is not written for performance, since it is not intended to run
        // in release builds.
        
        // Validate that all local variables at the head of the root block are dead.
        BasicBlock* root = m_graph.m_blocks[0].get();
        for (unsigned i = 0; i < root->variablesAtHead.numberOfLocals(); ++i)
            V_EQUAL((static_cast<VirtualRegister>(i), 0), NoNode, root->variablesAtHead.local(i));
        
        // Validate ref counts and uses.
        Vector<unsigned> myRefCounts;
        myRefCounts.fill(0, m_graph.size());
        BitVector acceptableNodeIndices;
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
            BasicBlock* block = m_graph.m_blocks[blockIndex].get();
            if (!block)
                continue;
            if (!block->isReachable)
                continue;
            for (size_t i = 0; i < block->numNodes(); ++i) {
                NodeIndex nodeIndex = block->nodeIndex(i);
                acceptableNodeIndices.set(nodeIndex);
                Node& node = m_graph[nodeIndex];
                if (!node.shouldGenerate())
                    continue;
                for (unsigned j = 0; j < m_graph.numChildren(node); ++j) {
                    Edge edge = m_graph.child(node, j);
                    if (!edge)
                        continue;
                    
                    myRefCounts[edge.index()]++;
                    
                    // Unless I'm a Flush, Phantom, GetLocal, or Phi, my children should hasResult().
                    switch (node.op()) {
                    case Flush:
                    case Phantom:
                    case GetLocal:
                    case Phi:
                        break;
                    default:
                        VALIDATE((nodeIndex, edge), m_graph[edge].hasResult());
                        break;
                    }
                }
            }
        }
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
            BasicBlock* block = m_graph.m_blocks[blockIndex].get();
            if (!block)
                continue;
            if (!block->isReachable)
                continue;
            
            BitVector phisInThisBlock;
            BitVector nodesInThisBlock;
            
            for (size_t i = 0; i < block->numNodes(); ++i) {
                NodeIndex nodeIndex = block->nodeIndex(i);
                Node& node = m_graph[nodeIndex];
                nodesInThisBlock.set(nodeIndex);
                if (block->isPhiIndex(i))
                    phisInThisBlock.set(nodeIndex);
                V_EQUAL((nodeIndex), myRefCounts[nodeIndex], node.adjustedRefCount());
                for (unsigned j = 0; j < m_graph.numChildren(node); ++j) {
                    Edge edge = m_graph.child(node, j);
                    if (!edge)
                        continue;
                    VALIDATE((nodeIndex, edge), acceptableNodeIndices.get(edge.index()));
                }
            }
            
            for (size_t i = 0; i < block->phis.size(); ++i) {
                NodeIndex nodeIndex = block->phis[i];
                Node& node = m_graph[nodeIndex];
                ASSERT(phisInThisBlock.get(nodeIndex));
                VALIDATE((nodeIndex), node.op() == Phi);
                VirtualRegister local = node.local();
                for (unsigned j = 0; j < m_graph.numChildren(node); ++j) {
                    Edge edge = m_graph.child(node, j);
                    if (!edge)
                        continue;
                    
                    VALIDATE((nodeIndex, edge),
                             m_graph[edge].op() == SetLocal
                             || m_graph[edge].op() == SetArgument
                             || m_graph[edge].op() == Flush
                             || m_graph[edge].op() == Phi);
                    
                    if (phisInThisBlock.get(edge.index()))
                        continue;
                    
                    if (nodesInThisBlock.get(edge.index())) {
                        VALIDATE((nodeIndex, edge),
                                 m_graph[edge].op() == SetLocal
                                 || m_graph[edge].op() == SetArgument
                                 || m_graph[edge].op() == Flush);
                        
                        continue;
                    }
                    
                    // There must exist a predecessor block that has this node index in
                    // its tail variables.
                    bool found = false;
                    for (unsigned k = 0; k < block->m_predecessors.size(); ++k) {
                        BasicBlock* prevBlock = m_graph.m_blocks[block->m_predecessors[k]].get();
                        VALIDATE((Block, block->m_predecessors[k]), prevBlock);
                        VALIDATE((Block, block->m_predecessors[k]), prevBlock->isReachable);
                        NodeIndex prevNodeIndex = prevBlock->variablesAtTail.operand(local);
                        // If we have a Phi that is not referring to *this* block then all predecessors
                        // must have that local available.
                        VALIDATE((local, blockIndex, Block, block->m_predecessors[k]), prevNodeIndex != NoNode);
                        Node* prevNode = &m_graph[prevNodeIndex];
                        if (prevNode->op() == GetLocal) {
                            prevNodeIndex = prevNode->child1().index();
                            prevNode = &m_graph[prevNodeIndex];
                        }
                        if (node.shouldGenerate()) {
                            VALIDATE((local, block->m_predecessors[k], prevNodeIndex),
                                     prevNode->shouldGenerate());
                        }
                        VALIDATE((local, block->m_predecessors[k], prevNodeIndex),
                                 prevNode->op() == SetLocal
                                 || prevNode->op() == SetArgument
                                 || prevNode->op() == Flush
                                 || prevNode->op() == Phi);
                        if (prevNodeIndex == edge.index()) {
                            found = true;
                            break;
                        }
                        // At this point it cannot refer into this block.
                        VALIDATE((local, block->m_predecessors[k], prevNodeIndex), !prevBlock->isInBlock(edge.index()));
                    }
                    
                    VALIDATE((nodeIndex, edge), found);
                }
            }
            
            Operands<size_t> getLocalPositions(
                block->variablesAtHead.numberOfArguments(),
                block->variablesAtHead.numberOfLocals());
            Operands<size_t> setLocalPositions(
                block->variablesAtHead.numberOfArguments(),
                block->variablesAtHead.numberOfLocals());
            
            for (size_t i = 0; i < block->variablesAtHead.numberOfArguments(); ++i) {
                getLocalPositions.argument(i) = notSet;
                setLocalPositions.argument(i) = notSet;
            }
            for (size_t i = 0; i < block->variablesAtHead.numberOfLocals(); ++i) {
                getLocalPositions.local(i) = notSet;
                setLocalPositions.local(i) = notSet;
            }
            
            for (size_t i = 0; i < block->size(); ++i) {
                NodeIndex nodeIndex = block->at(i);
                Node& node = m_graph[nodeIndex];
                ASSERT(nodesInThisBlock.get(nodeIndex));
                VALIDATE((nodeIndex), node.op() != Phi);
                for (unsigned j = 0; j < m_graph.numChildren(node); ++j) {
                    Edge edge = m_graph.child(node, j);
                    if (!edge)
                        continue;
                    VALIDATE((nodeIndex, edge), nodesInThisBlock.get(nodeIndex));
                }
                
                if (!node.shouldGenerate())
                    continue;
                switch (node.op()) {
                case GetLocal:
                    if (node.variableAccessData()->isCaptured())
                        break;
                    VALIDATE((nodeIndex, blockIndex), getLocalPositions.operand(node.local()) == notSet);
                    getLocalPositions.operand(node.local()) = i;
                    break;
                case SetLocal:
                    if (node.variableAccessData()->isCaptured())
                        break;
                    // Only record the first SetLocal. There may be multiple SetLocals
                    // because of flushing.
                    if (setLocalPositions.operand(node.local()) != notSet)
                        break;
                    setLocalPositions.operand(node.local()) = i;
                    break;
                default:
                    break;
                }
            }
            
            for (size_t i = 0; i < block->variablesAtHead.numberOfArguments(); ++i) {
                checkOperand(
                    blockIndex, getLocalPositions, setLocalPositions, argumentToOperand(i));
            }
            for (size_t i = 0; i < block->variablesAtHead.numberOfLocals(); ++i) {
                checkOperand(
                    blockIndex, getLocalPositions, setLocalPositions, i);
            }
        }
    }
    
private:
    Graph& m_graph;
    GraphDumpMode m_graphDumpMode;
    
    void checkOperand(
        BlockIndex blockIndex, Operands<size_t>& getLocalPositions,
        Operands<size_t>& setLocalPositions, int operand)
    {
        if (getLocalPositions.operand(operand) == notSet)
            return;
        if (setLocalPositions.operand(operand) == notSet)
            return;
        
        BasicBlock* block = m_graph.m_blocks[blockIndex].get();
        
        VALIDATE(
            (block->at(getLocalPositions.operand(operand)),
             block->at(setLocalPositions.operand(operand)),
             blockIndex),
            getLocalPositions.operand(operand) < setLocalPositions.operand(operand));
    }
    
    void reportValidationContext(NodeIndex nodeIndex)
    {
        dataLogF("@%u", nodeIndex);
    }
    
    enum BlockTag { Block };
    void reportValidationContext(BlockTag, BlockIndex blockIndex)
    {
        dataLogF("Block #%u", blockIndex);
    }
    
    void reportValidationContext(NodeIndex nodeIndex, Edge edge)
    {
        dataLogF("@%u -> %s@%u", nodeIndex, useKindToString(edge.useKind()), edge.index());
    }
    
    void reportValidationContext(VirtualRegister local, BlockIndex blockIndex)
    {
        dataLogF("r%d in Block #%u", local, blockIndex);
    }
    
    void reportValidationContext(
        VirtualRegister local, BlockIndex sourceBlockIndex, BlockTag, BlockIndex destinationBlockIndex)
    {
        dataLogF("r%d in Block #%u -> #%u", local, sourceBlockIndex, destinationBlockIndex);
    }
    
    void reportValidationContext(
        VirtualRegister local, BlockIndex sourceBlockIndex, NodeIndex prevNodeIndex)
    {
        dataLogF("@%u for r%d in Block #%u", prevNodeIndex, local, sourceBlockIndex);
    }
    
    void reportValidationContext(
        NodeIndex nodeIndex, BlockIndex blockIndex)
    {
        dataLogF("@%u in Block #%u", nodeIndex, blockIndex);
    }
    
    void reportValidationContext(
        NodeIndex nodeIndex, NodeIndex nodeIndex2, BlockIndex blockIndex)
    {
        dataLogF("@%u and @%u in Block #%u", nodeIndex, nodeIndex2, blockIndex);
    }
    
    void reportValidationContext(
        NodeIndex nodeIndex, BlockIndex blockIndex, NodeIndex expectedNodeIndex, Edge incomingEdge)
    {
        dataLogF("@%u in Block #%u, searching for @%u from @%u", nodeIndex, blockIndex, expectedNodeIndex, incomingEdge.index());
    }
    
    void dumpData(unsigned value)
    {
        dataLogF("%u", value);
    }
    
    void dumpGraphIfAppropriate()
    {
        if (m_graphDumpMode == DontDumpGraph)
            return;
        dataLog("Graph of ", CodeBlockWithJITType(m_graph.m_codeBlock, JITCode::DFGJIT), " at time of failure:\n");
        m_graph.dump();
    }
};

void validate(Graph& graph, GraphDumpMode graphDumpMode)
{
    Validate validationObject(graph, graphDumpMode);
    validationObject.validate();
}

#endif // DFG_ENABLE(VALIDATION)

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

