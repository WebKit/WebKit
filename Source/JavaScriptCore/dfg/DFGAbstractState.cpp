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
#include "DFGAbstractState.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGBasicBlock.h"

namespace JSC { namespace DFG {

AbstractState::AbstractState(Graph& graph)
    : m_codeBlock(graph.m_codeBlock)
    , m_graph(graph)
    , m_variables(m_codeBlock->numParameters(), graph.m_localVars)
    , m_block(0)
{
    m_nodes.resize(graph.size());
}

AbstractState::~AbstractState() { }

void AbstractState::beginBasicBlock(BasicBlock* basicBlock)
{
    ASSERT(!m_block);
    
    ASSERT(basicBlock->variablesAtHead.numberOfLocals() == basicBlock->valuesAtHead.numberOfLocals());
    ASSERT(basicBlock->variablesAtTail.numberOfLocals() == basicBlock->valuesAtTail.numberOfLocals());
    ASSERT(basicBlock->variablesAtHead.numberOfLocals() == basicBlock->variablesAtTail.numberOfLocals());
    
    for (size_t i = 0; i < basicBlock->size(); i++)
        m_nodes[basicBlock->at(i)].clear();

    m_variables = basicBlock->valuesAtHead;
    m_haveStructures = false;
    for (size_t i = 0; i < m_variables.numberOfArguments(); ++i) {
        if (m_variables.argument(i).m_structure.isNeitherClearNorTop()) {
            m_haveStructures = true;
            break;
        }
    }
    for (size_t i = 0; i < m_variables.numberOfLocals(); ++i) {
        if (m_variables.local(i).m_structure.isNeitherClearNorTop()) {
            m_haveStructures = true;
            break;
        }
    }
    
    basicBlock->cfaShouldRevisit = false;
    basicBlock->cfaHasVisited = true;
    m_block = basicBlock;
    m_isValid = true;
    m_foundConstants = false;
    m_branchDirection = InvalidBranchDirection;
}

void AbstractState::initialize(Graph& graph)
{
    BasicBlock* root = graph.m_blocks[0].get();
    root->cfaShouldRevisit = true;
    root->cfaHasVisited = false;
    root->cfaFoundConstants = false;
    for (size_t i = 0; i < root->valuesAtHead.numberOfArguments(); ++i) {
        Node& node = graph[root->variablesAtHead.argument(i)];
        ASSERT(node.op() == SetArgument);
        if (!node.shouldGenerate()) {
            // The argument is dead. We don't do any checks for such arguments, and so
            // for the purpose of the analysis, they contain no value.
            root->valuesAtHead.argument(i).clear();
            continue;
        }
        
        if (node.variableAccessData()->isCaptured()) {
            root->valuesAtHead.argument(i).makeTop();
            continue;
        }
        
        PredictedType prediction = node.variableAccessData()->prediction();
        if (isInt32Prediction(prediction))
            root->valuesAtHead.argument(i).set(PredictInt32);
        else if (isArrayPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictArray);
        else if (isBooleanPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictBoolean);
        else if (isInt8ArrayPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictInt8Array);
        else if (isInt16ArrayPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictInt16Array);
        else if (isInt32ArrayPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictInt32Array);
        else if (isUint8ArrayPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictUint8Array);
        else if (isUint8ClampedArrayPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictUint8ClampedArray);
        else if (isUint16ArrayPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictUint16Array);
        else if (isUint32ArrayPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictUint32Array);
        else if (isFloat32ArrayPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictFloat32Array);
        else if (isFloat64ArrayPrediction(prediction))
            root->valuesAtHead.argument(i).set(PredictFloat64Array);
        else
            root->valuesAtHead.argument(i).makeTop();
        
        root->valuesAtTail.argument(i).clear();
    }
    for (size_t i = 0; i < root->valuesAtHead.numberOfLocals(); ++i) {
        NodeIndex nodeIndex = root->variablesAtHead.local(i);
        if (nodeIndex != NoNode && graph[nodeIndex].variableAccessData()->isCaptured())
            root->valuesAtHead.local(i).makeTop();
        else
            root->valuesAtHead.local(i).clear();
        root->valuesAtTail.local(i).clear();
    }
    for (BlockIndex blockIndex = 1 ; blockIndex < graph.m_blocks.size(); ++blockIndex) {
        BasicBlock* block = graph.m_blocks[blockIndex].get();
        if (!block)
            continue;
        if (!block->isReachable)
            continue;
        block->cfaShouldRevisit = false;
        block->cfaHasVisited = false;
        block->cfaFoundConstants = false;
        for (size_t i = 0; i < block->valuesAtHead.numberOfArguments(); ++i) {
            block->valuesAtHead.argument(i).clear();
            block->valuesAtTail.argument(i).clear();
        }
        for (size_t i = 0; i < block->valuesAtHead.numberOfLocals(); ++i) {
            block->valuesAtHead.local(i).clear();
            block->valuesAtTail.local(i).clear();
        }
    }
}

bool AbstractState::endBasicBlock(MergeMode mergeMode, BranchDirection* branchDirectionPtr)
{
    ASSERT(m_block);
    
    BasicBlock* block = m_block; // Save the block for successor merging.
    
    block->cfaFoundConstants = m_foundConstants;
    
    if (!m_isValid) {
        reset();
        return false;
    }
    
    bool changed = false;
    
    if (mergeMode != DontMerge || !ASSERT_DISABLED) {
        for (size_t argument = 0; argument < block->variablesAtTail.numberOfArguments(); ++argument) {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("        Merging state for argument %zu.\n", argument);
#endif
            AbstractValue& destination = block->valuesAtTail.argument(argument);
            changed |= mergeStateAtTail(destination, m_variables.argument(argument), block->variablesAtTail.argument(argument));
        }
        
        for (size_t local = 0; local < block->variablesAtTail.numberOfLocals(); ++local) {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("        Merging state for local %zu.\n", local);
#endif
            AbstractValue& destination = block->valuesAtTail.local(local);
            changed |= mergeStateAtTail(destination, m_variables.local(local), block->variablesAtTail.local(local));
        }
    }
    
    ASSERT(mergeMode != DontMerge || !changed);
    
    BranchDirection branchDirection = m_branchDirection;
    if (branchDirectionPtr)
        *branchDirectionPtr = branchDirection;
    
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
    dataLog("        Branch direction = %s\n", branchDirectionToString(branchDirection));
#endif
    
    reset();
    
    if (mergeMode != MergeToSuccessors)
        return changed;
    
    return mergeToSuccessors(m_graph, block, branchDirection);
}

void AbstractState::reset()
{
    m_block = 0;
    m_isValid = false;
    m_branchDirection = InvalidBranchDirection;
}

bool AbstractState::execute(unsigned indexInBlock)
{
    ASSERT(m_block);
    ASSERT(m_isValid);
        
    NodeIndex nodeIndex = m_block->at(indexInBlock);
    Node& node = m_graph[nodeIndex];
        
    if (!node.shouldGenerate())
        return true;
        
    switch (node.op()) {
    case JSConstant:
    case WeakJSConstant:
    case PhantomArguments: {
        forNode(nodeIndex).set(m_graph.valueOfJSConstant(nodeIndex));
        node.setCanExit(false);
        break;
    }
            
    case GetLocal: {
        VariableAccessData* variableAccessData = node.variableAccessData();
        bool canExit = false;
        canExit |= variableAccessData->prediction() == PredictNone;
        AbstractValue value = m_variables.operand(variableAccessData->local());
        if (!variableAccessData->isCaptured()) {
            if (value.isClear())
                canExit |= true;
        }
        if (value.value())
            m_foundConstants = true;
        forNode(nodeIndex) = value;
        node.setCanExit(canExit);
        break;
    }
        
    case GetLocalUnlinked: {
        AbstractValue value = m_variables.operand(node.unlinkedLocal());
        if (value.value())
            m_foundConstants = true;
        forNode(nodeIndex) = value;
        node.setCanExit(false);
        break;
    }
        
    case SetLocal: {
        if (node.variableAccessData()->isCaptured()) {
            m_variables.operand(node.local()) = forNode(node.child1());
            node.setCanExit(false);
            break;
        }
        
        if (node.variableAccessData()->shouldUseDoubleFormat()) {
            speculateNumberUnary(node);
            m_variables.operand(node.local()).set(PredictDouble);
            break;
        }
        
        PredictedType predictedType = node.variableAccessData()->argumentAwarePrediction();
        if (isInt32Prediction(predictedType))
            speculateInt32Unary(node);
        else if (isArrayPrediction(predictedType)) {
            node.setCanExit(!isArrayPrediction(forNode(node.child1()).m_type));
            forNode(node.child1()).filter(PredictArray);
        } else if (isBooleanPrediction(predictedType))
            speculateBooleanUnary(node);
        else
            node.setCanExit(false);
        
        m_variables.operand(node.local()) = forNode(node.child1());
        break;
    }
            
    case SetArgument:
        // Assert that the state of arguments has been set.
        ASSERT(!m_block->valuesAtHead.operand(node.local()).isClear());
        node.setCanExit(false);
        break;
            
    case BitAnd:
    case BitOr:
    case BitXor:
    case BitRShift:
    case BitLShift:
    case BitURShift: {
        JSValue left = forNode(node.child1()).value();
        JSValue right = forNode(node.child2()).value();
        if (left && right && left.isInt32() && right.isInt32()) {
            int32_t a = left.asInt32();
            int32_t b = right.asInt32();
            switch (node.op()) {
            case BitAnd:
                forNode(nodeIndex).set(JSValue(a & b));
                break;
            case BitOr:
                forNode(nodeIndex).set(JSValue(a | b));
                break;
            case BitXor:
                forNode(nodeIndex).set(JSValue(a ^ b));
                break;
            case BitRShift:
                forNode(nodeIndex).set(JSValue(a >> static_cast<uint32_t>(b)));
                break;
            case BitLShift:
                forNode(nodeIndex).set(JSValue(a << static_cast<uint32_t>(b)));
                break;
            case BitURShift:
                forNode(nodeIndex).set(JSValue(static_cast<uint32_t>(a) >> static_cast<uint32_t>(b)));
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        speculateInt32Binary(node);
        forNode(nodeIndex).set(PredictInt32);
        break;
    }
        
    case UInt32ToNumber: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()) {
            ASSERT(child.isInt32());
            forNode(nodeIndex).set(JSValue(child.asUInt32()));
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (!node.canSpeculateInteger()) {
            forNode(nodeIndex).set(PredictDouble);
            node.setCanExit(false);
        } else {
            forNode(nodeIndex).set(PredictInt32);
            node.setCanExit(true);
        }
        break;
    }
              
            
    case DoubleAsInt32: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()) {
            double asDouble = child.asNumber();
            int32_t asInt = JSC::toInt32(asDouble);
            if (bitwise_cast<int64_t>(static_cast<double>(asInt)) == bitwise_cast<int64_t>(asDouble)) {
                forNode(nodeIndex).set(JSValue(asInt));
                m_foundConstants = true;
                break;
            }
        }
        node.setCanExit(true);
        forNode(node.child1()).filter(PredictNumber);
        forNode(nodeIndex).set(PredictInt32);
        break;
    }
            
    case ValueToInt32: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()) {
            if (child.isInt32())
                forNode(nodeIndex).set(child);
            else
                forNode(nodeIndex).set(JSValue(JSC::toInt32(child.asDouble())));
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateInteger())
            speculateInt32Unary(node);
        else if (m_graph[node.child1()].shouldSpeculateNumber())
            speculateNumberUnary(node);
        else if (m_graph[node.child1()].shouldSpeculateBoolean())
            speculateBooleanUnary(node);
        else
            node.setCanExit(false);
        
