/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#include "ArrayPrototype.h"
#include "CacheableIdentifierInlines.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "GetterSetter.h"
#include "JSCInlines.h"
#include "TypeLocation.h"

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
        ASSERT(m_graph.m_fixpointState == BeforeFixpoint);
        ASSERT(m_graph.m_form == ThreadedCPS);
        
        m_profitabilityChanged = false;
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex)
            fixupBlock(m_graph.block(blockIndex));
        
        while (m_profitabilityChanged) {
            m_profitabilityChanged = false;
            
            for (unsigned i = m_graph.m_argumentPositions.size(); i--;)
                m_graph.m_argumentPositions[i].mergeArgumentUnboxingAwareness();
            
            for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex)
                fixupGetAndSetLocalsInBlock(m_graph.block(blockIndex));
        }
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex)
            fixupChecksInBlock(m_graph.block(blockIndex));

        m_graph.m_planStage = PlanStage::AfterFixup;

        return true;
    }

private:

    void fixupArithDivInt32(Node* node, Edge& leftChild, Edge& rightChild)
    {
        if (optimizeForX86() || optimizeForARM64() || optimizeForARMv7IDIVSupported()) {
            fixIntOrBooleanEdge(leftChild);
            fixIntOrBooleanEdge(rightChild);
            if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                node->setArithMode(Arith::Unchecked);
            else if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                node->setArithMode(Arith::CheckOverflow);
            else
                node->setArithMode(Arith::CheckOverflowAndNegativeZero);
            return;
        }

        // This will cause conversion nodes to be inserted later.
        fixDoubleOrBooleanEdge(leftChild);
        fixDoubleOrBooleanEdge(rightChild);

        // We don't need to do ref'ing on the children because we're stealing them from
        // the original division.
        Node* newDivision = m_insertionSet.insertNode(m_indexInBlock, SpecBytecodeDouble, *node);
        newDivision->setResult(NodeResultDouble);

        node->setOp(DoubleAsInt32);
        node->children.initialize(Edge(newDivision, DoubleRepUse), Edge(), Edge());
        if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
            node->setArithMode(Arith::CheckOverflow);
        else
            node->setArithMode(Arith::CheckOverflowAndNegativeZero);

    }

    void fixupArithPow(Node* node)
    {
        if (node->child2()->shouldSpeculateInt32OrBooleanForArithmetic()) {
            fixDoubleOrBooleanEdge(node->child1());
            fixIntOrBooleanEdge(node->child2());
            return;
        }

        fixDoubleOrBooleanEdge(node->child1());
        fixDoubleOrBooleanEdge(node->child2());
    }

    void fixupArithDiv(Node* node, Edge& leftChild, Edge& rightChild)
    {
        if (m_graph.binaryArithShouldSpeculateInt32(node, FixupPass)) {
            fixupArithDivInt32(node, leftChild, rightChild);
            return;
        }
        
        fixDoubleOrBooleanEdge(leftChild);
        fixDoubleOrBooleanEdge(rightChild);
        node->setResult(NodeResultDouble);
    }
    
    void fixupArithMul(Node* node, Edge& leftChild, Edge& rightChild)
    {
        if (m_graph.binaryArithShouldSpeculateInt32(node, FixupPass)) {
            fixIntOrBooleanEdge(leftChild);
            fixIntOrBooleanEdge(rightChild);
            if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                node->setArithMode(Arith::Unchecked);
            else if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()) || leftChild.node() == rightChild.node())
                node->setArithMode(Arith::CheckOverflow);
            else
                node->setArithMode(Arith::CheckOverflowAndNegativeZero);
            return;
        }
        if (m_graph.binaryArithShouldSpeculateInt52(node, FixupPass)) {
            fixEdge<Int52RepUse>(leftChild);
            fixEdge<Int52RepUse>(rightChild);
            if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()) || leftChild.node() == rightChild.node())
                node->setArithMode(Arith::CheckOverflow);
            else
                node->setArithMode(Arith::CheckOverflowAndNegativeZero);
            node->setResult(NodeResultInt52);
            return;
        }

        fixDoubleOrBooleanEdge(leftChild);
        fixDoubleOrBooleanEdge(rightChild);
        node->setResult(NodeResultDouble);
    }

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
        NodeType op = node->op();

#if ASSERT_ENABLED
        bool usedToClobberExitState = clobbersExitState(m_graph, node);
