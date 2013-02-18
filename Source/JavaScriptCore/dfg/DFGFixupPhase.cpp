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
#include "DFGFixupPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "Operations.h"

namespace JSC { namespace DFG {

class FixupPhase : public Phase {
public:
    FixupPhase(Graph& graph)
        : Phase(graph, "fixup")
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex)
            fixupBlock(m_graph.m_blocks[blockIndex].get());
        return true;
    }

private:
    void fixupBlock(BasicBlock* block)
    {
        if (!block)
            return;
        ASSERT(block->isReachable);
        m_block = block;
        for (m_indexInBlock = 0; m_indexInBlock < block->size(); ++m_indexInBlock) {
            m_currentNode = block->at(m_indexInBlock);
            fixupNode(m_currentNode);
        }
        m_insertionSet.execute(block);
    }
    
    void fixupNode(Node* node)
    {
        if (!node->shouldGenerate())
            return;
        
        NodeType op = node->op();

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("   %s @%u: ", Graph::opName(op), node->index());
#endif
        
        switch (op) {
        case GetById: {
            if (m_graph.m_fixpointState > BeforeFixpoint)
                break;
            
            if (!isInt32Speculation(node->prediction()))
                break;
            if (codeBlock()->identifier(node->identifierNumber()) != globalData().propertyNames->length)
                break;
            ArrayProfile* arrayProfile = 
                m_graph.baselineCodeBlockFor(node->codeOrigin)->getArrayProfile(
                    node->codeOrigin.bytecodeIndex);
            ArrayMode arrayMode = ArrayMode(Array::SelectUsingPredictions);
            if (arrayProfile) {
                arrayProfile->computeUpdatedPrediction(m_graph.baselineCodeBlockFor(node->codeOrigin));
                arrayMode = ArrayMode::fromObserved(arrayProfile, Array::Read, false);
                arrayMode = arrayMode.refine(
                    node->child1()->prediction(), node->prediction());
                if (arrayMode.supportsLength() && arrayProfile->hasDefiniteStructure()) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, RefChildren, DontRefNode, SpecNone, CheckStructure,
                        node->codeOrigin, OpInfo(m_graph.addStructureSet(arrayProfile->expectedStructure())),
                        node->child1().node());
                }
            } else
                arrayMode = arrayMode.refine(node->child1()->prediction(), node->prediction());
            if (!arrayMode.supportsLength())
                break;
            node->setOp(GetArrayLength);
            ASSERT(node->flags() & NodeMustGenerate);
            node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
            m_graph.deref(node);
            node->setArrayMode(arrayMode);
            
            Node* storage = checkArray(arrayMode, node->codeOrigin, node->child1().node(), 0, lengthNeedsStorage, node->shouldGenerate());
            if (!storage)
                break;
            
            node->children.child2() = Edge(storage);
            break;
        }
        case GetIndexedPropertyStorage: {
            ASSERT(node->arrayMode().canCSEStorage());
            break;
        }
        case GetByVal: {
            node->setArrayMode(
                node->arrayMode().refine(
                    node->child1()->prediction(),
                    node->child2()->prediction(),
                    SpecNone, node->flags()));
            
            blessArrayOperation(node->child1(), node->child2(), 2);
            
            ArrayMode arrayMode = node->arrayMode();
            if (arrayMode.type() == Array::Double
                && arrayMode.arrayClass() == Array::OriginalArray
                && arrayMode.speculation() == Array::InBounds
                && arrayMode.conversion() == Array::AsIs
                && m_graph.globalObjectFor(node->codeOrigin)->arrayPrototypeChainIsSane()
                && !(node->flags() & NodeUsedAsOther))
                node->setArrayMode(arrayMode.withSpeculation(Array::SaneChain));
            
            break;
        }
        case StringCharAt:
        case StringCharCodeAt: {
            // Currently we have no good way of refining these.
            ASSERT(node->arrayMode() == ArrayMode(Array::String));
            blessArrayOperation(node->child1(), node->child2(), 2);
            break;
        }
            
