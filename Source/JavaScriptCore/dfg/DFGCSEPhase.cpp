/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
#include "DFGCSEPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGPhase.h"
#include <wtf/FastBitVector.h>

namespace JSC { namespace DFG {

class CSEPhase : public Phase {
public:
    CSEPhase(Graph& graph)
        : Phase(graph, "common subexpression elimination")
    {
        // Replacements are used to implement local common subexpression elimination.
        m_replacements.resize(m_graph.size());
        
        for (unsigned i = 0; i < m_graph.size(); ++i)
            m_replacements[i] = NoNode;
        
        m_relevantToOSR.resize(m_graph.size());
    }
    
    bool run()
    {
        m_changed = false;
        
        for (unsigned block = 0; block < m_graph.m_blocks.size(); ++block)
            performBlockCSE(m_graph.m_blocks[block].get());
        
        return m_changed;
    }
    
private:
    
    NodeIndex canonicalize(NodeIndex nodeIndex)
    {
        if (nodeIndex == NoNode)
            return NoNode;
        
        if (m_graph[nodeIndex].op() == ValueToInt32)
            nodeIndex = m_graph[nodeIndex].child1().index();
        
        return nodeIndex;
    }
    NodeIndex canonicalize(Edge nodeUse)
    {
        return canonicalize(nodeUse.indexUnchecked());
    }
    
    unsigned endIndexForPureCSE()
    {
        unsigned result = m_lastSeen[m_graph[m_compileIndex].op()];
        if (result == UINT_MAX)
            result = 0;
        else
            result++;
        ASSERT(result <= m_indexInBlock);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("  limit %u: ", result);
#endif
        return result;
    }
    
    NodeIndex pureCSE(Node& node)
    {
        NodeIndex child1 = canonicalize(node.child1());
        NodeIndex child2 = canonicalize(node.child2());
        NodeIndex child3 = canonicalize(node.child3());
        
        for (unsigned i = endIndexForPureCSE(); i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1 || index == child2 || index == child3)
                break;

            Node& otherNode = m_graph[index];
            if (!otherNode.shouldGenerate())
                continue;
            
            if (node.op() != otherNode.op())
                continue;
            
            if (node.arithNodeFlags() != otherNode.arithNodeFlags())
                continue;
            
            NodeIndex otherChild = canonicalize(otherNode.child1());
            if (otherChild == NoNode)
                return index;
            if (otherChild != child1)
                continue;
            
            otherChild = canonicalize(otherNode.child2());
            if (otherChild == NoNode)
                return index;
            if (otherChild != child2)
                continue;
            
            otherChild = canonicalize(otherNode.child3());
            if (otherChild == NoNode)
                return index;
            if (otherChild != child3)
                continue;
            
            return index;
        }
        return NoNode;
    }
    
    NodeIndex constantCSE(Node& node)
    {
        for (unsigned i = endIndexForPureCSE(); i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& otherNode = m_graph[index];
            if (otherNode.op() != JSConstant)
                continue;
            
            if (otherNode.constantNumber() != node.constantNumber())
                continue;
            
            return index;
        }
        return NoNode;
    }
    
    NodeIndex weakConstantCSE(Node& node)
    {
        for (unsigned i = endIndexForPureCSE(); i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& otherNode = m_graph[index];
            if (otherNode.op() != WeakJSConstant)
                continue;
            
            if (otherNode.weakConstant() != node.weakConstant())
                continue;
            
            return index;
        }
        return NoNode;
    }
    
    NodeIndex getArrayLengthElimination(NodeIndex array)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GetArrayLength:
                if (node.child1() == array)
                    return index;
                break;
                
            case PutByVal:
                if (!m_graph.byValIsPure(node))
                    return NoNode;
                if (node.arrayMode().mayStoreToHole())
                    return NoNode;
                break;
                