#endif

        switch (op) {
        case SetLocal: {
            // This gets handled by fixupGetAndSetLocalsInBlock().
            return;
        }

        case Inc:
        case Dec: {
            if (node->child1()->shouldSpeculateBigInt()) {
                if (node->child1()->shouldSpeculateHeapBigInt()) {
                    // FIXME: the freezing does not appear useful (since the JSCell is kept alive by vm), but it refuses to compile otherwise.
                    // FIXME: we might optimize inc/dec to a specialized function call instead in that case.
                    node->setOp(op == Inc ? ValueAdd : ValueSub);
                    Node* nodeConstantOne = m_insertionSet.insertNode(m_indexInBlock, SpecHeapBigInt, JSConstant, node->origin, OpInfo(m_graph.freeze(vm().heapBigIntConstantOne.get())));
                    node->children.setChild2(Edge(nodeConstantOne));
                    fixEdge<HeapBigIntUse>(node->child1());
                    fixEdge<HeapBigIntUse>(node->child2());
                    // HeapBigInts are cells, so the default of NodeResultJS is good here
                    break;
                }
#if USE(BIGINT32)
                if (m_graph.unaryArithShouldSpeculateBigInt32(node, FixupPass)) {
                    node->setOp(op == Inc ? ValueAdd : ValueSub);
                    Node* nodeConstantOne = m_insertionSet.insertNode(m_indexInBlock, SpecBigInt32, JSConstant, node->origin, OpInfo(m_graph.freeze(jsBigInt32(1))));
                    node->children.setChild2(Edge(nodeConstantOne));
                    fixEdge<BigInt32Use>(node->child1());
                    fixEdge<BigInt32Use>(node->child2());
                    // The default of NodeResultJS is good enough for now.
                    // FIXME: consider having a special representation for small BigInts instead.
                    break;
                }

                // FIXME: the freezing does not appear useful (since the JSCell is kept alive by vm), but it refuses to compile otherwise.
                // FIXME: we might optimize inc/dec to a specialized function call instead in that case.
                node->setOp(op == Inc ? ValueAdd : ValueSub);
                Node* nodeConstantOne = m_insertionSet.insertNode(m_indexInBlock, SpecBigInt32, JSConstant, node->origin, OpInfo(m_graph.freeze(jsBigInt32(1))));
                node->children.setChild2(Edge(nodeConstantOne));
                fixEdge<AnyBigIntUse>(node->child1());
                fixEdge<AnyBigIntUse>(node->child2());
                // The default of NodeResultJS is good here
#endif // USE(BIGINT32)
                break;
            }

            if (node->child1()->shouldSpeculateUntypedForArithmetic()) {
                fixEdge<UntypedUse>(node->child1());
                break;
            }

            Node* nodeConstantOne;
            if (node->child1()->shouldSpeculateInt32OrBoolean() && node->canSpeculateInt32(FixupPass)) {
                node->setOp(op == Inc ? ArithAdd : ArithSub);
                node->setArithMode(Arith::CheckOverflow);
                nodeConstantOne = m_insertionSet.insertNode(m_indexInBlock, SpecInt32Only, JSConstant, node->origin, OpInfo(m_graph.freeze(jsNumber(1))));
                node->children.setChild2(Edge(nodeConstantOne));
                fixEdge<Int32Use>(node->child1());
                fixEdge<Int32Use>(node->child2());
                node->setResult(NodeResultInt32);
            } else if (node->child1()->shouldSpeculateInt52()) {
                node->setOp(op == Inc ? ArithAdd : ArithSub);
                node->setArithMode(Arith::CheckOverflow);
                nodeConstantOne = m_insertionSet.insertNode(m_indexInBlock, SpecInt32AsInt52, JSConstant, node->origin, OpInfo(m_graph.freeze(jsNumber(1))));
                node->children.setChild2(Edge(nodeConstantOne));
                fixEdge<Int52RepUse>(node->child1());
                fixEdge<Int52RepUse>(node->child2());
                node->setResult(NodeResultInt52);
            } else {
                node->setOp(op == Inc ? ArithAdd : ArithSub);
                node->setArithMode(Arith::Unchecked);
                nodeConstantOne = m_insertionSet.insertNode(m_indexInBlock, SpecBytecodeDouble, JSConstant, node->origin, OpInfo(m_graph.freeze(jsNumber(1))));
                node->children.setChild2(Edge(nodeConstantOne));
                fixEdge<DoubleRepUse>(node->child1());
                fixEdge<DoubleRepUse>(node->child2());
                node->setResult(NodeResultDouble);
            }
            node->clearFlags(NodeMustGenerate);
            break;
        }

        case ValueSub: {
            Edge& child1 = node->child1();
            Edge& child2 = node->child2();

            if (Node::shouldSpeculateHeapBigInt(child1.node(), child2.node())) {
                fixEdge<HeapBigIntUse>(child1);
                fixEdge<HeapBigIntUse>(child2);
                break;
            }

#if USE(BIGINT32)
            if (m_graph.binaryArithShouldSpeculateBigInt32(node, FixupPass)) {
                fixEdge<BigInt32Use>(child1);
                fixEdge<BigInt32Use>(child2);
                break;
            }

            if (Node::shouldSpeculateBigInt(child1.node(), child2.node())) {
                fixEdge<AnyBigIntUse>(child1);
                fixEdge<AnyBigIntUse>(child2);
                break;
            }
#endif

            if (Node::shouldSpeculateUntypedForArithmetic(node->child1().node(), node->child2().node())) {
                fixEdge<UntypedUse>(child1);
                fixEdge<UntypedUse>(child2);
                break;
            }

            if (attemptToMakeIntegerAdd(node)) {
                // FIXME: Clear ArithSub's NodeMustGenerate when ArithMode is unchecked
                // https://bugs.webkit.org/show_bug.cgi?id=190607
                node->setOp(ArithSub);
                break;
            }

            fixDoubleOrBooleanEdge(node->child1());
            fixDoubleOrBooleanEdge(node->child2());
            node->setOp(ArithSub);
            node->setResult(NodeResultDouble);

            break;
        }

        case ValueBitLShift:
        case ValueBitRShift:
        case ValueBitXor:
        case ValueBitOr:
        case ValueBitAnd: {
            if (Node::shouldSpeculateBigInt(node->child1().node(), node->child2().node())) {
#if USE(BIGINT32)
                if (Node::shouldSpeculateBigInt32(node->child1().node(), node->child2().node())) {
                    fixEdge<BigInt32Use>(node->child1());
                    fixEdge<BigInt32Use>(node->child2());
                } else if (Node::shouldSpeculateHeapBigInt(node->child1().node(), node->child2().node())) {
                    fixEdge<HeapBigIntUse>(node->child1());
                    fixEdge<HeapBigIntUse>(node->child2());
                } else {
                    fixEdge<AnyBigIntUse>(node->child1());
                    fixEdge<AnyBigIntUse>(node->child2());
                }
#else
                fixEdge<HeapBigIntUse>(node->child1());
                fixEdge<HeapBigIntUse>(node->child2());
#endif
                node->clearFlags(NodeMustGenerate);
                break;
            }

            if (Node::shouldSpeculateUntypedForBitOps(node->child1().node(), node->child2().node())) {
                fixEdge<UntypedUse>(node->child1());
                fixEdge<UntypedUse>(node->child2());
                break;
            }
            
            switch (op) {
            case ValueBitXor:
                node->setOp(ArithBitXor);
                break;
            case ValueBitOr:
                node->setOp(ArithBitOr);
                break;
            case ValueBitAnd:
                node->setOp(ArithBitAnd);
                break;
            case ValueBitLShift:
                node->setOp(ArithBitLShift);
                break;
            case ValueBitRShift:
                node->setOp(ArithBitRShift);
                break;
            default:
                DFG_CRASH(m_graph, node, "Unexpected node during ValueBit operation fixup");
                break;
            }

            node->clearFlags(NodeMustGenerate);
            node->setResult(NodeResultInt32);
            fixIntConvertingEdge(node->child1());
            fixIntConvertingEdge(node->child2());
            break;
        }

        case ValueBitNot: {
            Edge& operandEdge = node->child1();

            if (operandEdge.node()->shouldSpeculateBigInt()) {
                node->clearFlags(NodeMustGenerate);

#if USE(BIGINT32)
                if (operandEdge.node()->shouldSpeculateBigInt32())
                    fixEdge<BigInt32Use>(operandEdge);
                else if (operandEdge.node()->shouldSpeculateHeapBigInt())
                    fixEdge<HeapBigIntUse>(operandEdge);
                else
                    fixEdge<AnyBigIntUse>(operandEdge);
#else
                fixEdge<HeapBigIntUse>(operandEdge);
#endif
            } else if (operandEdge.node()->shouldSpeculateUntypedForBitOps())
                fixEdge<UntypedUse>(operandEdge);
            else {
                node->setOp(ArithBitNot);
                node->setResult(NodeResultInt32);
                node->clearFlags(NodeMustGenerate);
                fixIntConvertingEdge(operandEdge);
            }
            break;
        }

        case ArithBitNot: {
            Edge& operandEdge = node->child1();

            fixIntConvertingEdge(operandEdge);
            break;
        }

        case ArithBitRShift:
        case ArithBitLShift: 
        case ArithBitXor:
        case ArithBitOr:
        case ArithBitAnd: {
            fixIntConvertingEdge(node->child1());
            fixIntConvertingEdge(node->child2());
            break;
        }

        case BitURShift: {
            if (Node::shouldSpeculateUntypedForBitOps(node->child1().node(), node->child2().node())) {
                fixEdge<UntypedUse>(node->child1());
                fixEdge<UntypedUse>(node->child2());
                break;
            }
            fixIntConvertingEdge(node->child1());
            fixIntConvertingEdge(node->child2());
            break;
        }

        case ArithIMul: {
            fixIntConvertingEdge(node->child1());
            fixIntConvertingEdge(node->child2());
            node->setOp(ArithMul);
            node->setArithMode(Arith::Unchecked);
            node->child1().setUseKind(Int32Use);
            node->child2().setUseKind(Int32Use);
            break;
        }

        case ArithClz32: {
            if (node->child1()->shouldSpeculateNotCellNorBigInt()) {
                fixIntConvertingEdge(node->child1());
                node->clearFlags(NodeMustGenerate);
            } else
                fixEdge<UntypedUse>(node->child1());
            break;
        }
            
        case UInt32ToNumber: {
            fixIntConvertingEdge(node->child1());
            if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                node->convertToIdentity();
            else if (node->canSpeculateInt32(FixupPass))
                node->setArithMode(Arith::CheckOverflow);
            else {
                node->setArithMode(Arith::DoOverflow);
                node->setResult(enableInt52() ? NodeResultInt52 : NodeResultDouble);
            }
            break;
        }

        case ValueNegate: {
            if (node->child1()->shouldSpeculateInt32OrBoolean() && node->canSpeculateInt32(FixupPass)) {
                node->setOp(ArithNegate);
                fixIntOrBooleanEdge(node->child1());
                if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                    node->setArithMode(Arith::Unchecked);
                else if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                node->setResult(NodeResultInt32);
                node->clearFlags(NodeMustGenerate);
                break;
            }
            
            if (m_graph.unaryArithShouldSpeculateInt52(node, FixupPass)) {
                node->setOp(ArithNegate);
                fixEdge<Int52RepUse>(node->child1());
                if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                node->setResult(NodeResultInt52);
                node->clearFlags(NodeMustGenerate);
                break;
            }
            if (node->child1()->shouldSpeculateNotCellNorBigInt()) {
                node->setOp(ArithNegate);
                fixDoubleOrBooleanEdge(node->child1());
                node->setResult(NodeResultDouble);
                node->clearFlags(NodeMustGenerate);
            } else {
                fixEdge<UntypedUse>(node->child1());
                node->setResult(NodeResultJS);
            }
            break;
        }

        case ValueAdd: {
            if (attemptToMakeIntegerAdd(node)) {
                node->setOp(ArithAdd);
                break;
            }
            if (Node::shouldSpeculateNumberOrBooleanExpectingDefined(node->child1().node(), node->child2().node())) {
                fixDoubleOrBooleanEdge(node->child1());
                fixDoubleOrBooleanEdge(node->child2());
                node->setOp(ArithAdd);
                node->setResult(NodeResultDouble);
                break;
            }
            
            if (attemptToMakeFastStringAdd(node))
                break;

            Edge& child1 = node->child1();
            Edge& child2 = node->child2();
            if (child1->shouldSpeculateString() || child2->shouldSpeculateString()) {
                if (child1->shouldSpeculateInt32() || child2->shouldSpeculateInt32()) {
                    auto convertString = [&](Node* node, Edge& edge) {
                        if (edge->shouldSpeculateInt32())
                            convertStringAddUse<Int32Use>(node, edge);
                        else {
                            ASSERT(edge->shouldSpeculateString());
                            convertStringAddUse<StringUse>(node, edge);
                        }
                    };
                    convertString(node, child1);
                    convertString(node, child2);
                    convertToMakeRope(node);
                    break;
                }
            }

            if (Node::shouldSpeculateHeapBigInt(child1.node(), child2.node())) {
                fixEdge<HeapBigIntUse>(child1);
                fixEdge<HeapBigIntUse>(child2);
#if USE(BIGINT32)
            } else if (m_graph.binaryArithShouldSpeculateBigInt32(node, FixupPass)) {
                fixEdge<BigInt32Use>(child1);
                fixEdge<BigInt32Use>(child2);
            } else if (Node::shouldSpeculateBigInt(child1.node(), child2.node())) {
                fixEdge<AnyBigIntUse>(child1);
                fixEdge<AnyBigIntUse>(child2);
#endif
            } else {
                fixEdge<UntypedUse>(child1);
                fixEdge<UntypedUse>(child2);
            }

            node->setResult(NodeResultJS);
            break;
        }

        case StrCat: {
            if (attemptToMakeFastStringAdd(node))
                break;

            // FIXME: Remove empty string arguments and possibly turn this into a ToString operation. That
            // would require a form of ToString that takes a KnownPrimitiveUse. This is necessary because
            // the implementation of StrCat doesn't dynamically optimize for empty strings.
            // https://bugs.webkit.org/show_bug.cgi?id=148540
            m_graph.doToChildren(
                node,
                [&] (Edge& edge) {
                    fixEdge<KnownPrimitiveUse>(edge);
                    // StrCat automatically coerces the values into strings before concatenating them.
                    // The ECMA spec says that we're not allowed to automatically coerce a Symbol into
                    // a string. If a Symbol is encountered, a TypeError will be thrown. As a result,
                    // our runtime functions for this slow path expect that they will never be passed
                    // Symbols.
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, Check, node->origin,
                        Edge(edge.node(), NotSymbolUse));
                });
            break;
        }
            
        case MakeRope: {
            fixupMakeRope(node);
            break;
        }
            
        case ArithAdd:
        case ArithSub: {
            // FIXME: Clear ArithSub's NodeMustGenerate when ArithMode is unchecked
            // https://bugs.webkit.org/show_bug.cgi?id=190607
            if (attemptToMakeIntegerAdd(node))
                break;
            fixDoubleOrBooleanEdge(node->child1());
            fixDoubleOrBooleanEdge(node->child2());
            node->setResult(NodeResultDouble);
            break;
        }
            
        case ArithNegate: {
            if (node->child1()->shouldSpeculateInt32OrBoolean() && node->canSpeculateInt32(FixupPass)) {
                fixIntOrBooleanEdge(node->child1());
                if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                    node->setArithMode(Arith::Unchecked);
                else if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                node->setResult(NodeResultInt32);
                node->clearFlags(NodeMustGenerate);
                break;
            }
            if (m_graph.unaryArithShouldSpeculateInt52(node, FixupPass)) {
                fixEdge<Int52RepUse>(node->child1());
                if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                node->setResult(NodeResultInt52);
                node->clearFlags(NodeMustGenerate);
                break;
            }

            fixDoubleOrBooleanEdge(node->child1());
            node->setResult(NodeResultDouble);
            node->clearFlags(NodeMustGenerate);
            break;
        }

        case ValueMul: {
            Edge& leftChild = node->child1();
            Edge& rightChild = node->child2();

            if (Node::shouldSpeculateBigInt(leftChild.node(), rightChild.node())) {
#if USE(BIGINT32)
                if (m_graph.binaryArithShouldSpeculateBigInt32(node, FixupPass)) {
                    fixEdge<BigInt32Use>(node->child1());
                    fixEdge<BigInt32Use>(node->child2());
                } else if (Node::shouldSpeculateHeapBigInt(leftChild.node(), rightChild.node())) {
                    fixEdge<HeapBigIntUse>(node->child1());
                    fixEdge<HeapBigIntUse>(node->child2());
                } else {
                    fixEdge<AnyBigIntUse>(node->child1());
                    fixEdge<AnyBigIntUse>(node->child2());
                }
#else
                fixEdge<HeapBigIntUse>(node->child1());
                fixEdge<HeapBigIntUse>(node->child2());
#endif
                node->clearFlags(NodeMustGenerate);
                break;
            }

            // There are cases where we can have BigInt + Int32 operands reaching ValueMul.
            // Imagine the scenario where ValueMul was never executed, but we can predict types
            // reaching the node:
            //
            // 63: GetLocal(Check:Untyped:@72, JS|MustGen, NonBoolInt32, ...)  predicting NonBoolInt32
            // 64: GetLocal(Check:Untyped:@71, JS|MustGen, BigInt, ...)  predicting BigInt
            // 65: ValueMul(Check:Untyped:@63, Check:Untyped:@64, BigInt|BoolInt32|NonBoolInt32, ...)
            // 
            // In such scenario, we need to emit ValueMul(Untyped, Untyped), so the runtime can throw 
            // an exception whenever it gets excuted.
            if (Node::shouldSpeculateUntypedForArithmetic(leftChild.node(), rightChild.node())) {
                fixEdge<UntypedUse>(leftChild);
                fixEdge<UntypedUse>(rightChild);
                break;
            }

            // At this point, all other possible specializations are only handled by ArithMul.
            node->setOp(ArithMul);
            node->setResult(NodeResultNumber);
            fixupArithMul(node, leftChild, rightChild);
            break;
        }

        case ArithMul: {
            Edge& leftChild = node->child1();
            Edge& rightChild = node->child2();

            fixupArithMul(node, leftChild, rightChild);
            break;
        }

        case ValueMod: 
        case ValueDiv: {
            Edge& leftChild = node->child1();
            Edge& rightChild = node->child2();

            if (Node::shouldSpeculateHeapBigInt(leftChild.node(), rightChild.node())) {
                fixEdge<HeapBigIntUse>(leftChild);
                fixEdge<HeapBigIntUse>(rightChild);
                node->clearFlags(NodeMustGenerate);
                break;
            }
#if USE(BIGINT32)
            if (Node::shouldSpeculateBigInt(leftChild.node(), rightChild.node())) {
                fixEdge<AnyBigIntUse>(leftChild);
                fixEdge<AnyBigIntUse>(rightChild);
                node->clearFlags(NodeMustGenerate);
                break; 
            }
#endif

            if (Node::shouldSpeculateUntypedForArithmetic(leftChild.node(), rightChild.node())) {
                fixEdge<UntypedUse>(leftChild);
                fixEdge<UntypedUse>(rightChild);
                break;
            }

            if (op == ValueDiv)
                node->setOp(ArithDiv);
            else
                node->setOp(ArithMod);

            node->setResult(NodeResultNumber);
            fixupArithDiv(node, leftChild, rightChild);
            break;

        }

        case ArithDiv:
        case ArithMod: {
            Edge& leftChild = node->child1();
            Edge& rightChild = node->child2();

            fixupArithDiv(node, leftChild, rightChild);
            break;
        }
            
        case ArithMin:
        case ArithMax: {
            if (m_graph.binaryArithShouldSpeculateInt32(node, FixupPass)) {
                fixIntOrBooleanEdge(node->child1());
                fixIntOrBooleanEdge(node->child2());
                break;
            }
            fixDoubleOrBooleanEdge(node->child1());
            fixDoubleOrBooleanEdge(node->child2());
            node->setResult(NodeResultDouble);
            break;
        }
            
        case ArithAbs: {
            if (node->child1()->shouldSpeculateInt32OrBoolean()
                && node->canSpeculateInt32(FixupPass)) {
                fixIntOrBooleanEdge(node->child1());
                if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                    node->setArithMode(Arith::Unchecked);
                else
                    node->setArithMode(Arith::CheckOverflow);
                node->clearFlags(NodeMustGenerate);
                node->setResult(NodeResultInt32);
                break;
            }

            if (node->child1()->shouldSpeculateNotCellNorBigInt()) {
                fixDoubleOrBooleanEdge(node->child1());
                node->clearFlags(NodeMustGenerate);
            } else
                fixEdge<UntypedUse>(node->child1());
            node->setResult(NodeResultDouble);
            break;
        }

        case ValuePow: {
            if (Node::shouldSpeculateHeapBigInt(node->child1().node(), node->child2().node())) {
                fixEdge<HeapBigIntUse>(node->child1());
                fixEdge<HeapBigIntUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }
#if USE(BIGINT32)
            if (Node::shouldSpeculateBigInt(node->child1().node(), node->child2().node())) {
                fixEdge<AnyBigIntUse>(node->child1());
                fixEdge<AnyBigIntUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }
#endif

            if (Node::shouldSpeculateUntypedForArithmetic(node->child1().node(), node->child2().node())) {
                fixEdge<UntypedUse>(node->child1());
                fixEdge<UntypedUse>(node->child2());
                break;
            }

            node->setOp(ArithPow);
            node->clearFlags(NodeMustGenerate);
            node->setResult(NodeResultDouble);

            fixupArithPow(node);
            break;
        }

        case ArithPow: {
            fixupArithPow(node);
            break;
        }

        case ArithRandom: {
            node->setResult(NodeResultDouble);
            break;
        }

        case ArithRound:
        case ArithFloor:
        case ArithCeil:
        case ArithTrunc: {
            if (node->child1()->shouldSpeculateInt32OrBoolean() && m_graph.roundShouldSpeculateInt32(node, FixupPass)) {
                fixIntOrBooleanEdge(node->child1());
                insertCheck<Int32Use>(node->child1().node());
                node->convertToIdentity();
                break;
            }
            if (node->child1()->shouldSpeculateNotCellNorBigInt()) {
                fixDoubleOrBooleanEdge(node->child1());

                if (isInt32OrBooleanSpeculation(node->getHeapPrediction()) && m_graph.roundShouldSpeculateInt32(node, FixupPass)) {
                    node->setResult(NodeResultInt32);
                    if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                        node->setArithRoundingMode(Arith::RoundingMode::Int32);
                    else
                        node->setArithRoundingMode(Arith::RoundingMode::Int32WithNegativeZeroCheck);
                } else {
                    node->setResult(NodeResultDouble);
                    node->setArithRoundingMode(Arith::RoundingMode::Double);
                }
                node->clearFlags(NodeMustGenerate);
            } else
                fixEdge<UntypedUse>(node->child1());
            break;
        }

        case ArithFRound:
        case ArithSqrt:
        case ArithUnary: {
            Edge& child1 = node->child1();
            if (child1->shouldSpeculateNotCellNorBigInt()) {
                fixDoubleOrBooleanEdge(child1);
                node->clearFlags(NodeMustGenerate);
            } else
                fixEdge<UntypedUse>(child1);
            break;
        }
            
        case LogicalNot: {
            if (node->child1()->shouldSpeculateBoolean()) {
                if (node->child1()->result() == NodeResultBoolean) {
                    // This is necessary in case we have a bytecode instruction implemented by:
                    //
                    // a: CompareEq(...)
                    // b: LogicalNot(@a)
                    //
                    // In that case, CompareEq might have a side-effect. Then, we need to make
                    // sure that we know that Branch does not exit.
                    fixEdge<KnownBooleanUse>(node->child1());
                } else
                    fixEdge<BooleanUse>(node->child1());
            } else if (node->child1()->shouldSpeculateObjectOrOther())
                fixEdge<ObjectOrOtherUse>(node->child1());
            else if (node->child1()->shouldSpeculateInt32OrBoolean())
                fixIntOrBooleanEdge(node->child1());
            else if (node->child1()->shouldSpeculateNumber())
                fixEdge<DoubleRepUse>(node->child1());
            else if (node->child1()->shouldSpeculateString())
                fixEdge<StringUse>(node->child1());
            else if (node->child1()->shouldSpeculateStringOrOther())
                fixEdge<StringOrOtherUse>(node->child1());
            else {
                WatchpointSet* masqueradesAsUndefinedWatchpoint = m_graph.globalObjectFor(node->origin.semantic)->masqueradesAsUndefinedWatchpoint();
                if (masqueradesAsUndefinedWatchpoint->isStillValid())
                    m_graph.watchpoints().addLazily(masqueradesAsUndefinedWatchpoint);
            }
            break;
        }

        case CompareEq:
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq: {
            if (node->op() == CompareEq
                && Node::shouldSpeculateBoolean(node->child1().node(), node->child2().node())) {
                fixEdge<BooleanUse>(node->child1());
                fixEdge<BooleanUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }
            if (Node::shouldSpeculateInt32OrBoolean(node->child1().node(), node->child2().node())) {
                fixIntOrBooleanEdge(node->child1());
                fixIntOrBooleanEdge(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }
            if (Node::shouldSpeculateInt52(node->child1().node(), node->child2().node())) {
                fixEdge<Int52RepUse>(node->child1());
                fixEdge<Int52RepUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }
            if (Node::shouldSpeculateHeapBigInt(node->child1().node(), node->child2().node())) {
                fixEdge<HeapBigIntUse>(node->child1());
                fixEdge<HeapBigIntUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                return;
            }
#if USE(BIGINT32)
            if (Node::shouldSpeculateBigInt32(node->child1().node(), node->child2().node())) {
                fixEdge<BigInt32Use>(node->child1());
                fixEdge<BigInt32Use>(node->child2());
                node->clearFlags(NodeMustGenerate);
                return;
            }
            if (Node::shouldSpeculateBigInt(node->child1().node(), node->child2().node())) {
                fixEdge<AnyBigIntUse>(node->child1());
                fixEdge<AnyBigIntUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                return;
            }
#endif
            if (Node::shouldSpeculateNumberOrBoolean(node->child1().node(), node->child2().node())) {
                fixDoubleOrBooleanEdge(node->child1());
                fixDoubleOrBooleanEdge(node->child2());
            }

            // FIXME: We can convert BigInt32 to Double in Compare nodes since they do not require ToNumber semantics.
            // https://bugs.webkit.org/show_bug.cgi?id=211407
            if (node->op() != CompareEq
                && node->child1()->shouldSpeculateNotCellNorBigInt()
                && node->child2()->shouldSpeculateNotCellNorBigInt()) {
                if (node->child1()->shouldSpeculateNumberOrBoolean())
                    fixDoubleOrBooleanEdge(node->child1());
                else
                    fixEdge<DoubleRepUse>(node->child1());
                if (node->child2()->shouldSpeculateNumberOrBoolean())
                    fixDoubleOrBooleanEdge(node->child2());
                else
                    fixEdge<DoubleRepUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }
            if (node->child1()->shouldSpeculateStringIdent() && node->child2()->shouldSpeculateStringIdent()) {
                fixEdge<StringIdentUse>(node->child1());
                fixEdge<StringIdentUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }
            if (node->child1()->shouldSpeculateString() && node->child2()->shouldSpeculateString() && GPRInfo::numberOfRegisters >= 7) {
                fixEdge<StringUse>(node->child1());
                fixEdge<StringUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }

            if (node->op() != CompareEq)
                break;
            if (Node::shouldSpeculateSymbol(node->child1().node(), node->child2().node())) {
                fixEdge<SymbolUse>(node->child1());
                fixEdge<SymbolUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }
            if (node->child1()->shouldSpeculateObject() && node->child2()->shouldSpeculateObject()) {
                fixEdge<ObjectUse>(node->child1());
                fixEdge<ObjectUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }

            // If either child can be proved to be Null or Undefined, comparing them is greatly simplified.
            bool oneArgumentIsUsedAsSpecOther = false;
            if (node->child1()->isUndefinedOrNullConstant()) {
                fixEdge<KnownOtherUse>(node->child1());
                oneArgumentIsUsedAsSpecOther = true;
            } else if (node->child1()->shouldSpeculateOther()) {
                m_insertionSet.insertNode(m_indexInBlock, SpecNone, Check, node->origin,
                    Edge(node->child1().node(), OtherUse));
                fixEdge<KnownOtherUse>(node->child1());
                oneArgumentIsUsedAsSpecOther = true;
            }
            if (node->child2()->isUndefinedOrNullConstant()) {
                fixEdge<KnownOtherUse>(node->child2());
                oneArgumentIsUsedAsSpecOther = true;
            } else if (node->child2()->shouldSpeculateOther()) {
                m_insertionSet.insertNode(m_indexInBlock, SpecNone, Check, node->origin,
                    Edge(node->child2().node(), OtherUse));
                fixEdge<KnownOtherUse>(node->child2());
                oneArgumentIsUsedAsSpecOther = true;
            }
            if (oneArgumentIsUsedAsSpecOther) {
                node->clearFlags(NodeMustGenerate);
                break;
            }

            if (node->child1()->shouldSpeculateObject() && node->child2()->shouldSpeculateObjectOrOther()) {
                fixEdge<ObjectUse>(node->child1());
                fixEdge<ObjectOrOtherUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }
            if (node->child1()->shouldSpeculateObjectOrOther() && node->child2()->shouldSpeculateObject()) {
                fixEdge<ObjectOrOtherUse>(node->child1());
                fixEdge<ObjectUse>(node->child2());
                node->clearFlags(NodeMustGenerate);
                break;
            }

            break;
        }
            
        case CompareStrictEq:
        case SameValue: {
            fixupCompareStrictEqAndSameValue(node);
            break;
        }

        case StringFromCharCode:
            if (node->child1()->shouldSpeculateInt32()) {
                fixEdge<Int32Use>(node->child1());
                node->clearFlags(NodeMustGenerate);
            } else
                fixEdge<UntypedUse>(node->child1());
            break;

        case StringCharAt:
        case StringCharCodeAt:
        case StringCodePointAt: {
            // Currently we have no good way of refining these.
            ASSERT(node->arrayMode() == ArrayMode(Array::String, Array::Read));
            blessArrayOperation(node->child1(), node->child2(), node->child3());
            fixEdge<KnownStringUse>(node->child1());
            fixEdge<Int32Use>(node->child2());
            break;
        }

        case GetByVal: {
            if (!node->prediction()) {
                m_insertionSet.insertNode(
                    m_indexInBlock, SpecNone, ForceOSRExit, node->origin);
            }

            {
                auto indexSpeculation = m_graph.varArgChild(node, 1)->prediction();
                if (!isInt32Speculation(indexSpeculation) 
                    && isFullNumberSpeculation(indexSpeculation)
                    && node->arrayMode().isSpecific()
                    && node->arrayMode().isInBounds()
                    && !m_graph.hasExitSite(node->origin.semantic, Overflow)) {

                    Node* newIndex = m_insertionSet.insertNode(
                        m_indexInBlock, SpecInt32Only, DoubleAsInt32, node->origin,
                        Edge(m_graph.varArgChild(node, 1).node(), DoubleRepUse));
                    newIndex->setArithMode(Arith::CheckOverflow);
                    m_graph.varArgChild(node, 1).setNode(newIndex);
                }
            }
            
            
            node->setArrayMode(
                node->arrayMode().refine(
                    m_graph, node,
                    m_graph.varArgChild(node, 0)->prediction(),
                    m_graph.varArgChild(node, 1)->prediction(),
                    SpecNone));

            switch (node->arrayMode().type()) {
            case Array::BigInt64Array:
            case Array::BigUint64Array:
                // Make it Array::Generic.
                // FIXME: Add BigInt64Array / BigUint64Array support.
                // https://bugs.webkit.org/show_bug.cgi?id=221172
                node->setArrayMode(ArrayMode(Array::Generic, node->arrayMode().action()));
                break;
            default:
                break;
            }

            blessArrayOperation(m_graph.varArgChild(node, 0), m_graph.varArgChild(node, 1), m_graph.varArgChild(node, 2));
            
            ArrayMode arrayMode = node->arrayMode();
            switch (arrayMode.type()) {
            case Array::Contiguous:
            case Array::Double:
            case Array::Int32: {
                Optional<Array::Speculation> saneChainSpeculation;
                if (arrayMode.isJSArrayWithOriginalStructure()) {
                    // Check if InBoundsSaneChain will work on a per-type basis. Note that:
                    //
                    // 1) We don't want double arrays to sometimes return undefined, since
                    // that would require a change to the return type and it would pessimise
                    // things a lot. So, we'd only want to do that if we actually had
                    // evidence that we could read from a hole. That's pretty annoying.
                    // Likely the best way to handle that case is with an equivalent of
                    // InBoundsSaneChain for OutOfBounds. For now we just detect when Undefined and
                    // NaN are indistinguishable according to backwards propagation, and just
                    // use InBoundsSaneChain in that case. This happens to catch a lot of cases.
                    //
                    // 2) We don't want int32 array loads to have to do a hole check just to
                    // coerce to Undefined, since that would mean twice the checks. We want to
                    // be able to say we always return Int32. FIXME: Maybe this should be profiling
                    // based?
                    //
                    // This has two implications. First, we have to do more checks than we'd
                    // like. It's unfortunate that we have to do the hole check. Second,
                    // some accesses that hit a hole will now need to take the full-blown
                    // out-of-bounds slow path. We can fix that with:
                    // https://bugs.webkit.org/show_bug.cgi?id=144668
                    
                    switch (arrayMode.type()) {
                    case Array::Int32:
                        if (is64Bit() && arrayMode.speculation() == Array::OutOfBounds && !m_graph.hasExitSite(node->origin.semantic, NegativeIndex))
                            saneChainSpeculation = Array::OutOfBoundsSaneChain;
                        break;
                    case Array::Contiguous:
                        // This is happens to be entirely natural. We already would have
                        // returned any JSValue, and now we'll return Undefined. We still do
                        // the check but it doesn't require taking any kind of slow path.
                        if (is64Bit() && arrayMode.speculation() == Array::OutOfBounds && !m_graph.hasExitSite(node->origin.semantic, NegativeIndex))
                            saneChainSpeculation = Array::OutOfBoundsSaneChain;
                        else if (arrayMode.speculation() == Array::InBounds)
                            saneChainSpeculation = Array::InBoundsSaneChain;
                        break;
                        
                    case Array::Double:
                        if (!(node->flags() & NodeBytecodeUsesAsOther) && arrayMode.speculation() == Array::InBounds) {
                            // Holes look like NaN already, so if the user doesn't care
                            // about the difference between Undefined and NaN then we can
                            // do this.
                            saneChainSpeculation = Array::InBoundsSaneChain;
                        } else if (is64Bit() && arrayMode.speculation() == Array::OutOfBounds && !m_graph.hasExitSite(node->origin.semantic, NegativeIndex))
                            saneChainSpeculation = Array::OutOfBoundsSaneChain;
                        break;
                        
                    default:
                        break;
                    }
                }

                if (saneChainSpeculation)
                    setSaneChainIfPossible(node, *saneChainSpeculation);

                break;
            }
                
            case Array::String:
                if ((node->prediction() & ~SpecString)
                    || m_graph.hasExitSite(node->origin.semantic, OutOfBounds))
                    node->setArrayMode(arrayMode.withSpeculation(Array::OutOfBounds));
                break;
                
            default:
                break;
            }
            
            arrayMode = node->arrayMode();
            switch (arrayMode.type()) {
            case Array::SelectUsingPredictions:
            case Array::Unprofiled:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            case Array::Generic:
                if (m_graph.varArgChild(node, 0)->shouldSpeculateObject()) {
                    if (m_graph.varArgChild(node, 1)->shouldSpeculateString()) {
                        fixEdge<ObjectUse>(m_graph.varArgChild(node, 0));
                        fixEdge<StringUse>(m_graph.varArgChild(node, 1));
                        break;
                    }

                    if (m_graph.varArgChild(node, 1)->shouldSpeculateSymbol()) {
                        fixEdge<ObjectUse>(m_graph.varArgChild(node, 0));
                        fixEdge<SymbolUse>(m_graph.varArgChild(node, 1));
                        break;
                    }
                }
#if USE(JSVALUE32_64)
                fixEdge<CellUse>(m_graph.varArgChild(node, 0)); // Speculating cell due to register pressure on 32-bit.
#endif
                break;
            case Array::ForceExit:
                break;
            case Array::String:
                fixEdge<KnownStringUse>(m_graph.varArgChild(node, 0));
                fixEdge<Int32Use>(m_graph.varArgChild(node, 1));
                break;
            default:
                fixEdge<KnownCellUse>(m_graph.varArgChild(node, 0));
                fixEdge<Int32Use>(m_graph.varArgChild(node, 1));
                break;
            }
            
            switch (arrayMode.type()) {
            case Array::Double:
                if (!arrayMode.isOutOfBounds()
                    || (arrayMode.isOutOfBoundsSaneChain() && !(node->flags() & NodeBytecodeUsesAsOther)))
                    node->setResult(NodeResultDouble);
                break;
                
            case Array::Float32Array:
            case Array::Float64Array:
                node->setResult(NodeResultDouble);
                break;
                
            case Array::Uint32Array:
                if (node->shouldSpeculateInt32())
                    break;
                if (node->shouldSpeculateInt52())
                    node->setResult(NodeResultInt52);
                else
                    node->setResult(NodeResultDouble);
                break;
                
            default:
                break;
            }
            
            break;
        }

        case PutByValDirect:
        case PutByVal:
        case PutByValAlias: {
            Edge& child1 = m_graph.varArgChild(node, 0);
            Edge& child2 = m_graph.varArgChild(node, 1);
            Edge& child3 = m_graph.varArgChild(node, 2);

            {
                auto indexSpeculation = child2->prediction();
                if (!isInt32Speculation(indexSpeculation) 
                    && isFullNumberSpeculation(indexSpeculation)
                    && node->arrayMode().isSpecific()
                    && node->arrayMode().isInBounds()
                    && !m_graph.hasExitSite(node->origin.semantic, Overflow)) {

                    Node* newIndex = m_insertionSet.insertNode(
                        m_indexInBlock, SpecInt32Only, DoubleAsInt32, node->origin,
                        Edge(child2.node(), DoubleRepUse));
                    newIndex->setArithMode(Arith::CheckOverflow);
                    child2.setNode(newIndex);
                }
            }

            node->setArrayMode(
                node->arrayMode().refine(
                    m_graph, node,
                    child1->prediction(),
                    child2->prediction(),
                    child3->prediction()));

            switch (node->arrayMode().type()) {
            case Array::BigInt64Array:
            case Array::BigUint64Array:
                // Make it Array::Generic.
                // FIXME: Add BigInt64Array / BigUint64Array support.
                // https://bugs.webkit.org/show_bug.cgi?id=221172
                node->setArrayMode(ArrayMode(Array::Generic, node->arrayMode().action()));
                break;
            default:
                break;
            }
            
            blessArrayOperation(child1, child2, m_graph.varArgChild(node, 3));
            
            switch (node->arrayMode().modeForPut().type()) {
            case Array::SelectUsingPredictions:
            case Array::SelectUsingArguments:
            case Array::Unprofiled:
            case Array::Undecided:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            case Array::ForceExit:
            case Array::Generic:
                if (child1->shouldSpeculateCell()) {
                    if (child2->shouldSpeculateString()) {
                        fixEdge<CellUse>(child1);
                        fixEdge<StringUse>(child2);
                        break;
                    }

                    if (child2->shouldSpeculateSymbol()) {
                        fixEdge<CellUse>(child1);
                        fixEdge<SymbolUse>(child2);
                        break;
                    }
                }
#if USE(JSVALUE32_64)
                // Due to register pressure on 32-bit, we speculate cell and
                // ignore the base-is-not-cell case entirely by letting the
                // baseline JIT handle it.
                fixEdge<CellUse>(child1);
#endif
                break;
            case Array::Int32:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                fixEdge<Int32Use>(child3);
                break;
            case Array::Double:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                fixEdge<DoubleRepRealUse>(child3);
                break;
            case Array::Int8Array:
            case Array::Int16Array:
            case Array::Int32Array:
            case Array::Uint8Array:
            case Array::Uint8ClampedArray:
            case Array::Uint16Array:
            case Array::Uint32Array:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                if (child3->shouldSpeculateInt32())
                    fixIntOrBooleanEdge(child3);
                else if (child3->shouldSpeculateInt52())
                    fixEdge<Int52RepUse>(child3);
                else
                    fixDoubleOrBooleanEdge(child3);
                break;
            case Array::Float32Array:
            case Array::Float64Array:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                fixDoubleOrBooleanEdge(child3);
                break;
            case Array::Contiguous:
            case Array::ArrayStorage:
            case Array::SlowPutArrayStorage:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                speculateForBarrier(child3);
                break;
            default:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                break;
            }
            break;
        }
            
        case AtomicsAdd:
        case AtomicsAnd:
        case AtomicsCompareExchange:
        case AtomicsExchange:
        case AtomicsLoad:
        case AtomicsOr:
        case AtomicsStore:
        case AtomicsSub:
        case AtomicsXor: {
            Edge& base = m_graph.child(node, 0);
            Edge& index = m_graph.child(node, 1);
            
            bool badNews = false;
            for (unsigned i = numExtraAtomicsArgs(node->op()); i--;) {
                Edge& child = m_graph.child(node, 2 + i);
                // NOTE: DFG is not smart enough to handle double->int conversions in atomics. So, we
                // just call the function when that happens. But the FTL is totally cool with those
                // conversions.
                if (!child->shouldSpeculateInt32()
                    && !child->shouldSpeculateInt52()
                    && !(child->shouldSpeculateNumberOrBoolean() && m_graph.m_plan.isFTL()))
                    badNews = true;
            }
            
            if (badNews) {
                node->setArrayMode(ArrayMode(Array::Generic, node->arrayMode().action()));
                break;
            }

            node->setArrayMode(
                node->arrayMode().refine(
                    m_graph, node, base->prediction(), index->prediction()));
            
            switch (node->arrayMode().type()) {
            case Array::Int8Array:
            case Array::Uint8Array:
            case Array::Int16Array:
            case Array::Uint16Array:
            case Array::Int32Array:
            case Array::Uint32Array: {
                for (unsigned i = numExtraAtomicsArgs(node->op()); i--;) {
                    Edge& child = m_graph.child(node, 2 + i);
                    if (child->shouldSpeculateInt32())
                        fixIntOrBooleanEdge(child);
                    else if (child->shouldSpeculateInt52())
                        fixEdge<Int52RepUse>(child);
                    else {
                        RELEASE_ASSERT(child->shouldSpeculateNumberOrBoolean() && m_graph.m_plan.isFTL());
                        fixDoubleOrBooleanEdge(child);
                    }
                }

                blessArrayOperation(base, index, m_graph.child(node, 2 + numExtraAtomicsArgs(node->op())));
                fixEdge<CellUse>(base);
                fixEdge<Int32Use>(index);

                if (node->op() == AtomicsStore) {
                    Edge& operand = m_graph.child(node, 2);
                    switch (operand.useKind()) {
                    case Int32Use:
                        // Default result type.
                        break;
                    case Int52RepUse:
                        node->setResult(NodeResultInt52);
                        break;
                    case DoubleRepUse:
                        node->setResult(NodeResultDouble);
                        break;
                    default:
                        DFG_CRASH(m_graph, node, "Bad use kind");
                        break;
                    }
                } else {
                    if (node->arrayMode().type() == Array::Uint32Array) {
                        // NOTE: This means basically always doing Int52.
                        if (node->shouldSpeculateInt52())
                            node->setResult(NodeResultInt52);
                        else
                            node->setResult(NodeResultDouble);
                    }
                }
                break;
            }
            default: {
                // Make it Array::Generic.
                // FIXME: Add BigInt64Array / BigUint64Array support.
                // https://bugs.webkit.org/show_bug.cgi?id=221172
                node->setArrayMode(ArrayMode(Array::Generic, node->arrayMode().action()));
                break;
            }
            }
            break;
        }
            
        case AtomicsIsLockFree:
            if (m_graph.child(node, 0)->shouldSpeculateInt32())
                fixIntOrBooleanEdge(m_graph.child(node, 0));
            break;
            
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
            Edge& storageEdge = m_graph.varArgChild(node, 0);
            Edge& arrayEdge = m_graph.varArgChild(node, 1);
            unsigned elementOffset = 2;
            unsigned elementCount = node->numChildren() - elementOffset;
            for (unsigned i = 0; i < elementCount; ++i) {
                Edge& element = m_graph.varArgChild(node, i + elementOffset);
                node->setArrayMode(
                    node->arrayMode().refine(
                        m_graph, node,
                        arrayEdge->prediction() & SpecCell,
                        SpecInt32Only,
                        element->prediction()));
            }
            blessArrayOperation(arrayEdge, Edge(), storageEdge);
            fixEdge<KnownCellUse>(arrayEdge);

            // Convert `array.push()` to GetArrayLength.
            if (!elementCount && node->arrayMode().supportsSelfLength()) {
                node->setOpAndDefaultFlags(GetArrayLength);
                node->child1() = arrayEdge;
                node->child2() = storageEdge;
                fixEdge<KnownCellUse>(node->child1());
                break;
            }

            // We do not want to perform osr exit and retry for ArrayPush. We insert Check with appropriate type,
            // and ArrayPush uses the edge as known typed edge. Therefore, ArrayPush do not need to perform type checks.
            for (unsigned i = 0; i < elementCount; ++i) {
                Edge& element = m_graph.varArgChild(node, i + elementOffset);
                switch (node->arrayMode().type()) {
                case Array::Int32:
                    fixEdge<Int32Use>(element);
                    break;
                case Array::Double:
                    fixEdge<DoubleRepRealUse>(element);
                    break;
                case Array::Contiguous:
                case Array::ArrayStorage:
                    speculateForBarrier(element);
                    break;
                default:
                    break;
                }
            }
            break;
        }
            
        case ArrayPop: {
            blessArrayOperation(node->child1(), Edge(), node->child2());
            fixEdge<KnownCellUse>(node->child1());
            break;
        }

        case ArraySlice: {
            fixEdge<KnownCellUse>(m_graph.varArgChild(node, 0));
            if (node->numChildren() >= 3) {
                fixEdge<Int32Use>(m_graph.varArgChild(node, 1));
                if (node->numChildren() == 4)
                    fixEdge<Int32Use>(m_graph.varArgChild(node, 2));
            }
            break;
        }

        case ArrayIndexOf:
            fixupArrayIndexOf(node);
            break;
            
        case RegExpExec:
        case RegExpTest: {
            fixEdge<KnownCellUse>(node->child1());
            
            if (node->child2()->shouldSpeculateRegExpObject()) {
                fixEdge<RegExpObjectUse>(node->child2());

                if (node->child3()->shouldSpeculateString())
                    fixEdge<StringUse>(node->child3());
            }
            break;
        }

        case RegExpMatchFast: {
            fixEdge<KnownCellUse>(node->child1());
            fixEdge<RegExpObjectUse>(node->child2());
            fixEdge<StringUse>(node->child3());
            break;
        }

        case StringReplace:
        case StringReplaceRegExp: {
            if (node->child2()->shouldSpeculateString()) {
                m_insertionSet.insertNode(
                    m_indexInBlock, SpecNone, Check, node->origin,
                    Edge(node->child2().node(), StringUse));
                fixEdge<StringUse>(node->child2());
            } else if (op == StringReplace) {
                if (node->child2()->shouldSpeculateRegExpObject())
                    addStringReplacePrimordialChecks(node->child2().node());
                else 
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, ForceOSRExit, node->origin);
            }

            if (node->child1()->shouldSpeculateString()
                && node->child2()->shouldSpeculateRegExpObject()
                && node->child3()->shouldSpeculateString()) {

                fixEdge<StringUse>(node->child1());
                fixEdge<RegExpObjectUse>(node->child2());
                fixEdge<StringUse>(node->child3());
                break;
            }
            break;
        }
            
        case Branch: {
            if (node->child1()->shouldSpeculateBoolean()) {
                if (node->child1()->result() == NodeResultBoolean) {
                    // This is necessary in case we have a bytecode instruction implemented by:
                    //
                    // a: CompareEq(...)
                    // b: Branch(@a)
                    //
                    // In that case, CompareEq might have a side-effect. Then, we need to make
                    // sure that we know that Branch does not exit.
                    fixEdge<KnownBooleanUse>(node->child1());
                } else
                    fixEdge<BooleanUse>(node->child1());
            } else if (node->child1()->shouldSpeculateObjectOrOther())
                fixEdge<ObjectOrOtherUse>(node->child1());
            else if (node->child1()->shouldSpeculateInt32OrBoolean())
                fixIntOrBooleanEdge(node->child1());
            else if (node->child1()->shouldSpeculateNumber())
                fixEdge<DoubleRepUse>(node->child1());
            else if (node->child1()->shouldSpeculateString())
                fixEdge<StringUse>(node->child1());
            else if (node->child1()->shouldSpeculateStringOrOther())
                fixEdge<StringOrOtherUse>(node->child1());
            else {
                WatchpointSet* masqueradesAsUndefinedWatchpoint = m_graph.globalObjectFor(node->origin.semantic)->masqueradesAsUndefinedWatchpoint();
                if (masqueradesAsUndefinedWatchpoint->isStillValid())
                    m_graph.watchpoints().addLazily(masqueradesAsUndefinedWatchpoint);
            }
            break;
        }
            
        case Switch: {
            SwitchData* data = node->switchData();
            switch (data->kind) {
            case SwitchImm:
                if (node->child1()->shouldSpeculateInt32())
                    fixEdge<Int32Use>(node->child1());
                break;
            case SwitchChar:
                if (node->child1()->shouldSpeculateString())
                    fixEdge<StringUse>(node->child1());
                break;
            case SwitchString:
                if (node->child1()->shouldSpeculateStringIdent())
                    fixEdge<StringIdentUse>(node->child1());
                else if (node->child1()->shouldSpeculateString())
                    fixEdge<StringUse>(node->child1());
                break;
            case SwitchCell:
                if (node->child1()->shouldSpeculateCell())
                    fixEdge<CellUse>(node->child1());
                // else it's fine for this to have UntypedUse; we will handle this by just making
                // non-cells take the default case.
                break;
            }
            break;
        }
            
        case ToPrimitive: {
            fixupToPrimitive(node);
            break;
        }

        case ToPropertyKey: {
            if (node->child1()->shouldSpeculateString()) {
                fixEdge<StringUse>(node->child1());
                node->convertToIdentity();

                return;
            }

            if (node->child1()->shouldSpeculateSymbol()) {
                fixEdge<SymbolUse>(node->child1());
                node->convertToIdentity();
                return;
            }

            if (node->child1()->shouldSpeculateStringObject()
                && m_graph.canOptimizeStringObjectAccess(node->origin.semantic)) {
                addCheckStructureForOriginalStringObjectUse(StringObjectUse, node->origin, node->child1().node());
                fixEdge<StringObjectUse>(node->child1());
                node->convertToToString();
                return;
            }

            if (node->child1()->shouldSpeculateStringOrStringObject()
                && m_graph.canOptimizeStringObjectAccess(node->origin.semantic)) {
                addCheckStructureForOriginalStringObjectUse(StringOrStringObjectUse, node->origin, node->child1().node());
                fixEdge<StringOrStringObjectUse>(node->child1());
                node->convertToToString();
                return;
            }
            break;
        }

        case ToNumber:
        case ToNumeric:
        case CallNumberConstructor: {
            fixupToNumberOrToNumericOrCallNumberConstructor(node);
            break;
        }

        case ToString:
        case CallStringConstructor: {
            fixupToStringOrCallStringConstructor(node);
            break;
        }
            
        case NewStringObject: {
            fixEdge<KnownStringUse>(node->child1());
            break;
        }

        case NewSymbol: {
            if (node->child1())
                fixEdge<KnownStringUse>(node->child1());
            break;
        }

        case NewArrayWithSpread: {
            watchHavingABadTime(node);
            
            BitVector* bitVector = node->bitVector();
            for (unsigned i = node->numChildren(); i--;) {
                if (bitVector->get(i))
                    fixEdge<KnownCellUse>(m_graph.m_varArgChildren[node->firstChild() + i]);
                else
                    fixEdge<UntypedUse>(m_graph.m_varArgChildren[node->firstChild() + i]);
            }

            break;
        }

        case Spread: {
            // Note: We care about performing the protocol on our child's global object, not necessarily ours.
            
            watchHavingABadTime(node->child1().node());

            JSGlobalObject* globalObject = m_graph.globalObjectFor(node->child1()->origin.semantic);
            // When we go down the fast path, we don't consult the prototype chain, so we must prove
            // that it doesn't contain any indexed properties, and that any holes will result in
            // jsUndefined().
            Structure* arrayPrototypeStructure = globalObject->arrayPrototype()->structure(vm());
            Structure* objectPrototypeStructure = globalObject->objectPrototype()->structure(vm());
            if (node->child1()->shouldSpeculateArray()
                && arrayPrototypeStructure->transitionWatchpointSetIsStillValid()
                && objectPrototypeStructure->transitionWatchpointSetIsStillValid()
                && globalObject->arrayPrototypeChainIsSane()
                && m_graph.isWatchingArrayIteratorProtocolWatchpoint(node->child1().node())
                && m_graph.isWatchingHavingABadTimeWatchpoint(node->child1().node())) {
                m_graph.registerAndWatchStructureTransition(objectPrototypeStructure);
                m_graph.registerAndWatchStructureTransition(arrayPrototypeStructure);
                fixEdge<ArrayUse>(node->child1());
            } else
                fixEdge<CellUse>(node->child1());
            break;
        }
            
        case NewArray: {
            watchHavingABadTime(node);
            
            for (unsigned i = m_graph.varArgNumChildren(node); i--;) {
                node->setIndexingType(
                    leastUpperBoundOfIndexingTypeAndType(
                        node->indexingType(), m_graph.varArgChild(node, i)->prediction()));
            }
            switch (node->indexingType()) {
            case ALL_BLANK_INDEXING_TYPES:
                CRASH();
                break;
            case ALL_UNDECIDED_INDEXING_TYPES:
                if (node->numChildren()) {
                    // This will only happen if the children have no type predictions. We
                    // would have already exited by now, but insert a forced exit just to
                    // be safe.
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, ForceOSRExit, node->origin);
                }
                break;
            case ALL_INT32_INDEXING_TYPES:
                for (unsigned operandIndex = 0; operandIndex < node->numChildren(); ++operandIndex)
                    fixEdge<Int32Use>(m_graph.m_varArgChildren[node->firstChild() + operandIndex]);
                break;
            case ALL_DOUBLE_INDEXING_TYPES:
                for (unsigned operandIndex = 0; operandIndex < node->numChildren(); ++operandIndex)
                    fixEdge<DoubleRepRealUse>(m_graph.m_varArgChildren[node->firstChild() + operandIndex]);
                break;
            case ALL_CONTIGUOUS_INDEXING_TYPES:
            case ALL_ARRAY_STORAGE_INDEXING_TYPES:
                break;
            default:
                CRASH();
                break;
            }
            break;
        }
            
        case NewTypedArray: {
            watchHavingABadTime(node);
            
            if (node->child1()->shouldSpeculateInt32()) {
                fixEdge<Int32Use>(node->child1());
                node->clearFlags(NodeMustGenerate);
                break;
            }
            break;
        }
            
        case NewArrayWithSize: {
            watchHavingABadTime(node);
            fixEdge<Int32Use>(node->child1());
            break;
        }

        case NewArrayBuffer: {
            watchHavingABadTime(node);
            break;
        }

        case ToObject: {
            fixupToObject(node);
            break;
        }

        case CallObjectConstructor: {
            fixupCallObjectConstructor(node);
            break;
        }

        case ToThis: {
            fixupToThis(node);
            break;
        }
            
        case PutStructure: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }
            
        case GetClosureVar:
        case GetFromArguments:
        case GetInternalField: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }

        case PutClosureVar:
        case PutToArguments:
        case PutInternalField: {
            fixEdge<KnownCellUse>(node->child1());
            speculateForBarrier(node->child2());
            break;
        }

        case SkipScope:
        case GetScope:
        case GetGetter:
        case GetSetter:
        case GetGlobalObject: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }
            
        case AllocatePropertyStorage:
        case ReallocatePropertyStorage: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }
            
        case NukeStructureAndSetButterfly: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }

        case TryGetById: {
            if (node->child1()->shouldSpeculateCell())
                fixEdge<CellUse>(node->child1());
            break;
        }

        case GetByIdDirect:
        case GetByIdDirectFlush: {
            if (node->child1()->shouldSpeculateCell())
                fixEdge<CellUse>(node->child1());
            break;
        }

        case GetPrivateName:
        case GetPrivateNameById:
            if (node->child1()->shouldSpeculateCell())
                fixEdge<CellUse>(node->child1());
            else
                fixEdge<UntypedUse>(node->child1());

            if (!node->hasCacheableIdentifier())
                fixEdge<SymbolUse>(node->child2());
            break;

        case DeleteByVal: {
            if (node->child1()->shouldSpeculateCell()) {
                fixEdge<CellUse>(node->child1());
                if (node->child2()->shouldSpeculateCell())
                    fixEdge<CellUse>(node->child2());
            }
            break;
        }

        case DeleteById: {
            if (node->child1()->shouldSpeculateCell())
                fixEdge<CellUse>(node->child1());
            break;
        }

        case GetById:
        case GetByIdFlush: {
            // FIXME: This should be done in the ByteCodeParser based on reading the
            // PolymorphicAccess, which will surely tell us that this is a AccessCase::ArrayLength.
            // https://bugs.webkit.org/show_bug.cgi?id=154990
            UniquedStringImpl* uid = node->cacheableIdentifier().uid();
            if (node->child1()->shouldSpeculateCellOrOther()
                && !m_graph.hasExitSite(node->origin.semantic, BadType)
                && !m_graph.hasExitSite(node->origin.semantic, BadCache)
                && !m_graph.hasExitSite(node->origin.semantic, BadIndexingType)
                && !m_graph.hasExitSite(node->origin.semantic, ExoticObjectMode)) {
                
                if (uid == vm().propertyNames->length.impl()) {
                    attemptToMakeGetArrayLength(node);
                    break;
                }

                if (uid == vm().propertyNames->lastIndex.impl()
                    && node->child1()->shouldSpeculateRegExpObject()) {
                    node->setOp(GetRegExpObjectLastIndex);
                    node->clearFlags(NodeMustGenerate);
                    fixEdge<RegExpObjectUse>(node->child1());
                    break;
                }
            }

            if (node->child1()->shouldSpeculateNumber()) {
                if (uid == vm().propertyNames->toString.impl()) {
                    if (m_graph.isWatchingNumberToStringWatchpoint(node)) {
                        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
                        if (node->child1()->shouldSpeculateInt32()) {
                            insertCheck<Int32Use>(node->child1().node());
                            m_graph.convertToConstant(node, m_graph.freeze(globalObject->numberProtoToStringFunction()));
                            break;
                        }

                        if (node->child1()->shouldSpeculateInt52()) {
                            insertCheck<Int52RepUse>(node->child1().node());
                            m_graph.convertToConstant(node, m_graph.freeze(globalObject->numberProtoToStringFunction()));
                            break;
                        }

                        ASSERT(node->child1()->shouldSpeculateNumber());
                        insertCheck<DoubleRepUse>(node->child1().node());
                        m_graph.convertToConstant(node, m_graph.freeze(globalObject->numberProtoToStringFunction()));
                        break;
                    }
                }
            }

            if (node->child1()->shouldSpeculateCell())
                fixEdge<CellUse>(node->child1());
            break;
        }
        
        case GetByIdWithThis: {
            if (node->child1()->shouldSpeculateCell() && node->child2()->shouldSpeculateCell()) {
                fixEdge<CellUse>(node->child1());
                fixEdge<CellUse>(node->child2());
            }
            break;
        }

        case PutById:
        case PutByIdFlush:
        case PutByIdDirect: {
            if (node->child1()->shouldSpeculateCellOrOther()
                && !m_graph.hasExitSite(node->origin.semantic, BadType)
                && !m_graph.hasExitSite(node->origin.semantic, BadCache)
                && !m_graph.hasExitSite(node->origin.semantic, BadIndexingType)
                && !m_graph.hasExitSite(node->origin.semantic, ExoticObjectMode)) {
                UniquedStringImpl* uid = node->cacheableIdentifier().uid();
                
                if (uid == vm().propertyNames->lastIndex.impl()
                    && node->child1()->shouldSpeculateRegExpObject()) {
                    node->convertToSetRegExpObjectLastIndex();
                    fixEdge<RegExpObjectUse>(node->child1());
                    speculateForBarrier(node->child2());
                    break;
                }
            }
            
            fixEdge<CellUse>(node->child1());
            break;
        }

        case PutGetterById:
        case PutSetterById: {
            fixEdge<KnownCellUse>(node->child1());
            fixEdge<KnownCellUse>(node->child2());
            break;
        }

        case PutGetterSetterById: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }

        case PutGetterByVal:
        case PutSetterByVal: {
            fixEdge<KnownCellUse>(node->child1());
            fixEdge<KnownCellUse>(node->child3());
            break;
        }

        case GetExecutable: {
            fixEdge<FunctionUse>(node->child1());
            break;
        }


        case SetPrivateBrand: {
            fixEdge<CellUse>(node->child1());
            fixEdge<SymbolUse>(node->child2());
            break;
        }

        case CheckPrivateBrand:
        case PutPrivateName: {
            fixEdge<SymbolUse>(node->child2());
            break;
        }

        case OverridesHasInstance:
        case CheckStructure:
        case CreateThis:
        case CreatePromise:
        case CreateGenerator:
        case CreateAsyncGenerator:
        case PutPrivateNameById:
        case GetButterfly: {
            fixEdge<CellUse>(node->child1());
            break;
        }

        case CheckIsConstant: {
            if (node->constant()->value().isCell() && node->constant()->value())
                fixEdge<CellUse>(node->child1());
            break;
        }

        case ObjectCreate: {
            if (node->child1()->shouldSpeculateObject()) {
                fixEdge<ObjectUse>(node->child1());
                node->clearFlags(NodeMustGenerate);
                break;
            }
            break;
        }

        case ObjectGetOwnPropertyNames:
        case ObjectKeys: {
            if (node->child1()->shouldSpeculateObject()) {
                watchHavingABadTime(node);
                fixEdge<ObjectUse>(node->child1());
            }
            break;
        }

        case CheckIdent: {
            if (node->uidOperand()->isSymbol())
                fixEdge<SymbolUse>(node->child1());
            else
                fixEdge<StringIdentUse>(node->child1());
            break;
        }
            
        case Arrayify:
        case ArrayifyToStructure: {
            fixEdge<CellUse>(node->child1());
            if (node->child2())
                fixEdge<Int32Use>(node->child2());
            break;
        }
            
        case GetByOffset:
        case GetGetterSetterByOffset: {
            if (!node->child1()->hasStorageResult())
                fixEdge<KnownCellUse>(node->child1());
            fixEdge<KnownCellUse>(node->child2());
            break;
        }
            
        case MultiGetByOffset: {
            fixEdge<CellUse>(node->child1());
            break;
        }
            
        case PutByOffset: {
            if (!node->child1()->hasStorageResult())
                fixEdge<KnownCellUse>(node->child1());
            fixEdge<KnownCellUse>(node->child2());
            speculateForBarrier(node->child3());
            break;
        }
            
        case MultiPutByOffset: {
            fixEdge<CellUse>(node->child1());
            break;
        }

        case MultiDeleteByOffset:  {
            fixEdge<CellUse>(node->child1());
            break;
        }
            
        case MatchStructure: {
            // FIXME: Introduce a variant of MatchStructure that doesn't do a cell check.
            // https://bugs.webkit.org/show_bug.cgi?id=185784
            fixEdge<CellUse>(node->child1());
            break;
        }
            
        case InstanceOf: {
            if (node->child1()->shouldSpeculateCell()
                && node->child2()->shouldSpeculateCell()
                && is64Bit()) {
                fixEdge<CellUse>(node->child1());
                fixEdge<CellUse>(node->child2());
                break;
            }
            break;
        }

        case InstanceOfCustom:
            fixEdge<CellUse>(node->child2());
            break;

        case InById: {
            fixEdge<CellUse>(node->child1());
            break;
        }

        case InByVal: {
            if (node->child2()->shouldSpeculateInt32()) {
                convertToHasIndexedProperty(node);
                break;
            }

            fixEdge<CellUse>(node->child1());
            break;
        }

        case HasOwnProperty: {
            fixEdge<ObjectUse>(node->child1());
            if (node->child2()->shouldSpeculateString())
                fixEdge<StringUse>(node->child2());
            else if (node->child2()->shouldSpeculateSymbol())
                fixEdge<SymbolUse>(node->child2());
            else
                fixEdge<UntypedUse>(node->child2());
            break;
        }

        case CheckVarargs:
        case Check: {
            m_graph.doToChildren(
                node,
                [&] (Edge& edge) {
                    switch (edge.useKind()) {
                    case NumberUse:
                        if (edge->shouldSpeculateInt32ForArithmetic())
                            edge.setUseKind(Int32Use);
                        break;
                    default:
                        break;
                    }
                    observeUseKindOnEdge(edge);
                });
            break;
        }

        case Phantom:
            // Phantoms are meaningless past Fixup. We recreate them on-demand in the backend.
            node->remove(m_graph);
            break;

        case FiatInt52: {
            RELEASE_ASSERT(enableInt52());
            node->convertToIdentity();
            fixEdge<Int52RepUse>(node->child1());
            node->setResult(NodeResultInt52);
            break;
        }

        case GetArrayLength: {
            ArrayMode arrayMode = node->arrayMode().refine(m_graph, node, node->child1()->prediction(), ArrayMode::unusedIndexSpeculatedType);
            // We don't know how to handle generic and we only emit this in the Parser when we have checked the value is an Array/TypedArray.
            if (arrayMode.type() == Array::Generic)
                arrayMode = arrayMode.withType(Array::ForceExit);
            ASSERT(arrayMode.isSpecific() || arrayMode.type() == Array::ForceExit);
            node->setArrayMode(arrayMode);
            blessArrayOperation(node->child1(), Edge(), node->child2(), lengthNeedsStorage);

            fixEdge<KnownCellUse>(node->child1());
            break;
        }

        case GetTypedArrayByteOffset: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }

        case CompareBelow:
        case CompareBelowEq: {
            fixEdge<Int32Use>(node->child1());
            fixEdge<Int32Use>(node->child2());
            break;
        }

        case GetPrototypeOf: {
            fixupGetPrototypeOf(node);
            break;
        }

        case CheckDetached:
        case CheckArray: {
            fixEdge<CellUse>(node->child1());
            break;
        }

        case Phi:
        case Upsilon:
        case EntrySwitch:
        case GetIndexedPropertyStorage:
        case LastNodeType:
        case CheckTierUpInLoop:
        case CheckTierUpAtReturn:
        case CheckTierUpAndOSREnter:
        case AssertInBounds:
        case CheckInBounds:
        case ConstantStoragePointer:
        case DoubleAsInt32:
        case ValueToInt32:
        case DoubleRep:
        case ValueRep:
        case Int52Rep:
        case Int52Constant:
        case Identity: // This should have been cleaned up.
        case BooleanToNumber:
        case PhantomNewObject:
        case PhantomNewFunction:
        case PhantomNewGeneratorFunction:
        case PhantomNewAsyncGeneratorFunction:
        case PhantomNewAsyncFunction:
        case PhantomNewInternalFieldObject:
        case PhantomCreateActivation:
        case PhantomDirectArguments:
        case PhantomCreateRest:
        case PhantomSpread:
        case PhantomNewArrayWithSpread:
        case PhantomNewArrayBuffer:
        case PhantomClonedArguments:
        case PhantomNewRegexp:
        case GetMyArgumentByVal:
        case GetMyArgumentByValOutOfBounds:
        case GetVectorLength:
        case PutHint:
        case CheckStructureImmediate:
        case CheckStructureOrEmpty:
        case CheckArrayOrEmpty:
        case MaterializeNewObject:
        case MaterializeCreateActivation:
        case MaterializeNewInternalFieldObject:
        case PutStack:
        case KillStack:
        case GetStack:
        case StoreBarrier:
        case FencedStoreBarrier:
        case GetRegExpObjectLastIndex:
        case SetRegExpObjectLastIndex:
        case RecordRegExpCachedResult:
        case RegExpExecNonGlobalOrSticky:
        case RegExpMatchFastGlobal:
            // These are just nodes that we don't currently expect to see during fixup.
            // If we ever wanted to insert them prior to fixup, then we just have to create
            // fixup rules for them.
            DFG_CRASH(m_graph, node, "Unexpected node during fixup");
            break;

        case PutGlobalVariable: {
            fixEdge<CellUse>(node->child1());
            speculateForBarrier(node->child2());
            break;
        }

        case IsObject:
            if (node->child1()->shouldSpeculateObject()) {
                m_insertionSet.insertNode(
                    m_indexInBlock, SpecNone, Check, node->origin,
                    Edge(node->child1().node(), ObjectUse));
                m_graph.convertToConstant(node, jsBoolean(true));
                observeUseKindOnNode<ObjectUse>(node);
            }
            break;

        case IsCellWithType: {
            fixupIsCellWithType(node);
            break;
        }

        case GetEnumerableLength: {
            fixEdge<CellUse>(node->child1());
            break;
        }
        case HasEnumerableProperty: {
            fixEdge<CellUse>(node->child2());
            break;
        }
        case HasEnumerableStructureProperty: {
            fixEdge<StringUse>(node->child2());
            fixEdge<KnownCellUse>(node->child3());
            break;
        }
        case HasOwnStructureProperty:
        case InStructureProperty: {
            fixEdge<CellUse>(node->child1());
            fixEdge<KnownStringUse>(node->child2());
            fixEdge<KnownCellUse>(node->child3());
            break;
        }
        case HasIndexedProperty:
        case HasEnumerableIndexedProperty: {
            node->setArrayMode(
                node->arrayMode().refine(
                    m_graph, node,
                    m_graph.varArgChild(node, 0)->prediction(),
                    m_graph.varArgChild(node, 1)->prediction(),
                    SpecNone));
            
            blessArrayOperation(m_graph.varArgChild(node, 0), m_graph.varArgChild(node, 1), m_graph.varArgChild(node, 2));
            fixEdge<CellUse>(m_graph.varArgChild(node, 0));
            fixEdge<Int32Use>(m_graph.varArgChild(node, 1));

            ArrayMode arrayMode = node->arrayMode();
            // FIXME: OutOfBounds shouldn't preclude going sane chain because OOB is just false and cannot have effects.
            // See: https://bugs.webkit.org/show_bug.cgi?id=209456
            if (arrayMode.isJSArrayWithOriginalStructure() && arrayMode.speculation() == Array::InBounds)
                setSaneChainIfPossible(node, Array::InBoundsSaneChain);
            break;
        }
        case GetDirectPname: {
            Edge& base = m_graph.varArgChild(node, 0);
            Edge& property = m_graph.varArgChild(node, 1);
            Edge& index = m_graph.varArgChild(node, 2);
            Edge& enumerator = m_graph.varArgChild(node, 3);
            fixEdge<CellUse>(base);
            fixEdge<KnownCellUse>(property);
            fixEdge<Int32Use>(index);
            fixEdge<KnownCellUse>(enumerator);
            break;
        }
        case GetPropertyEnumerator: {
            if (node->child1()->shouldSpeculateCell())
                fixEdge<CellUse>(node->child1());
            break;
        }
        case GetEnumeratorStructurePname: {
            fixEdge<KnownCellUse>(node->child1());
            fixEdge<Int32Use>(node->child2());
            break;
        }
        case GetEnumeratorGenericPname: {
            fixEdge<KnownCellUse>(node->child1());
            fixEdge<Int32Use>(node->child2());
            break;
        }
        case ToIndexString: {
            fixEdge<Int32Use>(node->child1());
            break;
        }
        case ProfileType: {
            // We want to insert type checks based on the instructionTypeSet of the TypeLocation, not the globalTypeSet.
            // Because the instructionTypeSet is contained in globalTypeSet, if we produce a type check for
            // type T for the instructionTypeSet, the global type set must also have information for type T.
            // So if it the type check succeeds for type T in the instructionTypeSet, a type check for type T 
            // in the globalTypeSet would've also succeeded.
            // (The other direction does not hold in general).

            RefPtr<TypeSet> typeSet = node->typeLocation()->m_instructionTypeSet;
            RuntimeTypeMask seenTypes = typeSet->seenTypes();
            if (typeSet->doesTypeConformTo(TypeAnyInt)) {
                if (node->child1()->shouldSpeculateInt32()) {
                    fixEdge<Int32Use>(node->child1());
                    node->remove(m_graph);
                    break;
                }

                if (enableInt52()) {
                    fixEdge<AnyIntUse>(node->child1());
                    node->remove(m_graph);
                    break;
                }

                // Must not perform fixEdge<NumberUse> here since the type set only includes TypeAnyInt. Double values should be logged.
            }

            if (typeSet->doesTypeConformTo(TypeNumber | TypeAnyInt)) {
                fixEdge<NumberUse>(node->child1());
                node->remove(m_graph);
            } else if (typeSet->doesTypeConformTo(TypeString)) {
                fixEdge<StringUse>(node->child1());
                node->remove(m_graph);
            } else if (typeSet->doesTypeConformTo(TypeBoolean)) {
                fixEdge<BooleanUse>(node->child1());
                node->remove(m_graph);
            } else if (typeSet->doesTypeConformTo(TypeUndefined | TypeNull) && (seenTypes & TypeUndefined) && (seenTypes & TypeNull)) {
                fixEdge<OtherUse>(node->child1());
                node->remove(m_graph);
            } else if (typeSet->doesTypeConformTo(TypeObject)) {
                StructureSet set;
                {
                    ConcurrentJSLocker locker(typeSet->m_lock);
                    set = typeSet->structureSet(locker);
                }
                if (!set.isEmpty()) {
                    fixEdge<CellUse>(node->child1());
                    node->convertToCheckStructureOrEmpty(m_graph.addStructureSet(set));
                }
            }

            break;
        }

        case CreateClonedArguments: {
            watchHavingABadTime(node);
            break;
        }

        case CreateScopedArguments:
        case CreateActivation:
        case NewFunction:
        case NewGeneratorFunction:
        case NewAsyncGeneratorFunction:
        case NewAsyncFunction: {
            // Child 1 is always the current scope, which is guaranteed to be an object
            // FIXME: should be KnownObjectUse once that exists (https://bugs.webkit.org/show_bug.cgi?id=175689)
            fixEdge<KnownCellUse>(node->child1());
            break;
        }

        case PushWithScope: {
            // Child 1 is always the current scope, which is guaranteed to be an object
            // FIXME: should be KnownObjectUse once that exists (https://bugs.webkit.org/show_bug.cgi?id=175689)
            fixEdge<KnownCellUse>(node->child1());
            if (node->child2()->shouldSpeculateObject())
                fixEdge<ObjectUse>(node->child2());
            break;
        }

        case SetFunctionName: {
            // The first child is guaranteed to be a cell because op_set_function_name is only used
            // on a newly instantiated function object (the first child).
            fixEdge<KnownCellUse>(node->child1());
            fixEdge<UntypedUse>(node->child2());
            break;
        }

        case CreateRest: {
            watchHavingABadTime(node);
            fixEdge<Int32Use>(node->child1());
            break;
        }

        case ResolveScopeForHoistingFuncDeclInEval: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }
        case ResolveScope:
        case GetDynamicVar:
        case PutDynamicVar: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }

        case LogShadowChickenPrologue: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }
        case LogShadowChickenTail: {
            fixEdge<UntypedUse>(node->child1());
            fixEdge<KnownCellUse>(node->child2());
            break;
        }

        case GetMapBucket:
            if (node->child1().useKind() == MapObjectUse)
                fixEdge<MapObjectUse>(node->child1());
            else if (node->child1().useKind() == SetObjectUse)
                fixEdge<SetObjectUse>(node->child1());
            else
                RELEASE_ASSERT_NOT_REACHED();

