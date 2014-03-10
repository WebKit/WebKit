/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "DFGAbstractInterpreterInlines.h"
#include "DFGBasicBlock.h"
#include "DFGGraph.h"
#include "DFGInPlaceAbstractState.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "GetByIdStatus.h"
#include "JSCInlines.h"
#include "PutByIdStatus.h"

namespace JSC { namespace DFG {

class ConstantFoldingPhase : public Phase {
public:
    ConstantFoldingPhase(Graph& graph)
        : Phase(graph, "constant folding")
        , m_state(graph)
        , m_interpreter(graph, m_state)
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        bool changed = false;
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            if (block->cfaFoundConstants)
                changed |= foldConstants(block);
        }
        
        return changed;
    }

private:
    bool foldConstants(BasicBlock* block)
    {
        bool changed = false;
        m_state.beginBasicBlock(block);
        for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
            if (!m_state.isValid())
                break;
            
            Node* node = block->at(indexInBlock);

            bool eliminated = false;
                    
            switch (node->op()) {
            case CheckArgumentsNotCreated: {
                if (!isEmptySpeculation(
                        m_state.variables().operand(
                            m_graph.argumentsRegisterFor(node->origin.semantic)).m_type))
                    break;
                node->convertToPhantom();
                eliminated = true;
                break;
            }
                    
            case CheckStructure:
            case ArrayifyToStructure: {
                AbstractValue& value = m_state.forNode(node->child1());
                StructureSet set;
                if (node->op() == ArrayifyToStructure)
                    set = node->structure();
                else
                    set = node->structureSet();
                if (value.m_currentKnownStructure.isSubsetOf(set)) {
                    m_interpreter.execute(indexInBlock); // Catch the fact that we may filter on cell.
                    node->convertToPhantom();
                    eliminated = true;
                    break;
                }
                StructureAbstractValue& structureValue = value.m_futurePossibleStructure;
                if (structureValue.isSubsetOf(set)
                    && structureValue.hasSingleton()) {
                    Structure* structure = structureValue.singleton();
                    m_interpreter.execute(indexInBlock); // Catch the fact that we may filter on cell.
                    AdjacencyList children = node->children;
                    children.removeEdge(0);
                    if (!!children.child1())
                        m_insertionSet.insertNode(indexInBlock, SpecNone, Phantom, node->origin, children);
                    node->children.setChild2(Edge());
                    node->children.setChild3(Edge());
                    node->convertToStructureTransitionWatchpoint(structure);
                    eliminated = true;
                    break;
                }
                break;
            }
                
            case CheckArray:
            case Arrayify: {
                if (!node->arrayMode().alreadyChecked(m_graph, node, m_state.forNode(node->child1())))
                    break;
                node->convertToPhantom();
                eliminated = true;
                break;
            }
                
            case CheckFunction: {
                if (m_state.forNode(node->child1()).value() != node->function())
                    break;
                node->convertToPhantom();
                eliminated = true;
                break;
            }
                
            case CheckInBounds: {
                JSValue left = m_state.forNode(node->child1()).value();
                JSValue right = m_state.forNode(node->child2()).value();
                if (left && right && left.isInt32() && right.isInt32()
                    && static_cast<uint32_t>(left.asInt32()) < static_cast<uint32_t>(right.asInt32())) {
                    node->convertToPhantom();
                    eliminated = true;
                    break;
                }
                
                break;
            }
                
            case MultiGetByOffset: {
                Edge childEdge = node->child1();
                Node* child = childEdge.node();
                MultiGetByOffsetData& data = node->multiGetByOffsetData();

                Structure* structure = m_state.forNode(child).bestProvenStructure();
                if (!structure)
                    break;
                
                for (unsigned i = data.variants.size(); i--;) {
                    const GetByIdVariant& variant = data.variants[i];
                    if (!variant.structureSet().contains(structure))
                        continue;
                    
                    if (variant.chain())
                        break;
                    
                    emitGetByOffset(indexInBlock, node, structure, variant, data.identifierNumber);
                    eliminated = true;
                    break;
                }
                break;
            }
                
            case MultiPutByOffset: {
                Edge childEdge = node->child1();
                Node* child = childEdge.node();
                MultiPutByOffsetData& data = node->multiPutByOffsetData();

                Structure* structure = m_state.forNode(child).bestProvenStructure();
                if (!structure)
                    break;
                
                for (unsigned i = data.variants.size(); i--;) {
                    const PutByIdVariant& variant = data.variants[i];
                    if (variant.oldStructure() != structure)
                        continue;
                    
                    emitPutByOffset(indexInBlock, node, structure, variant, data.identifierNumber);
                    eliminated = true;
                    break;
                }
                break;
            }
        
            case GetById:
            case GetByIdFlush: {
                Edge childEdge = node->child1();
                Node* child = childEdge.node();
                unsigned identifierNumber = node->identifierNumber();
                
                if (childEdge.useKind() != CellUse)
                    break;
                
                Structure* structure = m_state.forNode(child).bestProvenStructure();
                if (!structure)
                    break;

                GetByIdStatus status = GetByIdStatus::computeFor(
                    vm(), structure, m_graph.identifiers()[identifierNumber]);
                
                if (!status.isSimple() || status.numVariants() != 1) {
                    // FIXME: We could handle prototype cases.
                    // https://bugs.webkit.org/show_bug.cgi?id=110386
                    break;
                }
                
                emitGetByOffset(indexInBlock, node, structure, status[0], identifierNumber);
                eliminated = true;
                break;
            }
                
            case PutById:
            case PutByIdDirect: {
                NodeOrigin origin = node->origin;
                Edge childEdge = node->child1();
                Node* child = childEdge.node();
                unsigned identifierNumber = node->identifierNumber();
                
                ASSERT(childEdge.useKind() == CellUse);
                
                Structure* structure = m_state.forNode(child).bestProvenStructure();
                if (!structure)
                    break;
                
                PutByIdStatus status = PutByIdStatus::computeFor(
                    vm(),
                    m_graph.globalObjectFor(origin.semantic),
                    structure,
                    m_graph.identifiers()[identifierNumber],
                    node->op() == PutByIdDirect);
                
                if (!status.isSimple())
                    break;
                if (status.numVariants() != 1)
                    break;
                
                emitPutByOffset(indexInBlock, node, structure, status[0], identifierNumber);
                eliminated = true;
                break;
            }

            case ToPrimitive: {
                if (m_state.forNode(node->child1()).m_type & ~(SpecFullNumber | SpecBoolean | SpecString))
                    break;
                
                node->convertToIdentity();
                break;
            }

            default:
                break;
            }
                
            if (eliminated) {
                changed = true;
                continue;
            }
                
            m_interpreter.execute(indexInBlock);
            if (!m_state.isValid()) {
                // If we invalidated then we shouldn't attempt to constant-fold. Here's an
                // example:
                //
                //     c: JSConstant(4.2)
                //     x: ValueToInt32(Check:Int32:@const)
                //
                // It would be correct for an analysis to assume that execution cannot
                // proceed past @x. Therefore, constant-folding @x could be rather bad. But,
                // the CFA may report that it found a constant even though it also reported
                // that everything has been invalidated. This will only happen in a couple of
                // the constant folding cases; most of them are also separately defensive
                // about such things.
                break;
            }
            if (!node->shouldGenerate() || m_state.didClobber() || node->hasConstant())
                continue;
            JSValue value = m_state.forNode(node).value();
            if (!value)
                continue;
            
            // Check if merging the abstract value of the constant into the abstract value
            // we've proven for this node wouldn't widen the proof. If it widens the proof
            // (i.e. says that the set contains more things in it than it previously did)
            // then we refuse to fold.
            AbstractValue oldValue = m_state.forNode(node);
            AbstractValue constantValue;
            constantValue.set(m_graph, value);
            if (oldValue.merge(constantValue))
                continue;
                
            NodeOrigin origin = node->origin;
            AdjacencyList children = node->children;
            
            if (node->op() == GetLocal)
                m_graph.dethread();
            else
                ASSERT(!node->hasVariableAccessData(m_graph));
            
            m_graph.convertToConstant(node, value);
            m_insertionSet.insertNode(
                indexInBlock, SpecNone, Phantom, origin, children);
            
            changed = true;
        }
        m_state.reset();
        m_insertionSet.execute(block);
        
