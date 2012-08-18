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
#include "DFGStructureCheckHoistingPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlock.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include <wtf/HashMap.h>

namespace JSC { namespace DFG {

enum CheckBallot { VoteOther, VoteStructureCheck };

class StructureCheckHoistingPhase : public Phase {
public:
    StructureCheckHoistingPhase(Graph& graph)
        : Phase(graph, "structure check hoisting")
    {
    }
    
    bool run()
    {
        for (unsigned i = m_graph.m_variableAccessData.size(); i--;) {
            VariableAccessData* variable = &m_graph.m_variableAccessData[i];
            if (!variable->isRoot())
                continue;
            variable->clearVotes();
        }
        
        // Identify the set of variables that are always subject to the same structure
        // checks. For now, only consider monomorphic structure checks (one structure).
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
            BasicBlock* block = m_graph.m_blocks[blockIndex].get();
            if (!block)
                continue;
            for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
                NodeIndex nodeIndex = block->at(indexInBlock);
                Node& node = m_graph[nodeIndex];
                if (!node.shouldGenerate())
                    continue;
                switch (node.op()) {
                case CheckStructure: {
                    Node& child = m_graph[node.child1()];
                    if (child.op() != GetLocal)
                        break;
                    VariableAccessData* variable = child.variableAccessData();
                    variable->vote(VoteStructureCheck);
                    if (variable->isCaptured() || variable->structureCheckHoistingFailed())
                        break;
                    if (!isCellSpeculation(variable->prediction()))
                        break;
                    noticeStructureCheck(variable, node.structureSet());
                    break;
                }
                    
                case ForwardCheckStructure:
                    // We currently rely on the fact that we're the only ones who would
                    // insert this node.
                    ASSERT_NOT_REACHED();
                    break;
                    
                case GetByOffset:
                case PutByOffset:
                case PutStructure:
                case StructureTransitionWatchpoint:
                case AllocatePropertyStorage:
                case ReallocatePropertyStorage:
                case GetPropertyStorage:
                case GetByVal:
                case PutByVal:
                case PutByValAlias:
                case PutByValSafe:
                case GetArrayLength:
                case Phantom:
                    // Don't count these uses.
                    break;
                    
                default:
                    m_graph.vote(node, VoteOther);
                    break;
                }
            }
        }
        
        // Disable structure hoisting on variables that appear to mostly be used in
        // contexts where it doesn't make sense.
        
        for (unsigned i = m_graph.m_variableAccessData.size(); i--;) {
            VariableAccessData* variable = &m_graph.m_variableAccessData[i];
            if (!variable->isRoot())
                continue;
            if (variable->voteRatio() >= Options::structureCheckVoteRatioForHoisting())
                continue;
            HashMap<VariableAccessData*, CheckData>::iterator iter = m_map.find(variable);
            if (iter == m_map.end())
                continue;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("Zeroing the structure to hoist for %s because the ratio is %lf.\n",
                    m_graph.nameOfVariableAccessData(variable), variable->voteRatio());
#endif
            iter->second.m_structure = 0;
        }

        // Identify the set of variables that are live across a structure clobber.
        