#if USE(JSVALUE64)
            if (node->child2()->shouldSpeculateBoolean())
                fixEdge<BooleanUse>(node->child2());
            else if (node->child2()->shouldSpeculateInt32())
                fixEdge<Int32Use>(node->child2());
#if USE(BIGINT32)
            else if (node->child2()->shouldSpeculateBigInt32())
                fixEdge<BigInt32Use>(node->child2());
#endif
            else if (node->child2()->shouldSpeculateSymbol())
                fixEdge<SymbolUse>(node->child2());
            else if (node->child2()->shouldSpeculateObject())
                fixEdge<ObjectUse>(node->child2());
            else if (node->child2()->shouldSpeculateString())
                fixEdge<StringUse>(node->child2());
            else if (node->child2()->shouldSpeculateHeapBigInt())
                fixEdge<HeapBigIntUse>(node->child2());
            else if (node->child2()->shouldSpeculateCell())
                fixEdge<CellUse>(node->child2());
            else
                fixEdge<UntypedUse>(node->child2());
#else
            fixEdge<UntypedUse>(node->child2());
#endif // USE(JSVALUE64)

            fixEdge<Int32Use>(node->child3());
            break;

        case GetMapBucketHead:
            if (node->child1().useKind() == MapObjectUse)
                fixEdge<MapObjectUse>(node->child1());
            else if (node->child1().useKind() == SetObjectUse)
                fixEdge<SetObjectUse>(node->child1());
            else
                RELEASE_ASSERT_NOT_REACHED();
            break;

        case GetMapBucketNext:
        case LoadKeyFromMapBucket:
        case LoadValueFromMapBucket:
            fixEdge<CellUse>(node->child1());
            break;

        case MapHash: {
#if USE(JSVALUE64)
            if (node->child1()->shouldSpeculateBoolean()) {
                fixEdge<BooleanUse>(node->child1());
                break;
            }

            if (node->child1()->shouldSpeculateInt32()) {
                fixEdge<Int32Use>(node->child1());
                break;
            }

            if (node->child1()->shouldSpeculateSymbol()) {
                fixEdge<SymbolUse>(node->child1());
                break;
            }

            if (node->child1()->shouldSpeculateObject()) {
                fixEdge<ObjectUse>(node->child1());
                break;
            }

            if (node->child1()->shouldSpeculateString()) {
                fixEdge<StringUse>(node->child1());
                break;
            }

#if USE(BIGINT32)
            if (node->child1()->shouldSpeculateBigInt32()) {
                fixEdge<BigInt32Use>(node->child1());
                break;
            }
#endif

            if (node->child1()->shouldSpeculateHeapBigInt()) {
                fixEdge<HeapBigIntUse>(node->child1());
                break;
            }

            if (node->child1()->shouldSpeculateCell()) {
                fixEdge<CellUse>(node->child1());
                break;
            }

            fixEdge<UntypedUse>(node->child1());
#else
            fixEdge<UntypedUse>(node->child1());
#endif // USE(JSVALUE64)
            break;
        }

        case NormalizeMapKey: {
            fixupNormalizeMapKey(node);
            break;
        }

        case WeakMapGet: {
            if (node->child1().useKind() == WeakMapObjectUse)
                fixEdge<WeakMapObjectUse>(node->child1());
            else if (node->child1().useKind() == WeakSetObjectUse)
                fixEdge<WeakSetObjectUse>(node->child1());
            else
                RELEASE_ASSERT_NOT_REACHED();
            fixEdge<ObjectUse>(node->child2());
            fixEdge<Int32Use>(node->child3());
            break;
        }

        case SetAdd: {
            fixEdge<SetObjectUse>(node->child1());
            fixEdge<Int32Use>(node->child3());
            break;
        }

        case MapSet: {
            fixEdge<MapObjectUse>(m_graph.varArgChild(node, 0));
            fixEdge<Int32Use>(m_graph.varArgChild(node, 3));
            break;
        }

        case WeakSetAdd: {
            fixEdge<WeakSetObjectUse>(node->child1());
            fixEdge<ObjectUse>(node->child2());
            fixEdge<Int32Use>(node->child3());
            break;
        }

        case WeakMapSet: {
            fixEdge<WeakMapObjectUse>(m_graph.varArgChild(node, 0));
            fixEdge<ObjectUse>(m_graph.varArgChild(node, 1));
            fixEdge<Int32Use>(m_graph.varArgChild(node, 3));
            break;
        }

        case DefineDataProperty: {
            fixEdge<CellUse>(m_graph.varArgChild(node, 0));
            Edge& propertyEdge = m_graph.varArgChild(node, 1);
            if (propertyEdge->shouldSpeculateSymbol())
                fixEdge<SymbolUse>(propertyEdge);
            else if (propertyEdge->shouldSpeculateStringIdent())
                fixEdge<StringIdentUse>(propertyEdge);
            else if (propertyEdge->shouldSpeculateString())
                fixEdge<StringUse>(propertyEdge);
            else
                fixEdge<UntypedUse>(propertyEdge);
            fixEdge<UntypedUse>(m_graph.varArgChild(node, 2));
            fixEdge<Int32Use>(m_graph.varArgChild(node, 3));
            break;
        }

        case StringValueOf: {
            fixupStringValueOf(node);
            break;
        }

        case StringSlice: {
            fixEdge<StringUse>(node->child1());
            fixEdge<Int32Use>(node->child2());
            if (node->child3())
                fixEdge<Int32Use>(node->child3());
            break;
        }

        case ToLowerCase: {
            // We currently only support StringUse since that will ensure that
            // ToLowerCase is a pure operation. If we decide to update this with
            // more types in the future, we need to ensure that the clobberize rules
            // are correct.
            fixEdge<StringUse>(node->child1());
            break;
        }

        case NumberToStringWithRadix: {
            if (node->child1()->shouldSpeculateInt32())
                fixEdge<Int32Use>(node->child1());
            else if (node->child1()->shouldSpeculateInt52())
                fixEdge<Int52RepUse>(node->child1());
            else
                fixEdge<DoubleRepUse>(node->child1());
            fixEdge<Int32Use>(node->child2());
            break;
        }

        case DefineAccessorProperty: {
            fixEdge<CellUse>(m_graph.varArgChild(node, 0));
            Edge& propertyEdge = m_graph.varArgChild(node, 1);
            if (propertyEdge->shouldSpeculateSymbol())
                fixEdge<SymbolUse>(propertyEdge);
            else if (propertyEdge->shouldSpeculateStringIdent())
                fixEdge<StringIdentUse>(propertyEdge);
            else if (propertyEdge->shouldSpeculateString())
                fixEdge<StringUse>(propertyEdge);
            else
                fixEdge<UntypedUse>(propertyEdge);
            fixEdge<CellUse>(m_graph.varArgChild(node, 2));
            fixEdge<CellUse>(m_graph.varArgChild(node, 3));
            fixEdge<Int32Use>(m_graph.varArgChild(node, 4));
            break;
        }

        case CheckJSCast:
        case CheckNotJSCast: {
            fixupCheckJSCast(node);
            break;
        }

        case CallDOMGetter: {
            DOMJIT::CallDOMGetterSnippet* snippet = node->callDOMGetterData()->snippet;
            fixEdge<CellUse>(node->child1()); // DOM.
            if (snippet && snippet->requireGlobalObject)
                fixEdge<KnownCellUse>(node->child2()); // GlobalObject.
            break;
        }

        case CallDOM: {
            fixupCallDOM(node);
            break;
        }

        case Call: {
            attemptToMakeCallDOM(node);
            break;
        }

        case ParseInt: {
            if (node->child1()->shouldSpeculateInt32() && !node->child2()) {
                fixEdge<Int32Use>(node->child1());
                node->convertToIdentity();
                break;
            }

            if (node->child1()->shouldSpeculateString()) {
                fixEdge<StringUse>(node->child1());
                node->clearFlags(NodeMustGenerate);
            }

            if (node->child2())
                fixEdge<Int32Use>(node->child2());

            break;
        }

        case IdentityWithProfile: {
            node->convertToIdentity();
            break;
        }

        case ThrowStaticError:
            fixEdge<StringUse>(node->child1());
            break;

        case NumberIsInteger:
            if (node->child1()->shouldSpeculateInt32()) {
                m_insertionSet.insertNode(
                    m_indexInBlock, SpecNone, Check, node->origin,
                    Edge(node->child1().node(), Int32Use));
                m_graph.convertToConstant(node, jsBoolean(true));
                break;
            }
            break;

        case SetCallee:
            fixEdge<CellUse>(node->child1());
            break;

        case DateGetInt32OrNaN:
        case DateGetTime:
            fixEdge<DateObjectUse>(node->child1());
            break;

        case DataViewGetInt:
        case DataViewGetFloat: {
            fixEdge<DataViewObjectUse>(node->child1());
            fixEdge<Int32Use>(node->child2());
            if (node->child3())
                fixEdge<BooleanUse>(node->child3());

            if (node->op() == DataViewGetInt) {
                DataViewData data = node->dataViewData();
                switch (data.byteSize) {
                case 1:
                case 2:
                    node->setResult(NodeResultInt32);
                    break;
                case 4:
                    if (data.isSigned)
                        node->setResult(NodeResultInt32);
                    else
                        node->setResult(NodeResultInt52);
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }
            break;
        }

        case DataViewSet: {
            fixEdge<DataViewObjectUse>(m_graph.varArgChild(node, 0));
            fixEdge<Int32Use>(m_graph.varArgChild(node, 1));
            if (m_graph.varArgChild(node, 3))
                fixEdge<BooleanUse>(m_graph.varArgChild(node, 3));
            
            DataViewData data = node->dataViewData();
            Edge& valueToStore = m_graph.varArgChild(node, 2);
            if (data.isFloatingPoint)
                fixEdge<DoubleRepUse>(valueToStore);
            else {
                switch (data.byteSize) {
                case 1:
                case 2:
                    fixEdge<Int32Use>(valueToStore);
                    break;
                case 4:
                    if (data.isSigned)
                        fixEdge<Int32Use>(valueToStore);
                    else
                        fixEdge<Int52RepUse>(valueToStore);
                    break;
                }
            }
            break;
        }

        case ForwardVarargs:
        case LoadVarargs: {
            fixEdge<KnownInt32Use>(node->child1());
            break;
        }

#if ASSERT_ENABLED
        // Have these no-op cases here to ensure that nobody forgets to add handlers for new opcodes.
        case SetArgumentDefinitely:
        case SetArgumentMaybe:
        case JSConstant:
        case LazyJSConstant:
        case DoubleConstant:
        case GetLocal:
        case GetCallee:
        case GetArgumentCountIncludingThis:
        case SetArgumentCountIncludingThis:
        case GetRestLength:
        case GetArgument:
        case Flush:
        case PhantomLocal:
        case GetGlobalVar:
        case GetGlobalLexicalVariable:
        case NotifyWrite:
        case DirectCall:
        case CheckTypeInfoFlags:
        case TailCallInlinedCaller:
        case DirectTailCallInlinedCaller:
        case Construct:
        case DirectConstruct:
        case CallVarargs:
        case CallEval:
        case TailCallVarargsInlinedCaller:
        case ConstructVarargs:
        case CallForwardVarargs:
        case ConstructForwardVarargs:
        case TailCallForwardVarargs:
        case TailCallForwardVarargsInlinedCaller:
        case VarargsLength:
        case ProfileControlFlow:
        case NewObject:
        case NewGenerator:
        case NewAsyncGenerator:
        case NewInternalFieldObject:
        case NewRegexp:
        case IsTypedArrayView:
        case IsEmpty:
        case TypeOfIsUndefined:
        case TypeOfIsObject:
        case TypeOfIsFunction:
        case IsUndefinedOrNull:
        case IsBoolean:
        case IsNumber:
        case IsBigInt:
        case IsCallable:
        case IsConstructor:
        case CreateDirectArguments:
        case Jump:
        case Return:
        case TailCall:
        case DirectTailCall:
        case TailCallVarargs:
        case Throw:
        case CountExecution:
        case SuperSamplerBegin:
        case SuperSamplerEnd:
        case ForceOSRExit:
        case CheckBadValue:
        case CheckNotEmpty:
        case AssertNotEmpty:
        case CheckTraps:
        case Unreachable:
        case ExtractOSREntryLocal:
        case ExtractCatchLocal:
        case ClearCatchLocals:
        case LoopHint:
        case MovHint:
        case InitializeEntrypointArguments:
        case ExitOK:
        case BottomValue:
        case TypeOf:
        case PutByIdWithThis:
        case PutByValWithThis:
        case GetByValWithThis:
        case CompareEqPtr:
        case NumberToStringWithValidRadixConstant:
        case GetGlobalThis:
        case ExtractValueFromWeakMapGet:
        case CPUIntrinsic:
        case FilterCallLinkStatus:
        case FilterGetByStatus:
        case FilterPutByIdStatus:
        case FilterInByIdStatus:
        case FilterDeleteByStatus:
        case FilterCheckPrivateBrandStatus:
        case FilterSetPrivateBrandStatus:
        case InvalidationPoint:
        case CreateArgumentsButterfly:
            break;
#else // not ASSERT_ENABLED
        default:
            break;
#endif // not ASSERT_ENABLED
        }

