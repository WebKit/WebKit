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
#include "DFGFixupPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGVariableAccessDataDump.h"
#include "JSCInlines.h"

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
            injectTypeConversionsInBlock(m_graph.block(blockIndex));
        
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
            addPhantomsIfNecessary();
            fixupNode(m_currentNode);
        }
        clearPhantomsAtEnd();
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
            
        case UInt32ToNumber: {
            fixIntConvertingEdge(node->child1());
            if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                node->convertToIdentity();
            else if (node->canSpeculateInt32(FixupPass))
                node->setArithMode(Arith::CheckOverflow);
            else {
                node->setArithMode(Arith::DoOverflow);
                node->setResult(NodeResultDouble);
            }
            break;
        }
            
        case ValueAdd: {
            if (attemptToMakeIntegerAdd(node)) {
                node->setOp(ArithAdd);
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (Node::shouldSpeculateNumberOrBooleanExpectingDefined(node->child1().node(), node->child2().node())) {
                fixDoubleOrBooleanEdge(node->child1());
                fixDoubleOrBooleanEdge(node->child2());
                node->setOp(ArithAdd);
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                node->setResult(NodeResultDouble);
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
            fixDoubleOrBooleanEdge(node->child1());
            fixDoubleOrBooleanEdge(node->child2());
            node->setResult(NodeResultDouble);
            break;
        }
            
        case ArithNegate: {
            if (m_graph.negateShouldSpeculateInt32(node, FixupPass)) {
                fixIntOrBooleanEdge(node->child1());
                if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                    node->setArithMode(Arith::Unchecked);
                else if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                break;
            }
            if (m_graph.negateShouldSpeculateMachineInt(node, FixupPass)) {
                fixEdge<Int52RepUse>(node->child1());
                if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                node->setResult(NodeResultInt52);
                break;
            }
            fixDoubleOrBooleanEdge(node->child1());
            node->setResult(NodeResultDouble);
            break;
        }
            
        case ArithMul: {
            if (m_graph.mulShouldSpeculateInt32(node, FixupPass)) {
                fixIntOrBooleanEdge(node->child1());
                fixIntOrBooleanEdge(node->child2());
                if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                    node->setArithMode(Arith::Unchecked);
                else if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                break;
            }
            if (m_graph.mulShouldSpeculateMachineInt(node, FixupPass)) {
                fixEdge<Int52RepUse>(node->child1());
                fixEdge<Int52RepUse>(node->child2());
                if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                node->setResult(NodeResultInt52);
                break;
            }
            fixDoubleOrBooleanEdge(node->child1());
            fixDoubleOrBooleanEdge(node->child2());
            node->setResult(NodeResultDouble);
            break;
        }

        case ArithDiv:
        case ArithMod: {
            if (Node::shouldSpeculateInt32OrBooleanForArithmetic(node->child1().node(), node->child2().node())
                && node->canSpeculateInt32(FixupPass)) {
                if (optimizeForX86() || optimizeForARM64() || optimizeForARMv7s()) {
                    fixIntOrBooleanEdge(node->child1());
                    fixIntOrBooleanEdge(node->child2());
                    if (bytecodeCanTruncateInteger(node->arithNodeFlags()))
                        node->setArithMode(Arith::Unchecked);
                    else if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                        node->setArithMode(Arith::CheckOverflow);
                    else
                        node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                    break;
                }
                
                // This will cause conversion nodes to be inserted later.
                fixDoubleOrBooleanEdge(node->child1());
                fixDoubleOrBooleanEdge(node->child2());
                
                // But we have to make sure that everything is phantom'd until after the
                // DoubleAsInt32 node, which occurs after the Div/Mod node that the conversions
                // will be insered on.
                addRequiredPhantom(node->child1().node());
                addRequiredPhantom(node->child2().node());

                // We don't need to do ref'ing on the children because we're stealing them from
                // the original division.
                Node* newDivision = m_insertionSet.insertNode(
                    m_indexInBlock, SpecBytecodeDouble, *node);
                newDivision->setResult(NodeResultDouble);
                
                node->setOp(DoubleAsInt32);
                node->children.initialize(Edge(newDivision, DoubleRepUse), Edge(), Edge());
                if (bytecodeCanIgnoreNegativeZero(node->arithNodeFlags()))
                    node->setArithMode(Arith::CheckOverflow);
                else
                    node->setArithMode(Arith::CheckOverflowAndNegativeZero);
                break;
            }
            fixDoubleOrBooleanEdge(node->child1());
            fixDoubleOrBooleanEdge(node->child2());
            node->setResult(NodeResultDouble);
            break;
        }
            
        case ArithMin:
        case ArithMax: {
            if (Node::shouldSpeculateInt32OrBooleanForArithmetic(node->child1().node(), node->child2().node())
                && node->canSpeculateInt32(FixupPass)) {
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
            if (node->child1()->shouldSpeculateInt32OrBooleanForArithmetic()
                && node->canSpeculateInt32(FixupPass)) {
                fixIntOrBooleanEdge(node->child1());
                break;
            }
            fixDoubleOrBooleanEdge(node->child1());
            node->setResult(NodeResultDouble);
            break;
        }
            
        case ArithSqrt:
        case ArithFRound:
        case ArithSin:
        case ArithCos: {
            fixDoubleOrBooleanEdge(node->child1());
            node->setResult(NodeResultDouble);
            break;
        }
            
        case LogicalNot: {
            if (node->child1()->shouldSpeculateBoolean())
                fixEdge<BooleanUse>(node->child1());
            else if (node->child1()->shouldSpeculateObjectOrOther())
                fixEdge<ObjectOrOtherUse>(node->child1());
            else if (node->child1()->shouldSpeculateInt32OrBoolean())
                fixIntOrBooleanEdge(node->child1());
            else if (node->child1()->shouldSpeculateNumber())
                fixEdge<DoubleRepUse>(node->child1());
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
            if (node->op() == CompareEq
                && Node::shouldSpeculateBoolean(node->child1().node(), node->child2().node())) {
                fixEdge<BooleanUse>(node->child1());
                fixEdge<BooleanUse>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (Node::shouldSpeculateInt32OrBoolean(node->child1().node(), node->child2().node())) {
                fixIntOrBooleanEdge(node->child1());
                fixIntOrBooleanEdge(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (enableInt52()
                && Node::shouldSpeculateMachineInt(node->child1().node(), node->child2().node())) {
                fixEdge<Int52RepUse>(node->child1());
                fixEdge<Int52RepUse>(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (Node::shouldSpeculateNumberOrBoolean(node->child1().node(), node->child2().node())) {
                fixDoubleOrBooleanEdge(node->child1());
                fixDoubleOrBooleanEdge(node->child2());
                node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
                break;
            }
            if (node->op() != CompareEq)
                break;
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
                fixEdge<Int52RepUse>(node->child1());
                fixEdge<Int52RepUse>(node->child2());
                break;
            }
            if (Node::shouldSpeculateNumber(node->child1().node(), node->child2().node())) {
                fixEdge<DoubleRepUse>(node->child1());
                fixEdge<DoubleRepUse>(node->child2());
                break;
            }
            if (node->child1()->shouldSpeculateStringIdent() && node->child2()->shouldSpeculateStringIdent()) {
                fixEdge<StringIdentUse>(node->child1());
                fixEdge<StringIdentUse>(node->child2());
                break;
            }
            if (node->child1()->shouldSpeculateString() && node->child2()->shouldSpeculateString() && (GPRInfo::numberOfRegisters >= 7 || isFTL(m_graph.m_plan.mode))) {
                fixEdge<StringUse>(node->child1());
                fixEdge<StringUse>(node->child2());
                break;
            }
            if (node->child1()->shouldSpeculateObject() && node->child2()->shouldSpeculateObject()) {
                fixEdge<ObjectUse>(node->child1());
                fixEdge<ObjectUse>(node->child2());
                break;
            }
            if (node->child1()->shouldSpeculateMisc()) {
                fixEdge<MiscUse>(node->child1());
                break;
            }
            if (node->child2()->shouldSpeculateMisc()) {
                fixEdge<MiscUse>(node->child2());
                break;
            }
            if (node->child1()->shouldSpeculateStringIdent()
                && node->child2()->shouldSpeculateNotStringVar()) {
                fixEdge<StringIdentUse>(node->child1());
                fixEdge<NotStringVarUse>(node->child2());
                break;
            }
            if (node->child2()->shouldSpeculateStringIdent()
                && node->child1()->shouldSpeculateNotStringVar()) {
                fixEdge<StringIdentUse>(node->child2());
                fixEdge<NotStringVarUse>(node->child1());
                break;
            }
            if (node->child1()->shouldSpeculateString() && (GPRInfo::numberOfRegisters >= 8 || isFTL(m_graph.m_plan.mode))) {
                fixEdge<StringUse>(node->child1());
                break;
            }
            if (node->child2()->shouldSpeculateString() && (GPRInfo::numberOfRegisters >= 8 || isFTL(m_graph.m_plan.mode))) {
                fixEdge<StringUse>(node->child2());
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
                    m_graph, node,
                    node->child1()->prediction(),
                    node->child2()->prediction(),
                    SpecNone, node->flags()));
            
            blessArrayOperation(node->child1(), node->child2(), node->child3());
            
            ArrayMode arrayMode = node->arrayMode();
            switch (arrayMode.type()) {
            case Array::Double:
                if (arrayMode.arrayClass() == Array::OriginalArray
                    && arrayMode.speculation() == Array::InBounds
                    && m_graph.globalObjectFor(node->origin.semantic)->arrayPrototypeChainIsSane()
                    && !(node->flags() & NodeBytecodeUsesAsOther))
                    node->setArrayMode(arrayMode.withSpeculation(Array::SaneChain));
                break;
                
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
            
            switch (arrayMode.type()) {
            case Array::Double:
                if (!arrayMode.isOutOfBounds())
                    node->setResult(NodeResultDouble);
                break;
                
            case Array::Float32Array:
            case Array::Float64Array:
                node->setResult(NodeResultDouble);
                break;
                
            case Array::Uint32Array:
                if (node->shouldSpeculateInt32())
                    break;
                if (node->shouldSpeculateMachineInt() && enableInt52())
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

            node->setArrayMode(
                node->arrayMode().refine(
                    m_graph, node,
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
                else if (child3->shouldSpeculateMachineInt())
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
            case Array::Arguments:
                fixEdge<KnownCellUse>(child1);
                fixEdge<Int32Use>(child2);
                insertStoreBarrier(m_indexInBlock, child1);
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
                    m_graph, node,
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
                fixEdge<DoubleRepRealUse>(node->child2());
                break;
            case Array::Contiguous:
            case Array::ArrayStorage:
                insertStoreBarrier(m_indexInBlock, node->child1());
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
            // FIXME: We should just be able to do shouldSpeculateInt32OrBoolean() and
            // shouldSpeculateNumberOrBoolean() here, but we can't because then the Branch
            // could speculate on the result of a non-speculative conversion node.
            // https://bugs.webkit.org/show_bug.cgi?id=126778
            else if (node->child1()->shouldSpeculateInt32())
                fixEdge<Int32Use>(node->child1());
            else if (node->child1()->shouldSpeculateNumber())
                fixEdge<DoubleRepUse>(node->child1());

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
                        
                        BranchData* data = node->branchData();
                        std::swap(data->taken, data->notTaken);
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
            ECMAMode ecmaMode = m_graph.executableFor(node->origin.semantic)->isStrictMode() ? StrictMode : NotStrictMode;

            if (node->child1()->shouldSpeculateOther()) {
                if (ecmaMode == StrictMode) {
                    fixEdge<OtherUse>(node->child1());
                    node->convertToIdentity();
                    break;
                }

                m_insertionSet.insertNode(
                    m_indexInBlock, SpecNone, Phantom, node->origin,
                    Edge(node->child1().node(), OtherUse));
                observeUseKindOnNode<OtherUse>(node->child1().node());
                node->convertToWeakConstant(m_graph.globalThisObjectFor(node->origin.semantic));
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
            insertStoreBarrier(m_indexInBlock, node->child1());
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
        case PutByIdFlush:
        case PutByIdDirect: {
            fixEdge<CellUse>(node->child1());
            insertStoreBarrier(m_indexInBlock, node->child1());
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
            
        case MultiGetByOffset: {
            fixEdge<CellUse>(node->child1());
            break;
        }
            
        case PutByOffset: {
            if (!node->child1()->hasStorageResult())
                fixEdge<KnownCellUse>(node->child1());
            fixEdge<KnownCellUse>(node->child2());
            insertStoreBarrier(m_indexInBlock, node->child2());
            break;
        }
            
        case MultiPutByOffset: {
            fixEdge<CellUse>(node->child1());
            insertStoreBarrier(m_indexInBlock, node->child1());
            break;
        }
            
        case InstanceOf: {
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

        case FiatInt52: {
            RELEASE_ASSERT(enableInt52());
            node->convertToIdentity();
            fixEdge<Int52RepUse>(node->child1());
            node->setResult(NodeResultInt52);
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
        case InvalidationPoint:
        case CheckArray:
        case CheckInBounds:
        case ConstantStoragePointer:
        case DoubleAsInt32:
        case ValueToInt32:
        case HardPhantom: // HardPhantom would be trivial to handle but anyway we assert that we won't see it here yet.
        case DoubleRep:
        case ValueRep:
        case Int52Rep:
        case DoubleConstant:
        case Int52Constant:
        case Identity: // This should have been cleaned up.
        case BooleanToNumber:
            // These are just nodes that we don't currently expect to see during fixup.
            // If we ever wanted to insert them prior to fixup, then we just have to create
            // fixup rules for them.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        
        case PutGlobalVar: {
            Node* globalObjectNode = m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, WeakJSConstant, node->origin, 
                OpInfo(m_graph.globalObjectFor(node->origin.semantic)));
            Node* barrierNode = m_graph.addNode(
                SpecNone, StoreBarrier, m_currentNode->origin, 
                Edge(globalObjectNode, KnownCellUse));
            m_insertionSet.insert(m_indexInBlock, barrierNode);
            break;
        }

        case TearOffActivation: {
            Node* barrierNode = m_graph.addNode(
                SpecNone, StoreBarrierWithNullCheck, m_currentNode->origin, 
                Edge(node->child1().node(), UntypedUse));
            m_insertionSet.insert(m_indexInBlock, barrierNode);
            break;
        }

        case IsString:
            if (node->child1()->shouldSpeculateString()) {
                m_insertionSet.insertNode(
                    m_indexInBlock, SpecNone, Phantom, node->origin,
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
    }
    
    template<UseKind useKind>
    void createToString(Node* node, Edge& edge)
    {
        edge.setNode(m_insertionSet.insertNode(
            m_indexInBlock, SpecString, ToString, node->origin,
            Edge(edge.node(), useKind)));
    }
    
    template<UseKind useKind>
    void attemptToForceStringArrayModeByToStringConversion(ArrayMode& arrayMode, Node* node)
    {
        ASSERT(arrayMode == ArrayMode(Array::Generic));
        
        if (!canOptimizeStringObjectAccess(node->origin.semantic))
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
                m_indexInBlock, SpecNone, Phantom, node->origin,
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
            && canOptimizeStringObjectAccess(node->origin.semantic)) {
            fixEdge<StringObjectUse>(node->child1());
            node->convertToToString();
            return;
        }
        
        if (node->child1()->shouldSpeculateStringOrStringObject()
            && canOptimizeStringObjectAccess(node->origin.semantic)) {
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
            && canOptimizeStringObjectAccess(node->origin.semantic)) {
            fixEdge<StringObjectUse>(node->child1());
            return;
        }
        
        if (node->child1()->shouldSpeculateStringOrStringObject()
            && canOptimizeStringObjectAccess(node->origin.semantic)) {
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
        
        if (isStringObjectUse<leftUseKind>() && !canOptimizeStringObjectAccess(node->origin.semantic))
            return false;
        
        convertStringAddUse<leftUseKind>(node, left);
        
        if (right->shouldSpeculateString())
            convertStringAddUse<StringUse>(node, right);
        else if (right->shouldSpeculateStringObject() && canOptimizeStringObjectAccess(node->origin.semantic))
            convertStringAddUse<StringObjectUse>(node, right);
        else if (right->shouldSpeculateStringOrStringObject() && canOptimizeStringObjectAccess(node->origin.semantic))
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
                m_indexInBlock, resultOfToPrimitive(right->prediction()), ToPrimitive,
                node->origin, Edge(right.node()));
            Node* toString = m_insertionSet.insertNode(
                m_indexInBlock, SpecString, ToString, node->origin, Edge(toPrimitive));
            
            fixupToPrimitive(toPrimitive);
            fixupToString(toString);
            
            right.setNode(toString);
        }
        
        // We're doing checks up there, so we need to make sure that the
        // *original* inputs to the addition are live up to here.
        m_insertionSet.insertNode(
            m_indexInBlock, SpecNone, Phantom, node->origin,
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
    
    Node* checkArray(ArrayMode arrayMode, const NodeOrigin& origin, Node* array, Node* index, bool (*storageCheck)(const ArrayMode&) = canCSEStorage)
    {
        ASSERT(arrayMode.isSpecific());
        
        if (arrayMode.type() == Array::String) {
            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, Phantom, origin, Edge(array, StringUse));
        } else {
            Structure* structure = arrayMode.originalArrayStructure(m_graph, origin.semantic);
        
            Edge indexEdge = index ? Edge(index, Int32Use) : Edge();
        
            if (arrayMode.doesConversion()) {
                if (structure) {
                    m_insertionSet.insertNode(
                        m_indexInBlock, SpecNone, ArrayifyToStructure, origin,
                        OpInfo(structure), OpInfo(arrayMode.asWord()), Edge(array, CellUse), indexEdge);
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
            return 0;
        
        if (arrayMode.usesButterfly()) {
            return m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, GetButterfly, origin, Edge(array, CellUse));
        }
        
        return m_insertionSet.insertNode(
            m_indexInBlock, SpecNone, GetIndexedPropertyStorage, origin,
            OpInfo(arrayMode.asWord()), Edge(array, KnownCellUse));
    }
    
    void blessArrayOperation(Edge base, Edge index, Edge& storageChild)
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
            Node* storage = checkArray(node->arrayMode(), node->origin, base.node(), index.node());
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
        case DoubleRepUse:
        case DoubleRepRealUse:
            if (variable->doubleFormatState() == UsingDoubleFormat)
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        case BooleanUse:
            if (alwaysUnboxSimplePrimitives()
                || isBooleanSpeculation(variable->prediction()))
                m_profitabilityChanged |= variable->mergeIsProfitableToUnbox(true);
            break;
        case Int52RepUse:
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
    
    template<UseKind useKind>
    void fixEdge(Edge& edge)
    {
        observeUseKindOnNode<useKind>(edge.node());
        edge.setUseKind(useKind);
    }
    
    void insertStoreBarrier(unsigned indexInBlock, Edge child1)
    {
        Node* barrierNode = m_graph.addNode(SpecNone, StoreBarrier, m_currentNode->origin, child1);
        m_insertionSet.insert(indexInBlock, barrierNode);
    }

    void fixIntConvertingEdge(Edge& edge)
    {
        Node* node = edge.node();
        if (node->shouldSpeculateInt32OrBoolean()) {
            fixIntOrBooleanEdge(edge);
            return;
        }
        
        UseKind useKind;
        if (node->shouldSpeculateMachineInt())
            useKind = Int52RepUse;
        else if (node->shouldSpeculateNumber())
            useKind = DoubleRepUse;
        else
            useKind = NotCellUse;
        Node* newNode = m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32, ValueToInt32, m_currentNode->origin,
            Edge(node, useKind));
        observeUseKindOnNode(node, useKind);
        
        edge = Edge(newNode, KnownInt32Use);
        addRequiredPhantom(node);
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
            m_indexInBlock, SpecInt32, BooleanToNumber, m_currentNode->origin,
            Edge(node, useKind));
        observeUseKindOnNode(node, useKind);
        
        edge = Edge(newNode, Int32Use);
        addRequiredPhantom(node);
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
            m_indexInBlock, SpecInt32, BooleanToNumber, m_currentNode->origin,
            Edge(node, useKind));
        observeUseKindOnNode(node, useKind);
        
        edge = Edge(newNode, DoubleRepUse);
        addRequiredPhantom(node);
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
            m_indexInBlock, SpecInt32, JSConstant, m_currentNode->origin,
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
        
        if (m_graph.addShouldSpeculateMachineInt(node)) {
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
            profiledBlock->getArrayProfile(node->origin.semantic.bytecodeIndex);
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
            m_graph, node, node->child1()->prediction(), node->prediction());
            
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
            node->origin, node->child1().node(), ArrayMode(toArrayType(type)));
        
        Node* shiftAmount = m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32, JSConstant, node->origin,
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
            
        Node* storage = checkArray(arrayMode, node->origin, node->child1().node(), 0, lengthNeedsStorage);
        if (!storage)
            return;
            
        node->child2() = Edge(storage);
    }
    
    Node* prependGetArrayLength(NodeOrigin origin, Node* child, ArrayMode arrayMode)
    {
        Node* storage = checkArray(arrayMode, origin, child, 0, lengthNeedsStorage);
        return m_insertionSet.insertNode(
            m_indexInBlock, SpecInt32, GetArrayLength, origin,
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
            ArrayMode(toArrayType(type)), node->origin, node->child1().node(),
            0, neverNeedsStorage);
        
        node->setOp(GetTypedArrayByteOffset);
        node->clearFlags(NodeMustGenerate | NodeClobbersWorld);
        fixEdge<KnownCellUse>(node->child1());
        return true;
    }
    
    void injectTypeConversionsInBlock(BasicBlock* block)
    {
        if (!block)
            return;
        ASSERT(block->isReachable);
        m_block = block;
        for (m_indexInBlock = 0; m_indexInBlock < block->size(); ++m_indexInBlock) {
            m_currentNode = block->at(m_indexInBlock);
            addPhantomsIfNecessary();
            tryToRelaxRepresentation(m_currentNode);
            DFG_NODE_DO_TO_CHILDREN(m_graph, m_currentNode, injectTypeConversionsForEdge);
        }
        clearPhantomsAtEnd();
        m_insertionSet.execute(block);
    }
    
    void tryToRelaxRepresentation(Node* node)
    {
        // Some operations may be able to operate more efficiently over looser representations.
        // Identify those here. This avoids inserting a redundant representation conversion.
        // Also, for some operations, like MovHint, this is a necessary optimization: inserting
        // an otherwise-dead conversion just for a MovHint would break OSR's understanding of
        // the IR.
        
        switch (node->op()) {
        case MovHint:
        case Phantom:
        case Check:
        case HardPhantom:
            DFG_NODE_DO_TO_CHILDREN(m_graph, m_currentNode, fixEdgeRepresentation);
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
    }
    
    void fixEdgeRepresentation(Node*, Edge& edge)
    {
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
            
        default:
            break;
        }
    }
    
    void injectTypeConversionsForEdge(Node* node, Edge& edge)
    {
        ASSERT(node == m_currentNode);
        Node* result = nullptr;
        
        switch (edge.useKind()) {
        case DoubleRepUse:
        case DoubleRepRealUse:
        case DoubleRepMachineIntUse: {
            if (edge->hasDoubleResult())
                break;
            
            addRequiredPhantom(edge.node());

            if (edge->op() == JSConstant && m_graph.isNumberConstant(edge.node())) {
                result = m_insertionSet.insertNode(
                    m_indexInBlock, SpecBytecodeDouble, DoubleConstant, node->origin,
                    OpInfo(m_graph.constantRegisterForConstant(
                        jsDoubleNumber(m_graph.valueOfNumberConstant(edge.node())))));
            } else if (edge->hasInt52Result()) {
                result = m_insertionSet.insertNode(
                    m_indexInBlock, SpecInt52AsDouble, DoubleRep, node->origin,
                    Edge(edge.node(), Int52RepUse));
            } else {
                result = m_insertionSet.insertNode(
                    m_indexInBlock, SpecBytecodeDouble, DoubleRep, node->origin,
                    Edge(edge.node(), NumberUse));
            }

            edge.setNode(result);
            break;
        }
            
        case Int52RepUse: {
            if (edge->hasInt52Result())
                break;
            
            addRequiredPhantom(edge.node());

            if (edge->op() == JSConstant && m_graph.isMachineIntConstant(edge.node())) {
                result = m_insertionSet.insertNode(
                    m_indexInBlock, SpecMachineInt, Int52Constant, node->origin,
                    OpInfo(edge->constantNumber()));
            } else if (edge->hasDoubleResult()) {
                result = m_insertionSet.insertNode(
                    m_indexInBlock, SpecMachineInt, Int52Rep, node->origin,
                    Edge(edge.node(), DoubleRepMachineIntUse));
            } else if (edge->shouldSpeculateInt32ForArithmetic()) {
                result = m_insertionSet.insertNode(
                    m_indexInBlock, SpecInt32, Int52Rep, node->origin,
                    Edge(edge.node(), Int32Use));
            } else {
                result = m_insertionSet.insertNode(
                    m_indexInBlock, SpecMachineInt, Int52Rep, node->origin,
                    Edge(edge.node(), MachineIntUse));
            }

            edge.setNode(result);
            break;
        }
            
        default: {
            if (!edge->hasDoubleResult() && !edge->hasInt52Result())
                break;
            
            addRequiredPhantom(edge.node());
            
            if (edge->hasDoubleResult()) {
                result = m_insertionSet.insertNode(
                    m_indexInBlock, SpecBytecodeDouble, ValueRep, node->origin,
                    Edge(edge.node(), DoubleRepUse));
            } else {
                result = m_insertionSet.insertNode(
                    m_indexInBlock, SpecInt32 | SpecInt52AsDouble, ValueRep, node->origin,
                    Edge(edge.node(), Int52RepUse));
            }
            
            edge.setNode(result);
            break;
        } }
    }
    
    void addRequiredPhantom(Node* node)
    {
        m_requiredPhantoms.append(node);
    }

    void addPhantomsIfNecessary()
    {
        if (m_requiredPhantoms.isEmpty())
            return;
        
        for (unsigned i = m_requiredPhantoms.size(); i--;) {
            Node* node = m_requiredPhantoms[i];
            m_insertionSet.insertNode(
                m_indexInBlock, SpecNone, Phantom, m_currentNode->origin,
                node->defaultEdge());
        }
        
        m_requiredPhantoms.resize(0);
    }
    
    void clearPhantomsAtEnd()
    {
        // Terminal nodes don't need post-phantoms, and inserting them would violate
        // the current requirement that a terminal is the last thing in a block. We
        // should eventually change that requirement. Currently we get around this by
        // ensuring that all terminals accept just one input, and if that input is a
        // conversion node then no further speculations will be performed. See
        // references to the bug, below, for places where we have to have hacks to
        // work around this.
        // FIXME: Get rid of this by allowing Phantoms after terminals.
        // https://bugs.webkit.org/show_bug.cgi?id=126778
        
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

