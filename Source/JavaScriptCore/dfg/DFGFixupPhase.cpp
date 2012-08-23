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
#include "DFGFixupPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"

namespace JSC { namespace DFG {

class FixupPhase : public Phase {
public:
    FixupPhase(Graph& graph)
        : Phase(graph, "fixup")
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
        for (m_indexInBlock = 0; m_indexInBlock < block->size(); ++m_indexInBlock) {
            m_compileIndex = block->at(m_indexInBlock);
            fixupNode(m_graph[m_compileIndex]);
        }
        m_insertionSet.execute(*block);
    }
    
    void fixupNode(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = node.op();

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("   %s @%u: ", Graph::opName(op), m_compileIndex);
#endif
        
        switch (op) {
        case GetById: {
            Node* nodePtr = &node;
            
            if (!isInt32Speculation(m_graph[m_compileIndex].prediction()))
                break;
            if (codeBlock()->identifier(nodePtr->identifierNumber()) != globalData().propertyNames->length)
                break;
            ArrayProfile* arrayProfile = 
                m_graph.baselineCodeBlockFor(nodePtr->codeOrigin)->getArrayProfile(
                    nodePtr->codeOrigin.bytecodeIndex);
            Array::Mode arrayMode = Array::Undecided;
            if (arrayProfile) {
                arrayProfile->computeUpdatedPrediction();
                arrayMode = refineArrayMode(
                    fromObserved(arrayProfile->observedArrayModes(), false),
                    m_graph[node.child1()].prediction(),
                    m_graph[m_compileIndex].prediction());                    
                if (modeSupportsLength(arrayMode)
                    && arrayProfile->hasDefiniteStructure()) {
                    m_graph.ref(nodePtr->child1());
                    Node checkStructure(CheckStructure, nodePtr->codeOrigin, OpInfo(m_graph.addStructureSet(arrayProfile->expectedStructure())), nodePtr->child1().index());
                    checkStructure.ref();
                    NodeIndex checkStructureIndex = m_graph.size();
                    m_graph.append(checkStructure);
                    m_insertionSet.append(m_indexInBlock, checkStructureIndex);
                    nodePtr = &m_graph[m_compileIndex];
                }
            } else {
                arrayMode = refineArrayMode(
                    arrayMode,
                    m_graph[node.child1()].prediction(),
                    m_graph[m_compileIndex].prediction());
            }
            if (!modeSupportsLength(arrayMode))
                break;
            nodePtr->setOp(GetArrayLength);
            ASSERT(nodePtr->flags() & NodeMustGenerate);
            nodePtr->clearFlags(NodeMustGenerate);
            m_graph.deref(m_compileIndex);
            nodePtr->setArrayMode(arrayMode);
            break;
        }
        case GetIndexedPropertyStorage: {
            node.setArrayMode(
                refineArrayMode(
                    node.arrayMode(),
                    m_graph[node.child1()].prediction(),
                    m_graph[node.child2()].prediction()));
            // Predictions should only become more, rather than less, refined. Hence
            // if we were ever able to CSE the storage pointer for this operation,
            // then we should always continue to be able to do so.
            ASSERT(canCSEStorage(node.arrayMode()));
            break;
        }
        case GetByVal:
        case StringCharAt:
        case StringCharCodeAt: {
            node.setArrayMode(
                refineArrayMode(
                    node.arrayMode(),
                    m_graph[node.child1()].prediction(),
                    m_graph[node.child2()].prediction()));
            
            if (canCSEStorage(node.arrayMode())) {
                if (node.child3()) {
                    ASSERT(m_graph[node.child3()].op() == GetIndexedPropertyStorage);
                    ASSERT(modesCompatibleForStorageLoad(m_graph[node.child3()].arrayMode(), node.arrayMode()));
                } else {
                    // Make sure we don't use the node reference after we do the append.
                    Node getIndexedPropertyStorage(
                        GetIndexedPropertyStorage, node.codeOrigin, OpInfo(node.arrayMode()),
                        node.child1().index(), node.child2().index());
                    NodeIndex getIndexedPropertyStorageIndex = m_graph.size();
                    node.children.child3() = Edge(getIndexedPropertyStorageIndex);
                    m_graph.append(getIndexedPropertyStorage);
                    m_graph.ref(getIndexedPropertyStorageIndex); // Once because it's MustGenerate.
                    m_graph.ref(getIndexedPropertyStorageIndex); // And again because it's referenced from the GetByVal.
                    m_insertionSet.append(m_indexInBlock, getIndexedPropertyStorageIndex);
                }
            } else {
                // See above. Continued fixup of the graph should not regress our ability
                // to speculate.
                ASSERT(!node.child3());
            }
            break;
        }
            
        case ValueToInt32: {
            if (m_graph[node.child1()].shouldSpeculateNumber()
                && node.mustGenerate()) {
                node.clearFlags(NodeMustGenerate);
                m_graph.deref(m_compileIndex);
            }
            break;
        }
            
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift: {
            fixIntEdge(node.children.child1());
            fixIntEdge(node.children.child2());
            break;
        }
            
        case CompareEq:
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq:
        case CompareStrictEq: {
            if (Node::shouldSpeculateInteger(m_graph[node.child1()], m_graph[node.child2()]))
                break;
            if (!Node::shouldSpeculateNumber(m_graph[node.child1()], m_graph[node.child2()]))
                break;
            fixDoubleEdge(0);
            fixDoubleEdge(1);
            break;
        }
            
        case LogicalNot: {
            if (m_graph[node.child1()].shouldSpeculateInteger())
                break;
            if (!m_graph[node.child1()].shouldSpeculateNumber())
                break;
            fixDoubleEdge(0);
            break;
        }
            
        case Branch: {
            if (!m_graph[node.child1()].shouldSpeculateInteger()
                && m_graph[node.child1()].shouldSpeculateNumber())
                fixDoubleEdge(0);

            Node& myNode = m_graph[m_compileIndex]; // reload because the graph may have changed
            Edge logicalNotEdge = myNode.child1();
            Node& logicalNot = m_graph[logicalNotEdge];
            if (logicalNot.op() == LogicalNot
                && logicalNot.adjustedRefCount() == 1) {
                Edge newChildEdge = logicalNot.child1();
                if (m_graph[newChildEdge].hasBooleanResult()) {
                    m_graph.ref(newChildEdge);
                    m_graph.deref(logicalNotEdge);
                    myNode.children.setChild1(newChildEdge);
                    
                    BlockIndex toBeTaken = myNode.notTakenBlockIndex();
                    BlockIndex toBeNotTaken = myNode.takenBlockIndex();
                    myNode.setTakenBlockIndex(toBeTaken);
                    myNode.setNotTakenBlockIndex(toBeNotTaken);
                }
            }
            break;
        }
            
        case SetLocal: {
            if (node.variableAccessData()->isCaptured())
                break;
            if (!node.variableAccessData()->shouldUseDoubleFormat())
                break;
            fixDoubleEdge(0);
            break;
        }
            
        case ArithAdd:
        case ValueAdd: {
            if (m_graph.addShouldSpeculateInteger(node))
                break;
            if (!Node::shouldSpeculateNumber(m_graph[node.child1()], m_graph[node.child2()]))
                break;
            fixDoubleEdge(0);
            fixDoubleEdge(1);
            break;
        }
            
        case ArithSub: {
            if (m_graph.addShouldSpeculateInteger(node)
                && node.canSpeculateInteger())
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
            if (Node::shouldSpeculateInteger(m_graph[node.child1()], m_graph[node.child2()])
                && node.canSpeculateInteger())
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
            if (Node::shouldSpeculateInteger(m_graph[node.child1()], m_graph[node.child2()])
                && node.canSpeculateInteger()) {
                if (isX86())
                    break;
                fixDoubleEdge(0);
                fixDoubleEdge(1);
                
                Node& oldDivision = m_graph[m_compileIndex];
                
                Node newDivision = oldDivision;
                newDivision.setRefCount(2);
                newDivision.predict(SpecDouble);
                NodeIndex newDivisionIndex = m_graph.size();
                
                oldDivision.setOp(DoubleAsInt32);
                oldDivision.children.initialize(Edge(newDivisionIndex, DoubleUse), Edge(), Edge());
                
                m_graph.append(newDivision);
                m_insertionSet.append(m_indexInBlock, newDivisionIndex);
                
                break;
            }
            fixDoubleEdge(0);
            fixDoubleEdge(1);
            break;
        }
            
        case ArithAbs: {
            if (m_graph[node.child1()].shouldSpeculateInteger()
                && node.canSpeculateInteger())
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
            node.setArrayMode(
                refineArrayMode(
                    node.arrayMode(), m_graph[child1].prediction(), m_graph[child2].prediction()));
            
            switch (modeForPut(node.arrayMode())) {
            case Array::Int8Array:
            case Array::Int16Array:
            case Array::Int32Array:
            case Array::Uint8Array:
            case Array::Uint8ClampedArray:
            case Array::Uint16Array:
            case Array::Uint32Array:
                if (!m_graph[child3].shouldSpeculateInteger())
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
            
        default:
            break;
        }

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        if (!(node.flags() & NodeHasVarArgs)) {
            dataLog("new children: ");
            node.dumpChildren(WTF::dataFile());
        }
        dataLog("\n");
#endif
    }
    
    void fixIntEdge(Edge& edge)
    {
        Node& node = m_graph[edge];
        if (node.op() != ValueToInt32)
            return;
        
        if (!m_graph[node.child1()].shouldSpeculateInteger())
            return;
        
        Edge oldEdge = edge;
        Edge newEdge = node.child1();
        
        m_graph.ref(newEdge);
        m_graph.deref(oldEdge);
        
        edge = newEdge;
    }
    
    void fixDoubleEdge(unsigned childIndex)
    {
        Node& source = m_graph[m_compileIndex];
        Edge& edge = m_graph.child(source, childIndex);
        
        if (!m_graph[edge].shouldSpeculateInteger()) {
            edge.setUseKind(DoubleUse);
            return;
        }
        
        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("(replacing @%u->@%u with @%u->@%u) ",
                m_compileIndex, edge.index(), m_compileIndex, resultIndex);
#endif
        
        // Fix the edge up here because it's a reference that will be clobbered by
        // the append() below.
        NodeIndex oldIndex = edge.index();
        edge = Edge(resultIndex, DoubleUse);

        m_graph.append(Node(Int32ToDouble, source.codeOrigin, oldIndex));
        m_insertionSet.append(m_indexInBlock, resultIndex);
        
        Node& int32ToDouble = m_graph[resultIndex];
        int32ToDouble.predict(SpecDouble);
        int32ToDouble.ref();
    }
    
    unsigned m_indexInBlock;
    NodeIndex m_compileIndex;
    InsertionSet<NodeIndex> m_insertionSet;
};
    
bool performFixup(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Fixup Phase");
    return runPhase<FixupPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