#if ASSERT_ENABLED
        // It would be invalid for Fixup to take a node that didn't clobber exit state and mark it as clobbering afterwords.
        DFG_ASSERT(m_graph, node, usedToClobberExitState || !clobbersExitState(m_graph, node));
#endif

    }

    void watchHavingABadTime(Node* node)
    {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

        // If this global object is not having a bad time, watch it. We go down this path anytime the code
        // does an array allocation. The types of array allocations may change if we start to have a bad
        // time. It's easier to reason about this if we know that whenever the types change after we start
        // optimizing, the code just gets thrown out. Doing this at FixupPhase is just early enough, since
        // prior to this point nobody should have been doing optimizations based on the indexing type of
        // the allocation.
        if (!globalObject->isHavingABadTime()) {
            m_graph.watchpoints().addLazily(globalObject->havingABadTimeWatchpoint());
            m_graph.freeze(globalObject);
        }
    }
    
    template<UseKind useKind>
    void createToString(Node* node, Edge& edge)
    {
        Node* toString = m_insertionSet.insertNode(
            m_indexInBlock, SpecString, ToString, node->origin,
            Edge(edge.node(), useKind));
        switch (useKind) {
        case Int32Use:
        case Int52RepUse:
        case DoubleRepUse:
        case NotCellUse:
            toString->clearFlags(NodeMustGenerate);
            break;
        default:
            break;
        }
        edge.setNode(toString);
    }
    
    template<UseKind useKind>
    void attemptToForceStringArrayModeByToStringConversion(ArrayMode& arrayMode, Node* node)
    {
        ASSERT(arrayMode == ArrayMode(Array::Generic, Array::Read) || arrayMode == ArrayMode(Array::Generic, Array::OriginalNonArray, Array::Read));
        
        if (!m_graph.canOptimizeStringObjectAccess(node->origin.semantic))
            return;
        
        addCheckStructureForOriginalStringObjectUse(useKind, node->origin, node->child1().node());
        createToString<useKind>(node, node->child1());
        arrayMode = ArrayMode(Array::String, Array::Read);
    }

    void addCheckStructureForOriginalStringObjectUse(UseKind useKind, const NodeOrigin& origin, Node* node)
    {
        RELEASE_ASSERT(useKind == StringObjectUse || useKind == StringOrStringObjectUse);

        StructureSet set;
        set.add(m_graph.globalObjectFor(node->origin.semantic)->stringObjectStructure());
        if (useKind == StringOrStringObjectUse)
            set.add(vm().stringStructure.get());

        m_insertionSet.insertNode(
            m_indexInBlock, SpecNone, CheckStructure, origin,
            OpInfo(m_graph.addStructureSet(set)), Edge(node, CellUse));
    }
    
    template<UseKind useKind>
    void convertStringAddUse(Node* node, Edge& edge)
    {
        if (useKind == StringUse) {
            observeUseKindOnNode<StringUse>(edge.node());
            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, Check, node->origin,
                Edge(edge.node(), StringUse));
            edge.setUseKind(KnownStringUse);
            return;
        }
        
        observeUseKindOnNode<useKind>(edge.node());
        createToString<useKind>(node, edge);
    }
    
    void convertToMakeRope(Node* node)
    {
        node->setOpAndDefaultFlags(MakeRope);
        fixupMakeRope(node);
    }
    
    void fixupMakeRope(Node* node)
    {
        for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
            Edge& edge = node->children.child(i);
            if (!edge)
                break;
            edge.setUseKind(KnownStringUse);
            JSString* string = edge->dynamicCastConstant<JSString*>(vm());
            if (!string)
                continue;
            if (string->length())
                continue;
            
            // Don't allow the MakeRope to have zero children.
            if (!i && !node->child2())
                break;
            
            node->children.removeEdge(i--);
        }
        
        if (!node->child2()) {
            ASSERT(!node->child3());
            node->convertToIdentity();
        }
    }

    void fixupIsCellWithType(Node* node)
    {
        Optional<SpeculatedType> filter = node->speculatedTypeForQuery();
        if (filter) {
            switch (filter.value()) {
            case SpecString:
                if (node->child1()->shouldSpeculateString()) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, Check, node->origin,
                        Edge(node->child1().node(), StringUse));
                    m_graph.convertToConstant(node, jsBoolean(true));
                    observeUseKindOnNode<StringUse>(node);
                    return;
                }
                break;

            case SpecProxyObject:
                if (node->child1()->shouldSpeculateProxyObject()) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, Check, node->origin,
                        Edge(node->child1().node(), ProxyObjectUse));
                    m_graph.convertToConstant(node, jsBoolean(true));
                    observeUseKindOnNode<ProxyObjectUse>(node);
                    return;
                }
                break;

            case SpecRegExpObject:
                if (node->child1()->shouldSpeculateRegExpObject()) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, Check, node->origin,
                        Edge(node->child1().node(), RegExpObjectUse));
                    m_graph.convertToConstant(node, jsBoolean(true));
                    observeUseKindOnNode<RegExpObjectUse>(node);
                    return;
                }
                break;

            case SpecArray:
                if (node->child1()->shouldSpeculateArray()) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, Check, node->origin,
                        Edge(node->child1().node(), ArrayUse));
                    m_graph.convertToConstant(node, jsBoolean(true));
                    observeUseKindOnNode<ArrayUse>(node);
                    return;
                }
                break;

            case SpecDerivedArray:
                if (node->child1()->shouldSpeculateDerivedArray()) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, Check, node->origin,
                        Edge(node->child1().node(), DerivedArrayUse));
                    m_graph.convertToConstant(node, jsBoolean(true));
                    observeUseKindOnNode<DerivedArrayUse>(node);
                    return;
                }
                break;
            }
        }

        if (node->child1()->shouldSpeculateCell()) {
            fixEdge<CellUse>(node->child1());
            return;
        }

        if (node->child1()->shouldSpeculateNotCell()) {
            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, Check, node->origin,
                Edge(node->child1().node(), NotCellUse));
            m_graph.convertToConstant(node, jsBoolean(false));
            observeUseKindOnNode<NotCellUse>(node);
            return;
        }
    }

    void fixupGetPrototypeOf(Node* node)
    {
        // Reflect.getPrototypeOf only accepts Objects. For Reflect.getPrototypeOf, ByteCodeParser attaches ObjectUse edge filter before fixup phase.
        if (node->child1().useKind() != ObjectUse) {
            if (node->child1()->shouldSpeculateString()) {
                insertCheck<StringUse>(node->child1().node());
                m_graph.convertToConstant(node, m_graph.freeze(m_graph.globalObjectFor(node->origin.semantic)->stringPrototype()));
                return;
            }
            if (node->child1()->shouldSpeculateInt32()) {
                insertCheck<Int32Use>(node->child1().node());
                m_graph.convertToConstant(node, m_graph.freeze(m_graph.globalObjectFor(node->origin.semantic)->numberPrototype()));
                return;
            }
            if (node->child1()->shouldSpeculateInt52()) {
                insertCheck<Int52RepUse>(node->child1().node());
                m_graph.convertToConstant(node, m_graph.freeze(m_graph.globalObjectFor(node->origin.semantic)->numberPrototype()));
                return;
            }
            if (node->child1()->shouldSpeculateNumber()) {
                insertCheck<NumberUse>(node->child1().node());
                m_graph.convertToConstant(node, m_graph.freeze(m_graph.globalObjectFor(node->origin.semantic)->numberPrototype()));
                return;
            }
            if (node->child1()->shouldSpeculateSymbol()) {
                insertCheck<SymbolUse>(node->child1().node());
                m_graph.convertToConstant(node, m_graph.freeze(m_graph.globalObjectFor(node->origin.semantic)->symbolPrototype()));
                return;
            }
            if (node->child1()->shouldSpeculateBoolean()) {
                insertCheck<BooleanUse>(node->child1().node());
                m_graph.convertToConstant(node, m_graph.freeze(m_graph.globalObjectFor(node->origin.semantic)->booleanPrototype()));
                return;
            }
        }

        if (node->child1()->shouldSpeculateFinalObject()) {
            fixEdge<FinalObjectUse>(node->child1());
            node->clearFlags(NodeMustGenerate);
            return;
        }
        if (node->child1()->shouldSpeculateArray()) {
            fixEdge<ArrayUse>(node->child1());
            node->clearFlags(NodeMustGenerate);
            return;
        }
        if (node->child1()->shouldSpeculateFunction()) {
            fixEdge<FunctionUse>(node->child1());
            node->clearFlags(NodeMustGenerate);
            return;
        }
    }

    void fixupToThis(Node* node)
    {
        bool isStrictMode = node->ecmaMode().isStrict();

        if (isStrictMode) {
            if (node->child1()->shouldSpeculateBoolean()) {
                fixEdge<BooleanUse>(node->child1());
                node->convertToIdentity();
                return;
            }

            if (node->child1()->shouldSpeculateInt32()) {
                fixEdge<Int32Use>(node->child1());
                node->convertToIdentity();
                return;
            }

            if (node->child1()->shouldSpeculateInt52()) {
                fixEdge<Int52RepUse>(node->child1());
                node->convertToIdentity();
                node->setResult(NodeResultInt52);
                return;
            }

            if (node->child1()->shouldSpeculateNumber()) {
                fixEdge<DoubleRepUse>(node->child1());
                node->convertToIdentity();
                node->setResult(NodeResultDouble);
                return;
            }

            if (node->child1()->shouldSpeculateSymbol()) {
                fixEdge<SymbolUse>(node->child1());
                node->convertToIdentity();
                return;
            }

            if (node->child1()->shouldSpeculateStringIdent()) {
                fixEdge<StringIdentUse>(node->child1());
                node->convertToIdentity();
                return;
            }

            if (node->child1()->shouldSpeculateString()) {
                fixEdge<StringUse>(node->child1());
                node->convertToIdentity();
                return;
            }
            
            if (node->child1()->shouldSpeculateBigInt()) {
#if USE(BIGINT32)
                if (node->child1()->shouldSpeculateBigInt32())
                    fixEdge<BigInt32Use>(node->child1());
                else if (node->child1()->shouldSpeculateHeapBigInt())
                    fixEdge<HeapBigIntUse>(node->child1());
                else
                    fixEdge<AnyBigIntUse>(node->child1());
#else
                fixEdge<HeapBigIntUse>(node->child1());
#endif
                node->convertToIdentity();
                return;
            }
        }

        if (node->child1()->shouldSpeculateOther()) {
            if (isStrictMode) {
                fixEdge<OtherUse>(node->child1());
                node->convertToIdentity();
                return;
            }

            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, Check, node->origin,
                Edge(node->child1().node(), OtherUse));
            observeUseKindOnNode<OtherUse>(node->child1().node());
            m_graph.convertToConstant(
                node, m_graph.globalThisObjectFor(node->origin.semantic));
            return;
        }

        // FIXME: This should cover other use cases but we don't have use kinds for them. It's not critical,
        // however, since we cover all the missing cases in constant folding.
        // https://bugs.webkit.org/show_bug.cgi?id=157213
        if (node->child1()->shouldSpeculateStringObject()) {
            fixEdge<StringObjectUse>(node->child1());
            node->convertToIdentity();
            return;
        }

        if (isFinalObjectSpeculation(node->child1()->prediction())) {
            fixEdge<FinalObjectUse>(node->child1());
            node->convertToIdentity();
            return;
        }
    }
    
    void fixupToPrimitive(Node* node)
    {
        if (node->child1()->shouldSpeculateInt32()) {
            fixEdge<Int32Use>(node->child1());
            node->convertToIdentity();
            return;
        }
        
        if (node->child1()->shouldSpeculateString()) {
            fixEdge<StringUse>(node->child1());
            node->convertToIdentity();
            return;
        }
        
        if (node->child1()->shouldSpeculateStringObject()
            && m_graph.canOptimizeStringObjectAccess(node->origin.semantic)) {
            addCheckStructureForOriginalStringObjectUse(StringObjectUse, node->origin, node->child1().node());
            fixEdge<StringObjectUse>(node->child1());
            node->convertToToString();
            return;
        }
        
        if (node->child1()->shouldSpeculateStringOrStringObject()
            && m_graph.canOptimizeStringObjectAccess(node->origin.semantic)) {
            addCheckStructureForOriginalStringObjectUse(StringOrStringObjectUse, node->origin, node->child1().node());
            fixEdge<StringOrStringObjectUse>(node->child1());
            node->convertToToString();
            return;
        }
    }

    void fixupToNumberOrToNumericOrCallNumberConstructor(Node* node)
    {
#if USE(BIGINT32)
        if (node->op() == CallNumberConstructor) {
            if (node->child1()->shouldSpeculateBigInt32()) {
                fixEdge<BigInt32Use>(node->child1());
                node->clearFlags(NodeMustGenerate);
                node->setResult(NodeResultInt32);
                return;
            }
        }
#endif

        // If the prediction of the child is BigInt, we attempt to convert ToNumeric to Identity, since it can only return a BigInt when fed a BigInt.
        if (node->op() == ToNumeric) {
            if (node->child1()->shouldSpeculateBigInt()) {
#if USE(BIGINT32)
                if (node->child1()->shouldSpeculateBigInt32())
                    fixEdge<BigInt32Use>(node->child1());
                else if (node->child1()->shouldSpeculateHeapBigInt())
                    fixEdge<HeapBigIntUse>(node->child1());
                else
                    fixEdge<AnyBigIntUse>(node->child1());
#else
                fixEdge<HeapBigIntUse>(node->child1());
#endif
                node->convertToIdentity();
                return;
            }
        }

        // At first, attempt to fold Boolean or Int32 to Int32.
        if (node->child1()->shouldSpeculateInt32OrBoolean()) {
            if (isInt32Speculation(node->getHeapPrediction())) {
                fixIntOrBooleanEdge(node->child1());
                node->convertToIdentity();
                return;
            }
        }

        // If the prediction of the child is Number, we attempt to convert ToNumber to Identity.
        if (node->child1()->shouldSpeculateNumber()) {
            if (isInt32Speculation(node->getHeapPrediction())) {
                // If the both predictions of this node and the child is Int32, we just convert ToNumber to Identity, that's simple.
                if (node->child1()->shouldSpeculateInt32()) {
                    fixEdge<Int32Use>(node->child1());
                    node->convertToIdentity();
                    return;
                }

                // The another case is that the predicted type of the child is Int32, but the heap prediction tell the users that this will produce non Int32 values.
                // In that case, let's receive the child value as a Double value and convert it to Int32. This case happens in misc-bugs-847389-jpeg2000.
                fixEdge<DoubleRepUse>(node->child1());
                node->setOp(DoubleAsInt32);
                if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                return;
            }

            fixEdge<DoubleRepUse>(node->child1());
            node->convertToIdentity();
            node->setResult(NodeResultDouble);
            return;
        }

        fixEdge<UntypedUse>(node->child1());
        node->setResult(NodeResultJS);
    }

    void fixupToObject(Node* node)
    {
        if (node->child1()->shouldSpeculateObject()) {
            fixEdge<ObjectUse>(node->child1());
            node->convertToIdentity();
            return;
        }

        // ToObject(Null/Undefined) can throw an error. We can emit filters to convert ToObject to CallObjectConstructor.

        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

        if (node->child1()->shouldSpeculateString()) {
            insertCheck<StringUse>(node->child1().node());
            fixEdge<KnownStringUse>(node->child1());
            node->convertToNewStringObject(m_graph.registerStructure(globalObject->stringObjectStructure()));
            return;
        }

        if (node->child1()->shouldSpeculateSymbol()) {
            insertCheck<SymbolUse>(node->child1().node());
            node->convertToCallObjectConstructor(m_graph.freeze(globalObject));
            return;
        }

        if (node->child1()->shouldSpeculateNumber()) {
            insertCheck<NumberUse>(node->child1().node());
            node->convertToCallObjectConstructor(m_graph.freeze(globalObject));
            return;
        }

        if (node->child1()->shouldSpeculateBoolean()) {
            insertCheck<BooleanUse>(node->child1().node());
            node->convertToCallObjectConstructor(m_graph.freeze(globalObject));
            return;
        }

        fixEdge<UntypedUse>(node->child1());
    }

    void fixupCallObjectConstructor(Node* node)
    {
        if (node->child1()->shouldSpeculateObject()) {
            fixEdge<ObjectUse>(node->child1());
            node->convertToIdentity();
            return;
        }

        if (node->child1()->shouldSpeculateString()) {
            auto* globalObject = jsCast<JSGlobalObject*>(node->cellOperand()->cell());
            insertCheck<StringUse>(node->child1().node());
            fixEdge<KnownStringUse>(node->child1());
            node->convertToNewStringObject(m_graph.registerStructure(globalObject->stringObjectStructure()));
            return;
        }

        // While ToObject(Null/Undefined) throws an error, CallObjectConstructor(Null/Undefined) generates a new empty object.
        if (node->child1()->shouldSpeculateOther()) {
            insertCheck<OtherUse>(node->child1().node());
            node->convertToNewObject(m_graph.registerStructure(jsCast<JSGlobalObject*>(node->cellOperand()->cell())->objectStructureForObjectConstructor()));
            return;
        }

        fixEdge<UntypedUse>(node->child1());
    }
    
    void fixupToStringOrCallStringConstructor(Node* node)
    {
        if (node->child1()->shouldSpeculateString()) {
            fixEdge<StringUse>(node->child1());
            node->convertToIdentity();
            return;
        }
        
        if (node->child1()->shouldSpeculateStringObject()
            && m_graph.canOptimizeStringObjectAccess(node->origin.semantic)) {
            addCheckStructureForOriginalStringObjectUse(StringObjectUse, node->origin, node->child1().node());
            fixEdge<StringObjectUse>(node->child1());
            return;
        }
        
        if (node->child1()->shouldSpeculateStringOrStringObject()
            && m_graph.canOptimizeStringObjectAccess(node->origin.semantic)) {
            addCheckStructureForOriginalStringObjectUse(StringOrStringObjectUse, node->origin, node->child1().node());
            fixEdge<StringOrStringObjectUse>(node->child1());
            return;
        }
        
        if (node->child1()->shouldSpeculateCell()) {
            fixEdge<CellUse>(node->child1());
            return;
        }

        if (node->child1()->shouldSpeculateInt32()) {
            fixEdge<Int32Use>(node->child1());
            node->clearFlags(NodeMustGenerate);
            return;
        }

        if (node->child1()->shouldSpeculateInt52()) {
            fixEdge<Int52RepUse>(node->child1());
            node->clearFlags(NodeMustGenerate);
            return;
        }

        if (node->child1()->shouldSpeculateNumber()) {
            fixEdge<DoubleRepUse>(node->child1());
            node->clearFlags(NodeMustGenerate);
            return;
        }

        // ToString(Symbol) throws an error. So if the child1 can include Symbols,
        // we need to care about it in the clobberize. In the following case,
        // since NotCellUse edge filter is used and this edge filters Symbols,
        // we can say that ToString never throws an error!
        if (node->child1()->shouldSpeculateNotCell()) {
            fixEdge<NotCellUse>(node->child1());
            node->clearFlags(NodeMustGenerate);
            return;
        }
    }

    void fixupStringValueOf(Node* node)
    {
        if (node->child1()->shouldSpeculateString()) {
            fixEdge<StringUse>(node->child1());
            node->convertToIdentity();
            return;
        }

        if (node->child1()->shouldSpeculateStringObject()) {
            fixEdge<StringObjectUse>(node->child1());
            node->convertToToString();
            // It does not need to look up a toString property for the StringObject case. So we can clear NodeMustGenerate.
            node->clearFlags(NodeMustGenerate);
            return;
        }

        if (node->child1()->shouldSpeculateStringOrStringObject()) {
            fixEdge<StringOrStringObjectUse>(node->child1());
            node->convertToToString();
            // It does not need to look up a toString property for the StringObject case. So we can clear NodeMustGenerate.
            node->clearFlags(NodeMustGenerate);
            return;
        }
    }

    bool attemptToMakeFastStringAdd(Node* node)
    {
        bool goodToGo = true;
        m_graph.doToChildren(
            node,
            [&] (Edge& edge) {
                if (edge->shouldSpeculateString())
                    return;
                if (m_graph.canOptimizeStringObjectAccess(node->origin.semantic)) {
                    if (edge->shouldSpeculateStringObject())
                        return;
                    if (edge->shouldSpeculateStringOrStringObject())
                        return;
                }
                goodToGo = false;
            });
        if (!goodToGo)
            return false;

        m_graph.doToChildren(
            node,
            [&] (Edge& edge) {
                if (edge->shouldSpeculateString()) {
                    convertStringAddUse<StringUse>(node, edge);
                    return;
                }
                if (!Options::useConcurrentJIT())
                    ASSERT(m_graph.canOptimizeStringObjectAccess(node->origin.semantic));
                if (edge->shouldSpeculateStringObject()) {
                    addCheckStructureForOriginalStringObjectUse(StringObjectUse, node->origin, edge.node());
                    convertStringAddUse<StringObjectUse>(node, edge);
                    return;
                }
                if (edge->shouldSpeculateStringOrStringObject()) {
                    addCheckStructureForOriginalStringObjectUse(StringOrStringObjectUse, node->origin, edge.node());
                    convertStringAddUse<StringOrStringObjectUse>(node, edge);
                    return;
                }
                RELEASE_ASSERT_NOT_REACHED();
            });
        
        convertToMakeRope(node);
        return true;
    }

    void fixupGetAndSetLocalsInBlock(BasicBlock* block)
    {
        if (!block)
            return;
        ASSERT(block->isReachable);
        m_block = block;
        for (m_indexInBlock = 0; m_indexInBlock < block->size(); ++m_indexInBlock) {
            Node* node = m_currentNode = block->at(m_indexInBlock);
            if (node->op() != SetLocal && node->op() != GetLocal)
                continue;
            
            VariableAccessData* variable = node->variableAccessData();
            switch (node->op()) {
            case GetLocal:
                switch (variable->flushFormat()) {
                case FlushedDouble:
                    node->setResult(NodeResultDouble);
                    break;
                case FlushedInt52:
                    node->setResult(NodeResultInt52);
                    break;
                default:
                    break;
                }
                break;
                
            case SetLocal:
                // NOTE: Any type checks we put here may get hoisted by fixupChecksInBlock(). So, if we
                // add new type checking use kind for SetLocals, we need to modify that code as well.
                
                switch (variable->flushFormat()) {
                case FlushedJSValue:
                    break;
                case FlushedDouble:
                    fixEdge<DoubleRepUse>(node->child1());
                    break;
                case FlushedInt32:
                    fixEdge<Int32Use>(node->child1());
                    break;
                case FlushedInt52:
                    fixEdge<Int52RepUse>(node->child1());
                    break;
                case FlushedCell:
                    fixEdge<CellUse>(node->child1());
                    break;
                case FlushedBoolean:
                    fixEdge<BooleanUse>(node->child1());
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                break;
                
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
        m_insertionSet.execute(block);
    }
    
    void addStringReplacePrimordialChecks(Node* searchRegExp)
    {
        Node* node = m_currentNode;

        // Check that structure of searchRegExp is RegExp object
        m_insertionSet.insertNode(
            m_indexInBlock, SpecNone, Check, node->origin,
            Edge(searchRegExp, RegExpObjectUse));

        auto emitPrimordialCheckFor = [&] (JSValue primordialProperty, UniquedStringImpl* propertyUID) {
            m_graph.identifiers().ensure(propertyUID);
            Node* actualProperty = m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, TryGetById, node->origin,
                OpInfo(CacheableIdentifier::createFromImmortalIdentifier(propertyUID)), OpInfo(SpecFunction), Edge(searchRegExp, CellUse));

            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, CheckIsConstant, node->origin,
                OpInfo(m_graph.freeze(primordialProperty)), Edge(actualProperty, CellUse));
        };

        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

        // Check that searchRegExp.exec is the primordial RegExp.prototype.exec
        emitPrimordialCheckFor(globalObject->regExpProtoExecFunction(), vm().propertyNames->exec.impl());
        // Check that searchRegExp.global is the primordial RegExp.prototype.global
        emitPrimordialCheckFor(globalObject->regExpProtoGlobalGetter(), vm().propertyNames->global.impl());
        // Check that searchRegExp.unicode is the primordial RegExp.prototype.unicode
        emitPrimordialCheckFor(globalObject->regExpProtoUnicodeGetter(), vm().propertyNames->unicode.impl());
        // Check that searchRegExp[Symbol.match] is the primordial RegExp.prototype[Symbol.replace]
        emitPrimordialCheckFor(globalObject->regExpProtoSymbolReplaceFunction(), vm().propertyNames->replaceSymbol.impl());
    }

    Node* checkArray(ArrayMode arrayMode, const NodeOrigin& origin, Node* array, Node* index, bool (*storageCheck)(const ArrayMode&) = canCSEStorage)
    {
        ASSERT(arrayMode.isSpecific());
        
        if (arrayMode.type() == Array::String) {
            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, Check, origin, Edge(array, StringUse));
        } else {
            // Note that we only need to be using a structure check if we opt for InBoundsSaneChain, since
            // that needs to protect against JSArray's __proto__ being changed.
            Structure* structure = arrayMode.originalArrayStructure(m_graph, origin.semantic);
        
            Edge indexEdge = index ? Edge(index, Int32Use) : Edge();
            
            if (arrayMode.doesConversion()) {
                if (structure) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, ArrayifyToStructure, origin,
                        OpInfo(m_graph.registerStructure(structure)), OpInfo(arrayMode.asWord()), Edge(array, CellUse), indexEdge);
                } else {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, Arrayify, origin,
                        OpInfo(arrayMode.asWord()), Edge(array, CellUse), indexEdge);
                }
            } else {
                if (structure) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, CheckStructure, origin,
                        OpInfo(m_graph.addStructureSet(structure)), Edge(array, CellUse));
                } else {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, CheckArray, origin,
                        OpInfo(arrayMode.asWord()), Edge(array, CellUse));
                }
            }
        }
        
        if (!storageCheck(arrayMode))
            return nullptr;
        
        if (arrayMode.usesButterfly()) {
            return m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, GetButterfly, origin, Edge(array, CellUse));
        }
        
        return m_insertionSet.insertNode(
            m_indexInBlock, SpecNone, GetIndexedPropertyStorage, origin,
            OpInfo(arrayMode.asWord()), Edge(array, KnownCellUse));
    }

    void setSaneChainIfPossible(Node* node, Array::Speculation speculation)
    {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
        Structure* arrayPrototypeStructure = globalObject->arrayPrototype()->structure(vm());
        Structure* objectPrototypeStructure = globalObject->objectPrototype()->structure(vm());
        if (arrayPrototypeStructure->transitionWatchpointSetIsStillValid()
            && objectPrototypeStructure->transitionWatchpointSetIsStillValid()
            && globalObject->arrayPrototypeChainIsSane()) {
            m_graph.registerAndWatchStructureTransition(arrayPrototypeStructure);
            m_graph.registerAndWatchStructureTransition(objectPrototypeStructure);
            node->setArrayMode(node->arrayMode().withSpeculation(speculation));
            node->clearFlags(NodeMustGenerate);
        }
    }
    
    void blessArrayOperation(Edge base, Edge index, Edge& storageChild, bool (*storageCheck)(const ArrayMode&) = canCSEStorage)
    {
        Node* node = m_currentNode;
        
        switch (node->arrayMode().type()) {
        case Array::ForceExit: {
            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, ForceOSRExit, node->origin);
            return;
        }
            
        case Array::SelectUsingPredictions:
        case Array::Unprofiled:
            RELEASE_ASSERT_NOT_REACHED();
            return;
            
        case Array::Generic:
            return;
            
        default: {
            Node* storage = checkArray(node->arrayMode(), node->origin, base.node(), index.node(), storageCheck);
            if (!storage)
                return;
            
            storageChild = Edge(storage);
            return;
        } }
    }
    
    bool alwaysUnboxSimplePrimitives()
    {
#if USE(JSVALUE64)
        return false;
#else
        // Any boolean, int, or cell value is profitable to unbox on 32-bit because it
        // reduces traffic.
        return true;
#endif
    }

    template<UseKind useKind>
    void observeUseKindOnNode(Node* node)
    {
        if (useKind == UntypedUse)
            return;
        observeUseKindOnNode(node, useKind);
    }

    void observeUseKindOnEdge(Edge edge)
    {
        observeUseKindOnNode(edge.node(), edge.useKind());
    }

    void observeUseKindOnNode(Node* node, UseKind useKind)
    {
        if (node->op() != GetLocal)
            return;
        
        // FIXME: The way this uses alwaysUnboxSimplePrimitives() is suspicious.
        // https://bugs.webkit.org/show_bug.cgi?id=121518
        
        VariableAccessData* variable = node->variableAccessData();
        switch (useKind) {
        case Int32Use:
        case KnownInt32Use:
            if (alwaysUnboxSimplePrimitives()
                || isInt32Speculation(variable->prediction()))
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        case NumberUse:
        case RealNumberUse:
        case DoubleRepUse:
        case DoubleRepRealUse:
            if (variable->doubleFormatState() == UsingDoubleFormat)
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        case BooleanUse:
        case KnownBooleanUse:
            if (alwaysUnboxSimplePrimitives()
                || isBooleanSpeculation(variable->prediction()))
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        case Int52RepUse:
            if (!isInt32Speculation(variable->prediction()) && isInt32OrInt52Speculation(variable->prediction()))
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        case CellUse:
        case KnownCellUse:
        case ObjectUse:
        case FunctionUse:
        case StringUse:
        case KnownStringUse:
        case SymbolUse:
        case HeapBigIntUse:
        case StringObjectUse:
        case StringOrStringObjectUse:
            if (alwaysUnboxSimplePrimitives()
                || isCellSpeculation(variable->prediction()))
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        // FIXME: what should we do about BigInt32Use and AnyBigIntUse ?
        // Also more broadly, what about all of the UseKinds which are not listed in that switch?
        default:
            break;
        }
    }
    
    template<UseKind useKind>
    void fixEdge(Edge& edge)
    {
        observeUseKindOnNode<useKind>(edge.node());
        edge.setUseKind(useKind);
    }
    
    unsigned indexForChecks()
    {
        unsigned index = m_indexInBlock;
        while (!m_block->at(index)->origin.exitOK)
            index--;
        return index;
    }
    
    NodeOrigin originForCheck(unsigned index)
    {
        return m_block->at(index)->origin.withSemantic(m_currentNode->origin.semantic);
    }
    
    void speculateForBarrier(Edge value)
    {
        // Currently, the DFG won't take advantage of this speculation. But, we want to do it in
        // the DFG anyway because if such a speculation would be wrong, we want to know before
        // we do an expensive compile.
        
        if (value->shouldSpeculateInt32()) {
            insertCheck<Int32Use>(value.node());
            return;
        }
            
        if (value->shouldSpeculateBoolean()) {
            insertCheck<BooleanUse>(value.node());
            return;
        }
            
        if (value->shouldSpeculateOther()) {
            insertCheck<OtherUse>(value.node());
            return;
        }
            
        if (value->shouldSpeculateNumber()) {
            insertCheck<NumberUse>(value.node());
            return;
        }
            
        if (value->shouldSpeculateNotCell()) {
            insertCheck<NotCellUse>(value.node());
            return;
        }
    }
    
    template<UseKind useKind>
    void insertCheck(Node* node)
    {
        observeUseKindOnNode<useKind>(node);
        unsigned index = indexForChecks();
        m_insertionSet.insertNode(index, SpecNone, Check, originForCheck(index), Edge(node, useKind));
    }

    void fixIntConvertingEdge(Edge& edge)
    {
        Node* node = edge.node();
        if (node->shouldSpeculateInt32OrBoolean()) {
            fixIntOrBooleanEdge(edge);
            return;
        }
        
        UseKind useKind;
        if (node->shouldSpeculateInt52())
            useKind = Int52RepUse;
        else if (node->shouldSpeculateNumber())
            useKind = DoubleRepUse;
        else
            useKind = NotCellNorBigIntUse;
        Node* newNode = m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32Only, ValueToInt32, m_currentNode->origin,
            Edge(node, useKind));
        observeUseKindOnNode(node, useKind);
        
        edge = Edge(newNode, KnownInt32Use);
    }
    
    void fixIntOrBooleanEdge(Edge& edge)
    {
        Node* node = edge.node();
        if (!node->sawBooleans()) {
            fixEdge<Int32Use>(edge);
            return;
        }
        
        UseKind useKind;
        if (node->shouldSpeculateBoolean())
            useKind = BooleanUse;
        else
            useKind = UntypedUse;
        Node* newNode = m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32Only, BooleanToNumber, m_currentNode->origin,
            Edge(node, useKind));
        observeUseKindOnNode(node, useKind);
        
        edge = Edge(newNode, Int32Use);
    }
    
    void fixDoubleOrBooleanEdge(Edge& edge)
    {
        Node* node = edge.node();
        if (!node->sawBooleans()) {
            fixEdge<DoubleRepUse>(edge);
            return;
        }
        
        UseKind useKind;
        if (node->shouldSpeculateBoolean())
            useKind = BooleanUse;
        else
            useKind = UntypedUse;
        Node* newNode = m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32Only, BooleanToNumber, m_currentNode->origin,
            Edge(node, useKind));
        observeUseKindOnNode(node, useKind);
        
        edge = Edge(newNode, DoubleRepUse);
    }
    
    void truncateConstantToInt32(Edge& edge)
    {
        Node* oldNode = edge.node();
        
        JSValue value = oldNode->asJSValue();
        if (value.isInt32())
            return;
        
        value = jsNumber(JSC::toInt32(value.asNumber()));
        ASSERT(value.isInt32());
        edge.setNode(m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32Only, JSConstant, m_currentNode->origin,
            OpInfo(m_graph.freeze(value))));
    }
    
    void truncateConstantsIfNecessary(Node* node, AddSpeculationMode mode)
    {
        if (mode != SpeculateInt32AndTruncateConstants)
            return;
        
        ASSERT(node->child1()->hasConstant() || node->child2()->hasConstant());
        if (node->child1()->hasConstant())
            truncateConstantToInt32(node->child1());
        else
            truncateConstantToInt32(node->child2());
    }

    bool attemptToMakeIntegerAdd(Node* node)
    {
        AddSpeculationMode mode = m_graph.addSpeculationMode(node, FixupPass);
        if (mode != DontSpeculateInt32) {
            truncateConstantsIfNecessary(node, mode);
            fixIntOrBooleanEdge(node->child1());
            fixIntOrBooleanEdge(node->child2());
            if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                node->setArithMode(Arith::Unchecked);
            else
                node->setArithMode(Arith::CheckOverflow);
            return true;
        }
        
        if (m_graph.addShouldSpeculateInt52(node)) {
            fixEdge<Int52RepUse>(node->child1());
            fixEdge<Int52RepUse>(node->child2());
            node->setArithMode(Arith::CheckOverflow);
            node->setResult(NodeResultInt52);
            return true;
        }
        
        return false;
    }
    
    bool attemptToMakeGetArrayLength(Node* node)
    {
        if (!isInt32Speculation(node->prediction()))
            return false;
        CodeBlock* profiledBlock = m_graph.baselineCodeBlockFor(node->origin.semantic);
        ArrayProfile* arrayProfile = 
            profiledBlock->getArrayProfile(node->origin.semantic.bytecodeIndex());
        ArrayMode arrayMode = ArrayMode(Array::SelectUsingPredictions, Array::Read);
        if (arrayProfile) {
            ConcurrentJSLocker locker(profiledBlock->m_lock);
            arrayProfile->computeUpdatedPrediction(locker, profiledBlock);
            arrayMode = ArrayMode::fromObserved(locker, arrayProfile, Array::Read, false);
            if (arrayMode.type() == Array::Unprofiled) {
                // For normal array operations, it makes sense to treat Unprofiled
                // accesses as ForceExit and get more data rather than using
                // predictions and then possibly ending up with a Generic. But here,
                // we treat anything that is Unprofiled as Generic and keep the
                // GetById. I.e. ForceExit = Generic. So, there is no harm - and only
                // profit - from treating the Unprofiled case as
                // SelectUsingPredictions.
                arrayMode = ArrayMode(Array::SelectUsingPredictions, Array::Read);
            }
        }
            
        arrayMode = arrayMode.refine(
            m_graph, node, node->child1()->prediction(), ArrayMode::unusedIndexSpeculatedType);
            
        if (arrayMode.type() == Array::Generic) {
            // Check if the input is something that we can't get array length for, but for which we
            // could insert some conversions in order to transform it into something that we can do it
            // for.
            if (node->child1()->shouldSpeculateStringObject())
                attemptToForceStringArrayModeByToStringConversion<StringObjectUse>(arrayMode, node);
            else if (node->child1()->shouldSpeculateStringOrStringObject())
                attemptToForceStringArrayModeByToStringConversion<StringOrStringObjectUse>(arrayMode, node);
        }
            
        if (!arrayMode.supportsSelfLength())
            return false;
        
        convertToGetArrayLength(node, arrayMode);
        return true;
    }

    void convertToGetArrayLength(Node* node, ArrayMode arrayMode)
    {
        node->setOp(GetArrayLength);
        node->clearFlags(NodeMustGenerate);
        fixEdge<KnownCellUse>(node->child1());
        node->setArrayMode(arrayMode);
            
        Node* storage = checkArray(arrayMode, node->origin, node->child1().node(), nullptr, lengthNeedsStorage);
        if (!storage)
            return;
            
        node->child2() = Edge(storage);
    }
    
    Node* prependGetArrayLength(NodeOrigin origin, Node* child, ArrayMode arrayMode)
    {
        Node* storage = checkArray(arrayMode, origin, child, nullptr, lengthNeedsStorage);
        return m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32Only, GetArrayLength, origin,
            OpInfo(arrayMode.asWord()), Edge(child, KnownCellUse), Edge(storage));
    }

    void convertToHasIndexedProperty(Node* node)
    {
        node->setOp(HasIndexedProperty);

        {
            unsigned firstChild = m_graph.m_varArgChildren.size();
            unsigned numChildren = 3;
            m_graph.m_varArgChildren.append(node->child1());
            m_graph.m_varArgChildren.append(node->child2());
            m_graph.m_varArgChildren.append(Edge());
            node->setFlags(defaultFlags(HasIndexedProperty));
            node->children = AdjacencyList(AdjacencyList::Variable, firstChild, numChildren);
        }

        node->setArrayMode(
            node->arrayMode().refine(
                m_graph, node,
                m_graph.varArgChild(node, 0)->prediction(),
                m_graph.varArgChild(node, 1)->prediction(),
                SpecNone));

        blessArrayOperation(m_graph.varArgChild(node, 0), m_graph.varArgChild(node, 1), m_graph.varArgChild(node, 2));
        auto arrayMode = node->arrayMode();
        // FIXME: OutOfBounds shouldn't preclude going sane chain because OOB is just false and cannot have effects.
        // See: https://bugs.webkit.org/show_bug.cgi?id=209456
        if (arrayMode.isJSArrayWithOriginalStructure() && arrayMode.speculation() == Array::InBounds)
            setSaneChainIfPossible(node, Array::InBoundsSaneChain);

        fixEdge<CellUse>(m_graph.varArgChild(node, 0));
        fixEdge<Int32Use>(m_graph.varArgChild(node, 1));
    }

    void fixupNormalizeMapKey(Node* node)
    {
        if (node->child1()->shouldSpeculateBoolean()) {
            fixEdge<BooleanUse>(node->child1());
            node->convertToIdentity();
            return;
        }

        if (node->child1()->shouldSpeculateInt32()) {
            fixEdge<Int32Use>(node->child1());
            node->convertToIdentity();
            return;
        }

        if (node->child1()->shouldSpeculateSymbol()) {
            fixEdge<SymbolUse>(node->child1());
            node->convertToIdentity();
            return;
        }

        if (node->child1()->shouldSpeculateObject()) {
            fixEdge<ObjectUse>(node->child1());
            node->convertToIdentity();
            return;
        }

        if (node->child1()->shouldSpeculateString()) {
            fixEdge<StringUse>(node->child1());
            node->convertToIdentity();
            return;
        }

#if USE(BIGINT32)
        if (node->child1()->shouldSpeculateBigInt32()) {
            fixEdge<BigInt32Use>(node->child1());
            node->convertToIdentity();
            return;
        }
#endif

        fixEdge<UntypedUse>(node->child1());
    }

    bool attemptToMakeCallDOM(Node* node)
    {
        if (m_graph.hasExitSite(node->origin.semantic, BadType))
            return false;

        const DOMJIT::Signature* signature = node->signature();
        if (!signature)
            return false;

        {
            unsigned index = 0;
            bool shouldConvertToCallDOM = true;
            m_graph.doToChildren(node, [&](Edge& edge) {
                // Callee. Ignore this. DFGByteCodeParser already emit appropriate checks.
                if (!index)
                    return;

                if (index == 1) {
                    // DOM node case.
                    if (edge->shouldSpeculateNotCell())
                        shouldConvertToCallDOM = false;
                } else {
                    switch (signature->arguments[index - 2]) {
                    case SpecString:
                        if (edge->shouldSpeculateNotString())
                            shouldConvertToCallDOM = false;
                        break;
                    case SpecInt32Only:
                        if (edge->shouldSpeculateNotInt32())
                            shouldConvertToCallDOM = false;
                        break;
                    case SpecBoolean:
                        if (edge->shouldSpeculateNotBoolean())
                            shouldConvertToCallDOM = false;
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                        break;
                    }
                }
                ++index;
            });
            if (!shouldConvertToCallDOM)
                return false;
        }

        Node* thisNode = m_graph.varArgChild(node, 1).node();
        Node* checkSubClass = m_insertionSet.insertNode(m_indexInBlock, SpecNone, CheckJSCast, node->origin, OpInfo(signature->classInfo), Edge(thisNode));
        node->convertToCallDOM(m_graph);
        fixupCheckJSCast(checkSubClass);
        fixupCallDOM(node);
        RELEASE_ASSERT(node->child1().node() == thisNode);
        return true;
    }

    void fixupCheckJSCast(Node* node)
    {
        fixEdge<CellUse>(node->child1());
    }

    void fixupCallDOM(Node* node)
    {
        const DOMJIT::Signature* signature = node->signature();
        auto fixup = [&](Edge& edge, unsigned argumentIndex) {
            if (!edge)
                return;
            switch (signature->arguments[argumentIndex]) {
            case SpecString:
                fixEdge<StringUse>(edge);
                break;
            case SpecInt32Only:
                fixEdge<Int32Use>(edge);
                break;
            case SpecBoolean:
                fixEdge<BooleanUse>(edge);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        };
        fixEdge<CellUse>(node->child1()); // DOM.
        fixup(node->child2(), 0);
        fixup(node->child3(), 1);
    }

    void fixupArrayIndexOf(Node* node)
    {
        Edge& array = m_graph.varArgChild(node, 0);
        Edge& storage = m_graph.varArgChild(node, node->numChildren() == 3 ? 2 : 3);
        blessArrayOperation(array, Edge(), storage);
        ASSERT_WITH_MESSAGE(storage.node(), "blessArrayOperation for ArrayIndexOf must set Butterfly for storage edge.");

        Edge& searchElement = m_graph.varArgChild(node, 1);

        // Constant folding.
        switch (node->arrayMode().type()) {
        case Array::Double:
        case Array::Int32: {
            if (searchElement->shouldSpeculateCell()) {
                m_insertionSet.insertNode(m_indexInBlock, SpecNone, Check, node->origin, Edge(searchElement.node(), CellUse));
                m_graph.convertToConstant(node, jsNumber(-1));
                observeUseKindOnNode<CellUse>(searchElement.node());
                return;
            }

            if (searchElement->shouldSpeculateOther()) {
                m_insertionSet.insertNode(m_indexInBlock, SpecNone, Check, node->origin, Edge(searchElement.node(), OtherUse));
                m_graph.convertToConstant(node, jsNumber(-1));
                observeUseKindOnNode<OtherUse>(searchElement.node());
                return;
            }

            if (searchElement->shouldSpeculateBoolean()) {
                m_insertionSet.insertNode(m_indexInBlock, SpecNone, Check, node->origin, Edge(searchElement.node(), BooleanUse));
                m_graph.convertToConstant(node, jsNumber(-1));
                observeUseKindOnNode<BooleanUse>(searchElement.node());
                return;
            }
            break;
        }
        default:
            break;
        }

        fixEdge<KnownCellUse>(array);
        if (node->numChildren() == 4)
            fixEdge<Int32Use>(m_graph.varArgChild(node, 2));

        switch (node->arrayMode().type()) {
        case Array::Double: {
            if (searchElement->shouldSpeculateNumber())
                fixEdge<DoubleRepUse>(searchElement);
            return;
        }
        case Array::Int32: {
            if (searchElement->shouldSpeculateInt32())
                fixEdge<Int32Use>(searchElement);
            return;
        }
        case Array::Contiguous: {
            if (searchElement->shouldSpeculateString())
                fixEdge<StringUse>(searchElement);
            else if (searchElement->shouldSpeculateSymbol())
                fixEdge<SymbolUse>(searchElement);
            else if (searchElement->shouldSpeculateOther())
                fixEdge<OtherUse>(searchElement);
            else if (searchElement->shouldSpeculateObject())
                fixEdge<ObjectUse>(searchElement);
            return;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
    }

    void fixupCompareStrictEqAndSameValue(Node* node)
    {
        ASSERT(node->op() == SameValue || node->op() == CompareStrictEq);

        if (Node::shouldSpeculateBoolean(node->child1().node(), node->child2().node())) {
            fixEdge<BooleanUse>(node->child1());
            fixEdge<BooleanUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (Node::shouldSpeculateInt32(node->child1().node(), node->child2().node())) {
            fixEdge<Int32Use>(node->child1());
            fixEdge<Int32Use>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (Node::shouldSpeculateInt52(node->child1().node(), node->child2().node())) {
            fixEdge<Int52RepUse>(node->child1());
            fixEdge<Int52RepUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (Node::shouldSpeculateNumber(node->child1().node(), node->child2().node())) {
            fixEdge<DoubleRepUse>(node->child1());
            fixEdge<DoubleRepUse>(node->child2());
            // Do not convert SameValue to CompareStrictEq in this case since SameValue(NaN, NaN) and SameValue(-0, +0)
            // are not the same to CompareStrictEq(NaN, NaN) and CompareStrictEq(-0, +0).
            return;
        }
        if (Node::shouldSpeculateSymbol(node->child1().node(), node->child2().node())) {
            fixEdge<SymbolUse>(node->child1());
            fixEdge<SymbolUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (Node::shouldSpeculateHeapBigInt(node->child1().node(), node->child2().node())) {
            fixEdge<HeapBigIntUse>(node->child1());
            fixEdge<HeapBigIntUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
#if USE(BIGINT32)
        if (Node::shouldSpeculateBigInt32(node->child1().node(), node->child2().node())) {
            fixEdge<BigInt32Use>(node->child1());
            fixEdge<BigInt32Use>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (Node::shouldSpeculateBigInt(node->child1().node(), node->child2().node())) {
            fixEdge<AnyBigIntUse>(node->child1());
            fixEdge<AnyBigIntUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
#endif
        if (node->child1()->shouldSpeculateStringIdent() && node->child2()->shouldSpeculateStringIdent()) {
            fixEdge<StringIdentUse>(node->child1());
            fixEdge<StringIdentUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (node->child1()->shouldSpeculateString() && node->child2()->shouldSpeculateString() && ((GPRInfo::numberOfRegisters >= 7) || m_graph.m_plan.isFTL())) {
            fixEdge<StringUse>(node->child1());
            fixEdge<StringUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }

        if (node->child1()->shouldSpeculateObject()) {
            fixEdge<ObjectUse>(node->child1());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (node->child2()->shouldSpeculateObject()) {
            fixEdge<ObjectUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (node->child1()->shouldSpeculateSymbol()) {
            fixEdge<SymbolUse>(node->child1());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (node->child2()->shouldSpeculateSymbol()) {
            fixEdge<SymbolUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (node->child1()->shouldSpeculateMisc()) {
            fixEdge<MiscUse>(node->child1());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (node->child2()->shouldSpeculateMisc()) {
            fixEdge<MiscUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (node->child1()->shouldSpeculateStringIdent()
            && node->child2()->shouldSpeculateNotStringVar()) {
            fixEdge<StringIdentUse>(node->child1());
            fixEdge<NotStringVarUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (node->child2()->shouldSpeculateStringIdent()
            && node->child1()->shouldSpeculateNotStringVar()) {
            fixEdge<StringIdentUse>(node->child2());
            fixEdge<NotStringVarUse>(node->child1());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (node->child1()->shouldSpeculateString() && ((GPRInfo::numberOfRegisters >= 8) || m_graph.m_plan.isFTL())) {
            fixEdge<StringUse>(node->child1());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
        if (node->child2()->shouldSpeculateString() && ((GPRInfo::numberOfRegisters >= 8) || m_graph.m_plan.isFTL())) {
            fixEdge<StringUse>(node->child2());
            node->setOpAndDefaultFlags(CompareStrictEq);
            return;
        }
    }

    void fixupChecksInBlock(BasicBlock* block)
    {
        if (!block)
            return;
        ASSERT(block->isReachable);
        m_block = block;
        unsigned indexForChecks = UINT_MAX;
        NodeOrigin originForChecks;
        for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
            Node* node = block->at(indexInBlock);

            // If this is a node at which we could exit, then save its index. If nodes after this one
            // cannot exit, then we will hoist checks to here.
            if (node->origin.exitOK) {
                indexForChecks = indexInBlock;
                originForChecks = node->origin;
            }

            originForChecks = originForChecks.withSemantic(node->origin.semantic);
            
            // First, try to relax the representational demands of each node, in order to have
            // fewer conversions.
            switch (node->op()) {
            case MovHint:
            case Check:
            case CheckVarargs:
                m_graph.doToChildren(
                    node,
                    [&] (Edge& edge) {
                        switch (edge.useKind()) {
                        case DoubleRepUse:
                        case DoubleRepRealUse:
                            if (edge->hasDoubleResult())
                                break;
            
                            if (edge->hasInt52Result())
                                edge.setUseKind(Int52RepUse);
                            else if (edge.useKind() == DoubleRepUse)
                                edge.setUseKind(NumberUse);
                            break;
            
                        case Int52RepUse:
                            // Nothing we can really do.
                            break;
            
                        case UntypedUse:
                        case NumberUse:
                            if (edge->hasDoubleResult())
                                edge.setUseKind(DoubleRepUse);
                            else if (edge->hasInt52Result())
                                edge.setUseKind(Int52RepUse);
                            break;
            
                        case RealNumberUse:
                            if (edge->hasDoubleResult())
                                edge.setUseKind(DoubleRepRealUse);
                            else if (edge->hasInt52Result())
                                edge.setUseKind(Int52RepUse);
                            break;
            
                        default:
                            break;
                        }
                    });
                break;
                
            case ValueToInt32:
                if (node->child1().useKind() == DoubleRepUse
                    && !node->child1()->hasDoubleResult()) {
                    node->child1().setUseKind(NumberUse);
                    break;
                }
                break;
                
            default:
                break;
            }

            // Now, insert type conversions if necessary.
            m_graph.doToChildren(
                node,
                [&] (Edge& edge) {
                    Node* result = nullptr;

                    switch (edge.useKind()) {
                    case DoubleRepUse:
                    case DoubleRepRealUse:
                    case DoubleRepAnyIntUse: {
                        if (edge->hasDoubleResult())
                            break;
            
                        ASSERT(indexForChecks != UINT_MAX);
                        if (edge->isNumberConstant()) {
                            result = m_insertionSet.insertNode(
                                indexForChecks, SpecBytecodeDouble, DoubleConstant, originForChecks,
                                OpInfo(m_graph.freeze(jsDoubleNumber(edge->asNumber()))));
                        } else if (edge->hasInt52Result()) {
                            result = m_insertionSet.insertNode(
                                indexForChecks, SpecAnyIntAsDouble, DoubleRep, originForChecks,
                                Edge(edge.node(), Int52RepUse));
                        } else {
                            UseKind useKind;
                            if (edge->shouldSpeculateDoubleReal())
                                useKind = RealNumberUse;
                            else if (edge->shouldSpeculateNumber())
                                useKind = NumberUse;
                            else
                                useKind = NotCellNorBigIntUse;

                            result = m_insertionSet.insertNode(
                                indexForChecks, SpecBytecodeDouble, DoubleRep, originForChecks,
                                Edge(edge.node(), useKind));
                        }

                        edge.setNode(result);
                        break;
                    }
            
                    case Int52RepUse: {
                        if (edge->hasInt52Result())
                            break;
            
                        ASSERT(indexForChecks != UINT_MAX);
                        if (edge->isAnyIntConstant()) {
                            result = m_insertionSet.insertNode(
                                indexForChecks, SpecInt52Any, Int52Constant, originForChecks,
                                OpInfo(edge->constant()));
                        } else if (edge->hasDoubleResult()) {
                            result = m_insertionSet.insertNode(
                                indexForChecks, SpecInt52Any, Int52Rep, originForChecks,
                                Edge(edge.node(), DoubleRepAnyIntUse));
                        } else if (edge->shouldSpeculateInt32ForArithmetic()) {
                            result = m_insertionSet.insertNode(
                                indexForChecks, SpecInt32Only, Int52Rep, originForChecks,
                                Edge(edge.node(), Int32Use));
                        } else {
                            result = m_insertionSet.insertNode(
                                indexForChecks, SpecInt52Any, Int52Rep, originForChecks,
                                Edge(edge.node(), AnyIntUse));
                        }

                        edge.setNode(result);
                        break;
                    }

                    default: {
                        if (!edge->hasDoubleResult() && !edge->hasInt52Result())
                            break;
            
                        ASSERT(indexForChecks != UINT_MAX);
                        if (edge->hasDoubleResult()) {
                            result = m_insertionSet.insertNode(
                                indexForChecks, SpecBytecodeDouble, ValueRep, originForChecks,
                                Edge(edge.node(), DoubleRepUse));
                        } else {
                            result = m_insertionSet.insertNode(
                                indexForChecks, SpecInt32Only | SpecAnyIntAsDouble, ValueRep,
                                originForChecks, Edge(edge.node(), Int52RepUse));
                        }

                        edge.setNode(result);
                        break;
                    } }

                    // It's remotely possible that this node cannot do type checks, but we now have a
                    // type check on this node. We don't have to handle the general form of this
                    // problem. It only arises when ByteCodeParser emits an immediate SetLocal, rather
                    // than a delayed one. So, we only worry about those checks that we may have put on
                    // a SetLocal. Note that "indexForChecks != indexInBlock" is just another way of
                    // saying "!node->origin.exitOK".
                    if (indexForChecks != indexInBlock && mayHaveTypeCheck(edge.useKind())) {
                        UseKind knownUseKind;
                        
                        switch (edge.useKind()) {
                        case Int32Use:
                            knownUseKind = KnownInt32Use;
                            break;
                        case CellUse:
                            knownUseKind = KnownCellUse;
                            break;
                        case BooleanUse:
                            knownUseKind = KnownBooleanUse;
                            break;
                        default:
                            // This can only arise if we have a Check node, and in that case, we can
                            // just remove the original check.
                            DFG_ASSERT(m_graph, node, node->op() == Check, node->op(), edge.useKind());
                            knownUseKind = UntypedUse;
                            break;
                        }

                        ASSERT(indexForChecks != UINT_MAX);
                        m_insertionSet.insertNode(
                            indexForChecks, SpecNone, Check, originForChecks, edge);

                        edge.setUseKind(knownUseKind);
                    }
                });
        }
        
        m_insertionSet.execute(block);
    }
    
    BasicBlock* m_block;
    unsigned m_indexInBlock;
    Node* m_currentNode;
    InsertionSet m_insertionSet;
    bool m_profitabilityChanged;
};
    
bool performFixup(Graph& graph)
{
    return runPhase<FixupPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