        return changed;
    }
        
    void emitGetByOffset(unsigned indexInBlock, Node* node, Structure* structure, const GetByIdVariant& variant, unsigned identifierNumber)
    {
        NodeOrigin origin = node->origin;
        Edge childEdge = node->child1();
        Node* child = childEdge.node();

        bool needsWatchpoint = !m_state.forNode(child).m_currentKnownStructure.hasSingleton();
        bool needsCellCheck = m_state.forNode(child).m_type & ~SpecCell;
        
        ASSERT(!variant.chain());
        ASSERT(variant.structureSet().contains(structure));
        
        // Now before we do anything else, push the CFA forward over the GetById
        // and make sure we signal to the loop that it should continue and not
        // do any eliminations.
        m_interpreter.execute(indexInBlock);
        
        if (needsWatchpoint) {
            m_insertionSet.insertNode(
                indexInBlock, SpecNone, StructureTransitionWatchpoint, origin,
                OpInfo(structure), childEdge);
        } else if (needsCellCheck) {
            m_insertionSet.insertNode(
                indexInBlock, SpecNone, Phantom, origin, childEdge);
        }
        
        if (variant.specificValue()) {
            m_graph.convertToConstant(node, variant.specificValue());
            return;
        }
        
        childEdge.setUseKind(KnownCellUse);
        
        Edge propertyStorage;
        
        if (isInlineOffset(variant.offset()))
            propertyStorage = childEdge;
        else {
            propertyStorage = Edge(m_insertionSet.insertNode(
                indexInBlock, SpecNone, GetButterfly, origin, childEdge));
        }
        
        node->convertToGetByOffset(m_graph.m_storageAccessData.size(), propertyStorage);
        
        StorageAccessData storageAccessData;
        storageAccessData.offset = variant.offset();
        storageAccessData.identifierNumber = identifierNumber;
        m_graph.m_storageAccessData.append(storageAccessData);
    }

    void emitPutByOffset(unsigned indexInBlock, Node* node, Structure* structure, const PutByIdVariant& variant, unsigned identifierNumber)
    {
        NodeOrigin origin = node->origin;
        Edge childEdge = node->child1();
        Node* child = childEdge.node();

        ASSERT(variant.oldStructure() == structure);
        
        bool needsWatchpoint = !m_state.forNode(child).m_currentKnownStructure.hasSingleton();
        bool needsCellCheck = m_state.forNode(child).m_type & ~SpecCell;
        
        // Now before we do anything else, push the CFA forward over the PutById
        // and make sure we signal to the loop that it should continue and not
        // do any eliminations.
        m_interpreter.execute(indexInBlock);

        if (needsWatchpoint) {
            m_insertionSet.insertNode(
                indexInBlock, SpecNone, StructureTransitionWatchpoint, origin,
                OpInfo(structure), childEdge);
        } else if (needsCellCheck) {
            m_insertionSet.insertNode(
                indexInBlock, SpecNone, Phantom, origin, childEdge);
        }

        childEdge.setUseKind(KnownCellUse);

        StructureTransitionData* transitionData = 0;
        if (variant.kind() == PutByIdVariant::Transition) {
            transitionData = m_graph.addStructureTransitionData(
                StructureTransitionData(structure, variant.newStructure()));

            if (node->op() == PutById) {
                if (!structure->storedPrototype().isNull()) {
                    addStructureTransitionCheck(
                        origin, indexInBlock,
                        structure->storedPrototype().asCell());
                }

                m_graph.chains().addLazily(variant.structureChain());

                for (unsigned i = 0; i < variant.structureChain()->size(); ++i) {
                    JSValue prototype = variant.structureChain()->at(i)->storedPrototype();
                    if (prototype.isNull())
                        continue;
                    ASSERT(prototype.isCell());
                    addStructureTransitionCheck(
                        origin, indexInBlock, prototype.asCell());
                }
            }
        }

        Edge propertyStorage;

        if (isInlineOffset(variant.offset()))
            propertyStorage = childEdge;
        else if (
            variant.kind() == PutByIdVariant::Replace
            || structure->outOfLineCapacity() == variant.newStructure()->outOfLineCapacity()) {
            propertyStorage = Edge(m_insertionSet.insertNode(
                indexInBlock, SpecNone, GetButterfly, origin, childEdge));
        } else if (!structure->outOfLineCapacity()) {
            ASSERT(variant.newStructure()->outOfLineCapacity());
            ASSERT(!isInlineOffset(variant.offset()));
            Node* allocatePropertyStorage = m_insertionSet.insertNode(
                indexInBlock, SpecNone, AllocatePropertyStorage,
                origin, OpInfo(transitionData), childEdge);
            m_insertionSet.insertNode(indexInBlock, SpecNone, StoreBarrier, origin, Edge(node->child1().node(), KnownCellUse));
            propertyStorage = Edge(allocatePropertyStorage);
        } else {
            ASSERT(structure->outOfLineCapacity());
            ASSERT(variant.newStructure()->outOfLineCapacity() > structure->outOfLineCapacity());
            ASSERT(!isInlineOffset(variant.offset()));

            Node* reallocatePropertyStorage = m_insertionSet.insertNode(
                indexInBlock, SpecNone, ReallocatePropertyStorage, origin,
                OpInfo(transitionData), childEdge,
                Edge(m_insertionSet.insertNode(
                    indexInBlock, SpecNone, GetButterfly, origin, childEdge)));
            m_insertionSet.insertNode(indexInBlock, SpecNone, StoreBarrier, origin, Edge(node->child1().node(), KnownCellUse));
            propertyStorage = Edge(reallocatePropertyStorage);
        }

        if (variant.kind() == PutByIdVariant::Transition) {
            Node* putStructure = m_graph.addNode(SpecNone, PutStructure, origin, OpInfo(transitionData), childEdge);
            m_insertionSet.insertNode(indexInBlock, SpecNone, StoreBarrier, origin, Edge(node->child1().node(), KnownCellUse));
            m_insertionSet.insert(indexInBlock, putStructure);
        }

        node->convertToPutByOffset(m_graph.m_storageAccessData.size(), propertyStorage);
        m_insertionSet.insertNode(
            indexInBlock, SpecNone, StoreBarrier, origin, 
            Edge(node->child2().node(), KnownCellUse));

        StorageAccessData storageAccessData;
        storageAccessData.offset = variant.offset();
        storageAccessData.identifierNumber = identifierNumber;
        m_graph.m_storageAccessData.append(storageAccessData);
    }

    void addStructureTransitionCheck(NodeOrigin origin, unsigned indexInBlock, JSCell* cell)
    {
        Node* weakConstant = m_insertionSet.insertNode(
            indexInBlock, speculationFromValue(cell), WeakJSConstant, origin, OpInfo(cell));
        
        if (m_graph.watchpoints().isStillValid(cell->structure()->transitionWatchpointSet())) {
            m_insertionSet.insertNode(
                indexInBlock, SpecNone, StructureTransitionWatchpoint, origin,
                OpInfo(cell->structure()), Edge(weakConstant, CellUse));
            return;
        }

        m_insertionSet.insertNode(
            indexInBlock, SpecNone, CheckStructure, origin,
            OpInfo(m_graph.addStructureSet(cell->structure())), Edge(weakConstant, CellUse));
    }
    
    InPlaceAbstractState m_state;
    AbstractInterpreter<InPlaceAbstractState> m_interpreter;
    InsertionSet m_insertionSet;
};

bool performConstantFolding(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Constant Folding Phase");
    return runPhase<ConstantFoldingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


