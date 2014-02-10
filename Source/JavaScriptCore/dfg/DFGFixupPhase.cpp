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

#if ENABLE(DFG_JIT)

#include "DFGFixupPhase.h"

#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPredictionPropagationPhase.h"
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
                fixupSetLocalsInBlock(m_graph.block(blockIndex));
        }
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex)
            fixupUntypedSetLocalsInBlock(m_graph.block(blockIndex));
        
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
        NodeType op = node->op();

        switch (op) {
        case SetLocal: {
            // This gets handled by fixupSetLocalsInBlock().
            return;
        }
            
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift: {
            fixIntEdge(node->child1());
            fixIntEdge(node->child2());
            break;
        }

        case ArithIMul: {
            fixIntEdge(node->child1());
            fixIntEdge(node->child2());
            node->setOp(ArithMul);
            node->setArithMode(Arith::Unchecked);
            node->child1().setUseKind(Int32Use);
            node->child2().setUseKind(Int32Use);
            break;
        }
            
        case UInt32ToNumber: {
            fixIntEdge(node->child1());
            if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                node->convertToIdentity();
            else if (nodeCanSpeculateInt32(node->arithNodeFlags()))
                node->setArithMode(Arith::CheckOverflow);
            else
                node->setArithMode(Arith::DoOverflow);
            break;
        }
            
        case ValueAdd: {
            if (attemptToMakeIntegerAdd(node)) {
                node->setOp(ArithAdd);
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (Node::shouldSpeculateNumberExpectingDefined(node->child1().node(), node->child2().node())) {
                fixEdge<NumberUse>(node->child1());
                fixEdge<NumberUse>(node->child2());
                node->setOp(ArithAdd);
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            
            // FIXME: Optimize for the case where one of the operands is the
            // empty string. Also consider optimizing for the case where we don't
            // believe either side is the emtpy string. Both of these things should
            // be easy.
            
            if (node->child1()->shouldSpeculateString()
                && attemptToMakeFastStringAdd<StringUse>(node, node->child1(), node->child2()))
                break;
            if (node->child2()->shouldSpeculateString()
                && attemptToMakeFastStringAdd<StringUse>(node, node->child2(), node->child1()))
                break;
            if (node->child1()->shouldSpeculateStringObject()
                && attemptToMakeFastStringAdd<StringObjectUse>(node, node->child1(), node->child2()))
                break;
            if (node->child2()->shouldSpeculateStringObject()
                && attemptToMakeFastStringAdd<StringObjectUse>(node, node->child2(), node->child1()))
                break;
            if (node->child1()->shouldSpeculateStringOrStringObject()
                && attemptToMakeFastStringAdd<StringOrStringObjectUse>(node, node->child1(), node->child2()))
                break;
            if (node->child2()->shouldSpeculateStringOrStringObject()
                && attemptToMakeFastStringAdd<StringOrStringObjectUse>(node, node->child2(), node->child1()))
                break;
            break;
        }
            
        case MakeRope: {
            fixupMakeRope(node);
            break;
        }
            
        case ArithAdd:
        case ArithSub: {
            if (attemptToMakeIntegerAdd(node))
                break;
            fixEdge<NumberUse>(node->child1());
            fixEdge<NumberUse>(node->child2());
            break;
        }
            
        case ArithNegate: {
            if (m_graph.negateShouldSpeculateInt32(node)) {
                fixEdge<Int32Use>(node->child1());
                if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                    node->setArithMode(Arith::Unchecked);
                else if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                break;
            }
            if (m_graph.negateShouldSpeculateMachineInt(node)) {
                fixEdge<MachineIntUse>(node->child1());
                if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                break;
            }
            fixEdge<NumberUse>(node->child1());
            break;
        }
            
        case ArithMul: {
            if (m_graph.mulShouldSpeculateInt32(node)) {
                fixEdge<Int32Use>(node->child1());
                fixEdge<Int32Use>(node->child2());
                if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                    node->setArithMode(Arith::Unchecked);
                else if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                break;
            }
            if (m_graph.mulShouldSpeculateMachineInt(node)) {
                fixEdge<MachineIntUse>(node->child1());
                fixEdge<MachineIntUse>(node->child2());
                if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                break;
            }
            fixEdge<NumberUse>(node->child1());
            fixEdge<NumberUse>(node->child2());
            break;
        }

        case ArithDiv:
        case ArithMod: {
            if (Node::shouldSpeculateInt32ForArithmetic(node->child1().node(), node->child2().node())
                && node->canSpeculateInt32()) {
                if (optimizeForX86() || optimizeForARM64() || optimizeForARMv7s()) {
                    fixEdge<Int32Use>(node->child1());
                    fixEdge<Int32Use>(node->child2());
                    if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                        node->setArithMode(Arith::Unchecked);
                    else if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                        node->setArithMode(Arith::CheckOverflow);
                    else
                        node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                    break;
                }
                Edge child1 = node->child1();
                Edge child2 = node->child2();
                
                injectInt32ToDoubleNode(node->child1());
                injectInt32ToDoubleNode(node->child2());

                // We don't need to do ref'ing on the children because we're stealing them from
                // the original division.
                Node* newDivision = m_insertionSet.insertNode(
                    m_indexInBlock, SpecDouble, *node);
                
                node->setOp(DoubleAsInt32);
                node->children.initialize(Edge(newDivision, KnownNumberUse), Edge(), Edge());
                if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                
                m_insertionSet.insertNode(m_indexInBlock + 1, SpecNone, Phantom, node->codeOrigin, child1, child2);
                break;
            }
            fixEdge<NumberUse>(node->child1());
            fixEdge<NumberUse>(node->child2());
            break;
        }
            
        case ArithMin:
        case ArithMax: {
            if (Node::shouldSpeculateInt32ForArithmetic(node->child1().node(), node->child2().node())
                && node->canSpeculateInt32()) {
                fixEdge<Int32Use>(node->child1());
                fixEdge<Int32Use>(node->child2());
                break;
            }
            fixEdge<NumberUse>(node->child1());
            fixEdge<NumberUse>(node->child2());
            break;
        }
            
        case ArithAbs: {
            if (node->child1()->shouldSpeculateInt32ForArithmetic()
                && node->canSpeculateInt32()) {
                fixEdge<Int32Use>(node->child1());
                break;
            }
            fixEdge<NumberUse>(node->child1());
            break;
        }
            
        case ArithSqrt:
        case ArithSin:
        case ArithCos: {
            fixEdge<NumberUse>(node->child1());
            break;
        }
            
        case LogicalNot: {
            if (node->child1()->shouldSpeculateBoolean())
                fixEdge<BooleanUse>(node->child1());
            else if (node->child1()->shouldSpeculateObjectOrOther())
                fixEdge<ObjectOrOtherUse>(node->child1());
            else if (node->child1()->shouldSpeculateInt32())
                fixEdge<Int32Use>(node->child1());
            else if (node->child1()->shouldSpeculateNumber())
                fixEdge<NumberUse>(node->child1());
            else if (node->child1()->shouldSpeculateString())
                fixEdge<StringUse>(node->child1());
            break;
        }
            
        case TypeOf: {
            if (node->child1()->shouldSpeculateString())
                fixEdge<StringUse>(node->child1());
            else if (node->child1()->shouldSpeculateCell())
                fixEdge<CellUse>(node->child1());
            break;
        }
            
        case CompareEqConstant: {
            break;
        }

        case CompareEq:
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq: {
            if (Node::shouldSpeculateInt32(node->child1().node(), node->child2().node())) {
                fixEdge<Int32Use>(node->child1());
                fixEdge<Int32Use>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (enableInt52()
                && Node::shouldSpeculateMachineInt(node->child1().node(), node->child2().node())) {
                fixEdge<MachineIntUse>(node->child1());
                fixEdge<MachineIntUse>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (Node::shouldSpeculateNumber(node->child1().node(), node->child2().node())) {
                fixEdge<NumberUse>(node->child1());
                fixEdge<NumberUse>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (node->op() != CompareEq)
                break;
            if (Node::shouldSpeculateBoolean(node->child1().node(), node->child2().node())) {
                fixEdge<BooleanUse>(node->child1());
                fixEdge<BooleanUse>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (node->child1()->shouldSpeculateStringIdent() && node->child2()->shouldSpeculateStringIdent()) {
                fixEdge<StringIdentUse>(node->child1());
                fixEdge<StringIdentUse>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (node->child1()->shouldSpeculateString() && node->child2()->shouldSpeculateString() && GPRInfo::numberOfRegisters >= 7) {
                fixEdge<StringUse>(node->child1());
                fixEdge<StringUse>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (node->child1()->shouldSpeculateObject() && node->child2()->shouldSpeculateObject()) {
                fixEdge<ObjectUse>(node->child1());
                fixEdge<ObjectUse>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (node->child1()->shouldSpeculateObject() && node->child2()->shouldSpeculateObjectOrOther()) {
                fixEdge<ObjectUse>(node->child1());
                fixEdge<ObjectOrOtherUse>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (node->child1()->shouldSpeculateObjectOrOther() && node->child2()->shouldSpeculateObject()) {
                fixEdge<ObjectOrOtherUse>(node->child1());
                fixEdge<ObjectUse>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            break;
        }
            
        case CompareStrictEqConstant: {
            break;
        }
            
        case CompareStrictEq: {
            if (Node::shouldSpeculateBoolean(node->child1().node(), node->child2().node())) {
                fixEdge<BooleanUse>(node->child1());
                fixEdge<BooleanUse>(node->child2());
                break;
            }
            if (Node::shouldSpeculateInt32(node->child1().node(), node->child2().node())) {
                fixEdge<Int32Use>(node->child1());
                fixEdge<Int32Use>(node->child2());
                break;
            }
            if (enableInt52()
                && Node::shouldSpeculateMachineInt(node->child1().node(), node->child2().node())) {
                fixEdge<MachineIntUse>(node->child1());
                fixEdge<MachineIntUse>(node->child2());
                break;
            }
            if (Node::shouldSpeculateNumber(node->child1().node(), node->child2().node())) {
                fixEdge<NumberUse>(node->child1());
                fixEdge<NumberUse>(node->child2());
                break;
            }
            if (node->child1()->shouldSpeculateStringIdent() && node->child2()->shouldSpeculateStringIdent()) {
                fixEdge<StringIdentUse>(node->child1());
                fixEdge<StringIdentUse>(node->child2());
                break;
            }
            if (node->child1()->shouldSpeculateString() && node->child2()->shouldSpeculateString() && GPRInfo::numberOfRegisters >= 7) {
                fixEdge<StringUse>(node->child1());
                fixEdge<StringUse>(node->child2());
                break;
            }
            if (node->child1()->shouldSpeculateObject() && node->child2()->shouldSpeculateObject()) {
                fixEdge<ObjectUse>(node->child1());
                fixEdge<ObjectUse>(node->child2());
                break;
            }
            break;
        }

        case StringFromCharCode:
            fixEdge<Int32Use>(node->child1());
            break;

        case StringCharAt:
        case StringCharCodeAt: {
            // Currently we have no good way of refining these.
            ASSERT(node->arrayMode() == ArrayMode(Array::String));
            blessArrayOperation(node->child1(), node->child2(), node->child3());
            fixEdge<KnownCellUse>(node->child1());
            fixEdge<Int32Use>(node->child2());
            break;
        }

        case GetByVal: {
            node->setArrayMode(
                node->arrayMode().refine(
                    m_graph, node->codeOrigin,
                    node->child1()->prediction(),
                    node->child2()->prediction(),
                    SpecNone, node->flags()));
            
            blessArrayOperation(node->child1(), node->child2(), node->child3());
            
            ArrayMode arrayMode = node->arrayMode();
            switch (arrayMode.type()) {
            case Array::Double:
                if (arrayMode.arrayClass() == Array::OriginalArray
                    && arrayMode.speculation() == Array::InBounds
                    && m_graph.globalObjectFor(node->codeOrigin)->arrayPrototypeChainIsSane()
                    && !(node->flags() & NodeBytecodeUsesAsOther))
                    node->setArrayMode(arrayMode.withSpeculation(Array::SaneChain));
                break;
                
            case Array::String:
                if ((node->prediction() & ~SpecString)
                    || m_graph.hasExitSite(node->codeOrigin, OutOfBounds))
                    node->setArrayMode(arrayMode.withSpeculation(Array::OutOfBounds));
                break;
                
            default:
                break;
            }
            
            switch (node->arrayMode().type()) {
            case Array::SelectUsingPredictions:
            case Array::Unprofiled:
            case Array::Undecided:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            case Array::Generic:
#if USE(JSVALUE32_64)
                fixEdge<CellUse>(node->child1()); // Speculating cell due to register pressure on 32-bit.
#endif
                break;
            case Array::ForceExit:
                break;
            default:
                fixEdge<KnownCellUse>(node->child1());
                fixEdge<Int32Use>(node->child2());
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

            node->setArrayMode(
                node->arrayMode().refine(
                    m_graph, node->codeOrigin,
                    child1->prediction(),
                    child2->prediction(),
                    child3->prediction()));
            
            blessArrayOperation(child1, child2, m_graph.varArgChild(node, 3));
            
            switch (node->arrayMode().modeForPut().type()) {
            case Array::SelectUsingPredictions:
            case Array::Unprofiled:
            case Array::Undecided:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            case Array::ForceExit:
            case Array::Generic:
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
                if (child3->prediction() & SpecInt52)
                    fixEdge<MachineIntUse>(child3);
                else
                    fixEdge<Int32Use>(child3);
                break;
            case Array::Double:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                fixEdge<RealNumberUse>(child3);
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
                    fixEdge<Int32Use>(child3);
                else if (child3->shouldSpeculateMachineInt())
                    fixEdge<MachineIntUse>(child3);
                else
                    fixEdge<NumberUse>(child3);
                break;
            case Array::Float32Array:
            case Array::Float64Array:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                fixEdge<NumberUse>(child3);
                break;
            case Array::Contiguous:
            case Array::ArrayStorage:
            case Array::SlowPutArrayStorage:
            case Array::Arguments:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                insertStoreBarrier(m_indexInBlock, child1, child3);
                break;
            default:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                break;
            }
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
                    m_graph, node->codeOrigin,
                    node->child1()->prediction() & SpecCell,
                    SpecInt32,
                    node->child2()->prediction()));
            blessArrayOperation(node->child1(), Edge(), node->child3());
            fixEdge<KnownCellUse>(node->child1());
            
            switch (node->arrayMode().type()) {
            case Array::Int32:
                fixEdge<Int32Use>(node->child2());
                break;
            case Array::Double:
                fixEdge<RealNumberUse>(node->child2());
                break;
            case Array::Contiguous:
            case Array::ArrayStorage:
                insertStoreBarrier(m_indexInBlock, node->child1(), node->child2());
                break;
            default:
                break;
            }
            break;
        }
            
        case ArrayPop: {
            blessArrayOperation(node->child1(), Edge(), node->child2());
            fixEdge<KnownCellUse>(node->child1());
            break;
        }
            
        case RegExpExec:
        case RegExpTest: {
            fixEdge<CellUse>(node->child1());
            fixEdge<CellUse>(node->child2());
            break;
        }
            
        case Branch: {
            if (node->child1()->shouldSpeculateBoolean())
                fixEdge<BooleanUse>(node->child1());
            else if (node->child1()->shouldSpeculateObjectOrOther())
                fixEdge<ObjectOrOtherUse>(node->child1());
            else if (node->child1()->shouldSpeculateInt32())
                fixEdge<Int32Use>(node->child1());
            else if (node->child1()->shouldSpeculateNumber())
                fixEdge<NumberUse>(node->child1());

            Node* logicalNot = node->child1().node();
            if (logicalNot->op() == LogicalNot) {
                
                // Make sure that OSR exit can't observe the LogicalNot. If it can,
                // then we must compute it and cannot peephole around it.
                bool found = false;
                bool ok = true;
                for (unsigned i = m_indexInBlock; i--;) {
                    Node* candidate = m_block->at(i);
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
                        node->children.setChild1(newChildEdge);
                        
                        BasicBlock* toBeTaken = node->notTakenBlock();
                        BasicBlock* toBeNotTaken = node->takenBlock();
                        node->setTakenBlock(toBeTaken);
                        node->setNotTakenBlock(toBeNotTaken);
                    }
                }
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
            }
            break;
        }
            
        case ToPrimitive: {
            fixupToPrimitive(node);
            break;
        }
            
        case ToString: {
            fixupToString(node);
            break;
        }
            
        case NewStringObject: {
            fixEdge<KnownStringUse>(node->child1());
            break;
        }
            
        case NewArray: {
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
                        m_indexInBlock, SpecNone, ForceOSRExit, node->codeOrigin);
                }
                break;
            case ALL_INT32_INDEXING_TYPES:
                for (unsigned operandIndex = 0; operandIndex < node->numChildren(); ++operandIndex)
                    fixEdge<Int32Use>(m_graph.m_varArgChildren[node->firstChild() + operandIndex]);
                break;
            case ALL_DOUBLE_INDEXING_TYPES:
                for (unsigned operandIndex = 0; operandIndex < node->numChildren(); ++operandIndex)
                    fixEdge<RealNumberUse>(m_graph.m_varArgChildren[node->firstChild() + operandIndex]);
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
            if (node->child1()->shouldSpeculateInt32()) {
                fixEdge<Int32Use>(node->child1());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            break;
        }
            
        case NewArrayWithSize: {
            fixEdge<Int32Use>(node->child1());
            break;
        }
            
        case ToThis: {
            ECMAMode ecmaMode = m_graph.executableFor(node->codeOrigin)->isStrictMode() ? StrictMode : NotStrictMode;

            if (isOtherSpeculation(node->child1()->prediction())) {
                if (ecmaMode == StrictMode) {
                    fixEdge<OtherUse>(node->child1());
                    node->convertToIdentity();
                    break;
                }

                m_insertionSet.insertNode(
                    m_indexInBlock, SpecNone, Phantom, node->codeOrigin,
                    Edge(node->child1().node(), OtherUse));
                observeUseKindOnNode<OtherUse>(node->child1().node());
                node->convertToWeakConstant(m_graph.globalThisObjectFor(node->codeOrigin));
                break;
            }
            
            if (isFinalObjectSpeculation(node->child1()->prediction())) {
                fixEdge<FinalObjectUse>(node->child1());
                node->convertToIdentity();
                break;
            }
            
            break;
        }
            
        case GetMyArgumentByVal:
        case GetMyArgumentByValSafe: {
            fixEdge<Int32Use>(node->child1());
            break;
        }
            
        case PutStructure: {
            fixEdge<KnownCellUse>(node->child1());
            insertStoreBarrier(m_indexInBlock, node->child1());
            break;
        }

        case PutClosureVar: {
            fixEdge<KnownCellUse>(node->child1());
            insertStoreBarrier(m_indexInBlock, node->child1(), node->child3());
            break;
        }

        case GetClosureRegisters:
        case SkipTopScope:
        case SkipScope:
        case GetScope: {
            fixEdge<KnownCellUse>(node->child1());
            break;
        }
            
        case AllocatePropertyStorage:
        case ReallocatePropertyStorage: {
            fixEdge<KnownCellUse>(node->child1());
            insertStoreBarrier(m_indexInBlock + 1, node->child1());
            break;
        }

        case GetById:
        case GetByIdFlush: {
            if (!node->child1()->shouldSpeculateCell())
                break;
            StringImpl* impl = m_graph.identifiers()[node->identifierNumber()];
            if (impl == vm().propertyNames->length.impl()) {
                attemptToMakeGetArrayLength(node);
                break;
            }
            if (impl == vm().propertyNames->byteLength.impl()) {
                attemptToMakeGetTypedArrayByteLength(node);
                break;
            }
            if (impl == vm().propertyNames->byteOffset.impl()) {
                attemptToMakeGetTypedArrayByteOffset(node);
                break;
            }
            fixEdge<CellUse>(node->child1());
            break;
        }
            
        case PutById:
        case PutByIdDirect: {
            fixEdge<CellUse>(node->child1());
            insertStoreBarrier(m_indexInBlock, node->child1(), node->child2());
            break;
        }

        case CheckExecutable:
        case CheckStructure:
        case StructureTransitionWatchpoint:
        case CheckFunction:
        case CheckHasInstance:
        case CreateThis:
        case GetButterfly: {
            fixEdge<CellUse>(node->child1());
            break;
        }
            
        case Arrayify:
        case ArrayifyToStructure: {
            fixEdge<CellUse>(node->child1());
            if (node->child2())
                fixEdge<Int32Use>(node->child2());
            break;
        }
            
        case GetByOffset: {
            if (!node->child1()->hasStorageResult())
                fixEdge<KnownCellUse>(node->child1());
            fixEdge<KnownCellUse>(node->child2());
            break;
        }
            
        case PutByOffset: {
            if (!node->child1()->hasStorageResult())
                fixEdge<KnownCellUse>(node->child1());
            fixEdge<KnownCellUse>(node->child2());
            insertStoreBarrier(m_indexInBlock, node->child2(), node->child3());
            break;
        }
            
        case InstanceOf: {
            // FIXME: This appears broken: CheckHasInstance already does an unconditional cell
            // check. https://bugs.webkit.org/show_bug.cgi?id=107479
            if (!(node->child1()->prediction() & ~SpecCell))
                fixEdge<CellUse>(node->child1());
            fixEdge<CellUse>(node->child2());
            break;
        }
            
        case In: {
            // FIXME: We should at some point have array profiling on op_in, in which
            // case we would be able to turn this into a kind of GetByVal.
            
            fixEdge<CellUse>(node->child2());
            break;
        }

        case Phantom:
        case Identity:
        case Check: {
            switch (node->child1().useKind()) {
            case NumberUse:
                if (node->child1()->shouldSpeculateInt32ForArithmetic())
                    node->child1().setUseKind(Int32Use);
                break;
            default:
                break;
            }
            observeUseKindOnEdge(node->child1());
            break;
        }

        case GetArrayLength:
        case Phi:
        case Upsilon:
        case GetArgument:
        case PhantomPutStructure:
        case GetIndexedPropertyStorage:
        case GetTypedArrayByteOffset:
        case LastNodeType:
        case CheckTierUpInLoop:
        case CheckTierUpAtReturn:
        case CheckTierUpAndOSREnter:
        case Int52ToDouble:
        case Int52ToValue:
        case InvalidationPoint:
        case CheckArray:
        case CheckInBounds:
        case ConstantStoragePointer:
        case DoubleAsInt32:
        case Int32ToDouble:
        case ValueToInt32:
            // These are just nodes that we don't currently expect to see during fixup.
            // If we ever wanted to insert them prior to fixup, then we just have to create
            // fixup rules for them.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        
        case PutGlobalVar: {
            Node* globalObjectNode = m_insertionSet.insertNode(m_indexInBlock, SpecNone, WeakJSConstant, node->codeOrigin, 
                OpInfo(m_graph.globalObjectFor(node->codeOrigin)));
            Node* barrierNode = m_graph.addNode(SpecNone, ConditionalStoreBarrier, m_currentNode->codeOrigin, 
                Edge(globalObjectNode, KnownCellUse), Edge(node->child1().node(), UntypedUse));
            m_insertionSet.insert(m_indexInBlock, barrierNode);
            break;
        }

        case TearOffActivation: {
            Node* barrierNode = m_graph.addNode(SpecNone, StoreBarrierWithNullCheck, m_currentNode->codeOrigin, 
                Edge(node->child1().node(), UntypedUse));
            m_insertionSet.insert(m_indexInBlock, barrierNode);
            break;
        }

        case IsString:
            if (node->child1()->shouldSpeculateString()) {
                m_insertionSet.insertNode(m_indexInBlock, SpecNone, Phantom, node->codeOrigin,
                    Edge(node->child1().node(), StringUse));
                m_graph.convertToConstant(node, jsBoolean(true));
                observeUseKindOnNode<StringUse>(node);
            }
            break;
            
#if !ASSERT_DISABLED
        // Have these no-op cases here to ensure that nobody forgets to add handlers for new opcodes.
        case SetArgument:
        case JSConstant:
        case WeakJSConstant:
        case GetLocal:
        case GetCallee:
        case Flush:
        case PhantomLocal:
        case GetLocalUnlinked:
        case GetMyScope:
        case GetClosureVar:
        case GetGlobalVar:
        case NotifyWrite:
        case VariableWatchpoint:
        case VarInjectionWatchpoint:
        case AllocationProfileWatchpoint:
        case Call:
        case Construct:
        case NewObject:
        case NewArrayBuffer:
        case NewRegexp:
        case Breakpoint:
        case ProfileWillCall:
        case ProfileDidCall:
        case IsUndefined:
        case IsBoolean:
        case IsNumber:
        case IsObject:
        case IsFunction:
        case CreateActivation:
        case CreateArguments:
        case PhantomArguments:
        case TearOffArguments:
        case GetMyArgumentsLength:
        case GetMyArgumentsLengthSafe:
        case CheckArgumentsNotCreated:
        case NewFunction:
        case NewFunctionNoCheck:
        case NewFunctionExpression:
        case Jump:
        case Return:
        case Throw:
        case ThrowReferenceError:
        case CountExecution:
        case ForceOSRExit:
        case CheckWatchdogTimer:
        case Unreachable:
        case ExtractOSREntryLocal:
        case LoopHint:
        case StoreBarrier:
        case ConditionalStoreBarrier:
        case StoreBarrierWithNullCheck:
        case FunctionReentryWatchpoint:
        case TypedArrayWatchpoint:
        case MovHint:
        case ZombieHint:
            break;
#else
        default:
            break;
#endif
        }
        
        if (!node->containsMovHint())
            DFG_NODE_DO_TO_CHILDREN(m_graph, node, observeUntypedEdge);
        
        if (node->isTerminal()) {
            // Terminal nodes don't need post-phantoms, and inserting them would violate
            // the current requirement that a terminal is the last thing in a block. We
            // should eventually change that requirement but even if we did, this would
            // still be a valid optimization. All terminals accept just one input, and
            // if that input is a conversion node then no further speculations will be
            // performed.
            // FIXME: Get rid of this by allowing Phantoms after terminals.
            // https://bugs.webkit.org/show_bug.cgi?id=126778
            m_requiredPhantoms.resize(0);
        // Since StoreBarriers are recursively fixed up so that their children look 
        // identical to that of the node they're barrier-ing, we need to avoid adding
        // any Phantoms when processing them because this would invalidate the 
        // InsertionSet's invariant of inserting things in a monotonically increasing
        // order. This should be okay anyways because StoreBarriers can't exit. 
        } else
            addPhantomsIfNecessary();
    }
    
    void observeUntypedEdge(Node*, Edge& edge)
    {
        if (edge.useKind() != UntypedUse)
            return;
        fixEdge<UntypedUse>(edge);
    }
    
    template<UseKind useKind>
    void createToString(Node* node, Edge& edge)
    {
        edge.setNode(m_insertionSet.insertNode(
            m_indexInBlock, SpecString, ToString, node->codeOrigin,
            Edge(edge.node(), useKind)));
    }
    
    template<UseKind useKind>
    void attemptToForceStringArrayModeByToStringConversion(ArrayMode& arrayMode, Node* node)
    {
        ASSERT(arrayMode == ArrayMode(Array::Generic));
        
        if (!canOptimizeStringObjectAccess(node->codeOrigin))
            return;
        
        createToString<useKind>(node, node->child1());
        arrayMode = ArrayMode(Array::String);
    }
    
    template<UseKind useKind>
    bool isStringObjectUse()
    {
        switch (useKind) {
        case StringObjectUse:
        case StringOrStringObjectUse:
            return true;
        default:
            return false;
        }
    }
    
    template<UseKind useKind>
    void convertStringAddUse(Node* node, Edge& edge)
    {
        if (useKind == StringUse) {
            // This preserves the binaryUseKind() invariant ot ValueAdd: ValueAdd's
            // two edges will always have identical use kinds, which makes the
            // decision process much easier.
            observeUseKindOnNode<StringUse>(edge.node());
            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, Phantom, node->codeOrigin,
                Edge(edge.node(), StringUse));
            edge.setUseKind(KnownStringUse);
            return;
        }
        
        // FIXME: We ought to be able to have a ToPrimitiveToString node.
        
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
            if (!m_graph.isConstant(edge.node()))
                continue;
            JSString* string = jsCast<JSString*>(m_graph.valueOfJSConstant(edge.node()).asCell());
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
            && canOptimizeStringObjectAccess(node->codeOrigin)) {
            fixEdge<StringObjectUse>(node->child1());
            node->convertToToString();
            return;
        }
        
        if (node->child1()->shouldSpeculateStringOrStringObject()
            && canOptimizeStringObjectAccess(node->codeOrigin)) {
            fixEdge<StringOrStringObjectUse>(node->child1());
            node->convertToToString();
            return;
        }
    }
    
    void fixupToString(Node* node)
    {
        if (node->child1()->shouldSpeculateString()) {
            fixEdge<StringUse>(node->child1());
            node->convertToIdentity();
            return;
        }
        
        if (node->child1()->shouldSpeculateStringObject()
            && canOptimizeStringObjectAccess(node->codeOrigin)) {
            fixEdge<StringObjectUse>(node->child1());
            return;
        }
        
        if (node->child1()->shouldSpeculateStringOrStringObject()
            && canOptimizeStringObjectAccess(node->codeOrigin)) {
            fixEdge<StringOrStringObjectUse>(node->child1());
            return;
        }
        
        if (node->child1()->shouldSpeculateCell()) {
            fixEdge<CellUse>(node->child1());
            return;
        }
    }
    
    template<UseKind leftUseKind>
    bool attemptToMakeFastStringAdd(Node* node, Edge& left, Edge& right)
    {
        Node* originalLeft = left.node();
        Node* originalRight = right.node();
        
        ASSERT(leftUseKind == StringUse || leftUseKind == StringObjectUse || leftUseKind == StringOrStringObjectUse);
        
        if (isStringObjectUse<leftUseKind>() && !canOptimizeStringObjectAccess(node->codeOrigin))
            return false;
        
        convertStringAddUse<leftUseKind>(node, left);
        
        if (right->shouldSpeculateString())
            convertStringAddUse<StringUse>(node, right);
        else if (right->shouldSpeculateStringObject() && canOptimizeStringObjectAccess(node->codeOrigin))
            convertStringAddUse<StringObjectUse>(node, right);
        else if (right->shouldSpeculateStringOrStringObject() && canOptimizeStringObjectAccess(node->codeOrigin))
            convertStringAddUse<StringOrStringObjectUse>(node, right);
        else {
            // At this point we know that the other operand is something weird. The semantically correct
            // way of dealing with this is:
            //
            // MakeRope(@left, ToString(ToPrimitive(@right)))
            //
            // So that's what we emit. NB, we need to do all relevant type checks on @left before we do
            // anything to @right, since ToPrimitive may be effectful.
            
            Node* toPrimitive = m_insertionSet.insertNode(
                m_indexInBlock, resultOfToPrimitive(right->prediction()), ToPrimitive, node->codeOrigin,
                Edge(right.node()));
            Node* toString = m_insertionSet.insertNode(
                m_indexInBlock, SpecString, ToString, node->codeOrigin, Edge(toPrimitive));
            
            fixupToPrimitive(toPrimitive);
            fixupToString(toString);
            
            right.setNode(toString);
        }
        
        // We're doing checks up there, so we need to make sure that the
        // *original* inputs to the addition are live up to here.
        m_insertionSet.insertNode(
            m_indexInBlock, SpecNone, Phantom, node->codeOrigin,
            Edge(originalLeft), Edge(originalRight));
        
        convertToMakeRope(node);
        return true;
    }
    
    bool isStringPrototypeMethodSane(Structure* stringPrototypeStructure, StringImpl* uid)
    {
        unsigned attributesUnused;
        JSCell* specificValue;
        PropertyOffset offset = stringPrototypeStructure->getConcurrently(
            vm(), uid, attributesUnused, specificValue);
        if (!isValidOffset(offset))
            return false;
        
        if (!specificValue)
            return false;
        
        if (!specificValue->inherits(JSFunction::info()))
            return false;
        
        JSFunction* function = jsCast<JSFunction*>(specificValue);
        if (function->executable()->intrinsicFor(CodeForCall) != StringPrototypeValueOfIntrinsic)
            return false;
        
        return true;
    }
    
    bool canOptimizeStringObjectAccess(const CodeOrigin& codeOrigin)
    {
        if (m_graph.hasExitSite(codeOrigin, NotStringObject))
            return false;
        
        Structure* stringObjectStructure = m_graph.globalObjectFor(codeOrigin)->stringObjectStructure();
        ASSERT(stringObjectStructure->storedPrototype().isObject());
        ASSERT(stringObjectStructure->storedPrototype().asCell()->classInfo() == StringPrototype::info());
        
        JSObject* stringPrototypeObject = asObject(stringObjectStructure->storedPrototype());
        Structure* stringPrototypeStructure = stringPrototypeObject->structure();
        if (!m_graph.watchpoints().isStillValid(stringPrototypeStructure->transitionWatchpointSet()))
            return false;
        
        if (stringPrototypeStructure->isDictionary())
            return false;
        
        // We're being conservative here. We want DFG's ToString on StringObject to be
        // used in both numeric contexts (that would call valueOf()) and string contexts
        // (that would call toString()). We don't want the DFG to have to distinguish
        // between the two, just because that seems like it would get confusing. So we
        // just require both methods to be sane.
        if (!isStringPrototypeMethodSane(stringPrototypeStructure, vm().propertyNames->valueOf.impl()))
            return false;
        if (!isStringPrototypeMethodSane(stringPrototypeStructure, vm().propertyNames->toString.impl()))
            return false;
        
        return true;
    }
    
    void fixupSetLocalsInBlock(BasicBlock* block)
    {
        if (!block)
            return;
        ASSERT(block->isReachable);
        m_block = block;
        for (m_indexInBlock = 0; m_indexInBlock < block->size(); ++m_indexInBlock) {
            Node* node = m_currentNode = block->at(m_indexInBlock);
            if (node->op() != SetLocal)
                continue;
            
            VariableAccessData* variable = node->variableAccessData();
            switch (variable->flushFormat()) {
            case FlushedJSValue:
                break;
            case FlushedDouble:
                fixEdge<NumberUse>(node->child1());
                break;
            case FlushedInt32:
                fixEdge<Int32Use>(node->child1());
                break;
            case FlushedInt52:
                fixEdge<MachineIntUse>(node->child1());
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
            addPhantomsIfNecessary();
        }
        m_insertionSet.execute(block);
    }
    
    void fixupUntypedSetLocalsInBlock(BasicBlock* block)
    {
        if (!block)
            return;
        ASSERT(block->isReachable);
        m_block = block;
        for (m_indexInBlock = 0; m_indexInBlock < block->size(); ++m_indexInBlock) {
            Node* node = m_currentNode = block->at(m_indexInBlock);
            if (node->op() != SetLocal)
                continue;
            
            if (node->child1().useKind() == UntypedUse) {
                fixEdge<UntypedUse>(node->child1());
                addPhantomsIfNecessary();
            }
        }
        m_insertionSet.execute(block);
    }
    
    Node* checkArray(ArrayMode arrayMode, const CodeOrigin& codeOrigin, Node* array, Node* index, bool (*storageCheck)(const ArrayMode&) = canCSEStorage)
    {
        ASSERT(arrayMode.isSpecific());
        
        if (arrayMode.type() == Array::String) {
            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, Phantom, codeOrigin,
                Edge(array, StringUse));
        } else {
            Structure* structure = arrayMode.originalArrayStructure(m_graph, codeOrigin);
        
            Edge indexEdge = index ? Edge(index, Int32Use) : Edge();
        
            if (arrayMode.doesConversion()) {
                if (structure) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, ArrayifyToStructure, codeOrigin,
                        OpInfo(structure), OpInfo(arrayMode.asWord()), Edge(array, CellUse), indexEdge);
                } else {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, Arrayify, codeOrigin,
                        OpInfo(arrayMode.asWord()), Edge(array, CellUse), indexEdge);
                }
            } else {
                if (structure) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, CheckStructure, codeOrigin,
                        OpInfo(m_graph.addStructureSet(structure)), Edge(array, CellUse));
                } else {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, CheckArray, codeOrigin,
                        OpInfo(arrayMode.asWord()), Edge(array, CellUse));
                }
            }
        }
        
        if (!storageCheck(arrayMode))
            return 0;
        
        if (arrayMode.usesButterfly()) {
            return m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, GetButterfly, codeOrigin, Edge(array, CellUse));
        }
        
        return m_insertionSet.insertNode(
            m_indexInBlock, SpecNone, GetIndexedPropertyStorage, codeOrigin,
            OpInfo(arrayMode.asWord()), Edge(array, KnownCellUse));
    }
    
    void blessArrayOperation(Edge base, Edge index, Edge& storageChild)
    {
        Node* node = m_currentNode;
        
        switch (node->arrayMode().type()) {
        case Array::ForceExit: {
            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, ForceOSRExit, node->codeOrigin);
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
            if (alwaysUnboxSimplePrimitives()
                || isInt32Speculation(variable->prediction()))
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        case NumberUse:
        case RealNumberUse:
            if (variable->doubleFormatState() == UsingDoubleFormat)
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        case BooleanUse:
            if (alwaysUnboxSimplePrimitives()
                || isBooleanSpeculation(variable->prediction()))
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        case MachineIntUse:
            if (isMachineIntSpeculation(variable->prediction()))
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        case CellUse:
        case KnownCellUse:
        case ObjectUse:
        case StringUse:
        case KnownStringUse:
        case StringObjectUse:
        case StringOrStringObjectUse:
            if (alwaysUnboxSimplePrimitives()
                || isCellSpeculation(variable->prediction()))
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        default:
            break;
        }
    }
    
    // Set the use kind of the edge and perform any actions that need to be done for
    // that use kind, like inserting intermediate conversion nodes. Never call this
    // with useKind = UntypedUse explicitly; edges have UntypedUse implicitly and any
    // edge that survives fixup and still has UntypedUse will have this method called
    // from observeUntypedEdge(). Also, make sure that if you do change the type of an
    // edge, you either call fixEdge() or perform the equivalent functionality
    // yourself. Obviously, you should have a really good reason if you do the latter.
    template<UseKind useKind>
    void fixEdge(Edge& edge)
    {
        if (isDouble(useKind)) {
            if (edge->shouldSpeculateInt32ForArithmetic()) {
                injectInt32ToDoubleNode(edge, useKind);
                return;
            }
            
            if (enableInt52() && edge->shouldSpeculateMachineInt()) {
                // Make all double uses of int52 values have an intermediate Int52ToDouble.
                // This is for the same reason as Int52ToValue (see below) except that
                // Int8ToDouble will convert int52's that fit in an int32 into a double
                // rather than trying to create a boxed int32 like Int52ToValue does.
                
                m_requiredPhantoms.append(edge.node());
                Node* result = m_insertionSet.insertNode(
                    m_indexInBlock, SpecInt52AsDouble, Int52ToDouble,
                    m_currentNode->codeOrigin, Edge(edge.node(), NumberUse));
                edge = Edge(result, useKind);
                return;
            }
        }
        
        if (enableInt52() && useKind != MachineIntUse
            && edge->shouldSpeculateMachineInt() && !edge->shouldSpeculateInt32()) {
            // We make all non-int52 uses of int52 values have an intermediate Int52ToValue
            // node to ensure that we handle this properly:
            //
            // a: SomeInt52
            // b: ArithAdd(@a, ...)
            // c: Call(..., @a)
            // d: ArithAdd(@a, ...)
            //
            // Without an intermediate node and just labeling the uses, we will get:
            //
            // a: SomeInt52
            // b: ArithAdd(Int52:@a, ...)
            // c: Call(..., Untyped:@a)
            // d: ArithAdd(Int52:@a, ...)
            //
            // And now the c->Untyped:@a edge will box the value of @a into a double. This
            // is bad, because now the d->Int52:@a edge will either have to do double-to-int
            // conversions, or will have to OSR exit unconditionally. Alternatively we could
            // have the c->Untyped:@a edge box the value by copying rather than in-place.
            // But these boxings are also costly so this wouldn't be great.
            //
            // The solution we use is to always have non-Int52 uses of predicted Int52's use
            // an intervening Int52ToValue node:
            //
            // a: SomeInt52
            // b: ArithAdd(Int52:@a, ...)
            // x: Int52ToValue(Int52:@a)
            // c: Call(..., Untyped:@x)
            // d: ArithAdd(Int52:@a, ...)
            //
            // Note that even if we had multiple non-int52 uses of @a, the multiple
            // Int52ToValue's would get CSE'd together. So the boxing would only happen once.
            // At the same time, @a would continue to be represented as a native int52.
            //
            // An alternative would have been to insert ToNativeInt52 nodes on int52 uses of
            // int52's. This would have handled the above example but would fall over for:
            //
            // a: SomeInt52
            // b: Call(..., @a)
            // c: ArithAdd(@a, ...)
            //
            // But the solution we use handles the above gracefully.
            
            m_requiredPhantoms.append(edge.node());
            Node* result = m_insertionSet.insertNode(
                m_indexInBlock, SpecInt52, Int52ToValue,
                m_currentNode->codeOrigin, Edge(edge.node(), UntypedUse));
            edge = Edge(result, useKind);
            return;
        }
        
        observeUseKindOnNode<useKind>(edge.node());
        
        edge.setUseKind(useKind);
    }
    
    void insertStoreBarrier(unsigned indexInBlock, Edge child1, Edge child2 = Edge())
    {
        Node* barrierNode;
        if (!child2)
            barrierNode = m_graph.addNode(SpecNone, StoreBarrier, m_currentNode->codeOrigin, Edge(child1.node(), child1.useKind()));
        else {
            barrierNode = m_graph.addNode(SpecNone, ConditionalStoreBarrier, m_currentNode->codeOrigin, 
                Edge(child1.node(), child1.useKind()), Edge(child2.node(), child2.useKind()));
        }
        m_insertionSet.insert(indexInBlock, barrierNode);
    }

    void fixIntEdge(Edge& edge)
    {
        Node* node = edge.node();
        if (node->shouldSpeculateInt32()) {
            fixEdge<Int32Use>(edge);
            return;
        }
        
        UseKind useKind;
        if (node->shouldSpeculateMachineInt())
            useKind = MachineIntUse;
        else if (node->shouldSpeculateNumber())
            useKind = NumberUse;
        else if (node->shouldSpeculateBoolean())
            useKind = BooleanUse;
        else
            useKind = NotCellUse;
        Node* newNode = m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32, ValueToInt32, m_currentNode->codeOrigin,
            Edge(node, useKind));
        observeUseKindOnNode(node, useKind);
        
        edge = Edge(newNode, KnownInt32Use);
        m_requiredPhantoms.append(node);
    }
    
    void injectInt32ToDoubleNode(Edge& edge, UseKind useKind = NumberUse)
    {
        m_requiredPhantoms.append(edge.node());
        
        Node* result = m_insertionSet.insertNode(
            m_indexInBlock, SpecInt52AsDouble, Int32ToDouble,
            m_currentNode->codeOrigin, Edge(edge.node(), NumberUse));
        
        edge = Edge(result, useKind);
    }
    
    void truncateConstantToInt32(Edge& edge)
    {
        Node* oldNode = edge.node();
        
        ASSERT(oldNode->hasConstant());
        JSValue value = m_graph.valueOfJSConstant(oldNode);
        if (value.isInt32())
            return;
        
        value = jsNumber(JSC::toInt32(value.asNumber()));
        ASSERT(value.isInt32());
        unsigned constantRegister;
        if (!codeBlock()->findConstant(value, constantRegister)) {
            constantRegister = codeBlock()->addConstantLazily();
            initializeLazyWriteBarrierForConstant(
                m_graph.m_plan.writeBarriers,
                codeBlock()->constants()[constantRegister],
                codeBlock(),
                constantRegister,
                codeBlock()->ownerExecutable(),
                value);
        }
        edge.setNode(m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32, JSConstant, m_currentNode->codeOrigin,
            OpInfo(constantRegister)));
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
        AddSpeculationMode mode = m_graph.addSpeculationMode(node);
        if (mode != DontSpeculateInt32) {
            truncateConstantsIfNecessary(node, mode);
            fixEdge<Int32Use>(node->child1());
            fixEdge<Int32Use>(node->child2());
            if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                node->setArithMode(Arith::Unchecked);
            else
                node->setArithMode(Arith::CheckOverflow);
            return true;
        }
        
        if (m_graph.addShouldSpeculateMachineInt(node)) {
            fixEdge<MachineIntUse>(node->child1());
            fixEdge<MachineIntUse>(node->child2());
            node->setArithMode(Arith::CheckOverflow);
            return true;
        }
        
        return false;
    }
    
    bool attemptToMakeGetArrayLength(Node* node)
    {
        if (!isInt32Speculation(node->prediction()))
            return false;
        CodeBlock* profiledBlock = m_graph.baselineCodeBlockFor(node->codeOrigin);
        ArrayProfile* arrayProfile = 
            profiledBlock->getArrayProfile(node->codeOrigin.bytecodeIndex);
        ArrayMode arrayMode = ArrayMode(Array::SelectUsingPredictions);
        if (arrayProfile) {
            ConcurrentJITLocker locker(profiledBlock->m_lock);
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
                arrayMode = ArrayMode(Array::SelectUsingPredictions);
            }
        }
            
        arrayMode = arrayMode.refine(
            m_graph, node->codeOrigin, node->child1()->prediction(), node->prediction());
            
        if (arrayMode.type() == Array::Generic) {
            // Check if the input is something that we can't get array length for, but for which we
            // could insert some conversions in order to transform it into something that we can do it
            // for.
            if (node->child1()->shouldSpeculateStringObject())
                attemptToForceStringArrayModeByToStringConversion<StringObjectUse>(arrayMode, node);
            else if (node->child1()->shouldSpeculateStringOrStringObject())
                attemptToForceStringArrayModeByToStringConversion<StringOrStringObjectUse>(arrayMode, node);
        }
            
        if (!arrayMode.supportsLength())
            return false;
        
        convertToGetArrayLength(node, arrayMode);
        return true;
    }
    
    bool attemptToMakeGetTypedArrayByteLength(Node* node)
    {
        if (!isInt32Speculation(node->prediction()))
            return false;
        
        TypedArrayType type = typedArrayTypeFromSpeculation(node->child1()->prediction());
        if (!isTypedView(type))
            return false;
        
        if (elementSize(type) == 1) {
            convertToGetArrayLength(node, ArrayMode(toArrayType(type)));
            return true;
        }
        
        Node* length = prependGetArrayLength(
            node->codeOrigin, node->child1().node(), ArrayMode(toArrayType(type)));
        
        Node* shiftAmount = m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32, JSConstant, node->codeOrigin,
            OpInfo(m_graph.constantRegisterForConstant(jsNumber(logElementSize(type)))));
        
        // We can use a BitLShift here because typed arrays will never have a byteLength
        // that overflows int32.
        node->setOp(BitLShift);
        node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
        observeUseKindOnNode(length, Int32Use);
        observeUseKindOnNode(shiftAmount, Int32Use);
        node->child1() = Edge(length, Int32Use);
        node->child2() = Edge(shiftAmount, Int32Use);
        return true;
    }
    
    void convertToGetArrayLength(Node* node, ArrayMode arrayMode)
    {
        node->setOp(GetArrayLength);
        node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
        fixEdge<KnownCellUse>(node->child1());
        node->setArrayMode(arrayMode);
            
        Node* storage = checkArray(arrayMode, node->codeOrigin, node->child1().node(), 0, lengthNeedsStorage);
        if (!storage)
            return;
            
        node->child2() = Edge(storage);
    }
    
    Node* prependGetArrayLength(CodeOrigin codeOrigin, Node* child, ArrayMode arrayMode)
    {
        Node* storage = checkArray(arrayMode, codeOrigin, child, 0, lengthNeedsStorage);
        return m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32, GetArrayLength, codeOrigin,
            OpInfo(arrayMode.asWord()), Edge(child, KnownCellUse), Edge(storage));
    }
    
    bool attemptToMakeGetTypedArrayByteOffset(Node* node)
    {
        if (!isInt32Speculation(node->prediction()))
            return false;
        
        TypedArrayType type = typedArrayTypeFromSpeculation(node->child1()->prediction());
        if (!isTypedView(type))
            return false;
        
        checkArray(
            ArrayMode(toArrayType(type)), node->codeOrigin, node->child1().node(),
            0, neverNeedsStorage);
        
        node->setOp(GetTypedArrayByteOffset);
        node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
        fixEdge<KnownCellUse>(node->child1());
        return true;
    }
    
    void addPhantomsIfNecessary()
    {
        if (m_requiredPhantoms.isEmpty())
            return;
        
        for (unsigned i = m_requiredPhantoms.size(); i--;) {
            m_insertionSet.insertNode(
                m_indexInBlock + 1, SpecNone, Phantom, m_currentNode->codeOrigin,
                Edge(m_requiredPhantoms[i], UntypedUse));
        }
        
        m_requiredPhantoms.resize(0);
    }

    BasicBlock* m_block;
    unsigned m_indexInBlock;
    Node* m_currentNode;
    InsertionSet m_insertionSet;
    bool m_profitabilityChanged;
    Vector<Node*, 3> m_requiredPhantoms;
};
    
bool performFixup(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Fixup Phase");
    return runPhase<FixupPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

