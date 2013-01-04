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
#include "GetByIdStatus.h"
#include "PutByIdStatus.h"

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
        dataLogF("Constant folding considering Block #%u.\n", blockIndex);
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
            case ForwardCheckStructure:
            case ArrayifyToStructure: {
                AbstractValue& value = m_state.forNode(node.child1());
                StructureSet set;
                if (node.op() == ArrayifyToStructure)
                    set = node.structure();
                else
                    set = node.structureSet();
                if (!isCellSpeculation(value.m_type))
                    break;
                if (value.m_currentKnownStructure.isSubsetOf(set)) {
                    ASSERT(node.refCount() == 1);
                    node.setOpAndDefaultFlags(Phantom);
                    eliminated = true;
                    break;
                }
                StructureAbstractValue& structureValue = value.m_futurePossibleStructure;
                if (structureValue.isSubsetOf(set)
                    && structureValue.hasSingleton()) {
                    node.convertToStructureTransitionWatchpoint(structureValue.singleton());
                    changed = true;
                }
                break;
            }
                
            case CheckArray:
            case Arrayify: {
                if (!node.arrayMode().alreadyChecked(m_graph, node, m_state.forNode(node.child1())))
                    break;
                ASSERT(node.refCount() == 1);
                node.setOpAndDefaultFlags(Phantom);
                eliminated = true;
                break;
            }
                
            case CheckFunction: {
                if (m_state.forNode(node.child1()).value() != node.function())
                    break;
                node.setOpAndDefaultFlags(Phantom);
                eliminated = true;
                break;
            }
                
            case ConvertThis: {
                if (!isObjectSpeculation(m_state.forNode(node.child1()).m_type))
                    break;
                node.setOpAndDefaultFlags(Identity);
                changed = true;
                break;
            }
                
            case GetById:
            case GetByIdFlush: {
                CodeOrigin codeOrigin = node.codeOrigin;
                NodeIndex child = node.child1().index();
                unsigned identifierNumber = node.identifierNumber();
                
                if (!isCellSpeculation(m_graph[child].prediction()))
                    break;
                
                Structure* structure = m_state.forNode(child).bestProvenStructure();
                if (!structure)
                    break;
                
                bool needsWatchpoint = !m_state.forNode(child).m_currentKnownStructure.hasSingleton();
                
                GetByIdStatus status = GetByIdStatus::computeFor(
                    globalData(), structure, codeBlock()->identifier(identifierNumber));
                
                if (!status.isSimple())
                    break;
                
                ASSERT(status.structureSet().size() == 1);
                ASSERT(status.chain().isEmpty());
                ASSERT(status.structureSet().singletonStructure() == structure);
                
                // Now before we do anything else, push the CFA forward over the GetById
                // and make sure we signal to the loop that it should continue and not
                // do any eliminations.
                m_state.execute(indexInBlock);
                eliminated = true;
                
                if (needsWatchpoint) {
                    ASSERT(m_state.forNode(child).m_futurePossibleStructure.isSubsetOf(StructureSet(structure)));
                    m_graph[child].ref();
                    Node watchpoint(StructureTransitionWatchpoint, codeOrigin, OpInfo(structure), child);
                    watchpoint.ref();
                    NodeIndex watchpointIndex = m_graph.size();
                    m_graph.append(watchpoint);
                    m_insertionSet.append(indexInBlock, watchpointIndex);
                }
                
                NodeIndex propertyStorageIndex;
                
                m_graph[child].ref();
                if (isInlineOffset(status.offset()))
                    propertyStorageIndex = child;
                else {
                    Node getButterfly(GetButterfly, codeOrigin, child);
                    getButterfly.ref();
                    propertyStorageIndex = m_graph.size();
                    m_graph.append(getButterfly);
                    m_insertionSet.append(indexInBlock, propertyStorageIndex);
                }
                
                m_graph[nodeIndex].convertToGetByOffset(m_graph.m_storageAccessData.size(), propertyStorageIndex);
                
                StorageAccessData storageAccessData;
                storageAccessData.offset = indexRelativeToBase(status.offset());
                storageAccessData.identifierNumber = identifierNumber;
                m_graph.m_storageAccessData.append(storageAccessData);
                break;
            }
                
            case PutById:
            case PutByIdDirect: {
                CodeOrigin codeOrigin = node.codeOrigin;
                NodeIndex child = node.child1().index();
                unsigned identifierNumber = node.identifierNumber();
                
                Structure* structure = m_state.forNode(child).bestProvenStructure();
                if (!structure)
                    break;
                
                bool needsWatchpoint = !m_state.forNode(child).m_currentKnownStructure.hasSingleton();
                
                PutByIdStatus status = PutByIdStatus::computeFor(
                    globalData(),
                    m_graph.globalObjectFor(codeOrigin),
                    structure,
                    codeBlock()->identifier(identifierNumber),
                    node.op() == PutByIdDirect);
                
                if (!status.isSimpleReplace() && !status.isSimpleTransition())
                    break;
                
                ASSERT(status.oldStructure() == structure);
                
                // Now before we do anything else, push the CFA forward over the PutById
                // and make sure we signal to the loop that it should continue and not
                // do any eliminations.
                m_state.execute(indexInBlock);
                eliminated = true;
                
                if (needsWatchpoint) {
                    ASSERT(m_state.forNode(child).m_futurePossibleStructure.isSubsetOf(StructureSet(structure)));
                    m_graph[child].ref();
                    Node watchpoint(StructureTransitionWatchpoint, codeOrigin, OpInfo(structure), child);
                    watchpoint.ref();
                    NodeIndex watchpointIndex = m_graph.size();
                    m_graph.append(watchpoint);
                    m_insertionSet.append(indexInBlock, watchpointIndex);
                }
                
                StructureTransitionData* transitionData = 0;
                if (status.isSimpleTransition()) {
                    transitionData = m_graph.addStructureTransitionData(
                        StructureTransitionData(structure, status.newStructure()));
                    
                    if (node.op() == PutById) {
                        if (!structure->storedPrototype().isNull()) {
                            addStructureTransitionCheck(
                                codeOrigin, indexInBlock,
                                structure->storedPrototype().asCell());
                        }
                        
                        for (WriteBarrier<Structure>* it = status.structureChain()->head(); *it; ++it) {
                            JSValue prototype = (*it)->storedPrototype();
                            if (prototype.isNull())
                                continue;
                            ASSERT(prototype.isCell());
                            addStructureTransitionCheck(
                                codeOrigin, indexInBlock, prototype.asCell());
                        }
                    }
                }

                NodeIndex propertyStorageIndex;
                
                m_graph[child].ref();
                if (isInlineOffset(status.offset()))
                    propertyStorageIndex = child;
                else if (status.isSimpleReplace() || structure->outOfLineCapacity() == status.newStructure()->outOfLineCapacity()) {
                    Node getButterfly(GetButterfly, codeOrigin, child);
                    getButterfly.ref();
                    propertyStorageIndex = m_graph.size();
                    m_graph.append(getButterfly);
                    m_insertionSet.append(indexInBlock, propertyStorageIndex);
                } else if (!structure->outOfLineCapacity()) {
                    ASSERT(status.newStructure()->outOfLineCapacity());
                    ASSERT(!isInlineOffset(status.offset()));
                    Node allocateStorage(AllocatePropertyStorage, codeOrigin, OpInfo(transitionData), child);
                    allocateStorage.ref(); // Once for the use.
                    allocateStorage.ref(); // Twice because it's must-generate.
                    propertyStorageIndex = m_graph.size();
                    m_graph.append(allocateStorage);
                    m_insertionSet.append(indexInBlock, propertyStorageIndex);
                } else {
                    ASSERT(structure->outOfLineCapacity());
                    ASSERT(status.newStructure()->outOfLineCapacity() > structure->outOfLineCapacity());
                    ASSERT(!isInlineOffset(status.offset()));
                    
                    Node getButterfly(GetButterfly, codeOrigin, child);
                    getButterfly.ref();
                    NodeIndex getButterflyIndex = m_graph.size();
                    m_graph.append(getButterfly);
                    m_insertionSet.append(indexInBlock, getButterflyIndex);
                    
                    m_graph[child].ref();
                    Node reallocateStorage(ReallocatePropertyStorage, codeOrigin, OpInfo(transitionData), child, getButterflyIndex);
                    reallocateStorage.ref(); // Once for the use.
                    reallocateStorage.ref(); // Twice because it's must-generate.
                    propertyStorageIndex = m_graph.size();
                    m_graph.append(reallocateStorage);
                    m_insertionSet.append(indexInBlock, propertyStorageIndex);
                }
                
                if (status.isSimpleTransition()) {
                    m_graph[child].ref();
                    Node putStructure(PutStructure, codeOrigin, OpInfo(transitionData), child);
                    putStructure.ref();
                    NodeIndex putStructureIndex = m_graph.size();
                    m_graph.append(putStructure);
                    m_insertionSet.append(indexInBlock, putStructureIndex);
                }
                
                m_graph[nodeIndex].convertToPutByOffset(m_graph.m_storageAccessData.size(), propertyStorageIndex);
                
                StorageAccessData storageAccessData;
                storageAccessData.offset = indexRelativeToBase(status.offset());
                storageAccessData.identifierNumber = identifierNumber;
                m_graph.m_storageAccessData.append(storageAccessData);
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
                        || m_graph[tailNodeIndex].op() == SetLocal
                        || node.variableAccessData()->isCaptured());
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
    
    void addStructureTransitionCheck(CodeOrigin codeOrigin, unsigned indexInBlock, JSCell* cell)
    {
        Node weakConstant(WeakJSConstant, codeOrigin, OpInfo(cell));
        weakConstant.ref();
        weakConstant.predict(speculationFromValue(cell));
        NodeIndex weakConstantIndex = m_graph.size();
        m_graph.append(weakConstant);
        m_insertionSet.append(indexInBlock, weakConstantIndex);
        
        if (cell->structure()->transitionWatchpointSetIsStillValid()) {
            Node watchpoint(StructureTransitionWatchpoint, codeOrigin, OpInfo(cell->structure()), weakConstantIndex);
            watchpoint.ref();
            NodeIndex watchpointIndex = m_graph.size();
            m_graph.append(watchpoint);
            m_insertionSet.append(indexInBlock, watchpointIndex);
            return;
        }
        
        Node check(CheckStructure, codeOrigin, OpInfo(m_graph.addStructureSet(cell->structure())), weakConstantIndex);
        check.ref();
        NodeIndex checkIndex = m_graph.size();
        m_graph.append(check);
        m_insertionSet.append(indexInBlock, checkIndex);
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
        dataLogF("Painting unreachable code in Block #%u.\n", blockIndex);
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


