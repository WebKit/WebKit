/*
 * Copyright (C) 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "DFGAbstractHeap.h"
#include "DFGClobberize.h"
#include "DFGEdgeUsesStructure.h"
#include "DFGGraph.h"
#include "DFGPhase.h"
#include "JSCInlines.h"
#include <array>
#include <wtf/FastBitVector.h>

namespace JSC { namespace DFG {

class CSEPhase : public Phase {
public:
    CSEPhase(Graph& graph)
        : Phase(graph, "common subexpression elimination")
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_fixpointState != BeforeFixpoint);
        
        m_changed = false;
        
        m_graph.clearReplacements();
        
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
                if (!node->containsMovHint())
                    continue;
                
                ASSERT(node->op() != ZombieHint);
                node->child1()->mergeFlags(NodeRelevantToOSR);
            }
        }
        
        if (m_graph.m_form == SSA) {
            Vector<BasicBlock*> depthFirst;
            m_graph.getBlocksInDepthFirstOrder(depthFirst);
            for (unsigned i = 0; i < depthFirst.size(); ++i)
                performBlockCSE(depthFirst[i]);
        } else {
            for (unsigned blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex)
                performBlockCSE(m_graph.block(blockIndex));
        }
        
        return m_changed;
    }
    
private:
    
    unsigned endIndexForPureCSE()
    {
        unsigned result = m_lastSeen[m_currentNode->op()];
        if (result == UINT_MAX)
            result = 0;
        else
            result++;
        ASSERT(result <= m_indexInBlock);
        return result;
    }

    Node* pureCSE(Node* node)
    {
        Edge child1 = node->child1().sanitized();
        Edge child2 = node->child2().sanitized();
        Edge child3 = node->child3().sanitized();
        
        for (unsigned i = endIndexForPureCSE(); i--;) {
            Node* otherNode = m_currentBlock->at(i);
            if (otherNode == child1 || otherNode == child2 || otherNode == child3)
                break;

            if (node->op() != otherNode->op())
                continue;
            
            Edge otherChild = otherNode->child1().sanitized();
            if (!otherChild)
                return otherNode;
            if (otherChild != child1)
                continue;
            
            otherChild = otherNode->child2().sanitized();
            if (!otherChild)
                return otherNode;
            if (otherChild != child2)
                continue;
            
            otherChild = otherNode->child3().sanitized();
            if (!otherChild)
                return otherNode;
            if (otherChild != child3)
                continue;
            
            return otherNode;
        }
        return 0;
    }
    
    Node* constantCSE(Node* node)
    {
        for (unsigned i = endIndexForPureCSE(); i--;) {
            Node* otherNode = m_currentBlock->at(i);
            if (otherNode->op() != node->op())
                continue;
            
            if (otherNode->constant() != node->constant())
                continue;
            
            return otherNode;
        }
        return 0;
    }
    
    Node* constantStoragePointerCSE(Node* node)
    {
        for (unsigned i = endIndexForPureCSE(); i--;) {
            Node* otherNode = m_currentBlock->at(i);
            if (otherNode->op() != ConstantStoragePointer)
                continue;
            
            if (otherNode->storagePointer() != node->storagePointer())
                continue;
            
            return otherNode;
        }
        return 0;
    }
    
    Node* getCalleeLoadElimination()
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            switch (node->op()) {
            case GetCallee:
                return node;
            default:
                break;
            }
        }
        return 0;
    }
    
    Node* getArrayLengthElimination(Node* array)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            switch (node->op()) {
            case GetArrayLength:
                if (node->child1() == array)
                    return node;
                break;
                
            case PutByValDirect:
            case PutByVal:
                if (!m_graph.byValIsPure(node))
                    return 0;
                if (node->arrayMode().mayStoreToHole())
                    return 0;
                break;
                
            default:
                if (m_graph.clobbersWorld(node))
                    return 0;
            }
        }
        return 0;
    }
    
    Node* globalVarLoadElimination(WriteBarrier<Unknown>* registerPointer)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            switch (node->op()) {
            case GetGlobalVar:
                if (node->registerPointer() == registerPointer)
                    return node;
                break;
            case PutGlobalVar:
                if (node->registerPointer() == registerPointer)
                    return node->child1().node();
                break;
            default:
                break;
            }
            if (m_graph.clobbersWorld(node))
                break;
        }
        return 0;
    }
    
    Node* scopedVarLoadElimination(Node* registers, int varNumber)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            switch (node->op()) {
            case GetClosureVar: {
                if (node->child1() == registers && node->varNumber() == varNumber)
                    return node;
                break;
            } 
            case PutClosureVar: {
                if (node->varNumber() != varNumber)
                    break;
                if (node->child2() == registers)
                    return node->child3().node();
                return 0;
            }
            case SetLocal: {
                VariableAccessData* variableAccessData = node->variableAccessData();
                if (variableAccessData->isCaptured()
                    && variableAccessData->local() == static_cast<VirtualRegister>(varNumber))
                    return 0;
                break;
            }
            default:
                break;
            }
            if (m_graph.clobbersWorld(node))
                break;
        }
        return 0;
    }
    
    bool varInjectionWatchpointElimination()
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node->op() == VarInjectionWatchpoint)
                return true;
            if (m_graph.clobbersWorld(node))
                break;
        }
        return false;
    }
    
    Node* getByValLoadElimination(Node* child1, Node* child2, ArrayMode arrayMode)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == child1 || node == child2) 
                break;

            switch (node->op()) {
            case GetByVal:
                if (!m_graph.byValIsPure(node))
                    return 0;
                if (node->child1() == child1
                    && node->child2() == child2
                    && node->arrayMode().type() == arrayMode.type())
                    return node;
                break;
                    
            case PutByValDirect:
            case PutByVal:
            case PutByValAlias: {
                if (!m_graph.byValIsPure(node))
                    return 0;
                // Typed arrays 
                if (arrayMode.typedArrayType() != NotTypedArray)
                    return 0;
                if (m_graph.varArgChild(node, 0) == child1
                    && m_graph.varArgChild(node, 1) == child2
                    && node->arrayMode().type() == arrayMode.type())
                    return m_graph.varArgChild(node, 2).node();
                // We must assume that the PutByVal will clobber the location we're getting from.
                // FIXME: We can do better; if we know that the PutByVal is accessing an array of a
                // different type than the GetByVal, then we know that they won't clobber each other.
                // ... except of course for typed arrays, where all typed arrays clobber all other
                // typed arrays!  An Int32Array can alias a Float64Array for example, and so on.
                return 0;
            }
            default:
                if (m_graph.clobbersWorld(node))
                    return 0;
                break;
            }
        }
        return 0;
    }

    bool checkFunctionElimination(FrozenValue* function, Node* child1)
    {
        for (unsigned i = endIndexForPureCSE(); i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == child1) 
                break;

            if (node->op() == CheckFunction && node->child1() == child1 && node->function() == function)
                return true;
        }
        return false;
    }
    
    bool checkExecutableElimination(ExecutableBase* executable, Node* child1)
    {
        for (unsigned i = endIndexForPureCSE(); i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == child1)
                break;

            if (node->op() == CheckExecutable && node->child1() == child1 && node->executable() == executable)
                return true;
        }
        return false;
    }

    bool checkStructureElimination(const StructureSet& structureSet, Node* child1)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == child1) 
                break;

            switch (node->op()) {
            case CheckStructure:
                if (node->child1() == child1
                    && structureSet.isSupersetOf(node->structureSet()))
                    return true;
                break;
                
            case PutStructure:
                if (node->child1() == child1
                    && structureSet.contains(node->transition()->next))
                    return true;
                if (structureSet.contains(node->transition()->previous))
                    return false;
                break;
                
            case PutByOffset:
                // Setting a property cannot change the structure.
                break;
                
            case MultiPutByOffset:
                if (node->multiPutByOffsetData().writesStructures())
                    return false;
                break;
                
            case PutByValDirect:
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
                if (m_graph.clobbersWorld(node))
                    return false;
                break;
            }
        }
        return false;
    }
    
    bool structureTransitionWatchpointElimination(Structure* structure, Node* child1)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == child1) 
                break;

            switch (node->op()) {
            case CheckStructure:
                if (node->child1() == child1
                    && node->structureSet().isSubsetOf(StructureSet(structure)))
                    return true;
                break;
                
            case PutStructure:
                ASSERT(node->transition()->previous != structure);
                break;
                
            case PutByOffset:
                // Setting a property cannot change the structure.
                break;
                    
            case MultiPutByOffset:
                if (node->multiPutByOffsetData().writesStructures())
                    return false;
                break;
                
            case PutByValDirect:
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
                if (m_graph.clobbersWorld(node))
                    return false;
                break;
            }
        }
        return false;
    }
    
    Node* getByOffsetLoadElimination(unsigned identifierNumber, Node* base)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == base)
                break;

            switch (node->op()) {
            case GetByOffset:
                if (node->child2() == base
                    && m_graph.m_storageAccessData[node->storageAccessDataIndex()].identifierNumber == identifierNumber)
                    return node;
                break;
                
            case MultiGetByOffset:
                if (node->child1() == base
                    && node->multiGetByOffsetData().identifierNumber == identifierNumber)
                    return node;
                break;
                
            case PutByOffset:
                if (m_graph.m_storageAccessData[node->storageAccessDataIndex()].identifierNumber == identifierNumber) {
                    if (node->child2() == base) // Must be same property storage.
                        return node->child3().node();
                    return 0;
                }
                break;
                
            case MultiPutByOffset:
                if (node->multiPutByOffsetData().identifierNumber == identifierNumber) {
                    if (node->child1() == base)
                        return node->child2().node();
                    return 0;
                }
                break;
                    
            case PutByValDirect:
            case PutByVal:
            case PutByValAlias:
                if (m_graph.byValIsPure(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return 0;
                
            default:
                if (m_graph.clobbersWorld(node))
                    return 0;
                break;
            }
        }
        return 0;
    }
    
    Node* getGetterSetterByOffsetLoadElimination(unsigned identifierNumber, Node* base)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == base)
                break;

            switch (node->op()) {
            case GetGetterSetterByOffset:
                if (node->child2() == base
                    && m_graph.m_storageAccessData[node->storageAccessDataIndex()].identifierNumber == identifierNumber)
                    return node;
                break;
                    
            case PutByValDirect:
            case PutByVal:
            case PutByValAlias:
                if (m_graph.byValIsPure(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return 0;
                
            default:
                if (m_graph.clobbersWorld(node))
                    return 0;
                break;
            }
        }
        return 0;
    }
    
    Node* getPropertyStorageLoadElimination(Node* child1)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == child1) 
                break;

            switch (node->op()) {
            case GetButterfly:
                if (node->child1() == child1)
                    return node;
                break;

            case AllocatePropertyStorage:
            case ReallocatePropertyStorage:
                // If we can cheaply prove this is a change to our object's storage, we
                // can optimize and use its result.
                if (node->child1() == child1)
                    return node;
                // Otherwise, we currently can't prove that this doesn't change our object's
                // storage, so we conservatively assume that it may change the storage
                // pointer of any object, including ours.
                return 0;
                    
            case PutByValDirect:
            case PutByVal:
            case PutByValAlias:
                if (m_graph.byValIsPure(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return 0;
                
            case Arrayify:
            case ArrayifyToStructure:
                // We could check if the arrayification could affect our butterfly.
                // But that seems like it would take Effort.
                return 0;
                
            case MultiPutByOffset:
                if (node->multiPutByOffsetData().reallocatesStorage())
                    return 0;
                break;
                
            default:
                if (m_graph.clobbersWorld(node))
                    return 0;
                break;
            }
        }
        return 0;
    }
    
    bool checkArrayElimination(Node* child1, ArrayMode arrayMode)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == child1) 
                break;

            switch (node->op()) {
            case CheckArray:
                if (node->child1() == child1 && node->arrayMode() == arrayMode)
                    return true;
                break;
                
            case Arrayify:
            case ArrayifyToStructure:
                // We could check if the arrayification could affect our array.
                // But that seems like it would take Effort.
                return false;
                
            default:
                if (m_graph.clobbersWorld(node))
                    return false;
                break;
            }
        }
        return false;
    }

    Node* getIndexedPropertyStorageLoadElimination(Node* child1, ArrayMode arrayMode)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == child1) 
                break;

            switch (node->op()) {
            case GetIndexedPropertyStorage: {
                if (node->child1() == child1 && node->arrayMode() == arrayMode)
                    return node;
                break;
            }

            default:
                if (m_graph.clobbersWorld(node))
                    return 0;
                break;
            }
        }
        return 0;
    }
    
    Node* getInternalFieldLoadElimination(NodeType op, Node* child1)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == child1) 
                break;

            if (node->op() == op && node->child1() == child1)
                return node;

            if (m_graph.clobbersWorld(node))
                return 0;
        }
        return 0;
    }
    
    Node* getMyScopeLoadElimination()
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            switch (node->op()) {
            case CreateActivation:
                // This may cause us to return a different scope.
                return 0;
            case GetMyScope:
                return node;
            default:
                break;
            }
        }
        return 0;
    }
    
    Node* getLocalLoadElimination(VirtualRegister local, Node*& relevantLocalOp, bool careAboutClobbering)
    {
        relevantLocalOp = 0;
        
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            switch (node->op()) {
            case GetLocal:
                if (node->local() == local) {
                    relevantLocalOp = node;
                    return node;
                }
                break;
                
            case GetLocalUnlinked:
                if (node->unlinkedLocal() == local) {
                    relevantLocalOp = node;
                    return node;
                }
                break;
                
            case SetLocal:
                if (node->local() == local) {
                    relevantLocalOp = node;
                    return node->child1().node();
                }
                break;
                
            case GetClosureVar:
            case PutClosureVar:
                if (static_cast<VirtualRegister>(node->varNumber()) == local)
                    return 0;
                break;
                
            default:
                if (careAboutClobbering && m_graph.clobbersWorld(node))
                    return 0;
                break;
            }
        }
        return 0;
    }
    
    Node* uncapturedSetLocalStoreElimination(VirtualRegister local)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            switch (node->op()) {
            case GetLocal:
            case Flush:
                if (node->local() == local)
                    return nullptr;
                break;
                
            case GetLocalUnlinked:
                if (node->unlinkedLocal() == local)
                    return nullptr;
                break;
                
            case SetLocal: {
                if (node->local() != local)
                    break;
                return node;
            }
                
            case GetClosureVar:
            case PutClosureVar:
                if (static_cast<VirtualRegister>(node->varNumber()) == local)
                    return nullptr;
                break;
                
            case GetMyScope:
            case SkipTopScope:
                if (node->origin.semantic.inlineCallFrame)
                    break;
                if (m_graph.uncheckedActivationRegister() == local)
                    return nullptr;
                break;
                
            case CheckArgumentsNotCreated:
            case GetMyArgumentsLength:
            case GetMyArgumentsLengthSafe:
                if (m_graph.uncheckedArgumentsRegisterFor(node->origin.semantic) == local)
                    return nullptr;
                break;
                
            case GetMyArgumentByVal:
            case GetMyArgumentByValSafe:
                return nullptr;
                
            case GetByVal:
                // If this is accessing arguments then it's potentially accessing locals.
                if (node->arrayMode().type() == Array::Arguments)
                    return nullptr;
                break;
                
            case CreateArguments:
            case TearOffActivation:
            case TearOffArguments:
                // If an activation is being torn off then it means that captured variables
                // are live. We could be clever here and check if the local qualifies as an
                // argument register. But that seems like it would buy us very little since
                // any kind of tear offs are rare to begin with.
                return nullptr;
                
            default:
                break;
            }
            if (m_graph.clobbersWorld(node))
                return nullptr;
        }
        return nullptr;
    }

    Node* capturedSetLocalStoreElimination(VirtualRegister local)
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            switch (node->op()) {
            case GetLocal:
            case Flush:
                if (node->local() == local)
                    return nullptr;
                break;
                
            case GetLocalUnlinked:
                if (node->unlinkedLocal() == local)
                    return nullptr;
                break;
                
            case SetLocal: {
                if (node->local() != local)
                    break;
                return node;
            }
                
            case Phantom:
            case Check:
            case HardPhantom:
            case MovHint:
            case JSConstant:
            case DoubleConstant:
            case Int52Constant:
                break;
                
            default:
                return nullptr;
            }
        }
        return nullptr;
    }
    
    Node* setLocalStoreElimination(VariableAccessData* variableAccessData)
    {
        if (variableAccessData->isCaptured())
            return capturedSetLocalStoreElimination(variableAccessData->local());
        return uncapturedSetLocalStoreElimination(variableAccessData->local());
    }
    
    bool invalidationPointElimination()
    {
        for (unsigned i = m_indexInBlock; i--;) {
            Node* node = m_currentBlock->at(i);
            if (node->op() == InvalidationPoint)
                return true;
            if (writesOverlap(m_graph, node, Watchpoint_fire))
                break;
        }
        return false;
    }
    
    void eliminateIrrelevantPhantomChildren(Node* node)
    {
        ASSERT(node->op() == Phantom);
        for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
            Edge edge = node->children.child(i);
            if (!edge)
                continue;
            if (edge.useKind() != UntypedUse)
                continue; // Keep the type check.
            if (edge->flags() & NodeRelevantToOSR)
                continue;
            
            node->children.removeEdge(i--);
            m_changed = true;
        }
    }
    
    bool setReplacement(Node* replacement)
    {
        if (!replacement)
            return false;
        
        if (!ASSERT_DISABLED
            && canonicalResultRepresentation(m_currentNode->result()) != canonicalResultRepresentation(replacement->result())) {
            startCrashing();
            dataLog("CSE attempting to replace a node with a mismatched result: ", m_currentNode, " with ", replacement, "\n");
            dataLog("\n");
            m_graph.dump();
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        m_currentNode->convertToPhantom();
        eliminateIrrelevantPhantomChildren(m_currentNode);
        
        // At this point we will eliminate all references to this node.
        m_currentNode->misc.replacement = replacement;
        
        m_changed = true;
        
        return true;
    }
    
    void eliminate()
    {
        ASSERT(m_currentNode->mustGenerate());
        m_currentNode->convertToPhantom();
        eliminateIrrelevantPhantomChildren(m_currentNode);
        
        m_changed = true;
    }
    
    void eliminate(Node* node, NodeType phantomType = Phantom)
    {
        if (!node)
            return;
        ASSERT(node->mustGenerate());
        node->setOpAndDefaultFlags(phantomType);
        if (phantomType == Phantom)
            eliminateIrrelevantPhantomChildren(node);
        
        m_changed = true;
    }
    
    void performNodeCSE(Node* node)
    {
        m_graph.performSubstitution(node);
        
        switch (node->op()) {
        
        case Identity:
            setReplacement(node->child1().node());
            break;
            
        // Handle the pure nodes. These nodes never have any side-effects.
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift:
        case ArithAbs:
        case ArithMin:
        case ArithMax:
        case ArithSqrt:
        case ArithFRound:
        case ArithSin:
        case ArithCos:
        case StringCharAt:
        case StringCharCodeAt:
        case IsUndefined:
        case IsBoolean:
        case IsNumber:
        case IsString:
        case IsObject:
        case IsFunction:
        case LogicalNot:
        case SkipTopScope:
        case SkipScope:
        case GetClosureRegisters:
        case GetScope:
        case TypeOf:
        case CompareEqConstant:
        case ValueToInt32:
        case MakeRope:
        case DoubleRep:
        case ValueRep:
        case Int52Rep:
        case BooleanToNumber:
            setReplacement(pureCSE(node));
            break;
            
        case ArithAdd:
        case ArithSub:
        case ArithNegate:
        case ArithMul:
        case ArithDiv:
        case ArithMod:
        case UInt32ToNumber:
        case DoubleAsInt32: {
            Node* candidate = pureCSE(node);
            if (!candidate)
                break;
            if (!subsumes(candidate->arithMode(), node->arithMode())) {
                if (!subsumes(node->arithMode(), candidate->arithMode()))
                    break;
                candidate->setArithMode(node->arithMode());
            }
            setReplacement(candidate);
            break;
        }
            
        case GetCallee:
            setReplacement(getCalleeLoadElimination());
            break;

        case GetLocal: {
            VariableAccessData* variableAccessData = node->variableAccessData();
            if (!variableAccessData->isCaptured())
                break;
            Node* relevantLocalOp;
            Node* possibleReplacement = getLocalLoadElimination(variableAccessData->local(), relevantLocalOp, variableAccessData->isCaptured());
            if (!relevantLocalOp)
                break;
            if (relevantLocalOp->op() != GetLocalUnlinked
                && relevantLocalOp->variableAccessData() != variableAccessData)
                break;
            Node* phi = node->child1().node();
            if (!setReplacement(possibleReplacement))
                break;
            
            m_graph.dethread();
            
            // If we replace a GetLocal with a GetLocalUnlinked, then turn the GetLocalUnlinked
            // into a GetLocal.
            if (relevantLocalOp->op() == GetLocalUnlinked)
                relevantLocalOp->convertToGetLocal(variableAccessData, phi);

            m_changed = true;
            break;
        }
            
        case GetLocalUnlinked: {
            Node* relevantLocalOpIgnored;
            setReplacement(getLocalLoadElimination(node->unlinkedLocal(), relevantLocalOpIgnored, true));
            break;
        }
            
        case JSConstant:
        case DoubleConstant:
        case Int52Constant:
            // This is strange, but necessary. Some phases will convert nodes to constants,
            // which may result in duplicated constants. We use CSE to clean this up.
            setReplacement(constantCSE(node));
            break;
            
        case ConstantStoragePointer:
            setReplacement(constantStoragePointerCSE(node));
            break;
            
        case GetArrayLength:
            setReplacement(getArrayLengthElimination(node->child1().node()));
            break;

        case GetMyScope:
            setReplacement(getMyScopeLoadElimination());
            break;
            
        // Handle nodes that are conditionally pure: these are pure, and can
        // be CSE'd, so long as the prediction is the one we want.
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq:
        case CompareEq: {
            if (m_graph.isPredictedNumerical(node)) {
                Node* replacement = pureCSE(node);
                if (replacement && m_graph.isPredictedNumerical(replacement))
                    setReplacement(replacement);
            }
            break;
        }
            
        // Finally handle heap accesses. These are not quite pure, but we can still
        // optimize them provided that some subtle conditions are met.
        case GetGlobalVar:
            setReplacement(globalVarLoadElimination(node->registerPointer()));
            break;

        case GetClosureVar: {
            setReplacement(scopedVarLoadElimination(node->child1().node(), node->varNumber()));
            break;
        }

        case VarInjectionWatchpoint:
            if (varInjectionWatchpointElimination())
                eliminate();
            break;
            
        case GetByVal:
            if (m_graph.byValIsPure(node))
                setReplacement(getByValLoadElimination(node->child1().node(), node->child2().node(), node->arrayMode()));
            break;
                
        case PutByValDirect:
        case PutByVal: {
            Edge child1 = m_graph.varArgChild(node, 0);
            Edge child2 = m_graph.varArgChild(node, 1);
            if (node->arrayMode().canCSEStorage()) {
                Node* replacement = getByValLoadElimination(child1.node(), child2.node(), node->arrayMode());
                if (!replacement)
                    break;
                node->setOp(PutByValAlias);
            }
            break;
        }
            
        case CheckStructure:
            if (checkStructureElimination(node->structureSet(), node->child1().node()))
                eliminate();
            break;
            
        case CheckFunction:
            if (checkFunctionElimination(node->function(), node->child1().node()))
                eliminate();
            break;
                
        case CheckExecutable:
            if (checkExecutableElimination(node->executable(), node->child1().node()))
                eliminate();
            break;
                
        case CheckArray:
            if (checkArrayElimination(node->child1().node(), node->arrayMode()))
                eliminate();
            break;
            
        case GetIndexedPropertyStorage: {
            setReplacement(getIndexedPropertyStorageLoadElimination(node->child1().node(), node->arrayMode()));
            break;
        }
            
        case GetTypedArrayByteOffset:
        case GetGetter:
        case GetSetter: {
            setReplacement(getInternalFieldLoadElimination(node->op(), node->child1().node()));
            break;
        }

        case GetButterfly:
            setReplacement(getPropertyStorageLoadElimination(node->child1().node()));
            break;

        case GetByOffset:
            setReplacement(getByOffsetLoadElimination(m_graph.m_storageAccessData[node->storageAccessDataIndex()].identifierNumber, node->child2().node()));
            break;
            
        case GetGetterSetterByOffset:
            setReplacement(getGetterSetterByOffsetLoadElimination(m_graph.m_storageAccessData[node->storageAccessDataIndex()].identifierNumber, node->child2().node()));
            break;
            
        case MultiGetByOffset:
            setReplacement(getByOffsetLoadElimination(node->multiGetByOffsetData().identifierNumber, node->child1().node()));
            break;
            
        case InvalidationPoint:
            if (invalidationPointElimination())
                eliminate();
            break;
            
        case Phantom:
            // FIXME: we ought to remove Phantom's that have no children.
            // NB. It would be incorrect to do this for HardPhantom. In fact, the whole point
            // of HardPhantom is that we *don't* do this for HardPhantoms, since they signify
            // a more strict kind of liveness than the Phantom bytecode liveness.
            eliminateIrrelevantPhantomChildren(node);
            break;
            
        case Flush:
            // This is needed for arguments simplification to work. We need to eliminate the
            // redundancy between op_enter's undefined-all-the-things and the subsequent
            // op_init_lazy_reg.
            
            ASSERT(m_graph.m_form != SSA);
            
            if (Node* setLocal = setLocalStoreElimination(node->variableAccessData())) {
                node->convertToPhantom();
                Node* dataNode = setLocal->child1().node();
                ASSERT(dataNode->hasResult());
                node->child1() = dataNode->defaultEdge();
                m_graph.dethread();
                m_changed = true;
            }
            break;
            
        default:
            // do nothing.
            break;
        }
        
        m_lastSeen[node->op()] = m_indexInBlock;
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
        
        for (m_indexInBlock = 0; m_indexInBlock < block->size(); ++m_indexInBlock) {
            m_currentNode = block->at(m_indexInBlock);
            performNodeCSE(m_currentNode);
        }
    }
    
    BasicBlock* m_currentBlock;
    Node* m_currentNode;
    unsigned m_indexInBlock;
    std::array<unsigned, LastNodeType> m_lastSeen;
    bool m_changed; // Only tracks changes that have a substantive effect on other optimizations.
};

bool performCSE(Graph& graph)
{
    SamplingRegion samplingRegion("DFG CSE Phase");
    return runPhase<CSEPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