        case ArrayPush: {
            // May need to refine the array mode in case the value prediction contravenes
            // the array prediction. For example, we may have evidence showing that the
            // array is in Int32 mode, but the value we're storing is likely to be a double.
            // Then we should turn this into a conversion to Double array followed by the
            // push. On the other hand, we absolutely don't want to refine based on the
            // base prediction. If it has non-cell garbage in it, then we want that to be
            // ignored. That's because ArrayPush can't handle any array modes that aren't
            // array-related - so if refine() turned this into a "Generic" ArrayPush then
            // that would break things.
            node->setArrayMode(
                node->arrayMode().refine(
                    node->child1()->prediction() & SpecCell,
                    SpecInt32,
                    node->child2()->prediction()));
            blessArrayOperation(node->child1(), Edge(), 2);
            
            switch (node->arrayMode().type()) {
            case Array::Double:
                fixDoubleEdge(1);
                break;
            default:
                break;
            }
            break;
        }
            
        case ArrayPop: {
            blessArrayOperation(node->child1(), Edge(), 1);
            break;
        }
            
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift: {
            fixIntEdge(node->children.child1());
            fixIntEdge(node->children.child2());
            break;
        }
            
        case CompareEq:
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq:
        case CompareStrictEq: {
            if (Node::shouldSpeculateInteger(node->child1().node(), node->child2().node()))
                break;
            if (!Node::shouldSpeculateNumber(node->child1().node(), node->child2().node()))
                break;
            fixDoubleEdge(0);
            fixDoubleEdge(1);
            break;
        }
            
        case LogicalNot: {
            if (node->child1()->shouldSpeculateInteger())
                break;
            if (!node->child1()->shouldSpeculateNumber())
                break;
            fixDoubleEdge(0);
            break;
        }
            
        case Branch: {
            if (!node->child1()->shouldSpeculateInteger()
                && node->child1()->shouldSpeculateNumber())
                fixDoubleEdge(0);

            Node* logicalNot = node->child1().node();
            if (logicalNot->op() == LogicalNot
                && logicalNot->adjustedRefCount() == 1) {
                
                // Make sure that OSR exit can't observe the LogicalNot. If it can,
                // then we must compute it and cannot peephole around it.
                bool found = false;
                bool ok = true;
                for (unsigned i = m_indexInBlock; i--;) {
                    Node* candidate = m_block->at(i);
                    if (!candidate->shouldGenerate())
                        continue;
                    if (candidate == logicalNot) {
                        found = true;
                        break;
                    }
                    if (candidate->canExit()) {
                        ok = false;
                        found = true;
                        break;
                    }
                }
                ASSERT_UNUSED(found, found);
                
                if (ok) {
                    Edge newChildEdge = logicalNot->child1();
                    if (newChildEdge->hasBooleanResult()) {
                        m_graph.ref(newChildEdge);
                        m_graph.deref(logicalNot);
                        node->children.setChild1(newChildEdge);
                        
                        BlockIndex toBeTaken = node->notTakenBlockIndex();
                        BlockIndex toBeNotTaken = node->takenBlockIndex();
                        node->setTakenBlockIndex(toBeTaken);
                        node->setNotTakenBlockIndex(toBeNotTaken);
                    }
                }
            }
            break;
        }
            
        case SetLocal: {
            if (node->variableAccessData()->isCaptured())
                break;
            if (!node->variableAccessData()->shouldUseDoubleFormat())
                break;
            fixDoubleEdge(0, ForwardSpeculation);
            break;
        }
            
        case ArithAdd:
        case ValueAdd: {
            if (m_graph.addShouldSpeculateInteger(node))
                break;
            if (!Node::shouldSpeculateNumberExpectingDefined(node->child1().node(), node->child2().node()))
                break;
            fixDoubleEdge(0);
            fixDoubleEdge(1);
            break;
        }
            
        case ArithSub: {
            if (m_graph.addShouldSpeculateInteger(node)
                && node->canSpeculateInteger())
                break;
            fixDoubleEdge(0);
            fixDoubleEdge(1);
            break;
        }
            
        case ArithNegate: {
            if (m_graph.negateShouldSpeculateInteger(node))
                break;
            fixDoubleEdge(0);
            break;
        }
            
        case ArithMin:
        case ArithMax:
        case ArithMod: {
            if (Node::shouldSpeculateIntegerForArithmetic(node->child1().node(), node->child2().node())
                && node->canSpeculateInteger())
                break;
            fixDoubleEdge(0);
            fixDoubleEdge(1);
            break;
        }
            