        Operands<VariableAccessData*> live(
            m_graph.m_blocks[0]->variablesAtTail.numberOfArguments(),
            m_graph.m_blocks[0]->variablesAtTail.numberOfLocals());
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
            BasicBlock* block = m_graph.m_blocks[blockIndex].get();
            if (!block)
                continue;
            ASSERT(live.numberOfArguments() == block->variablesAtTail.numberOfArguments());
            ASSERT(live.numberOfLocals() == block->variablesAtTail.numberOfLocals());
            for (unsigned i = live.size(); i--;) {
                NodeIndex indexAtTail = block->variablesAtTail[i];
                VariableAccessData* variable;
                if (indexAtTail == NoNode)
                    variable = 0;
                else
                    variable = m_graph[indexAtTail].variableAccessData();
                live[i] = variable;
            }
            for (unsigned indexInBlock = block->size(); indexInBlock--;) {
                NodeIndex nodeIndex = block->at(indexInBlock);
                Node& node = m_graph[nodeIndex];
                if (!node.shouldGenerate())
                    continue;
                switch (node.op()) {
                case GetLocal:
                case Flush:
                    // This is a birth.
                    live.operand(node.local()) = node.variableAccessData();
                    break;
                    
                case SetLocal:
                case SetArgument:
                    ASSERT(live.operand(node.local())); // Must be live.
                    ASSERT(live.operand(node.local()) == node.variableAccessData()); // Must have the variable we expected.
                    // This is a death.
                    live.operand(node.local()) = 0;
                    break;
                    
                // Use the CFA's notion of what clobbers the world.
                case ValueAdd:
                    if (m_graph.addShouldSpeculateInteger(node))
                        break;
                    if (Node::shouldSpeculateNumber(m_graph[node.child1()], m_graph[node.child2()]))
                        break;
                    clobber(live);
                    break;
                    
                case CompareLess:
                case CompareLessEq:
                case CompareGreater:
                case CompareGreaterEq:
                case CompareEq: {
                    Node& left = m_graph[node.child1()];
                    Node& right = m_graph[node.child2()];
                    if (Node::shouldSpeculateInteger(left, right))
                        break;
                    if (Node::shouldSpeculateNumber(left, right))
                        break;
                    if (node.op() == CompareEq) {
                        if ((m_graph.isConstant(node.child1().index())
                             && m_graph.valueOfJSConstant(node.child1().index()).isNull())
                            || (m_graph.isConstant(node.child2().index())
                                && m_graph.valueOfJSConstant(node.child2().index()).isNull()))
                            break;
                        
                        if (Node::shouldSpeculateFinalObject(left, right))
                            break;
                        if (Node::shouldSpeculateArray(left, right))
                            break;
                        if (left.shouldSpeculateFinalObject() && right.shouldSpeculateFinalObjectOrOther())
                            break;
                        if (right.shouldSpeculateFinalObject() && left.shouldSpeculateFinalObjectOrOther())
                            break;
                        if (left.shouldSpeculateArray() && right.shouldSpeculateArrayOrOther())
                            break;
                        if (right.shouldSpeculateArray() && left.shouldSpeculateArrayOrOther())
                            break;
                    }
                    clobber(live);
                    break;
                }
                    
                case GetByVal:
                    if (!node.prediction() || !m_graph[node.child1()].prediction() || !m_graph[node.child2()].prediction())
                        break;
                    if (!isActionableArraySpeculation(m_graph[node.child1()].prediction()) || !m_graph[node.child2()].shouldSpeculateInteger())
                        clobber(live);
                    break;
                    
                case PutByVal:
                case PutByValAlias:
                case PutByValSafe: {
                    Edge child1 = m_graph.varArgChild(node, 0);
                    Edge child2 = m_graph.varArgChild(node, 1);
                    
                    if (!m_graph[child1].prediction() || !m_graph[child2].prediction())
                        break;
                    if (!m_graph[child2].shouldSpeculateInteger()
#if USE(JSVALUE32_64)
                        || m_graph[child1].shouldSpeculateArguments()
#endif
                        ) {
                        clobber(live);
                        break;
                    }
                    if (node.op() != PutByValSafe)
                        break;
                    if (m_graph[child1].shouldSpeculateArguments())
                        break;
                    if (m_graph[child1].shouldSpeculateInt8Array())
                        break;
                    if (m_graph[child1].shouldSpeculateInt16Array())
                        break;
                    if (m_graph[child1].shouldSpeculateInt32Array())
                        break;
                    if (m_graph[child1].shouldSpeculateUint8Array())
                        break;
                    if (m_graph[child1].shouldSpeculateUint8ClampedArray())
                        break;
                    if (m_graph[child1].shouldSpeculateUint16Array())
                        break;
                    if (m_graph[child1].shouldSpeculateUint32Array())
                        break;
                    if (m_graph[child1].shouldSpeculateFloat32Array())
                        break;
                    if (m_graph[child1].shouldSpeculateFloat64Array())
                        break;
                    clobber(live);
                    break;
                }
                    
                case GetMyArgumentsLengthSafe:
                case GetMyArgumentByValSafe:
                case GetById:
                case GetByIdFlush:
                case PutStructure:
                case PhantomPutStructure:
                case PutById:
                case PutByIdDirect:
                case Call:
                case Construct:
                case Resolve:
                case ResolveBase:
                case ResolveBaseStrictPut:
                case ResolveGlobal:
                    clobber(live);
                    break;
                    
                default:
                    ASSERT(node.op() != Phi);
                    break;
                }
            }
        }
        