            default:
                if (m_graph.clobbersWorld(index))
                    return NoNode;
            }
        }
        return NoNode;
    }
    
    NodeIndex globalVarLoadElimination(WriteBarrier<Unknown>* registerPointer)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GetGlobalVar:
                if (node.registerPointer() == registerPointer)
                    return index;
                break;
            case PutGlobalVar:
                if (node.registerPointer() == registerPointer)
                    return node.child1().index();
                break;
            default:
                break;
            }
            if (m_graph.clobbersWorld(index))
                break;
        }
        return NoNode;
    }
    
    NodeIndex scopedVarLoadElimination(NodeIndex registers, unsigned varNumber)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GetScopedVar: {
                if (node.child1() == registers && node.varNumber() == varNumber)
                    return index;
                break;
            } 
            case PutScopedVar: {
                if (node.child2() == registers && node.varNumber() == varNumber)
                    return node.child3().index();
                break;
            }
            case SetLocal: {
                VariableAccessData* variableAccessData = node.variableAccessData();
                if (variableAccessData->isCaptured()
                    && variableAccessData->local() == static_cast<VirtualRegister>(varNumber))
                    return NoNode;
                break;
            }
            default:
                break;
            }
            if (m_graph.clobbersWorld(index))
                break;
        }
        return NoNode;
    }
    
    bool globalVarWatchpointElimination(WriteBarrier<Unknown>* registerPointer)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GlobalVarWatchpoint:
                if (node.registerPointer() == registerPointer)
                    return true;
                break;
            case PutGlobalVar:
                if (node.registerPointer() == registerPointer)
                    return false;
                break;
            default:
                break;
            }
            if (m_graph.clobbersWorld(index))
                break;
        }
        return false;
    }
    
    NodeIndex globalVarStoreElimination(WriteBarrier<Unknown>* registerPointer)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case PutGlobalVar:
            case PutGlobalVarCheck:
                if (node.registerPointer() == registerPointer)
                    return index;
                break;
                
            case GetGlobalVar:
                if (node.registerPointer() == registerPointer)
                    return NoNode;
                break;
                
            default:
                break;
            }
            if (m_graph.clobbersWorld(index) || node.canExit())
                return NoNode;
        }
        return NoNode;
    }
    
    NodeIndex scopedVarStoreElimination(NodeIndex scope, NodeIndex registers, unsigned varNumber)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case PutScopedVar: {
                if (node.child1() == scope && node.child2() == registers && node.varNumber() == varNumber)
                    return index;
                break;
            }
                
            case GetScopedVar: {
                // Let's be conservative.
                if (node.varNumber() == varNumber)
                    return NoNode;
                break;
            }
                
            case GetLocal: {
                VariableAccessData* variableAccessData = node.variableAccessData();
                if (variableAccessData->isCaptured()
                    && variableAccessData->local() == static_cast<VirtualRegister>(varNumber))
                    return NoNode;
                break;
            }

            default:
                break;
            }
            if (m_graph.clobbersWorld(index) || node.canExit())
                return NoNode;
        }
        return NoNode;
    }
    
    NodeIndex getByValLoadElimination(NodeIndex child1, NodeIndex child2)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1 || index == canonicalize(child2)) 
                break;

            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GetByVal:
                if (!m_graph.byValIsPure(node))
                    return NoNode;
                if (node.child1() == child1 && canonicalize(node.child2()) == canonicalize(child2))
                    return index;
                break;
            case PutByVal:
            case PutByValAlias: {
                if (!m_graph.byValIsPure(node))
                    return NoNode;
                if (m_graph.varArgChild(node, 0) == child1 && canonicalize(m_graph.varArgChild(node, 1)) == canonicalize(child2))
                    return m_graph.varArgChild(node, 2).index();
                // We must assume that the PutByVal will clobber the location we're getting from.
                // FIXME: We can do better; if we know that the PutByVal is accessing an array of a
                // different type than the GetByVal, then we know that they won't clobber each other.
                return NoNode;
            }
            case PutStructure:
            case PutByOffset:
                // GetByVal currently always speculates that it's accessing an
                // array with an integer index, which means that it's impossible
                // for a structure change or a put to property storage to affect
                // the GetByVal.
                break;
            default:
                if (m_graph.clobbersWorld(index))
                    return NoNode;
                break;
            }
        }
        return NoNode;
    }

    bool checkFunctionElimination(JSCell* function, NodeIndex child1)
    {
        for (unsigned i = endIndexForPureCSE(); i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1) 
                break;

            Node& node = m_graph[index];
            if (node.op() == CheckFunction && node.child1() == child1 && node.function() == function)
                return true;
        }
        return false;
    }

    bool checkStructureElimination(const StructureSet& structureSet, NodeIndex child1)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1) 
                break;

            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case CheckStructure:
            case ForwardCheckStructure:
                if (node.child1() == child1
                    && structureSet.isSupersetOf(node.structureSet()))
                    return true;
                break;
                
            case StructureTransitionWatchpoint:
            case ForwardStructureTransitionWatchpoint:
                if (node.child1() == child1
                    && structureSet.contains(node.structure()))
                    return true;
                break;
                
            case PutStructure:
                if (node.child1() == child1
                    && structureSet.contains(node.structureTransitionData().newStructure))
                    return true;
                if (structureSet.contains(node.structureTransitionData().previousStructure))
                    return false;
                break;
                
            case PutByOffset:
                // Setting a property cannot change the structure.
                break;
                
            case PutByVal:
            case PutByValAlias:
                if (m_graph.byValIsPure(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return false;
                
            case Arrayify:
            case ArrayifyToStructure:
                // We could check if the arrayification could affect our structures.
                // But that seems like it would take Effort.
                return false;
                
            default:
                if (m_graph.clobbersWorld(index))
                    return false;
                break;
            }
        }
        return false;
    }
    
    bool structureTransitionWatchpointElimination(Structure* structure, NodeIndex child1)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1) 
                break;

            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case CheckStructure:
            case ForwardCheckStructure:
                if (node.child1() == child1
                    && node.structureSet().containsOnly(structure))
                    return true;
                break;
                
            case PutStructure:
                ASSERT(node.structureTransitionData().previousStructure != structure);
                break;
                
            case PutByOffset:
                // Setting a property cannot change the structure.
                break;
                
            case PutByVal:
            case PutByValAlias:
                if (m_graph.byValIsPure(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return false;
                
            case StructureTransitionWatchpoint:
            case ForwardStructureTransitionWatchpoint:
                if (node.structure() == structure && node.child1() == child1)
                    return true;
                break;
                
            case Arrayify:
            case ArrayifyToStructure:
                // We could check if the arrayification could affect our structures.
                // But that seems like it would take Effort.
                return false;
                
            default:
                if (m_graph.clobbersWorld(index))
                    return false;
                break;
            }
        }
        return false;
    }
    
    NodeIndex putStructureStoreElimination(NodeIndex child1)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1)
                break;
            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case CheckStructure:
            case ForwardCheckStructure:
                return NoNode;
                
            case PhantomPutStructure:
                if (node.child1() == child1) // No need to retrace our steps.
                    return NoNode;
                break;
                
            case PutStructure:
                if (node.child1() == child1)
                    return index;
                break;
                
            // PutStructure needs to execute if we GC. Hence this needs to
            // be careful with respect to nodes that GC.
            case CreateArguments:
            case TearOffArguments:
            case NewFunctionNoCheck:
            case NewFunction:
            case NewFunctionExpression:
            case CreateActivation:
            case TearOffActivation:
            case StrCat:
            case ToPrimitive:
            case NewRegexp:
            case NewArrayBuffer:
            case NewArray:
            case NewObject:
            case CreateThis:
            case AllocatePropertyStorage:
            case ReallocatePropertyStorage:
                return NoNode;
                
            case GetIndexedPropertyStorage:
                if (node.arrayMode().getIndexedPropertyStorageMayTriggerGC())
                    return NoNode;
                break;
                
            default:
                break;
            }
            if (m_graph.clobbersWorld(index) || node.canExit())
                return NoNode;
        }
        return NoNode;
    }
    
    NodeIndex getByOffsetLoadElimination(unsigned identifierNumber, NodeIndex child1)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1)
                break;

            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GetByOffset:
                if (node.child1() == child1
                    && m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber == identifierNumber)
                    return index;
                break;
                
            case PutByOffset:
                if (m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber == identifierNumber) {
                    if (node.child1() == child1) // Must be same property storage.
                        return node.child3().index();
                    return NoNode;
                }
                break;
                
            case PutStructure:
                // Changing the structure cannot change the outcome of a property get.
                break;
                
            case PutByVal:
            case PutByValAlias:
                if (m_graph.byValIsPure(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return NoNode;
                
            default:
                if (m_graph.clobbersWorld(index))
                    return NoNode;
                break;
            }
        }
        return NoNode;
    }
    
    NodeIndex putByOffsetStoreElimination(unsigned identifierNumber, NodeIndex child1)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1)
                break;

            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GetByOffset:
                if (m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber == identifierNumber)
                    return NoNode;
                break;
                
            case PutByOffset:
                if (m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber == identifierNumber) {
                    if (node.child1() == child1) // Must be same property storage.
                        return index;
                    return NoNode;
                }
                break;
                
            case PutByVal:
            case PutByValAlias:
            case GetByVal:
                if (m_graph.byValIsPure(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return NoNode;
                
            default:
                if (m_graph.clobbersWorld(index))
                    return NoNode;
                break;
            }
            if (node.canExit())
                return NoNode;
        }
        return NoNode;
    }
    
    NodeIndex getPropertyStorageLoadElimination(NodeIndex child1)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1) 
                break;

            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GetButterfly:
                if (node.child1() == child1)
                    return index;
                break;

            case AllocatePropertyStorage:
            case ReallocatePropertyStorage:
                // If we can cheaply prove this is a change to our object's storage, we
                // can optimize and use its result.
                if (node.child1() == child1)
                    return index;
                // Otherwise, we currently can't prove that this doesn't change our object's
                // storage, so we conservatively assume that it may change the storage
                // pointer of any object, including ours.
                return NoNode;
                
            case PutByOffset:
            case PutStructure:
                // Changing the structure or putting to the storage cannot
                // change the property storage pointer.
                break;
                
            case PutByVal:
            case PutByValAlias:
                if (m_graph.byValIsPure(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return NoNode;
                
            case Arrayify:
            case ArrayifyToStructure:
                // We could check if the arrayification could affect our butterfly.
                // But that seems like it would take Effort.
                return NoNode;
                
            default:
                if (m_graph.clobbersWorld(index))
                    return NoNode;
                break;
            }
        }
        return NoNode;
    }
    
    bool checkArrayElimination(NodeIndex child1, ArrayMode arrayMode)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1) 
                break;

            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case PutByOffset:
            case PutStructure:
                // Changing the structure or putting to the storage cannot
                // change the property storage pointer.
                break;
                
            case CheckArray:
                if (node.child1() == child1 && node.arrayMode() == arrayMode)
                    return true;
                break;
                
            case Arrayify:
            case ArrayifyToStructure:
                // We could check if the arrayification could affect our array.
                // But that seems like it would take Effort.
                return false;
                
            default:
                if (m_graph.clobbersWorld(index))
                    return false;
                break;
            }
        }
        return false;
    }

    NodeIndex getIndexedPropertyStorageLoadElimination(NodeIndex child1, ArrayMode arrayMode)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            if (index == child1) 
                break;

            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GetIndexedPropertyStorage: {
                if (node.child1() == child1 && node.arrayMode() == arrayMode)
                    return index;
                break;
            }

            case PutByOffset:
            case PutStructure:
                // Changing the structure or putting to the storage cannot
                // change the property storage pointer.
                break;
                
            default:
                if (m_graph.clobbersWorld(index))
                    return NoNode;
                break;
            }
        }
        return NoNode;
    }
    
    NodeIndex getMyScopeLoadElimination()
    {
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case CreateActivation:
                // This may cause us to return a different scope.
                return NoNode;
            case GetMyScope:
                return index;
            default:
                break;
            }
        }
        return NoNode;
    }
    
    NodeIndex getLocalLoadElimination(VirtualRegister local, NodeIndex& relevantLocalOp, bool careAboutClobbering)
    {
        relevantLocalOp = NoNode;
        
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GetLocal:
                if (node.local() == local) {
                    relevantLocalOp = index;
                    return index;
                }
                break;
                
            case GetLocalUnlinked:
                if (node.unlinkedLocal() == local) {
                    relevantLocalOp = index;
                    return index;
                }
                break;
                
            case SetLocal:
                if (node.local() == local) {
                    relevantLocalOp = index;
                    return node.child1().index();
                }
                break;
                
            case PutScopedVar:
                if (static_cast<VirtualRegister>(node.varNumber()) == local)
                    return NoNode;
                break;
                
            default:
                if (careAboutClobbering && m_graph.clobbersWorld(index))
                    return NoNode;
                break;
            }
        }
        return NoNode;
    }
    
    struct SetLocalStoreEliminationResult {
        SetLocalStoreEliminationResult()
            : mayBeAccessed(false)
            , mayExit(false)
            , mayClobberWorld(false)
        {
        }
        
        bool mayBeAccessed;
        bool mayExit;
        bool mayClobberWorld;
    };
    SetLocalStoreEliminationResult setLocalStoreElimination(
        VirtualRegister local, NodeIndex expectedNodeIndex)
    {
        SetLocalStoreEliminationResult result;
        for (unsigned i = m_indexInBlock; i--;) {
            NodeIndex index = m_currentBlock->at(i);
            Node& node = m_graph[index];
            if (!node.shouldGenerate())
                continue;
            switch (node.op()) {
            case GetLocal:
            case Flush:
                if (node.local() == local)
                    result.mayBeAccessed = true;
                break;
                
            case GetLocalUnlinked:
                if (node.unlinkedLocal() == local)
                    result.mayBeAccessed = true;
                break;
                
            case SetLocal: {
                if (node.local() != local)
                    break;
                if (index != expectedNodeIndex)
                    result.mayBeAccessed = true;
                if (m_graph[index].refCount() > 1)
                    result.mayBeAccessed = true;
                return result;
            }
                
            case GetScopedVar:
                if (static_cast<VirtualRegister>(node.varNumber()) == local)
                    result.mayBeAccessed = true;
                break;
                
            case GetMyScope:
            case SkipTopScope:
                if (m_graph.uncheckedActivationRegisterFor(node.codeOrigin) == local)
                    result.mayBeAccessed = true;
                break;
                
            case CheckArgumentsNotCreated:
            case GetMyArgumentsLength:
            case GetMyArgumentsLengthSafe:
                if (m_graph.uncheckedArgumentsRegisterFor(node.codeOrigin) == local)
                    result.mayBeAccessed = true;
                break;
                
            case GetMyArgumentByVal:
            case GetMyArgumentByValSafe:
                result.mayBeAccessed = true;
                break;
                
            case GetByVal:
                // If this is accessing arguments then it's potentially accessing locals.
                if (m_graph[node.child1()].shouldSpeculateArguments())
                    result.mayBeAccessed = true;
                break;
                
            case CreateArguments:
            case TearOffActivation:
            case TearOffArguments:
                // If an activation is being torn off then it means that captured variables
                // are live. We could be clever here and check if the local qualifies as an
                // argument register. But that seems like it would buy us very little since
                // any kind of tear offs are rare to begin with.
                result.mayBeAccessed = true;
                break;
                
            default:
                break;
            }
            result.mayExit |= node.canExit();
            result.mayClobberWorld |= m_graph.clobbersWorld(index);
        }
        ASSERT_NOT_REACHED();
        // Be safe in release mode.
        result.mayBeAccessed = true;
        return result;
    }
    
    void performSubstitution(Edge& child, bool addRef = true)
    {
        // Check if this operand is actually unused.
        if (!child)
            return;
        
        // Check if there is any replacement.
        NodeIndex replacement = m_replacements[child.index()];
        if (replacement == NoNode)
            return;
        
        child.setIndex(replacement);
        
        // There is definitely a replacement. Assert that the replacement does not
        // have a replacement.
        ASSERT(m_replacements[child.index()] == NoNode);
        
        if (addRef)
            m_graph.ref(child);
    }
    
    void eliminateIrrelevantPhantomChildren(Node& node)
    {
        ASSERT(node.op() == Phantom);
        for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
            Edge edge = node.children.child(i);
            if (!edge)
                continue;
            if (m_relevantToOSR.get(edge.index()))
                continue;
            
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("   Eliminating edge @", m_compileIndex, " -> @", edge.index());
#endif
            m_graph.deref(edge);
            node.children.removeEdgeFromBag(i--);
            m_changed = true;
        }
    }
    
    enum PredictionHandlingMode { RequireSamePrediction, AllowPredictionMismatch };
    bool setReplacement(NodeIndex replacement, PredictionHandlingMode predictionHandlingMode = RequireSamePrediction)
    {
        if (replacement == NoNode)
            return false;
        
        // Be safe. Don't try to perform replacements if the predictions don't
        // agree.
        if (predictionHandlingMode == RequireSamePrediction
            && m_graph[m_compileIndex].prediction() != m_graph[replacement].prediction())
            return false;
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("   Replacing @%u -> @%u", m_compileIndex, replacement);
#endif
        
        Node& node = m_graph[m_compileIndex];
        node.setOpAndDefaultFlags(Phantom);
        node.setRefCount(1);
        eliminateIrrelevantPhantomChildren(node);
        
        // At this point we will eliminate all references to this node.
        m_replacements[m_compileIndex] = replacement;
        
        m_changed = true;
        
        return true;
    }
    
    void eliminate()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("   Eliminating @%u", m_compileIndex);