        case ArithMul: {
            if (m_graph.mulShouldSpeculateInteger(node))
                break;
            fixDoubleEdge(0);
            fixDoubleEdge(1);
            break;
        }

        case ArithDiv: {
            if (Node::shouldSpeculateIntegerForArithmetic(node->child1().node(), node->child2().node())
                && node->canSpeculateInteger()) {
                if (isX86() || isARMv7s())
                    break;
                injectInt32ToDoubleNode(0);
                injectInt32ToDoubleNode(1);

                // We don't need to do ref'ing on the children because we're stealing them from
                // the original division.
                Node* newDivision = m_insertionSet.insertNode(
                    m_indexInBlock, DontRefChildren, RefNode, SpecDouble, *node);
                
                node->setOp(DoubleAsInt32);
                node->children.initialize(Edge(newDivision, DoubleUse), Edge(), Edge());
                break;
            }
            fixDoubleEdge(0);
            fixDoubleEdge(1);
            break;
        }
            
        case ArithAbs: {
            if (node->child1()->shouldSpeculateIntegerForArithmetic()
                && node->canSpeculateInteger())
                break;
            fixDoubleEdge(0);
            break;
        }
            
        case ArithSqrt: {
            fixDoubleEdge(0);
            break;
        }
            
        case PutByVal:
        case PutByValAlias: {
            Edge child1 = m_graph.varArgChild(node, 0);
            Edge child2 = m_graph.varArgChild(node, 1);
            Edge child3 = m_graph.varArgChild(node, 2);

            node->setArrayMode(
                node->arrayMode().refine(
                    child1->prediction(),
                    child2->prediction(),
                    child3->prediction()));
            
            blessArrayOperation(child1, child2, 3);
            
            switch (node->arrayMode().modeForPut().type()) {
            case Array::Double:
                fixDoubleEdge(2);
                break;
            case Array::Int8Array:
            case Array::Int16Array:
            case Array::Int32Array:
            case Array::Uint8Array:
            case Array::Uint8ClampedArray:
            case Array::Uint16Array:
            case Array::Uint32Array:
                if (!child3->shouldSpeculateInteger())
                    fixDoubleEdge(2);
                break;
            case Array::Float32Array:
            case Array::Float64Array:
                fixDoubleEdge(2);
                break;
            default:
                break;
            }
            break;
        }
            
        case NewArray: {
            for (unsigned i = m_graph.varArgNumChildren(node); i--;) {
                node->setIndexingType(
                    leastUpperBoundOfIndexingTypeAndType(
                        node->indexingType(), m_graph.varArgChild(node, i)->prediction()));
            }
            if (node->indexingType() == ArrayWithDouble) {
                for (unsigned i = m_graph.varArgNumChildren(node); i--;)
                    fixDoubleEdge(i);
            }
            break;
        }
            
        default:
            break;
        }

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        if (!(node->flags() & NodeHasVarArgs)) {
            dataLogF("new children: ");
            node->dumpChildren(WTF::dataFile());
        }
        dataLogF("\n");