        forNode(nodeIndex).set(PredictInt32);
        break;
    }
        
    case Int32ToDouble: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()) {
            forNode(nodeIndex).set(JSValue(JSValue::EncodeAsDouble, child.asNumber()));
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        speculateNumberUnary(node);
        forNode(nodeIndex).set(PredictDouble);
        break;
    }
        
    case CheckNumber:
        forNode(node.child1()).filter(PredictNumber);
        break;
            
    case ValueAdd:
    case ArithAdd: {
        JSValue left = forNode(node.child1()).value();
        JSValue right = forNode(node.child2()).value();
        if (left && right && left.isNumber() && right.isNumber()) {
            forNode(nodeIndex).set(JSValue(left.asNumber() + right.asNumber()));
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph.addShouldSpeculateInteger(node)) {
            speculateInt32Binary(
                node, !nodeCanTruncateInteger(node.arithNodeFlags()));
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        if (Node::shouldSpeculateNumber(m_graph[node.child1()], m_graph[node.child2()])) {
            speculateNumberBinary(node);
            forNode(nodeIndex).set(PredictDouble);
            break;
        }
        if (node.op() == ValueAdd) {
            clobberWorld(node.codeOrigin, indexInBlock);
            forNode(nodeIndex).set(PredictString | PredictInt32 | PredictNumber);
            node.setCanExit(false);
            break;
        }
        // We don't handle this yet. :-(
        m_isValid = false;
        node.setCanExit(true);
        break;
    }
            
    case ArithSub: {
        JSValue left = forNode(node.child1()).value();
        JSValue right = forNode(node.child2()).value();
        if (left && right && left.isNumber() && right.isNumber()) {
            forNode(nodeIndex).set(JSValue(left.asNumber() - right.asNumber()));
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph.addShouldSpeculateInteger(node)) {
            speculateInt32Binary(
                node, !nodeCanTruncateInteger(node.arithNodeFlags()));
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        speculateNumberBinary(node);
        forNode(nodeIndex).set(PredictDouble);
        break;
    }
        
    case ArithNegate: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()) {
            forNode(nodeIndex).set(JSValue(-child.asNumber()));
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph.negateShouldSpeculateInteger(node)) {
            speculateInt32Unary(
                node, !nodeCanTruncateInteger(node.arithNodeFlags()));
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        speculateNumberUnary(node);
        forNode(nodeIndex).set(PredictDouble);
        break;
    }
        
    case ArithMul: {
        JSValue left = forNode(node.child1()).value();
        JSValue right = forNode(node.child2()).value();
        if (left && right && left.isNumber() && right.isNumber()) {
            forNode(nodeIndex).set(JSValue(left.asNumber() * right.asNumber()));
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph.mulShouldSpeculateInteger(node)) {
            speculateInt32Binary(
                node,
                !nodeCanTruncateInteger(node.arithNodeFlags())
                || !nodeCanIgnoreNegativeZero(node.arithNodeFlags()));
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        speculateNumberBinary(node);
        forNode(nodeIndex).set(PredictDouble);
        break;
    }
        
    case ArithDiv:
    case ArithMin:
    case ArithMax:
    case ArithMod: {
        JSValue left = forNode(node.child1()).value();
        JSValue right = forNode(node.child2()).value();
        if (left && right && left.isNumber() && right.isNumber()) {
            double a = left.asNumber();
            double b = right.asNumber();
            switch (node.op()) {
            case ArithDiv:
                forNode(nodeIndex).set(JSValue(a / b));
                break;
            case ArithMin:
                forNode(nodeIndex).set(JSValue(a < b ? a : (b <= a ? b : a + b)));
                break;
            case ArithMax:
                forNode(nodeIndex).set(JSValue(a > b ? a : (b >= a ? b : a + b)));
                break;
            case ArithMod:
                forNode(nodeIndex).set(JSValue(fmod(a, b)));
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (Node::shouldSpeculateInteger(
                m_graph[node.child1()], m_graph[node.child2()])
            && node.canSpeculateInteger()) {
            speculateInt32Binary(node, true); // forcing can-exit, which is a bit on the conservative side.
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        speculateNumberBinary(node);
        forNode(nodeIndex).set(PredictDouble);
        break;
    }
            
    case ArithAbs: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()) {
            forNode(nodeIndex).set(JSValue(fabs(child.asNumber())));
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateInteger()
            && node.canSpeculateInteger()) {
            speculateInt32Unary(node, true);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        speculateNumberUnary(node);
        forNode(nodeIndex).set(PredictDouble);
        break;
    }
            
    case ArithSqrt: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()) {
            forNode(nodeIndex).set(JSValue(sqrt(child.asNumber())));
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        speculateNumberUnary(node);
        forNode(nodeIndex).set(PredictDouble);
        break;
    }
            
    case LogicalNot: {
        JSValue childConst = forNode(node.child1()).value();
        if (childConst) {
            forNode(nodeIndex).set(jsBoolean(!childConst.toBoolean()));
            node.setCanExit(false);
            break;
        }
        Node& child = m_graph[node.child1()];
        if (isBooleanPrediction(child.prediction()))
            speculateBooleanUnary(node);
        else if (child.shouldSpeculateFinalObjectOrOther()) {
            node.setCanExit(
                !isFinalObjectOrOtherPrediction(forNode(node.child1()).m_type));
            forNode(node.child1()).filter(PredictFinalObject | PredictOther);
        } else if (child.shouldSpeculateArrayOrOther()) {
            node.setCanExit(
                !isArrayOrOtherPrediction(forNode(node.child1()).m_type));
            forNode(node.child1()).filter(PredictArray | PredictOther);
        } else if (child.shouldSpeculateInteger())
            speculateInt32Unary(node);
        else if (child.shouldSpeculateNumber())
            speculateNumberUnary(node);
        else
            node.setCanExit(false);
        forNode(nodeIndex).set(PredictBoolean);
        break;
    }
        
    case IsUndefined:
    case IsBoolean:
    case IsNumber:
    case IsString:
    case IsObject:
    case IsFunction: {
        node.setCanExit(false);
        JSValue child = forNode(node.child1()).value();
        if (child) {
            bool foundConstant = true;
            switch (node.op()) {
            case IsUndefined:
                forNode(nodeIndex).set(jsBoolean(
                    child.isCell()
                    ? child.asCell()->structure()->typeInfo().masqueradesAsUndefined()
                    : child.isUndefined()));
                break;
            case IsBoolean:
                forNode(nodeIndex).set(jsBoolean(child.isBoolean()));
                break;
            case IsNumber:
                forNode(nodeIndex).set(jsBoolean(child.isNumber()));
                break;
            case IsString:
                forNode(nodeIndex).set(jsBoolean(isJSString(child)));
                break;
            default:
                break;
            }
            if (foundConstant) {
                m_foundConstants = true;
                break;
            }
        }
        forNode(nodeIndex).set(PredictBoolean);
        break;
    }
            
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareEq: {
        JSValue leftConst = forNode(node.child1()).value();
        JSValue rightConst = forNode(node.child2()).value();
        if (leftConst && rightConst && leftConst.isNumber() && rightConst.isNumber()) {
            double a = leftConst.asNumber();
            double b = rightConst.asNumber();
            switch (node.op()) {
            case CompareLess:
                forNode(nodeIndex).set(jsBoolean(a < b));
                break;
            case CompareLessEq:
                forNode(nodeIndex).set(jsBoolean(a <= b));
                break;
            case CompareGreater:
                forNode(nodeIndex).set(jsBoolean(a > b));
                break;
            case CompareGreaterEq:
                forNode(nodeIndex).set(jsBoolean(a >= b));
                break;
            case CompareEq:
                forNode(nodeIndex).set(jsBoolean(a == b));
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        
        forNode(nodeIndex).set(PredictBoolean);
        
        Node& left = m_graph[node.child1()];
        Node& right = m_graph[node.child2()];
        PredictedType filter;
        PredictionChecker checker;
        if (Node::shouldSpeculateInteger(left, right)) {
            filter = PredictInt32;
            checker = isInt32Prediction;
        } else if (Node::shouldSpeculateNumber(left, right)) {
            filter = PredictNumber;
            checker = isNumberPrediction;
        } else if (node.op() == CompareEq) {
            if ((m_graph.isConstant(node.child1().index())
                 && m_graph.valueOfJSConstant(node.child1().index()).isNull())
                || (m_graph.isConstant(node.child2().index())
                    && m_graph.valueOfJSConstant(node.child2().index()).isNull())) {
                // We know that this won't clobber the world. But that's all we know.
                node.setCanExit(false);
                break;
            }
            
            if (Node::shouldSpeculateFinalObject(left, right)) {
                filter = PredictFinalObject;
                checker = isFinalObjectPrediction;
            } else if (Node::shouldSpeculateArray(left, right)) {
                filter = PredictArray;
                checker = isArrayPrediction;
            } else if (left.shouldSpeculateFinalObject() && right.shouldSpeculateFinalObjectOrOther()) {
                node.setCanExit(
                    !isFinalObjectPrediction(forNode(node.child1()).m_type)
                    || !isFinalObjectOrOtherPrediction(forNode(node.child2()).m_type));
                forNode(node.child1()).filter(PredictFinalObject);
                forNode(node.child2()).filter(PredictFinalObject | PredictOther);
                break;
            } else if (right.shouldSpeculateFinalObject() && left.shouldSpeculateFinalObjectOrOther()) {
                node.setCanExit(
                    !isFinalObjectOrOtherPrediction(forNode(node.child1()).m_type)
                    || !isFinalObjectPrediction(forNode(node.child2()).m_type));
                forNode(node.child1()).filter(PredictFinalObject | PredictOther);
                forNode(node.child2()).filter(PredictFinalObject);
                break;
            } else if (left.shouldSpeculateArray() && right.shouldSpeculateArrayOrOther()) {
                node.setCanExit(
                    !isArrayPrediction(forNode(node.child1()).m_type)
                    || !isArrayOrOtherPrediction(forNode(node.child2()).m_type));
                forNode(node.child1()).filter(PredictArray);
                forNode(node.child2()).filter(PredictArray | PredictOther);
                break;
            } else if (right.shouldSpeculateArray() && left.shouldSpeculateArrayOrOther()) {
                node.setCanExit(
                    !isArrayOrOtherPrediction(forNode(node.child1()).m_type)
                    || !isArrayPrediction(forNode(node.child2()).m_type));
                forNode(node.child1()).filter(PredictArray | PredictOther);
                forNode(node.child2()).filter(PredictArray);
                break;
            } else {
                filter = PredictTop;
                checker = isAnyPrediction;
                clobberWorld(node.codeOrigin, indexInBlock);
            }
        } else {
            filter = PredictTop;
            checker = isAnyPrediction;
            clobberWorld(node.codeOrigin, indexInBlock);
        }
        node.setCanExit(
            !checker(forNode(node.child1()).m_type)
            || !checker(forNode(node.child2()).m_type));
        forNode(node.child1()).filter(filter);
        forNode(node.child2()).filter(filter);
        break;
    }
            
    case CompareStrictEq: {
        JSValue left = forNode(node.child1()).value();
        JSValue right = forNode(node.child2()).value();
        if (left && right && left.isNumber() && right.isNumber()) {
            forNode(nodeIndex).set(jsBoolean(left.asNumber() == right.asNumber()));
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        forNode(nodeIndex).set(PredictBoolean);
        if (m_graph.isJSConstant(node.child1().index())) {
            JSValue value = m_graph.valueOfJSConstant(node.child1().index());
            if (!value.isNumber() && !value.isString()) {
                node.setCanExit(false);
                break;
            }
        }
        if (m_graph.isJSConstant(node.child2().index())) {
            JSValue value = m_graph.valueOfJSConstant(node.child2().index());
            if (!value.isNumber() && !value.isString()) {
                node.setCanExit(false);
                break;
            }
        }
        if (Node::shouldSpeculateInteger(
                m_graph[node.child1()], m_graph[node.child2()])) {
            speculateInt32Binary(node);
            break;
        }
        if (Node::shouldSpeculateNumber(
                m_graph[node.child1()], m_graph[node.child2()])) {
            speculateNumberBinary(node);
            break;
        }
        if (Node::shouldSpeculateFinalObject(
                m_graph[node.child1()], m_graph[node.child2()])) {
            node.setCanExit(
                !isFinalObjectPrediction(forNode(node.child1()).m_type)
                || !isFinalObjectPrediction(forNode(node.child2()).m_type));
            forNode(node.child1()).filter(PredictFinalObject);
            forNode(node.child2()).filter(PredictFinalObject);
            break;
        }
        if (Node::shouldSpeculateArray(
                m_graph[node.child1()], m_graph[node.child2()])) {
            node.setCanExit(
                !isArrayPrediction(forNode(node.child1()).m_type)
                || !isArrayPrediction(forNode(node.child2()).m_type));
            forNode(node.child1()).filter(PredictArray);
            forNode(node.child2()).filter(PredictArray);
            break;
        }
        node.setCanExit(false);
        break;
    }
        
    case StringCharCodeAt:
        node.setCanExit(true);
        forNode(node.child1()).filter(PredictString);
        forNode(node.child2()).filter(PredictInt32);
        forNode(nodeIndex).set(PredictInt32);
        break;
        
    case StringCharAt:
        node.setCanExit(true);
        forNode(node.child1()).filter(PredictString);
        forNode(node.child2()).filter(PredictInt32);
        forNode(nodeIndex).set(PredictString);
        break;
            
    case GetByVal: {
        node.setCanExit(true);
        if (!node.prediction() || !m_graph[node.child1()].prediction() || !m_graph[node.child2()].prediction()) {
            m_isValid = false;
            break;
        }
        if (!isActionableArrayPrediction(m_graph[node.child1()].prediction()) || !m_graph[node.child2()].shouldSpeculateInteger()) {
            clobberWorld(node.codeOrigin, indexInBlock);
            forNode(nodeIndex).makeTop();
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateArguments()) {
            forNode(node.child1()).filter(PredictArguments);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).makeTop();
            break;
        }
        if (m_graph[node.child1()].prediction() == PredictString) {
            forNode(node.child1()).filter(PredictString);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictString);
            break;
        }
        
        if (m_graph[node.child1()].shouldSpeculateInt8Array()) {
            forNode(node.child1()).filter(PredictInt8Array);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateInt16Array()) {
            forNode(node.child1()).filter(PredictInt16Array);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateInt32Array()) {
            forNode(node.child1()).filter(PredictInt32Array);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint8Array()) {
            forNode(node.child1()).filter(PredictUint8Array);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint8ClampedArray()) {
            forNode(node.child1()).filter(PredictUint8ClampedArray);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint16Array()) {
            forNode(node.child1()).filter(PredictUint16Array);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint32Array()) {
            forNode(node.child1()).filter(PredictUint32Array);
            forNode(node.child2()).filter(PredictInt32);
            if (node.shouldSpeculateInteger())
                forNode(nodeIndex).set(PredictInt32);
            else
                forNode(nodeIndex).set(PredictDouble);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateFloat32Array()) {
            forNode(node.child1()).filter(PredictFloat32Array);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictDouble);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateFloat64Array()) {
            forNode(node.child1()).filter(PredictFloat64Array);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictDouble);
            break;
        }
        ASSERT(m_graph[node.child1()].shouldSpeculateArray());
        forNode(node.child1()).filter(PredictArray);
        forNode(node.child2()).filter(PredictInt32);
        forNode(nodeIndex).makeTop();
        break;
    }
            
    case PutByVal:
    case PutByValAlias: {
        node.setCanExit(true);
        if (!m_graph[node.child1()].prediction() || !m_graph[node.child2()].prediction()) {
            m_isValid = false;
            break;
        }
        if (!m_graph[node.child2()].shouldSpeculateInteger() || !isActionableMutableArrayPrediction(m_graph[node.child1()].prediction())
#if USE(JSVALUE32_64)
            || m_graph[node.child1()].shouldSpeculateArguments()
#endif
            ) {
            ASSERT(node.op() == PutByVal);
            clobberWorld(node.codeOrigin, indexInBlock);
            forNode(nodeIndex).makeTop();
            break;
        }
        
        if (m_graph[node.child1()].shouldSpeculateArguments()) {
            forNode(node.child1()).filter(PredictArguments);
            forNode(node.child2()).filter(PredictInt32);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateInt8Array()) {
            forNode(node.child1()).filter(PredictInt8Array);
            forNode(node.child2()).filter(PredictInt32);
            if (m_graph[node.child3()].shouldSpeculateInteger())
                forNode(node.child3()).filter(PredictInt32);
            else
                forNode(node.child3()).filter(PredictNumber);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateInt16Array()) {
            forNode(node.child1()).filter(PredictInt16Array);
            forNode(node.child2()).filter(PredictInt32);
            if (m_graph[node.child3()].shouldSpeculateInteger())
                forNode(node.child3()).filter(PredictInt32);
            else
                forNode(node.child3()).filter(PredictNumber);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateInt32Array()) {
            forNode(node.child1()).filter(PredictInt32Array);
            forNode(node.child2()).filter(PredictInt32);
            if (m_graph[node.child3()].shouldSpeculateInteger())
                forNode(node.child3()).filter(PredictInt32);
            else
                forNode(node.child3()).filter(PredictNumber);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint8Array()) {
            forNode(node.child1()).filter(PredictUint8Array);
            forNode(node.child2()).filter(PredictInt32);
            if (m_graph[node.child3()].shouldSpeculateInteger())
                forNode(node.child3()).filter(PredictInt32);
            else
                forNode(node.child3()).filter(PredictNumber);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint8ClampedArray()) {
            forNode(node.child1()).filter(PredictUint8ClampedArray);
            forNode(node.child2()).filter(PredictInt32);
            if (m_graph[node.child3()].shouldSpeculateInteger())
                forNode(node.child3()).filter(PredictInt32);
            else
                forNode(node.child3()).filter(PredictNumber);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint16Array()) {
            forNode(node.child1()).filter(PredictUint16Array);
            forNode(node.child2()).filter(PredictInt32);
            if (m_graph[node.child3()].shouldSpeculateInteger())
                forNode(node.child3()).filter(PredictInt32);
            else
                forNode(node.child3()).filter(PredictNumber);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint32Array()) {
            forNode(node.child1()).filter(PredictUint32Array);
            forNode(node.child2()).filter(PredictInt32);
            if (m_graph[node.child3()].shouldSpeculateInteger())
                forNode(node.child3()).filter(PredictInt32);
            else
                forNode(node.child3()).filter(PredictNumber);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateFloat32Array()) {
            forNode(node.child1()).filter(PredictFloat32Array);
            forNode(node.child2()).filter(PredictInt32);
            forNode(node.child3()).filter(PredictNumber);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateFloat64Array()) {
            forNode(node.child1()).filter(PredictFloat64Array);
            forNode(node.child2()).filter(PredictInt32);
            forNode(node.child3()).filter(PredictNumber);
            break;
        }
        ASSERT(m_graph[node.child1()].shouldSpeculateArray());
        forNode(node.child1()).filter(PredictArray);
        forNode(node.child2()).filter(PredictInt32);
        if (node.op() == PutByVal)
            clobberWorld(node.codeOrigin, indexInBlock);
        break;
    }
            
    case ArrayPush:
        node.setCanExit(true);
        forNode(node.child1()).filter(PredictArray);
        forNode(nodeIndex).set(PredictNumber);
        break;
            
    case ArrayPop:
        node.setCanExit(true);
        forNode(node.child1()).filter(PredictArray);
        forNode(nodeIndex).makeTop();
        break;
            
    case RegExpExec:
    case RegExpTest:
        node.setCanExit(
            !isCellPrediction(forNode(node.child1()).m_type)
            || !isCellPrediction(forNode(node.child2()).m_type));
        forNode(node.child1()).filter(PredictCell);
        forNode(node.child2()).filter(PredictCell);
        forNode(nodeIndex).makeTop();
        break;
            
    case Jump:
        node.setCanExit(false);
        break;
            
    case Branch: {
        JSValue value = forNode(node.child1()).value();
        if (value) {
            bool booleanValue = value.toBoolean();
            if (booleanValue)
                m_branchDirection = TakeTrue;
            else
                m_branchDirection = TakeFalse;
            node.setCanExit(false);
            break;
        }
        // FIXME: The above handles the trivial cases of sparse conditional
        // constant propagation, but we can do better:
        // 1) If the abstract value does not have a concrete value but describes
        //    something that is known to evaluate true (or false) then we ought
        //    to sparse conditional that.
        // 2) We can specialize the source variable's value on each direction of
        //    the branch.
        Node& child = m_graph[node.child1()];
        if (child.shouldSpeculateBoolean())
            speculateBooleanUnary(node);
        else if (child.shouldSpeculateFinalObjectOrOther()) {
            node.setCanExit(
                !isFinalObjectOrOtherPrediction(forNode(node.child1()).m_type));
            forNode(node.child1()).filter(PredictFinalObject | PredictOther);
        } else if (child.shouldSpeculateArrayOrOther()) {
            node.setCanExit(
                !isArrayOrOtherPrediction(forNode(node.child1()).m_type));
            forNode(node.child1()).filter(PredictArray | PredictOther);
        } else if (child.shouldSpeculateInteger())
            speculateInt32Unary(node);
        else if (child.shouldSpeculateNumber())
            speculateNumberUnary(node);
        else
            node.setCanExit(false);
        m_branchDirection = TakeBoth;
        break;
    }
            
    case Return:
        m_isValid = false;
        node.setCanExit(false);
        break;
        
    case Throw:
    case ThrowReferenceError:
        m_isValid = false;
        node.setCanExit(true);
        break;
            
    case ToPrimitive: {
        JSValue childConst = forNode(node.child1()).value();
        if (childConst && childConst.isNumber()) {
            forNode(nodeIndex).set(childConst);
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        
        Node& child = m_graph[node.child1()];
        if (child.shouldSpeculateInteger()) {
            speculateInt32Unary(node);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }

        AbstractValue& source = forNode(node.child1());
        AbstractValue& destination = forNode(nodeIndex);
            
        PredictedType type = source.m_type;
        if (type & ~(PredictNumber | PredictString | PredictBoolean)) {
            type &= (PredictNumber | PredictString | PredictBoolean);
            type |= PredictString;
        }
        destination.set(type);
        node.setCanExit(false);
        break;
    }
            
    case StrCat:
        node.setCanExit(false);
        forNode(nodeIndex).set(PredictString);
        break;
            
    case NewArray:
    case NewArrayBuffer:
        node.setCanExit(false);
        forNode(nodeIndex).set(m_codeBlock->globalObject()->arrayStructure());
        m_haveStructures = true;
        break;
            
    case NewRegexp:
        node.setCanExit(false);
        forNode(nodeIndex).set(m_codeBlock->globalObject()->regExpStructure());
        m_haveStructures = true;
        break;
            
    case ConvertThis: {
        Node& child = m_graph[node.child1()];
        AbstractValue& source = forNode(node.child1());
        AbstractValue& destination = forNode(nodeIndex);
            
        if (isObjectPrediction(source.m_type)) {
            // This is the simple case. We already know that the source is an
            // object, so there's nothing to do. I don't think this case will
            // be hit, but then again, you never know.
            destination = source;
            node.setCanExit(false);
            break;
        }
        
        node.setCanExit(true);
        
        if (isOtherPrediction(child.prediction())) {
            source.filter(PredictOther);
            destination.set(PredictObjectOther);
            break;
        }
        
        if (isObjectPrediction(child.prediction())) {
            source.filter(PredictObjectMask);
            destination = source;
            break;
        }
            
        destination = source;
        destination.merge(PredictObjectOther);
        break;
    }

    case CreateThis: {
        AbstractValue& source = forNode(node.child1());
        AbstractValue& destination = forNode(nodeIndex);
        
        node.setCanExit(!isCellPrediction(source.m_type));
            
        source.filter(PredictFunction);
        destination.set(PredictFinalObject);
        break;
    }

    case NewObject:
        node.setCanExit(false);
        forNode(nodeIndex).set(m_codeBlock->globalObjectFor(node.codeOrigin)->emptyObjectStructure());
        m_haveStructures = true;
        break;
        
    case CreateActivation:
        node.setCanExit(false);
        forNode(nodeIndex).set(m_graph.m_globalData.activationStructure.get());
        m_haveStructures = true;
        break;
        
    case CreateArguments:
        node.setCanExit(false);
        forNode(nodeIndex).set(m_codeBlock->globalObjectFor(node.codeOrigin)->argumentsStructure());
        m_haveStructures = true;
        break;
        
    case TearOffActivation:
    case TearOffArguments:
        node.setCanExit(false);
        // Does nothing that is user-visible.
        break;

    case CheckArgumentsNotCreated:
        if (isEmptyPrediction(
                m_variables.operand(
                    m_graph.argumentsRegisterFor(node.codeOrigin)).m_type)) {
            node.setCanExit(false);
            m_foundConstants = true;
        } else
            node.setCanExit(true);
        break;
        
    case GetMyArgumentsLength:
        // We know that this executable does not escape its arguments, so we can optimize
        // the arguments a bit. Note that this is not sufficient to force constant folding
        // of GetMyArgumentsLength, because GetMyArgumentsLength is a clobbering operation.
        // We perform further optimizations on this later on.
        if (node.codeOrigin.inlineCallFrame)
            forNode(nodeIndex).set(jsNumber(node.codeOrigin.inlineCallFrame->arguments.size() - 1));
        else
            forNode(nodeIndex).set(PredictInt32);
        node.setCanExit(
            !isEmptyPrediction(
                m_variables.operand(
                    m_graph.argumentsRegisterFor(node.codeOrigin)).m_type));
        break;
        
    case GetMyArgumentsLengthSafe:
        node.setCanExit(false);
        // This potentially clobbers all structures if the arguments object had a getter
        // installed on the length property.
        clobberWorld(node.codeOrigin, indexInBlock);
        // We currently make no guarantee about what this returns because it does not
        // speculate that the length property is actually a length.
        forNode(nodeIndex).makeTop();
        break;
        
    case GetMyArgumentByVal:
        node.setCanExit(true);
        // We know that this executable does not escape its arguments, so we can optimize
        // the arguments a bit. Note that this ends up being further optimized by the
        // ArgumentsSimplificationPhase.
        forNode(node.child1()).filter(PredictInt32);
        forNode(nodeIndex).makeTop();
        break;
        
    case GetMyArgumentByValSafe:
        node.setCanExit(true);
        // This potentially clobbers all structures if the property we're accessing has
        // a getter. We don't speculate against this.
        clobberWorld(node.codeOrigin, indexInBlock);
        // But we do speculate that the index is an integer.
        forNode(node.child1()).filter(PredictInt32);
        // And the result is unknown.
        forNode(nodeIndex).makeTop();
        break;
        
    case NewFunction:
    case NewFunctionExpression:
    case NewFunctionNoCheck:
        node.setCanExit(false);
        forNode(nodeIndex).set(m_codeBlock->globalObjectFor(node.codeOrigin)->functionStructure());
        break;
        
    case GetCallee:
        node.setCanExit(false);
        forNode(nodeIndex).set(PredictFunction);
        break;
            
    case GetScopeChain:
        node.setCanExit(false);
        forNode(nodeIndex).set(PredictCellOther);
        break;
            
    case GetScopedVar:
        node.setCanExit(false);
        forNode(nodeIndex).makeTop();
        break;
            
    case PutScopedVar:
        node.setCanExit(false);
        clobberStructures(indexInBlock);
        break;
            
    case GetById:
    case GetByIdFlush:
        node.setCanExit(true);
        if (!node.prediction()) {
            m_isValid = false;
            break;
        }
        if (isCellPrediction(m_graph[node.child1()].prediction()))
            forNode(node.child1()).filter(PredictCell);
        clobberWorld(node.codeOrigin, indexInBlock);
        forNode(nodeIndex).makeTop();
        break;
            
    case GetArrayLength:
        node.setCanExit(true);
        forNode(node.child1()).filter(PredictArray);
        forNode(nodeIndex).set(PredictInt32);
        break;

    case GetArgumentsLength:
        node.setCanExit(true);
        forNode(node.child1()).filter(PredictArguments);
        forNode(nodeIndex).set(PredictInt32);
        break;

    case GetStringLength:
        node.setCanExit(!isStringPrediction(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(PredictString);
        forNode(nodeIndex).set(PredictInt32);
        break;
        
    case GetInt8ArrayLength:
        node.setCanExit(!isInt8ArrayPrediction(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(PredictInt8Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetInt16ArrayLength:
        node.setCanExit(!isInt16ArrayPrediction(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(PredictInt16Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetInt32ArrayLength:
        node.setCanExit(!isInt32ArrayPrediction(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(PredictInt32Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetUint8ArrayLength:
        node.setCanExit(!isUint8ArrayPrediction(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(PredictUint8Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetUint8ClampedArrayLength:
        node.setCanExit(!isUint8ClampedArrayPrediction(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(PredictUint8ClampedArray);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetUint16ArrayLength:
        node.setCanExit(!isUint16ArrayPrediction(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(PredictUint16Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetUint32ArrayLength:
        node.setCanExit(!isUint32ArrayPrediction(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(PredictUint32Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetFloat32ArrayLength:
        node.setCanExit(!isFloat32ArrayPrediction(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(PredictFloat32Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetFloat64ArrayLength:
        node.setCanExit(!isFloat64ArrayPrediction(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(PredictFloat64Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
            
    case CheckStructure: {
        // FIXME: We should be able to propagate the structure sets of constants (i.e. prototypes).
        AbstractValue& value = forNode(node.child1());
        node.setCanExit(
            !value.m_structure.isSubsetOf(node.structureSet())
            || !isCellPrediction(value.m_type));
        value.filter(node.structureSet());
        m_haveStructures = true;
        break;
    }
            
    case PutStructure:
    case PhantomPutStructure:
        node.setCanExit(false);
        clobberStructures(indexInBlock);
        forNode(node.child1()).set(node.structureTransitionData().newStructure);
        m_haveStructures = true;
        break;
    case GetPropertyStorage:
        node.setCanExit(false);
        forNode(node.child1()).filter(PredictCell);
        forNode(nodeIndex).clear(); // The result is not a JS value.
        break;
    case GetIndexedPropertyStorage: {
        node.setCanExit(true); // Lies, but this is (almost) always followed by GetByVal, which does exit. So no point in trying to be more precise.
        PredictedType basePrediction = m_graph[node.child2()].prediction();
        if (!(basePrediction & PredictInt32) && basePrediction) {
            forNode(nodeIndex).clear();
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateArguments()) {
            ASSERT_NOT_REACHED();
            break;
        }
        if (m_graph[node.child1()].prediction() == PredictString) {
            forNode(node.child1()).filter(PredictString);
            forNode(nodeIndex).clear();
            break;
        }
        
        if (m_graph[node.child1()].shouldSpeculateInt8Array()) {
            forNode(node.child1()).filter(PredictInt8Array);
            forNode(nodeIndex).clear();
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateInt16Array()) {
            forNode(node.child1()).filter(PredictInt16Array);
            forNode(nodeIndex).clear();
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateInt32Array()) {
            forNode(node.child1()).filter(PredictInt32Array);
            forNode(nodeIndex).clear();
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint8Array()) {
            forNode(node.child1()).filter(PredictUint8Array);
            forNode(nodeIndex).clear();
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint8ClampedArray()) {
            forNode(node.child1()).filter(PredictUint8ClampedArray);
            forNode(nodeIndex).clear();
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint16Array()) {
            forNode(node.child1()).filter(PredictUint16Array);
            forNode(nodeIndex).set(PredictOther);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateUint32Array()) {
            forNode(node.child1()).filter(PredictUint32Array);
            forNode(nodeIndex).clear();
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateFloat32Array()) {
            forNode(node.child1()).filter(PredictFloat32Array);
            forNode(nodeIndex).clear();
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateFloat64Array()) {
            forNode(node.child1()).filter(PredictFloat64Array);
            forNode(nodeIndex).clear();
            break;
        }
        forNode(node.child1()).filter(PredictArray);
        forNode(nodeIndex).clear();
        break; 
    }
    case GetByOffset:
        node.setCanExit(false);
        forNode(node.child1()).filter(PredictCell);
        forNode(nodeIndex).makeTop();
        break;
            
    case PutByOffset:
        node.setCanExit(false);
        forNode(node.child1()).filter(PredictCell);
        break;
            
    case CheckFunction:
        node.setCanExit(true); // Lies! We can do better.
        forNode(node.child1()).filter(PredictFunction);
        // FIXME: Should be able to propagate the fact that we know what the function is.
        break;
            
    case PutById:
    case PutByIdDirect:
        node.setCanExit(true);
        forNode(node.child1()).filter(PredictCell);
        clobberWorld(node.codeOrigin, indexInBlock);
        break;
            
    case GetGlobalVar:
        node.setCanExit(false);
        forNode(nodeIndex).makeTop();
        break;
            
    case PutGlobalVar:
        node.setCanExit(false);
        break;
            
    case CheckHasInstance:
        node.setCanExit(true);
        forNode(node.child1()).filter(PredictCell);
        // Sadly, we don't propagate the fact that we've done CheckHasInstance
        break;
            
    case InstanceOf:
        node.setCanExit(true);
        // Again, sadly, we don't propagate the fact that we've done InstanceOf
        if (!(m_graph[node.child1()].prediction() & ~PredictCell) && !(forNode(node.child1()).m_type & ~PredictCell))
            forNode(node.child1()).filter(PredictCell);
        forNode(node.child3()).filter(PredictCell);
        forNode(nodeIndex).set(PredictBoolean);
        break;
            
    case Phi:
    case Flush:
        node.setCanExit(false);
        break;
            
    case Breakpoint:
        node.setCanExit(false);
        break;
            
    case Call:
    case Construct:
    case Resolve:
    case ResolveBase:
    case ResolveBaseStrictPut:
    case ResolveGlobal:
        node.setCanExit(true);
        clobberWorld(node.codeOrigin, indexInBlock);
        forNode(nodeIndex).makeTop();
        break;
            
    case ForceOSRExit:
        node.setCanExit(true);
        m_isValid = false;
        break;
            
    case Phantom:
    case InlineStart:
    case Nop:
        node.setCanExit(false);
        break;
        
    case LastNodeType:
        ASSERT_NOT_REACHED();
        break;
    }
    
    return m_isValid;
}

inline void AbstractState::clobberWorld(const CodeOrigin& codeOrigin, unsigned indexInBlock)
{
    if (codeOrigin.inlineCallFrame) {
        const BitVector& capturedVars = codeOrigin.inlineCallFrame->capturedVars;
        for (size_t i = capturedVars.size(); i--;) {
            if (!capturedVars.quickGet(i))
                continue;
            m_variables.local(i).makeTop();
        }
    } else {
        for (size_t i = m_codeBlock->m_numCapturedVars; i--;)
            m_variables.local(i).makeTop();
    }
    if (m_codeBlock->argumentsAreCaptured()) {
        for (size_t i = m_variables.numberOfArguments(); i--;)
            m_variables.argument(i).makeTop();
    }
    clobberStructures(indexInBlock);
}

inline void AbstractState::clobberStructures(unsigned indexInBlock)
{
    if (!m_haveStructures)
        return;
    for (size_t i = indexInBlock + 1; i--;)
        forNode(m_block->at(i)).clobberStructures();
    for (size_t i = m_variables.numberOfArguments(); i--;)
        m_variables.argument(i).clobberStructures();
    for (size_t i = m_variables.numberOfLocals(); i--;)
        m_variables.local(i).clobberStructures();
    m_haveStructures = false;
}

inline bool AbstractState::mergeStateAtTail(AbstractValue& destination, AbstractValue& inVariable, NodeIndex nodeIndex)
{
    if (nodeIndex == NoNode)
        return false;
        
    AbstractValue source;
        
    Node& node = m_graph[nodeIndex];
    if (!node.refCount())
        return false;
    
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
    dataLog("          It's live, node @%u.\n", nodeIndex);
#endif
    
    if (node.variableAccessData()->isCaptured()) {
        source = inVariable;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("          Transfering ");
        source.dump(WTF::dataFile());
        dataLog(" from last access due to captured variable.\n");
#endif
    } else {
        switch (node.op()) {
        case Phi:
        case SetArgument:
        case Flush:
            // The block transfers the value from head to tail.
            source = inVariable;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("          Transfering ");
            source.dump(WTF::dataFile());
            dataLog(" from head to tail.\n");
#endif
            break;
            
        case GetLocal:
            // The block refines the value with additional speculations.
            source = forNode(nodeIndex);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("          Refining to ");
            source.dump(WTF::dataFile());
            dataLog("\n");
#endif
            break;
            
        case SetLocal:
            // The block sets the variable, and potentially refines it, both
            // before and after setting it.
            if (node.variableAccessData()->shouldUseDoubleFormat()) {
                // FIXME: This unnecessarily loses precision.
                source.set(PredictDouble);
            } else
                source = forNode(node.child1());
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("          Setting to ");
            source.dump(WTF::dataFile());
            dataLog("\n");
#endif
            break;
        
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    
    if (destination == source) {
        // Abstract execution did not change the output value of the variable, for this
        // basic block, on this iteration.
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("          Not changed!\n");
#endif
        return false;
    }
    
    // Abstract execution reached a new conclusion about the speculations reached about
    // this variable after execution of this basic block. Update the state, and return
    // true to indicate that the fixpoint must go on!
    destination = source;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
    dataLog("          Changed!\n");
#endif
    return true;
}

inline bool AbstractState::merge(BasicBlock* from, BasicBlock* to)
{
    ASSERT(from->variablesAtTail.numberOfArguments() == to->variablesAtHead.numberOfArguments());
    ASSERT(from->variablesAtTail.numberOfLocals() == to->variablesAtHead.numberOfLocals());
    
    bool changed = false;
    
    for (size_t argument = 0; argument < from->variablesAtTail.numberOfArguments(); ++argument) {
        AbstractValue& destination = to->valuesAtHead.argument(argument);
        changed |= mergeVariableBetweenBlocks(destination, from->valuesAtTail.argument(argument), to->variablesAtHead.argument(argument), from->variablesAtTail.argument(argument));
    }
    
    for (size_t local = 0; local < from->variablesAtTail.numberOfLocals(); ++local) {
        AbstractValue& destination = to->valuesAtHead.local(local);
        changed |= mergeVariableBetweenBlocks(destination, from->valuesAtTail.local(local), to->variablesAtHead.local(local), from->variablesAtTail.local(local));
    }

    if (!to->cfaHasVisited)
        changed = true;
    
    to->cfaShouldRevisit |= changed;
    
    return changed;
}

inline bool AbstractState::mergeToSuccessors(
    Graph& graph, BasicBlock* basicBlock, BranchDirection branchDirection)
{
    Node& terminal = graph[basicBlock->last()];
    
    ASSERT(terminal.isTerminal());
    
    switch (terminal.op()) {
    case Jump: {
        ASSERT(branchDirection == InvalidBranchDirection);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("        Merging to block #%u.\n", terminal.takenBlockIndex());
#endif
        return merge(basicBlock, graph.m_blocks[terminal.takenBlockIndex()].get());
    }
        
    case Branch: {
        ASSERT(branchDirection != InvalidBranchDirection);
        bool changed = false;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("        Merging to block #%u.\n", terminal.takenBlockIndex());
#endif
        if (branchDirection != TakeFalse)
            changed |= merge(basicBlock, graph.m_blocks[terminal.takenBlockIndex()].get());
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("        Merging to block #%u.\n", terminal.notTakenBlockIndex());
#endif
        if (branchDirection != TakeTrue)
            changed |= merge(basicBlock, graph.m_blocks[terminal.notTakenBlockIndex()].get());
        return changed;
    }
        
    case Return:
    case Throw:
    case ThrowReferenceError:
        ASSERT(branchDirection == InvalidBranchDirection);
        return false;
        
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

inline bool AbstractState::mergeVariableBetweenBlocks(AbstractValue& destination, AbstractValue& source, NodeIndex destinationNodeIndex, NodeIndex sourceNodeIndex)
{
    if (destinationNodeIndex == NoNode)
        return false;
    
    ASSERT_UNUSED(sourceNodeIndex, sourceNodeIndex != NoNode);
    
    // FIXME: We could do some sparse conditional propagation here!
    
    return destination.merge(source);
}

void AbstractState::dump(FILE* out)
{
    bool first = true;
    for (size_t i = 0; i < m_block->size(); ++i) {
        NodeIndex index = m_block->at(i);
        AbstractValue& value = m_nodes[index];
        if (value.isClear())
            continue;
        if (first)
            first = false;
        else
            fprintf(out, " ");
        fprintf(out, "@%lu:", static_cast<unsigned long>(index));
        value.dump(out);
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