        bool changed = false;

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        for (HashMap<VariableAccessData*, CheckData>::iterator it = m_map.begin();
             it != m_map.end(); ++it) {
            if (!it->second.m_structure) {
                dataLog("Not hoisting checks for %s because of heuristics.\n", m_graph.nameOfVariableAccessData(it->first));
                continue;
            }
            if (it->second.m_isClobbered && !it->second.m_structure->transitionWatchpointSetIsStillValid()) {
                dataLog("Not hoisting checks for %s because the structure is clobbered and has an invalid watchpoint set.\n", m_graph.nameOfVariableAccessData(it->first));
                continue;
            }
            dataLog("Hoisting checks for %s\n", m_graph.nameOfVariableAccessData(it->first));
        }
#endif // DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        
        // Make changes:
        // 1) If a variable's live range does not span a clobber, then inject structure
        //    checks before the SetLocal.
        // 2) If a variable's live range spans a clobber but is watchpointable, then
        //    inject structure checks before the SetLocal and replace all other structure
        //    checks on that variable with structure transition watchpoints.
        
        InsertionSet<NodeIndex> insertionSet;
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
            BasicBlock* block = m_graph.m_blocks[blockIndex].get();
            if (!block)
                continue;
            for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
                NodeIndex nodeIndex = block->at(indexInBlock);
                Node& node = m_graph[nodeIndex];
                // Be careful not to use 'node' after appending to the graph. In those switch
                // cases where we need to append, we first carefully extract everything we need
                // from the node, before doing any appending.
                if (!node.shouldGenerate())
                    continue;
                switch (node.op()) {
                case SetArgument: {
                    ASSERT(!blockIndex);
                    // Insert a GetLocal and a CheckStructure immediately following this
                    // SetArgument, if the variable was a candidate for structure hoisting.
                    // If the basic block previously only had the SetArgument as its
                    // variable-at-tail, then replace it with this GetLocal.
                    VariableAccessData* variable = node.variableAccessData();
                    HashMap<VariableAccessData*, CheckData>::iterator iter = m_map.find(variable);
                    if (iter == m_map.end())
                        break;
                    if (!iter->second.m_structure)
                        break;
                    if (iter->second.m_isClobbered && !iter->second.m_structure->transitionWatchpointSetIsStillValid())
                        break;
                    
                    node.ref();

                    CodeOrigin codeOrigin = node.codeOrigin;
                    
                    Node getLocal(GetLocal, codeOrigin, OpInfo(variable), nodeIndex);
                    getLocal.predict(variable->prediction());
                    getLocal.ref();
                    NodeIndex getLocalIndex = m_graph.size();
                    m_graph.append(getLocal);
                    insertionSet.append(indexInBlock + 1, getLocalIndex);
                    
                    Node checkStructure(CheckStructure, codeOrigin, OpInfo(m_graph.addStructureSet(iter->second.m_structure)), getLocalIndex);
                    checkStructure.ref();
                    NodeIndex checkStructureIndex = m_graph.size();
                    m_graph.append(checkStructure);
                    insertionSet.append(indexInBlock + 1, checkStructureIndex);
                    
                    if (block->variablesAtTail.operand(variable->local()) == nodeIndex)
                        block->variablesAtTail.operand(variable->local()) = getLocalIndex;
                    
                    m_graph.substituteGetLocal(*block, indexInBlock, variable, getLocalIndex);
                    
                    changed = true;
                    break;
                }
                    
                case SetLocal: {
                    VariableAccessData* variable = node.variableAccessData();
                    HashMap<VariableAccessData*, CheckData>::iterator iter = m_map.find(variable);
                    if (iter == m_map.end())
                        break;
                    if (!iter->second.m_structure)
                        break;
                    if (iter->second.m_isClobbered && !iter->second.m_structure->transitionWatchpointSetIsStillValid())
                        break;

                    // First insert a dead SetLocal to tell OSR that the child's value should
                    // be dropped into this bytecode variable if the CheckStructure decides
                    // to exit.
                    
                    CodeOrigin codeOrigin = node.codeOrigin;
                    NodeIndex child1 = node.child1().index();
                    
                    Node setLocal(SetLocal, codeOrigin, OpInfo(variable), child1);
                    NodeIndex setLocalIndex = m_graph.size();
                    m_graph.append(setLocal);
                    insertionSet.append(indexInBlock, setLocalIndex);
                    m_graph[child1].ref();
                    // Use a ForwardCheckStructure to indicate that we should exit to the
                    // next bytecode instruction rather than reexecuting the current one.
                    Node checkStructure(ForwardCheckStructure, codeOrigin, OpInfo(m_graph.addStructureSet(iter->second.m_structure)), child1);
                    checkStructure.ref();
                    NodeIndex checkStructureIndex = m_graph.size();
                    m_graph.append(checkStructure);
                    insertionSet.append(indexInBlock, checkStructureIndex);
                    changed = true;
                    break;
                }
                    
                case CheckStructure: {
                    Node& child = m_graph[node.child1()];
                    if (child.op() != GetLocal)
                        break;
                    HashMap<VariableAccessData*, CheckData>::iterator iter = m_map.find(child.variableAccessData());
                    if (iter == m_map.end())
                        break;
                    if (!iter->second.m_structure)
                        break;
                    if (!iter->second.m_isClobbered) {
                        node.setOpAndDefaultFlags(Phantom);
                        ASSERT(node.refCount() == 1);
                        break;
                    }
                    if (!iter->second.m_structure->transitionWatchpointSetIsStillValid())
                        break;
                    ASSERT(iter->second.m_structure == node.structureSet().singletonStructure());
                    node.convertToStructureTransitionWatchpoint();
                    changed = true;
                    break;
                }
                    
                default:
                    break;
                }
            }
            insertionSet.execute(*block);
        }
        
        return changed;
    }

