/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "DFGConstantFoldingPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGAbstractState.h"
#include "DFGBasicBlock.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"

namespace JSC { namespace DFG {

class ConstantFoldingPhase : public Phase {
public:
    ConstantFoldingPhase(Graph& graph)
        : Phase(graph, "constant folding")
    {
    }
    
    bool run()
    {
        bool changed = false;
        
        AbstractState state(m_graph);
        InsertionSet<NodeIndex> insertionSet;
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
            BasicBlock* block = m_graph.m_blocks[blockIndex].get();
            if (!block)
                continue;
            if (!block->cfaFoundConstants)
                continue;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("Constant folding considering Block #%u.\n", blockIndex);
#endif
            state.beginBasicBlock(block);
            for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
                if (!state.isValid())
                    break;
                NodeIndex nodeIndex = block->at(indexInBlock);
                Node& node = m_graph[nodeIndex];
                
                bool eliminated = false;
                    
                switch (node.op()) {
                case CheckArgumentsNotCreated: {
                    if (!isEmptySpeculation(
                            state.variables().operand(
                                m_graph.argumentsRegisterFor(node.codeOrigin)).m_type))
                        break;
                    ASSERT(node.refCount() == 1);
                    node.setOpAndDefaultFlags(Phantom);
                    eliminated = true;
                    break;
                }
                    
                // FIXME: This would be a great place to remove CheckStructure's.
                    
                default:
                    break;
                }
                
                if (eliminated) {
                    changed = true;
                    continue;
                }
                
                state.execute(indexInBlock);
                if (!node.shouldGenerate()
                    || state.didClobber()
                    || node.hasConstant())
                    continue;
                JSValue value = state.forNode(nodeIndex).value();
                if (!value)
                    continue;
                
                Node phantom(Phantom, node.codeOrigin);
                
                if (node.op() == GetLocal) {
                    NodeIndex previousLocalAccess = NoNode;
                    if (block->variablesAtHead.operand(node.local()) == nodeIndex
                        && m_graph[node.child1()].op() == Phi) {
                        // We expect this to be the common case.
                        ASSERT(block->isInPhis(node.child1().index()));
                        previousLocalAccess = node.child1().index();
                        block->variablesAtHead.operand(node.local()) = previousLocalAccess;
                    } else {
                        ASSERT(indexInBlock > 0);
                        // Must search for the previous access to this local.
                        for (BlockIndex subIndexInBlock = indexInBlock; subIndexInBlock--;) {
                            NodeIndex subNodeIndex = block->at(subIndexInBlock);
                            Node& subNode = m_graph[subNodeIndex];
                            if (!subNode.shouldGenerate())
                                continue;
                            if (!subNode.hasVariableAccessData())
                                continue;
                            if (subNode.local() != node.local())
                                continue;
                            // The two must have been unified.
                            ASSERT(subNode.variableAccessData() == node.variableAccessData());
                            previousLocalAccess = subNodeIndex;
                            break;
                        }
                        ASSERT(previousLocalAccess != NoNode);
                    }
                    
                    NodeIndex tailNodeIndex = block->variablesAtTail.operand(node.local());
                    if (tailNodeIndex == nodeIndex)
                        block->variablesAtTail.operand(node.local()) = previousLocalAccess;
                    else {
                        ASSERT(m_graph[tailNodeIndex].op() == Flush
                               || m_graph[tailNodeIndex].op() == SetLocal);
                    }
                }
                
                phantom.children = node.children;
                phantom.ref();
                
                m_graph.convertToConstant(nodeIndex, value);
                NodeIndex phantomNodeIndex = m_graph.size();
                m_graph.append(phantom);
                insertionSet.append(indexInBlock, phantomNodeIndex);
                
                changed = true;
            }
            insertionSet.execute(*block);
            state.reset();
        }
        
        return changed;
    }
};

bool performConstantFolding(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Constant Folding Phase");
    return runPhase<ConstantFoldingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


