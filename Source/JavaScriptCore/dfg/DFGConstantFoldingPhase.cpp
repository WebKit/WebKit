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
        , m_state(graph)
    {
    }
    
    bool run()
    {
        bool changed = false;
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
            BasicBlock* block = m_graph.m_blocks[blockIndex].get();
            if (!block)
                continue;
            if (!block->cfaDidFinish)
                changed |= paintUnreachableCode(blockIndex);
            if (block->cfaFoundConstants)
                changed |= foldConstants(blockIndex);
        }
        
        return changed;
    }

private:
    bool foldConstants(BlockIndex blockIndex)
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("Constant folding considering Block #%u.\n", blockIndex);
#endif
        BasicBlock* block = m_graph.m_blocks[blockIndex].get();
        bool changed = false;
        m_state.beginBasicBlock(block);
        for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
            NodeIndex nodeIndex = block->at(indexInBlock);
            Node& node = m_graph[nodeIndex];
                
            if (!m_state.isValid())
                break;
            
            bool eliminated = false;
                    
            switch (node.op()) {
            case CheckArgumentsNotCreated: {
                if (!isEmptySpeculation(
                        m_state.variables().operand(
                            m_graph.argumentsRegisterFor(node.codeOrigin)).m_type))
                    break;
                ASSERT(node.refCount() == 1);
                node.setOpAndDefaultFlags(Phantom);
                eliminated = true;
                break;
            }
                    
            case CheckStructure:
            case ForwardCheckStructure: {
                AbstractValue& value = m_state.forNode(node.child1());
                StructureAbstractValue& structureValue = value.m_futurePossibleStructure;
                if (structureValue.isSubsetOf(node.structureSet())
                    && structureValue.hasSingleton()
                    && isCellSpeculation(value.m_type))
                    node.convertToStructureTransitionWatchpoint(structureValue.singleton());
                break;
            }
                    
            default:
                break;
            }
                
            if (eliminated) {
                changed = true;
                continue;
            }
                
            m_state.execute(indexInBlock);
            if (!node.shouldGenerate() || m_state.didClobber() || node.hasConstant())
                continue;
            JSValue value = m_state.forNode(nodeIndex).value();
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
                    if (previousLocalAccess == NoNode) {
                        // The previous access must have been a Phi.
                        for (BlockIndex phiIndexInBlock = block->phis.size(); phiIndexInBlock--;) {
                            NodeIndex phiNodeIndex = block->phis[phiIndexInBlock];
                            Node& phiNode = m_graph[phiNodeIndex];
                            if (!phiNode.shouldGenerate())
                                continue;
                            if (phiNode.local() != node.local())
                                continue;
                            // The two must have been unified.
                            ASSERT(phiNode.variableAccessData() == node.variableAccessData());
                            previousLocalAccess = phiNodeIndex;
                            break;
                        }
                        ASSERT(previousLocalAccess != NoNode);
                    }
                }
                    
                ASSERT(previousLocalAccess != NoNode);
                
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
            m_insertionSet.append(indexInBlock, phantomNodeIndex);
                
            changed = true;
        }
        m_state.reset();
        m_insertionSet.execute(*block);
        
        return changed;
    }
    
    // This is necessary because the CFA may reach conclusions about constants based on its
    // assumption that certain code must exit, but then those constants may lead future
    // reexecutions of the CFA to believe that the same code will now no longer exit. Thus
    // to ensure soundness, we must paint unreachable code as such, by inserting an
    // unconditional ForceOSRExit wherever we find that a node would have always exited.
    // This will only happen in cases where we are making static speculations, or we're
    // making totally wrong speculations due to imprecision on the prediction propagator.
    bool paintUnreachableCode(BlockIndex blockIndex)
    {
        bool changed = false;
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("Painting unreachable code in Block #%u.\n", blockIndex);
#endif
        BasicBlock* block = m_graph.m_blocks[blockIndex].get();
        m_state.beginBasicBlock(block);
        
        for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
            m_state.execute(indexInBlock);
            if (m_state.isValid())
                continue;
            
            NodeIndex nodeIndex = block->at(indexInBlock);
            Node& node = m_graph[nodeIndex];
            switch (node.op()) {
            case Return:
            case Throw:
            case ThrowReferenceError:
            case ForceOSRExit:
                // Do nothing. These nodes will already do the right thing.
                break;
                
            default:
                Node forceOSRExit(ForceOSRExit, node.codeOrigin);
                forceOSRExit.ref();
                NodeIndex forceOSRExitIndex = m_graph.size();
                m_graph.append(forceOSRExit);
                m_insertionSet.append(indexInBlock, forceOSRExitIndex);
                changed = true;
                break;
            }
            break;
        }
        m_state.reset();
        m_insertionSet.execute(*block);
        
        return changed;
    }

    AbstractState m_state;
    InsertionSet<NodeIndex> m_insertionSet;
};

bool performConstantFolding(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Constant Folding Phase");
    return runPhase<ConstantFoldingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