private:
    void noticeStructureCheck(VariableAccessData* variable, Structure* structure)
    {
        HashMap<VariableAccessData*, CheckData>::AddResult result =
            m_map.add(variable, CheckData(structure, false));
        if (result.isNewEntry)
            return;
        if (result.iterator->second.m_structure == structure)
            return;
        result.iterator->second.m_structure = 0;
    }
    
    void noticeStructureCheck(VariableAccessData* variable, const StructureSet& set)
    {
        if (set.size() != 1) {
            noticeStructureCheck(variable, 0);
            return;
        }
        noticeStructureCheck(variable, set.singletonStructure());
    }
    
    void noticeClobber(VariableAccessData* variable)
    {
        HashMap<VariableAccessData*, CheckData>::iterator iter =
            m_map.find(variable);
        if (iter == m_map.end())
            return;
        iter->second.m_isClobbered = true;
    }
    
    void clobber(const Operands<VariableAccessData*>& live)
    {
        for (size_t i = live.size(); i--;) {
            VariableAccessData* variable = live[i];
            if (!variable)
                continue;
            noticeClobber(variable);
        }
    }
    
    struct CheckData {
        Structure* m_structure;
        bool m_isClobbered;
        
        CheckData()
            : m_structure(0)
            , m_isClobbered(false)
        {
        }
        
        CheckData(Structure* structure, bool isClobbered)
            : m_structure(structure)
            , m_isClobbered(isClobbered)
        {
        }
    };
    
    HashMap<VariableAccessData*, CheckData> m_map;
};

bool performStructureCheckHoisting(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Structure Check Hoisting Phase");
    return runPhase<StructureCheckHoistingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