#endif
        
        Node& node = m_graph[m_compileIndex];
        ASSERT(node.refCount() == 1);
        ASSERT(node.mustGenerate());
        node.setOpAndDefaultFlags(Phantom);
        eliminateIrrelevantPhantomChildren(node);
        
        m_changed = true;
    }
    
    void eliminate(NodeIndex nodeIndex, NodeType phantomType = Phantom)
    {
        if (nodeIndex == NoNode)
            return;
        Node& node = m_graph[nodeIndex];
        if (node.refCount() != 1)
            return;
        ASSERT(node.mustGenerate());
        node.setOpAndDefaultFlags(phantomType);
        if (phantomType == Phantom)
            eliminateIrrelevantPhantomChildren(node);
        
        m_changed = true;
    }
    
    void performNodeCSE(Node& node)
    {
        bool shouldGenerate = node.shouldGenerate();

        if (node.flags() & NodeHasVarArgs) {
            for (unsigned childIdx = node.firstChild(); childIdx < node.firstChild() + node.numChildren(); childIdx++)
                performSubstitution(m_graph.m_varArgChildren[childIdx], shouldGenerate);
        } else {
            performSubstitution(node.children.child1(), shouldGenerate);
            performSubstitution(node.children.child2(), shouldGenerate);
            performSubstitution(node.children.child3(), shouldGenerate);
        }
        
        if (node.op() == SetLocal)
            m_relevantToOSR.set(node.child1().index());
        
        if (!shouldGenerate)
            return;
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("   %s @%u: ", Graph::opName(m_graph[m_compileIndex].op()), m_compileIndex);
#endif
        
        // NOTE: there are some nodes that we deliberately don't CSE even though we
        // probably could, like StrCat and ToPrimitive. That's because there is no
        // evidence that doing CSE on these nodes would result in a performance
        // progression. Hence considering these nodes in CSE would just mean that this
        // code does more work with no win. Of course, we may want to reconsider this,
        // since StrCat is trivially CSE-able. It's not trivially doable for
        // ToPrimitive, but we could change that with some speculations if we really
        // needed to.
        
        switch (node.op()) {
        
        case Identity:
            setReplacement(node.child1().index());
            break;
            
        // Handle the pure nodes. These nodes never have any side-effects.
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift:
        case ArithAdd:
        case ArithSub:
        case ArithNegate:
        case ArithMul:
        case ArithMod:
        case ArithDiv:
        case ArithAbs:
        case ArithMin:
        case ArithMax:
        case ArithSqrt:
        case GetCallee:
        case StringCharAt:
        case StringCharCodeAt:
        case Int32ToDouble:
        case IsUndefined:
        case IsBoolean:
        case IsNumber:
        case IsString:
        case IsObject:
        case IsFunction:
        case DoubleAsInt32:
        case LogicalNot:
        case SkipTopScope:
        case SkipScope:
        case GetScopeRegisters:
            setReplacement(pureCSE(node));
            break;
            
        case GetLocal: {
            VariableAccessData* variableAccessData = node.variableAccessData();
            if (!variableAccessData->isCaptured())
                break;
            NodeIndex relevantLocalOp;
            NodeIndex possibleReplacement = getLocalLoadElimination(variableAccessData->local(), relevantLocalOp, variableAccessData->isCaptured());
            if (relevantLocalOp == NoNode)
                break;
            if (m_graph[relevantLocalOp].op() != GetLocalUnlinked
                && m_graph[relevantLocalOp].variableAccessData() != variableAccessData)
                break;
            NodeIndex phiIndex = node.child1().index();
            if (!setReplacement(possibleReplacement))
                break;
            // If the GetLocal we replaced used to refer to a SetLocal, then it now
            // should refer to the child of the SetLocal instead.
            if (m_graph[phiIndex].op() == SetLocal) {
                ASSERT(node.child1().index() == phiIndex);
                m_graph.changeEdge(node.children.child1(), m_graph[phiIndex].child1());
            }
            NodeIndex oldTailIndex = m_currentBlock->variablesAtTail.operand(
                variableAccessData->local());
            if (oldTailIndex == m_compileIndex) {
                m_currentBlock->variablesAtTail.operand(variableAccessData->local()) =
                    relevantLocalOp;
                
                // Maintain graph integrity: since we're replacing a GetLocal with a GetLocalUnlinked,
                // make sure that the GetLocalUnlinked is now linked.
                if (m_graph[relevantLocalOp].op() == GetLocalUnlinked) {
                    m_graph[relevantLocalOp].setOp(GetLocal);
                    m_graph[relevantLocalOp].children.child1() = Edge(phiIndex);
                    m_graph.ref(phiIndex);
                }
            }
            m_changed = true;
            break;
        }
            
        case GetLocalUnlinked: {
            NodeIndex relevantLocalOpIgnored;
            setReplacement(getLocalLoadElimination(node.unlinkedLocal(), relevantLocalOpIgnored, true));
            break;
        }
            
        case Flush: {
            VariableAccessData* variableAccessData = node.variableAccessData();
            VirtualRegister local = variableAccessData->local();
            NodeIndex replacementIndex = node.child1().index();
            Node& replacement = m_graph[replacementIndex];
            if (replacement.op() != SetLocal)
                break;
            ASSERT(replacement.variableAccessData() == variableAccessData);
            // FIXME: We should be able to remove SetLocals that can exit; we just need
            // to replace them with appropriate type checks.
            if (m_graph.m_fixpointState == FixpointNotConverged) {
                // Need to be conservative at this time; if the SetLocal has any chance of performing
                // any speculations then we cannot do anything.
                if (variableAccessData->isCaptured()) {
                    // Captured SetLocals never speculate and hence never exit.
                } else {
                    if (variableAccessData->shouldUseDoubleFormat())
                        break;
                    SpeculatedType prediction = variableAccessData->argumentAwarePrediction();
                    if (isInt32Speculation(prediction))
                        break;
                    if (isArraySpeculation(prediction))
                        break;
                    if (isBooleanSpeculation(prediction))
                        break;
                }
            } else {
                if (replacement.canExit())
                    break;
            }
            SetLocalStoreEliminationResult result =
                setLocalStoreElimination(local, replacementIndex);
            if (result.mayBeAccessed || result.mayClobberWorld)
                break;
            ASSERT(replacement.op() == SetLocal);
            ASSERT(replacement.refCount() == 1);
            ASSERT(replacement.shouldGenerate());
            // FIXME: Investigate using mayExit as a further optimization.
            node.setOpAndDefaultFlags(Phantom);
            NodeIndex dataNodeIndex = replacement.child1().index();
            ASSERT(m_graph[dataNodeIndex].hasResult());
            m_graph.clearAndDerefChild1(node);
            node.children.child1() = Edge(dataNodeIndex);
            m_graph.ref(dataNodeIndex);
            NodeIndex oldTailIndex = m_currentBlock->variablesAtTail.operand(local);
            if (oldTailIndex == m_compileIndex)
                m_currentBlock->variablesAtTail.operand(local) = replacementIndex;
            m_changed = true;
            break;
        }
            
        case JSConstant:
            // This is strange, but necessary. Some phases will convert nodes to constants,
            // which may result in duplicated constants. We use CSE to clean this up.
            setReplacement(constantCSE(node), AllowPredictionMismatch);
            break;
            
        case WeakJSConstant:
            // FIXME: have CSE for weak constants against strong constants and vice-versa.
            setReplacement(weakConstantCSE(node));
            break;
            
        case GetArrayLength:
            setReplacement(getArrayLengthElimination(node.child1().index()));
            break;

        case GetMyScope:
            setReplacement(getMyScopeLoadElimination());
            break;
            
        // Handle nodes that are conditionally pure: these are pure, and can
        // be CSE'd, so long as the prediction is the one we want.
        case ValueAdd:
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq:
        case CompareEq: {
            if (m_graph.isPredictedNumerical(node)) {
                NodeIndex replacementIndex = pureCSE(node);
                if (replacementIndex != NoNode && m_graph.isPredictedNumerical(m_graph[replacementIndex]))
                    setReplacement(replacementIndex);
            }
            break;
        }
            
        // Finally handle heap accesses. These are not quite pure, but we can still
        // optimize them provided that some subtle conditions are met.
        case GetGlobalVar:
            setReplacement(globalVarLoadElimination(node.registerPointer()));
            break;

        case GetScopedVar: {
            setReplacement(scopedVarLoadElimination(node.child1().index(), node.varNumber()));
            break;
        }

        case GlobalVarWatchpoint:
            if (globalVarWatchpointElimination(node.registerPointer()))
                eliminate();
            break;
            
        case PutGlobalVar:
        case PutGlobalVarCheck:
            if (m_graph.m_fixpointState == FixpointNotConverged)
                break;
            eliminate(globalVarStoreElimination(node.registerPointer()));
            break;
            
        case PutScopedVar: {
            if (m_graph.m_fixpointState == FixpointNotConverged)
                break;
            eliminate(scopedVarStoreElimination(node.child1().index(), node.child2().index(), node.varNumber()));
            break;
        }

        case GetByVal:
            if (m_graph.byValIsPure(node))
                setReplacement(getByValLoadElimination(node.child1().index(), node.child2().index()));
            break;
            
        case PutByVal: {
            Edge child1 = m_graph.varArgChild(node, 0);
            Edge child2 = m_graph.varArgChild(node, 1);
            if (node.arrayMode().canCSEStorage()) {
                NodeIndex nodeIndex = getByValLoadElimination(child1.index(), child2.index());
                if (nodeIndex == NoNode)
                    break;
                node.setOp(PutByValAlias);
            }
            break;
        }
            
        case CheckStructure:
        case ForwardCheckStructure:
            if (checkStructureElimination(node.structureSet(), node.child1().index()))
                eliminate();
            break;
            
        case StructureTransitionWatchpoint:
        case ForwardStructureTransitionWatchpoint:
            if (structureTransitionWatchpointElimination(node.structure(), node.child1().index()))
                eliminate();
            break;
            
        case PutStructure:
            if (m_graph.m_fixpointState == FixpointNotConverged)
                break;
            eliminate(putStructureStoreElimination(node.child1().index()), PhantomPutStructure);
            break;

        case CheckFunction:
            if (checkFunctionElimination(node.function(), node.child1().index()))
                eliminate();
            break;
                
        case CheckArray:
            if (checkArrayElimination(node.child1().index(), node.arrayMode()))
                eliminate();
            break;
            
        case GetIndexedPropertyStorage: {
            setReplacement(getIndexedPropertyStorageLoadElimination(node.child1().index(), node.arrayMode()));
            break;
        }

        case GetButterfly:
            setReplacement(getPropertyStorageLoadElimination(node.child1().index()));
            break;

        case GetByOffset:
            setReplacement(getByOffsetLoadElimination(m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber, node.child1().index()));
            break;
            
        case PutByOffset:
            if (m_graph.m_fixpointState == FixpointNotConverged)
                break;
            eliminate(putByOffsetStoreElimination(m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber, node.child1().index()));
            break;
            
        case Phantom:
            // FIXME: we ought to remove Phantom's that have no children.
            
            eliminateIrrelevantPhantomChildren(node);
            break;
            
        default:
            // do nothing.
            break;
        }
        
        m_lastSeen[node.op()] = m_indexInBlock;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("\n");
#endif
    }
    
    void performBlockCSE(BasicBlock* block)
    {
        if (!block)
            return;
        if (!block->isReachable)
            return;
        
        m_currentBlock = block;
        for (unsigned i = 0; i < LastNodeType; ++i)
            m_lastSeen[i] = UINT_MAX;
        
        // Make all of my Phi nodes relevant to OSR.
        for (unsigned i = 0; i < block->phis.size(); ++i)
            m_relevantToOSR.set(block->phis[i]);
        
        // Make all of my SetLocal nodes relevant to OSR.
        for (unsigned i = 0; i < block->size(); ++i) {
            NodeIndex nodeIndex = block->at(i);
            Node& node = m_graph[nodeIndex];
            switch (node.op()) {
            case SetLocal:
                m_relevantToOSR.set(nodeIndex);
                break;
            default:
                break;
            }
        }

        for (m_indexInBlock = 0; m_indexInBlock < block->size(); ++m_indexInBlock) {
            m_compileIndex = block->at(m_indexInBlock);
            performNodeCSE(m_graph[m_compileIndex]);
        }
    }
    
    BasicBlock* m_currentBlock;
    NodeIndex m_compileIndex;
    unsigned m_indexInBlock;
    Vector<NodeIndex, 16> m_replacements;
    FixedArray<unsigned, LastNodeType> m_lastSeen;
    FastBitVector m_relevantToOSR;
    bool m_changed; // Only tracks changes that have a substantive effect on other optimizations.
};

bool performCSE(Graph& graph)
{
    SamplingRegion samplingRegion("DFG CSE Phase");
    return runPhase<CSEPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