#endif
    }
    
    Node* checkArray(ArrayMode arrayMode, CodeOrigin codeOrigin, Node* array, Node* index, bool (*storageCheck)(const ArrayMode&) = canCSEStorage, bool shouldGenerate = true)
    {
        ASSERT(arrayMode.isSpecific());
        
        Structure* structure = arrayMode.originalArrayStructure(m_graph, codeOrigin);
        
        if (arrayMode.doesConversion()) {
            if (structure) {
                if (m_indexInBlock > 0) {
                    // If the previous node was a CheckStructure inserted because of stuff
                    // that the array profile told us, then remove it.
                    Node* previousNode = m_block->at(m_indexInBlock - 1);
                    if (previousNode->op() == CheckStructure
                        && previousNode->child1() == array
                        && previousNode->codeOrigin == codeOrigin)
                        previousNode->setOpAndDefaultFlags(Phantom);
                }
                
                m_insertionSet.insertNode(
                    m_indexInBlock, RefChildren, DontRefNode, SpecNone, ArrayifyToStructure, codeOrigin,
                    OpInfo(structure), OpInfo(arrayMode.asWord()), array, index);
            } else {
                m_insertionSet.insertNode(
                    m_indexInBlock, RefChildren, DontRefNode, SpecNone, Arrayify, codeOrigin,
                    OpInfo(arrayMode.asWord()), array, index);
            }
        } else {
            if (structure) {
                m_insertionSet.insertNode(
                    m_indexInBlock, RefChildren, DontRefNode, SpecNone, CheckStructure, codeOrigin,
                    OpInfo(m_graph.addStructureSet(structure)), array);
            } else {
                m_insertionSet.insertNode(
                    m_indexInBlock, RefChildren, DontRefNode, SpecNone, CheckArray, codeOrigin,
                    OpInfo(arrayMode.asWord()), array);
            }
        }
        
        if (!storageCheck(arrayMode))
            return 0;
        
        if (arrayMode.usesButterfly()) {
            return m_insertionSet.insertNode(
                m_indexInBlock,
                shouldGenerate ? RefChildren : DontRefChildren,
                shouldGenerate ? RefNode : DontRefNode,
                SpecNone, GetButterfly, codeOrigin, array);
        }
        
        return m_insertionSet.insertNode(
            m_indexInBlock,
            shouldGenerate ? RefChildren : DontRefChildren,
            shouldGenerate ? RefNode : DontRefNode,
            SpecNone, GetIndexedPropertyStorage, codeOrigin, OpInfo(arrayMode.asWord()), array);
    }
    
    void blessArrayOperation(Edge base, Edge index, unsigned storageChildIdx)
    {
        if (m_graph.m_fixpointState > BeforeFixpoint)
            return;
        
        Node* node = m_currentNode;
        
        switch (node->arrayMode().type()) {
        case Array::ForceExit: {
            m_insertionSet.insertNode(
                m_indexInBlock, DontRefChildren, DontRefNode, SpecNone, ForceOSRExit,
                node->codeOrigin);
            return;
        }
            
        case Array::SelectUsingPredictions:
        case Array::Unprofiled:
            RELEASE_ASSERT_NOT_REACHED();
            return;
            
        case Array::Generic:
            return;
            
        default: {
            Node* storage = checkArray(node->arrayMode(), node->codeOrigin, base.node(), index.node());
            if (!storage)
                return;
            
            m_graph.child(node, storageChildIdx) = Edge(storage);
            return;
        } }
    }
    
    void fixIntEdge(Edge& edge)
    {
        Node* node = edge.node();
        if (node->op() != ValueToInt32)
            return;
        
        if (!node->child1()->shouldSpeculateInteger())
            return;
        
        Edge oldEdge = edge;
        Edge newEdge = node->child1();
        
        m_graph.ref(newEdge);
        m_graph.deref(oldEdge);
        
        edge = newEdge;
    }
    
    void fixDoubleEdge(unsigned childIndex, SpeculationDirection direction = BackwardSpeculation)
    {
        Node* source = m_currentNode;
        Edge& edge = m_graph.child(source, childIndex);
        
        if (edge->prediction() & SpecDouble) {
            edge.setUseKind(DoubleUse);
            return;
        }
        
        injectInt32ToDoubleNode(childIndex, direction);
    }

    void injectInt32ToDoubleNode(unsigned childIndex, SpeculationDirection direction = BackwardSpeculation)
    {
        Node* result = m_insertionSet.insertNode(
            m_indexInBlock, DontRefChildren, RefNode, SpecDouble,
            direction == BackwardSpeculation ? Int32ToDouble : ForwardInt32ToDouble,
            m_currentNode->codeOrigin, m_graph.child(m_currentNode, childIndex).node());
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF(
            "(replacing @%u->@%u with @%u->@%u) ",
            m_currentNode->index(), m_graph.child(m_currentNode, childIndex)->index(), m_currentNode->index(), result->index());
#endif

        m_graph.child(m_currentNode, childIndex) = Edge(result, DoubleUse);
    }

    BasicBlock* m_block;
    unsigned m_indexInBlock;
    Node* m_currentNode;
    InsertionSet m_insertionSet;
};
    
bool performFixup(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Fixup Phase");
    return runPhase<FixupPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

