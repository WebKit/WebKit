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
#include "GetByIdStatus.h"
#include "PutByIdStatus.h"

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
    
    // This is usually a no-op, but it is possible that the graph has grown since the
    // abstract state was last used.
    m_nodes.resize(m_graph.size());
    
    for (size_t i = 0; i < basicBlock->size(); i++)
        m_nodes[basicBlock->at(i)].clear();

    m_variables = basicBlock->valuesAtHead;
    m_haveStructures = false;
    for (size_t i = 0; i < m_variables.numberOfArguments(); ++i) {
        if (m_variables.argument(i).m_currentKnownStructure.isNeitherClearNorTop()) {
            m_haveStructures = true;
            break;
        }
    }
    for (size_t i = 0; i < m_variables.numberOfLocals(); ++i) {
        if (m_variables.local(i).m_currentKnownStructure.isNeitherClearNorTop()) {
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
        
        SpeculatedType prediction = node.variableAccessData()->prediction();
        if (isInt32Speculation(prediction))
            root->valuesAtHead.argument(i).set(SpecInt32);
        else if (isBooleanSpeculation(prediction))
            root->valuesAtHead.argument(i).set(SpecBoolean);
        else if (isCellSpeculation(prediction))
            root->valuesAtHead.argument(i).set(SpecCell);
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
        if (!block->isOSRTarget)
            continue;
        if (block->bytecodeBegin != graph.m_osrEntryBytecodeIndex)
            continue;
        for (size_t i = 0; i < graph.m_mustHandleValues.size(); ++i) {
            AbstractValue value;
            value.setMostSpecific(graph.m_mustHandleValues[i]);
            int operand = graph.m_mustHandleValues.operandForIndex(i);
            block->valuesAtHead.operand(operand).merge(value);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLogF("    Initializing Block #%u, operand r%d, to ", blockIndex, operand);
            block->valuesAtHead.operand(operand).dump(WTF::dataFile());
            dataLogF("\n");
#endif
        }
        block->cfaShouldRevisit = true;
    }
}

bool AbstractState::endBasicBlock(MergeMode mergeMode)
{
    ASSERT(m_block);
    
    BasicBlock* block = m_block; // Save the block for successor merging.
    
    block->cfaFoundConstants = m_foundConstants;
    block->cfaDidFinish = m_isValid;
    block->cfaBranchDirection = m_branchDirection;
    
    if (!m_isValid) {
        reset();
        return false;
    }
    
    bool changed = false;
    
    if (mergeMode != DontMerge || !ASSERT_DISABLED) {
        for (size_t argument = 0; argument < block->variablesAtTail.numberOfArguments(); ++argument) {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLogF("        Merging state for argument %zu.\n", argument);
#endif
            AbstractValue& destination = block->valuesAtTail.argument(argument);
            changed |= mergeStateAtTail(destination, m_variables.argument(argument), block->variablesAtTail.argument(argument));
        }
        
        for (size_t local = 0; local < block->variablesAtTail.numberOfLocals(); ++local) {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLogF("        Merging state for local %zu.\n", local);
#endif
            AbstractValue& destination = block->valuesAtTail.local(local);
            changed |= mergeStateAtTail(destination, m_variables.local(local), block->variablesAtTail.local(local));
        }
    }
    
    ASSERT(mergeMode != DontMerge || !changed);
    
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
    dataLogF("        Branch direction = %s\n", branchDirectionToString(m_branchDirection));
#endif
    
    reset();
    
    if (mergeMode != MergeToSuccessors)
        return changed;
    
    return mergeToSuccessors(m_graph, block);
}

void AbstractState::reset()
{
    m_block = 0;
    m_isValid = false;
    m_branchDirection = InvalidBranchDirection;
}

AbstractState::BooleanResult AbstractState::booleanResult(Node& node, AbstractValue& value)
{
    JSValue childConst = value.value();
    if (childConst) {
        if (childConst.toBoolean(m_codeBlock->globalObjectFor(node.codeOrigin)->globalExec()))
            return DefinitelyTrue;
        return DefinitelyFalse;
    }

    // Next check if we can fold because we know that the source is an object or string and does not equal undefined.
    if (isCellSpeculation(value.m_type)
        && value.m_currentKnownStructure.hasSingleton()) {
        Structure* structure = value.m_currentKnownStructure.singleton();
        if (!structure->masqueradesAsUndefined(m_codeBlock->globalObjectFor(node.codeOrigin))
            && structure->typeInfo().type() != StringType)
            return DefinitelyTrue;
    }
    
    return UnknownBooleanResult;
}

bool AbstractState::execute(unsigned indexInBlock)
{
    ASSERT(m_block);
    ASSERT(m_isValid);
    
    m_didClobber = false;
    
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
        
    case Identity: {
        forNode(nodeIndex) = forNode(node.child1());
        node.setCanExit(false);
        break;
    }
            
    case GetLocal: {
        VariableAccessData* variableAccessData = node.variableAccessData();
        if (variableAccessData->prediction() == SpecNone) {
            m_isValid = false;
            node.setCanExit(true);
            break;
        }
        bool canExit = false;
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
        if (node.variableAccessData()->isCaptured()
            || m_graph.isCreatedThisArgument(node.local())) {
            m_variables.operand(node.local()) = forNode(node.child1());
            node.setCanExit(false);
            break;
        }
        
        if (node.variableAccessData()->shouldUseDoubleFormat()) {
            speculateNumberUnary(node);
            m_variables.operand(node.local()).set(SpecDouble);
            break;
        }
        
        SpeculatedType predictedType = node.variableAccessData()->argumentAwarePrediction();
        if (isInt32Speculation(predictedType))
            speculateInt32Unary(node);
        else if (isCellSpeculation(predictedType)) {
            node.setCanExit(!isCellSpeculation(forNode(node.child1()).m_type));
            forNode(node.child1()).filter(SpecCell);
        } else if (isBooleanSpeculation(predictedType))
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
            bool constantWasSet;
            switch (node.op()) {
            case BitAnd:
                constantWasSet = trySetConstant(nodeIndex, JSValue(a & b));
                break;
            case BitOr:
                constantWasSet = trySetConstant(nodeIndex, JSValue(a | b));
                break;
            case BitXor:
                constantWasSet = trySetConstant(nodeIndex, JSValue(a ^ b));
                break;
            case BitRShift:
                constantWasSet = trySetConstant(nodeIndex, JSValue(a >> static_cast<uint32_t>(b)));
                break;
            case BitLShift:
                constantWasSet = trySetConstant(nodeIndex, JSValue(a << static_cast<uint32_t>(b)));
                break;
            case BitURShift:
                constantWasSet = trySetConstant(nodeIndex, JSValue(static_cast<uint32_t>(a) >> static_cast<uint32_t>(b)));
                break;
            default:
                ASSERT_NOT_REACHED();
                constantWasSet = false;
            }
            if (constantWasSet) {
                m_foundConstants = true;
                node.setCanExit(false);
                break;
            }
        }
        speculateInt32Binary(node);
        forNode(nodeIndex).set(SpecInt32);
        break;
    }
        
    case UInt32ToNumber: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()) {
            ASSERT(child.isInt32());
            if (trySetConstant(nodeIndex, JSValue(child.asUInt32()))) {
                m_foundConstants = true;
                node.setCanExit(false);
                break;
            }
        }
        if (!node.canSpeculateInteger()) {
            forNode(nodeIndex).set(SpecDouble);
            node.setCanExit(false);
        } else {
            forNode(nodeIndex).set(SpecInt32);
            node.setCanExit(true);
        }
        break;
    }
              
            
    case DoubleAsInt32: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()) {
            double asDouble = child.asNumber();
            int32_t asInt = JSC::toInt32(asDouble);
            if (bitwise_cast<int64_t>(static_cast<double>(asInt)) == bitwise_cast<int64_t>(asDouble)
                && trySetConstant(nodeIndex, JSValue(asInt))) {
                m_foundConstants = true;
                break;
            }
        }
        node.setCanExit(true);
        forNode(node.child1()).filter(SpecNumber);
        forNode(nodeIndex).set(SpecInt32);
        break;
    }
            
    case ValueToInt32: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()) {
            bool constantWasSet;
            if (child.isInt32())
                constantWasSet = trySetConstant(nodeIndex, child);
            else
                constantWasSet = trySetConstant(nodeIndex, JSValue(JSC::toInt32(child.asDouble())));
            if (constantWasSet) {
                m_foundConstants = true;
                node.setCanExit(false);
                break;
            }
        }
        if (m_graph[node.child1()].shouldSpeculateInteger())
            speculateInt32Unary(node);
        else if (m_graph[node.child1()].shouldSpeculateNumber())
            speculateNumberUnary(node);
        else if (m_graph[node.child1()].shouldSpeculateBoolean())
            speculateBooleanUnary(node);
        else
            node.setCanExit(false);
        
        forNode(nodeIndex).set(SpecInt32);
        break;
    }
        
    case Int32ToDouble: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()
            && trySetConstant(nodeIndex, JSValue(JSValue::EncodeAsDouble, child.asNumber()))) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        speculateNumberUnary(node);
        if (isInt32Speculation(forNode(node.child1()).m_type))
            forNode(nodeIndex).set(SpecDoubleReal);
        else
            forNode(nodeIndex).set(SpecDouble);
        break;
    }
        
    case CheckNumber:
        forNode(node.child1()).filter(SpecNumber);
        break;
            
    case ValueAdd:
    case ArithAdd: {
        JSValue left = forNode(node.child1()).value();
        JSValue right = forNode(node.child2()).value();
        if (left && right && left.isNumber() && right.isNumber()
            && trySetConstant(nodeIndex, JSValue(left.asNumber() + right.asNumber()))) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph.addShouldSpeculateInteger(node)) {
            speculateInt32Binary(
                node, !nodeCanTruncateInteger(node.arithNodeFlags()));
            forNode(nodeIndex).set(SpecInt32);
            break;
        }
        if (Node::shouldSpeculateNumberExpectingDefined(m_graph[node.child1()], m_graph[node.child2()])) {
            speculateNumberBinary(node);
            if (isRealNumberSpeculation(forNode(node.child1()).m_type)
                && isRealNumberSpeculation(forNode(node.child2()).m_type))
                forNode(nodeIndex).set(SpecDoubleReal);
            else
                forNode(nodeIndex).set(SpecDouble);
            break;
        }
        if (node.op() == ValueAdd) {
            clobberWorld(node.codeOrigin, indexInBlock);
            forNode(nodeIndex).set(SpecString | SpecInt32 | SpecNumber);
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
        if (left && right && left.isNumber() && right.isNumber()
            && trySetConstant(nodeIndex, JSValue(left.asNumber() - right.asNumber()))) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph.addShouldSpeculateInteger(node)) {
            speculateInt32Binary(
                node, !nodeCanTruncateInteger(node.arithNodeFlags()));
            forNode(nodeIndex).set(SpecInt32);
            break;
        }
        speculateNumberBinary(node);
        forNode(nodeIndex).set(SpecDouble);
        break;
    }
        
    case ArithNegate: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()
            && trySetConstant(nodeIndex, JSValue(-child.asNumber()))) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph.negateShouldSpeculateInteger(node)) {
            speculateInt32Unary(
                node, !nodeCanTruncateInteger(node.arithNodeFlags()));
            forNode(nodeIndex).set(SpecInt32);
            break;
        }
        speculateNumberUnary(node);
        forNode(nodeIndex).set(SpecDouble);
        break;
    }
        
    case ArithMul: {
        JSValue left = forNode(node.child1()).value();
        JSValue right = forNode(node.child2()).value();
        if (left && right && left.isNumber() && right.isNumber()
            && trySetConstant(nodeIndex, JSValue(left.asNumber() * right.asNumber()))) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph.mulShouldSpeculateInteger(node)) {
            speculateInt32Binary(
                node,
                !nodeCanTruncateInteger(node.arithNodeFlags())
                || !nodeCanIgnoreNegativeZero(node.arithNodeFlags()));
            forNode(nodeIndex).set(SpecInt32);
            break;
        }
        speculateNumberBinary(node);
        if (isRealNumberSpeculation(forNode(node.child1()).m_type)
            || isRealNumberSpeculation(forNode(node.child2()).m_type))
            forNode(nodeIndex).set(SpecDoubleReal);
        else
            forNode(nodeIndex).set(SpecDouble);
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
            bool constantWasSet;
            switch (node.op()) {
            case ArithDiv:
                constantWasSet = trySetConstant(nodeIndex, JSValue(a / b));
                break;
            case ArithMin:
                constantWasSet = trySetConstant(nodeIndex, JSValue(a < b ? a : (b <= a ? b : a + b)));
                break;
            case ArithMax:
                constantWasSet = trySetConstant(nodeIndex, JSValue(a > b ? a : (b >= a ? b : a + b)));
                break;
            case ArithMod:
                constantWasSet = trySetConstant(nodeIndex, JSValue(fmod(a, b)));
                break;
            default:
                ASSERT_NOT_REACHED();
                constantWasSet = false;
                break;
            }
            if (constantWasSet) {
                m_foundConstants = true;
                node.setCanExit(false);
                break;
            }
        }
        if (Node::shouldSpeculateIntegerForArithmetic(
                m_graph[node.child1()], m_graph[node.child2()])
            && node.canSpeculateInteger()) {
            speculateInt32Binary(node, true); // forcing can-exit, which is a bit on the conservative side.
            forNode(nodeIndex).set(SpecInt32);
            break;
        }
        speculateNumberBinary(node);
        forNode(nodeIndex).set(SpecDouble);
        break;
    }
            
    case ArithAbs: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()
            && trySetConstant(nodeIndex, JSValue(fabs(child.asNumber())))) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        if (m_graph[node.child1()].shouldSpeculateIntegerForArithmetic()
            && node.canSpeculateInteger()) {
            speculateInt32Unary(node, true);
            forNode(nodeIndex).set(SpecInt32);
            break;
        }
        speculateNumberUnary(node);
        forNode(nodeIndex).set(SpecDouble);
        break;
    }
            
    case ArithSqrt: {
        JSValue child = forNode(node.child1()).value();
        if (child && child.isNumber()
            && trySetConstant(nodeIndex, JSValue(sqrt(child.asNumber())))) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        speculateNumberUnary(node);
        forNode(nodeIndex).set(SpecDouble);
        break;
    }
            
    case LogicalNot: {
        bool didSetConstant = false;
        switch (booleanResult(node, forNode(node.child1()))) {
        case DefinitelyTrue:
            didSetConstant = trySetConstant(nodeIndex, jsBoolean(false));
            break;
        case DefinitelyFalse:
            didSetConstant = trySetConstant(nodeIndex, jsBoolean(true));
            break;
        default:
            break;
        }
        if (didSetConstant) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        Node& child = m_graph[node.child1()];
        if (isBooleanSpeculation(child.prediction()))
            speculateBooleanUnary(node);
        else if (child.shouldSpeculateNonStringCellOrOther()) {
            node.setCanExit(true);
            forNode(node.child1()).filter((SpecCell & ~SpecString) | SpecOther);
        } else if (child.shouldSpeculateInteger())
            speculateInt32Unary(node);
        else if (child.shouldSpeculateNumber())
            speculateNumberUnary(node);
        else
            node.setCanExit(false);
        forNode(nodeIndex).set(SpecBoolean);
        break;
    }
        
    case IsUndefined:
    case IsBoolean:
    case IsNumber:
    case IsString:
    case IsObject:
    case IsFunction: {
        node.setCanExit(node.op() == IsUndefined && m_codeBlock->globalObjectFor(node.codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid());
        JSValue child = forNode(node.child1()).value();
        if (child) {
            bool constantWasSet;
            switch (node.op()) {
            case IsUndefined:
                if (m_codeBlock->globalObjectFor(node.codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid()) {
                    constantWasSet = trySetConstant(nodeIndex, jsBoolean(
                        child.isCell()
                        ? false 
                        : child.isUndefined()));
                } else {
                    constantWasSet = trySetConstant(nodeIndex, jsBoolean(
                        child.isCell()
                        ? child.asCell()->structure()->masqueradesAsUndefined(m_codeBlock->globalObjectFor(node.codeOrigin))
                        : child.isUndefined()));
                }
                break;
            case IsBoolean:
                constantWasSet = trySetConstant(nodeIndex, jsBoolean(child.isBoolean()));
                break;
            case IsNumber:
                constantWasSet = trySetConstant(nodeIndex, jsBoolean(child.isNumber()));
                break;
            case IsString:
                constantWasSet = trySetConstant(nodeIndex, jsBoolean(isJSString(child)));
                break;
            default:
                constantWasSet = false;
                break;
            }
            if (constantWasSet) {
                m_foundConstants = true;
                break;
            }
        }
        forNode(nodeIndex).set(SpecBoolean);
        break;
    }
            
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareEq: {
        bool constantWasSet = false;

        JSValue leftConst = forNode(node.child1()).value();
        JSValue rightConst = forNode(node.child2()).value();
        if (leftConst && rightConst && leftConst.isNumber() && rightConst.isNumber()) {
            double a = leftConst.asNumber();
            double b = rightConst.asNumber();
            switch (node.op()) {
            case CompareLess:
                constantWasSet = trySetConstant(nodeIndex, jsBoolean(a < b));
                break;
            case CompareLessEq:
                constantWasSet = trySetConstant(nodeIndex, jsBoolean(a <= b));
                break;
            case CompareGreater:
                constantWasSet = trySetConstant(nodeIndex, jsBoolean(a > b));
                break;
            case CompareGreaterEq:
                constantWasSet = trySetConstant(nodeIndex, jsBoolean(a >= b));
                break;
            case CompareEq:
                constantWasSet = trySetConstant(nodeIndex, jsBoolean(a == b));
                break;
            default:
                ASSERT_NOT_REACHED();
                constantWasSet = false;
                break;
            }
        }
        
        if (!constantWasSet && node.op() == CompareEq) {
            SpeculatedType leftType = forNode(node.child1()).m_type;
            SpeculatedType rightType = forNode(node.child2()).m_type;
            if ((isInt32Speculation(leftType) && isOtherSpeculation(rightType))
                || (isOtherSpeculation(leftType) && isInt32Speculation(rightType)))
                constantWasSet = trySetConstant(nodeIndex, jsBoolean(false));
        }
        
        if (constantWasSet) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        
        forNode(nodeIndex).set(SpecBoolean);
        
        Node& left = m_graph[node.child1()];
        Node& right = m_graph[node.child2()];
        SpeculatedType filter;
        SpeculatedTypeChecker checker;
        if (Node::shouldSpeculateInteger(left, right)) {
            filter = SpecInt32;
            checker = isInt32Speculation;
        } else if (Node::shouldSpeculateNumber(left, right)) {
            filter = SpecNumber;
            checker = isNumberSpeculation;
        } else if (node.op() == CompareEq) {
            if ((m_graph.isConstant(node.child1().index())
                 && m_graph.valueOfJSConstant(node.child1().index()).isNull())
                || (m_graph.isConstant(node.child2().index())
                    && m_graph.valueOfJSConstant(node.child2().index()).isNull())) {
                // We can exit if we haven't fired the MasqueradesAsUndefind watchpoint yet.
                node.setCanExit(m_codeBlock->globalObjectFor(node.codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid());
                break;
            }
            
            if (left.shouldSpeculateString() || right.shouldSpeculateString()) {
                node.setCanExit(false);
                break;
            } 
            if (left.shouldSpeculateNonStringCell() && right.shouldSpeculateNonStringCellOrOther()) {
                node.setCanExit(true);
                forNode(node.child1()).filter(SpecCell & ~SpecString);
                forNode(node.child2()).filter((SpecCell & ~SpecString) | SpecOther);
                break;
            }
            if (left.shouldSpeculateNonStringCellOrOther() && right.shouldSpeculateNonStringCell()) {
                node.setCanExit(true);
                forNode(node.child1()).filter((SpecCell & ~SpecString) | SpecOther);
                forNode(node.child2()).filter(SpecCell & ~SpecString);
                break;
            }
            if (left.shouldSpeculateNonStringCell() && right.shouldSpeculateNonStringCell()) {
                node.setCanExit(true);
                forNode(node.child1()).filter(SpecCell & ~SpecString);
                forNode(node.child2()).filter(SpecCell & ~SpecString);
                break;
            }
 
            filter = SpecTop;
            checker = isAnySpeculation;
            clobberWorld(node.codeOrigin, indexInBlock);
        } else {
            filter = SpecTop;
            checker = isAnySpeculation;
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
        if (left && right && left.isNumber() && right.isNumber()
            && trySetConstant(nodeIndex, jsBoolean(left.asNumber() == right.asNumber()))) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        forNode(nodeIndex).set(SpecBoolean);
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
        Node& leftNode = m_graph[node.child1()];
        Node& rightNode = m_graph[node.child2()];
        if (leftNode.shouldSpeculateString() || rightNode.shouldSpeculateString()) {
            node.setCanExit(false);
            break;
        }
        if (leftNode.shouldSpeculateNonStringCell() && rightNode.shouldSpeculateNonStringCell()) {
            node.setCanExit(true);
            forNode(node.child1()).filter((SpecCell & ~SpecString) | SpecOther);
            forNode(node.child2()).filter((SpecCell & ~SpecString) | SpecOther);
            break;
        }
        node.setCanExit(false);
        break;
    }
        
    case StringCharCodeAt:
        node.setCanExit(true);
        forNode(node.child1()).filter(SpecString);
        forNode(node.child2()).filter(SpecInt32);
        forNode(nodeIndex).set(SpecInt32);
        break;
        
    case StringCharAt:
        node.setCanExit(true);
        forNode(node.child1()).filter(SpecString);
        forNode(node.child2()).filter(SpecInt32);
        forNode(nodeIndex).set(SpecString);
        break;
            
    case GetByVal: {
        node.setCanExit(true);
        switch (node.arrayMode().type()) {
        case Array::SelectUsingPredictions:
        case Array::Unprofiled:
        case Array::Undecided:
            ASSERT_NOT_REACHED();
            break;
        case Array::ForceExit:
            m_isValid = false;
            break;
        case Array::Generic:
            clobberWorld(node.codeOrigin, indexInBlock);
            forNode(nodeIndex).makeTop();
            break;
        case Array::String:
            forNode(node.child2()).filter(SpecInt32);
            forNode(nodeIndex).set(SpecString);
            break;
        case Array::Arguments:
            forNode(node.child2()).filter(SpecInt32);
            forNode(nodeIndex).makeTop();
            break;
        case Array::Int32:
            forNode(node.child2()).filter(SpecInt32);
            if (node.arrayMode().isOutOfBounds()) {
                clobberWorld(node.codeOrigin, indexInBlock);
                forNode(nodeIndex).makeTop();
            } else
                forNode(nodeIndex).set(SpecInt32);
            break;
        case Array::Double:
            forNode(node.child2()).filter(SpecInt32);
            if (node.arrayMode().isOutOfBounds()) {
                clobberWorld(node.codeOrigin, indexInBlock);
                forNode(nodeIndex).makeTop();
            } else if (node.arrayMode().isSaneChain())
                forNode(nodeIndex).set(SpecDouble);
            else
                forNode(nodeIndex).set(SpecDoubleReal);
            break;
        case Array::Contiguous:
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            forNode(node.child2()).filter(SpecInt32);
            if (node.arrayMode().isOutOfBounds())
                clobberWorld(node.codeOrigin, indexInBlock);
            forNode(nodeIndex).makeTop();
            break;
        case Array::Int8Array:
            forNode(node.child2()).filter(SpecInt32);
            forNode(nodeIndex).set(SpecInt32);
            break;
        case Array::Int16Array:
            forNode(node.child2()).filter(SpecInt32);
            forNode(nodeIndex).set(SpecInt32);
            break;
        case Array::Int32Array:
            forNode(node.child2()).filter(SpecInt32);
            forNode(nodeIndex).set(SpecInt32);
            break;
        case Array::Uint8Array:
            forNode(node.child2()).filter(SpecInt32);
            forNode(nodeIndex).set(SpecInt32);
            break;
        case Array::Uint8ClampedArray:
            forNode(node.child2()).filter(SpecInt32);
            forNode(nodeIndex).set(SpecInt32);
            break;
        case Array::Uint16Array:
            forNode(node.child2()).filter(SpecInt32);
            forNode(nodeIndex).set(SpecInt32);
            break;
        case Array::Uint32Array:
            forNode(node.child2()).filter(SpecInt32);
            if (node.shouldSpeculateInteger())
                forNode(nodeIndex).set(SpecInt32);
            else
                forNode(nodeIndex).set(SpecDouble);
            break;
        case Array::Float32Array:
            forNode(node.child2()).filter(SpecInt32);
            forNode(nodeIndex).set(SpecDouble);
            break;
        case Array::Float64Array:
            forNode(node.child2()).filter(SpecInt32);
            forNode(nodeIndex).set(SpecDouble);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
            
    case PutByVal:
    case PutByValAlias: {
        node.setCanExit(true);
        Edge child1 = m_graph.varArgChild(node, 0);
        Edge child2 = m_graph.varArgChild(node, 1);
        Edge child3 = m_graph.varArgChild(node, 2);
        switch (node.arrayMode().modeForPut().type()) {
        case Array::ForceExit:
            m_isValid = false;
            break;
        case Array::Generic:
            clobberWorld(node.codeOrigin, indexInBlock);
            break;
        case Array::Int32:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            forNode(child3).filter(SpecInt32);
            if (node.arrayMode().isOutOfBounds())
                clobberWorld(node.codeOrigin, indexInBlock);
            break;
        case Array::Double:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            forNode(child3).filter(SpecRealNumber);
            if (node.arrayMode().isOutOfBounds())
                clobberWorld(node.codeOrigin, indexInBlock);
            break;
        case Array::Contiguous:
        case Array::ArrayStorage:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            if (node.arrayMode().isOutOfBounds())
                clobberWorld(node.codeOrigin, indexInBlock);
            break;
        case Array::SlowPutArrayStorage:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            if (node.arrayMode().mayStoreToHole())
                clobberWorld(node.codeOrigin, indexInBlock);
            break;
        case Array::Arguments:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            break;
        case Array::Int8Array:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            if (m_graph[child3].shouldSpeculateInteger())
                forNode(child3).filter(SpecInt32);
            else
                forNode(child3).filter(SpecNumber);
            break;
        case Array::Int16Array:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            if (m_graph[child3].shouldSpeculateInteger())
                forNode(child3).filter(SpecInt32);
            else
                forNode(child3).filter(SpecNumber);
            break;
        case Array::Int32Array:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            if (m_graph[child3].shouldSpeculateInteger())
                forNode(child3).filter(SpecInt32);
            else
                forNode(child3).filter(SpecNumber);
            break;
        case Array::Uint8Array:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            if (m_graph[child3].shouldSpeculateInteger())
                forNode(child3).filter(SpecInt32);
            else
                forNode(child3).filter(SpecNumber);
            break;
        case Array::Uint8ClampedArray:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            if (m_graph[child3].shouldSpeculateInteger())
                forNode(child3).filter(SpecInt32);
            else
                forNode(child3).filter(SpecNumber);
            break;
        case Array::Uint16Array:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            if (m_graph[child3].shouldSpeculateInteger())
                forNode(child3).filter(SpecInt32);
            else
                forNode(child3).filter(SpecNumber);
            break;
        case Array::Uint32Array:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            if (m_graph[child3].shouldSpeculateInteger())
                forNode(child3).filter(SpecInt32);
            else
                forNode(child3).filter(SpecNumber);
            break;
        case Array::Float32Array:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            forNode(child3).filter(SpecNumber);
            break;
        case Array::Float64Array:
            forNode(child1).filter(SpecCell);
            forNode(child2).filter(SpecInt32);
            forNode(child3).filter(SpecNumber);
            break;
        default:
            CRASH();
            break;
        }
        break;
    }
            
    case ArrayPush:
        node.setCanExit(true);
        switch (node.arrayMode().type()) {
        case Array::Int32:
            forNode(node.child2()).filter(SpecInt32);
            break;
        case Array::Double:
            forNode(node.child2()).filter(SpecRealNumber);
            break;
        default:
            break;
        }
        clobberWorld(node.codeOrigin, indexInBlock);
        forNode(nodeIndex).set(SpecNumber);
        break;
            
    case ArrayPop:
        node.setCanExit(true);
        clobberWorld(node.codeOrigin, indexInBlock);
        forNode(nodeIndex).makeTop();
        break;
            
    case RegExpExec:
    case RegExpTest:
        node.setCanExit(
            !isCellSpeculation(forNode(node.child1()).m_type)
            || !isCellSpeculation(forNode(node.child2()).m_type));
        forNode(node.child1()).filter(SpecCell);
        forNode(node.child2()).filter(SpecCell);
        forNode(nodeIndex).makeTop();
        break;
            
    case Jump:
        node.setCanExit(false);
        break;
            
    case Branch: {
        BooleanResult result = booleanResult(node, forNode(node.child1()));
        if (result == DefinitelyTrue) {
            m_branchDirection = TakeTrue;
            node.setCanExit(false);
            break;
        }
        if (result == DefinitelyFalse) {
            m_branchDirection = TakeFalse;
            node.setCanExit(false);
            break;
        }
        // FIXME: The above handles the trivial cases of sparse conditional
        // constant propagation, but we can do better:
        // We can specialize the source variable's value on each direction of
        // the branch.
        Node& child = m_graph[node.child1()];
        if (child.shouldSpeculateBoolean())
            speculateBooleanUnary(node);
        else if (child.shouldSpeculateNonStringCellOrOther()) {
            node.setCanExit(true);
            forNode(node.child1()).filter((SpecCell & ~SpecString) | SpecOther);
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
        if (childConst && childConst.isNumber() && trySetConstant(nodeIndex, childConst)) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        
        Node& child = m_graph[node.child1()];
        if (child.shouldSpeculateInteger()) {
            speculateInt32Unary(node);
            forNode(nodeIndex).set(SpecInt32);
            break;
        }

        AbstractValue& source = forNode(node.child1());
        AbstractValue& destination = forNode(nodeIndex);
            
        SpeculatedType type = source.m_type;
        if (type & ~(SpecNumber | SpecString | SpecBoolean)) {
            type &= (SpecNumber | SpecString | SpecBoolean);
            type |= SpecString;
        }
        destination.set(type);
        node.setCanExit(false);
        break;
    }
            
    case StrCat:
        node.setCanExit(false);
        forNode(nodeIndex).set(SpecString);
        break;
            
    case NewArray:
        node.setCanExit(true);
        forNode(nodeIndex).set(m_graph.globalObjectFor(node.codeOrigin)->arrayStructureForIndexingTypeDuringAllocation(node.indexingType()));
        m_haveStructures = true;
        break;
        
    case NewArrayBuffer:
        node.setCanExit(true);
        forNode(nodeIndex).set(m_graph.globalObjectFor(node.codeOrigin)->arrayStructureForIndexingTypeDuringAllocation(node.indexingType()));
        m_haveStructures = true;
        break;

    case NewArrayWithSize:
        node.setCanExit(true);
        forNode(node.child1()).filter(SpecInt32);
        forNode(nodeIndex).set(SpecArray);
        m_haveStructures = true;
        break;
            
    case NewRegexp:
        node.setCanExit(false);
        forNode(nodeIndex).set(m_graph.globalObjectFor(node.codeOrigin)->regExpStructure());
        m_haveStructures = true;
        break;
            
    case ConvertThis: {
        Node& child = m_graph[node.child1()];
        AbstractValue& source = forNode(node.child1());
        AbstractValue& destination = forNode(nodeIndex);
            
        if (isObjectSpeculation(source.m_type)) {
            // This is the simple case. We already know that the source is an
            // object, so there's nothing to do. I don't think this case will
            // be hit, but then again, you never know.
            destination = source;
            node.setCanExit(false);
            m_foundConstants = true; // Tell the constant folder to turn this into Identity.
            break;
        }
        
        node.setCanExit(true);
        
        if (isOtherSpeculation(child.prediction())) {
            source.filter(SpecOther);
            destination.set(SpecObjectOther);
            break;
        }
        
        if (isObjectSpeculation(child.prediction())) {
            source.filter(SpecObjectMask);
            destination = source;
            break;
        }
            
        destination = source;
        destination.merge(SpecObjectOther);
        break;
    }

    case CreateThis: {
        AbstractValue& source = forNode(node.child1());
        AbstractValue& destination = forNode(nodeIndex);
        
        node.setCanExit(!isCellSpeculation(source.m_type));
            
        source.filter(SpecFunction);
        destination.set(SpecFinalObject);
        break;
    }
        
    case InheritorIDWatchpoint:
        node.setCanExit(true);
        break;

    case NewObject:
        node.setCanExit(false);
        forNode(nodeIndex).set(node.structure());
        m_haveStructures = true;
        break;
        
    case CreateActivation:
        node.setCanExit(false);
        forNode(nodeIndex).set(m_codeBlock->globalObjectFor(node.codeOrigin)->activationStructure());
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
        if (isEmptySpeculation(
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
            forNode(nodeIndex).set(SpecInt32);
        node.setCanExit(
            !isEmptySpeculation(
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
        forNode(node.child1()).filter(SpecInt32);
        forNode(nodeIndex).makeTop();
        break;
        
    case GetMyArgumentByValSafe:
        node.setCanExit(true);
        // This potentially clobbers all structures if the property we're accessing has
        // a getter. We don't speculate against this.
        clobberWorld(node.codeOrigin, indexInBlock);
        // But we do speculate that the index is an integer.
        forNode(node.child1()).filter(SpecInt32);
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
        forNode(nodeIndex).set(SpecFunction);
        break;
        
    case SetCallee:
    case SetMyScope:
        node.setCanExit(false);
        break;
            
    case GetScope: // FIXME: We could get rid of these if we know that the JSFunction is a constant. https://bugs.webkit.org/show_bug.cgi?id=106202
    case GetMyScope:
    case SkipTopScope:
        node.setCanExit(false);
        forNode(nodeIndex).set(SpecCellOther);
        break;

    case SkipScope: {
        node.setCanExit(false);
        JSValue child = forNode(node.child1()).value();
        if (child && trySetConstant(nodeIndex, JSValue(jsCast<JSScope*>(child.asCell())->next()))) {
            m_foundConstants = true;
            break;
        }
        forNode(nodeIndex).set(SpecCellOther);
        break;
    }

    case GetScopeRegisters:
        node.setCanExit(false);
        forNode(node.child1()).filter(SpecCell);
        forNode(nodeIndex).clear(); // The result is not a JS value.
        break;

    case GetScopedVar:
        node.setCanExit(false);
        forNode(nodeIndex).makeTop();
        break;
            
    case PutScopedVar:
        node.setCanExit(false);
        clobberCapturedVars(node.codeOrigin);
        break;
            
    case GetById:
    case GetByIdFlush:
        node.setCanExit(true);
        if (!node.prediction()) {
            m_isValid = false;
            break;
        }
        if (isCellSpeculation(m_graph[node.child1()].prediction())) {
            forNode(node.child1()).filter(SpecCell);

            if (Structure* structure = forNode(node.child1()).bestProvenStructure()) {
                GetByIdStatus status = GetByIdStatus::computeFor(
                    m_graph.m_globalData, structure,
                    m_graph.m_codeBlock->identifier(node.identifierNumber()));
                if (status.isSimple()) {
                    // Assert things that we can't handle and that the computeFor() method
                    // above won't be able to return.
                    ASSERT(status.structureSet().size() == 1);
                    ASSERT(status.chain().isEmpty());
                    
                    if (status.specificValue())
                        forNode(nodeIndex).set(status.specificValue());
                    else
                        forNode(nodeIndex).makeTop();
                    forNode(node.child1()).filter(status.structureSet());
                    
                    m_foundConstants = true;
                    break;
                }
            }
        }
        clobberWorld(node.codeOrigin, indexInBlock);
        forNode(nodeIndex).makeTop();
        break;
            
    case GetArrayLength:
        node.setCanExit(true); // Lies, but it's true for the common case of JSArray, so it's good enough.
        forNode(nodeIndex).set(SpecInt32);
        break;
        
    case CheckExecutable: {
        // FIXME: We could track executables in AbstractValue, which would allow us to get rid of these checks
        // more thoroughly. https://bugs.webkit.org/show_bug.cgi?id=106200
        // FIXME: We could eliminate these entirely if we know the exact value that flows into this.
        // https://bugs.webkit.org/show_bug.cgi?id=106201
        forNode(node.child1()).filter(SpecCell);
        node.setCanExit(true);
        break;
    }

    case CheckStructure:
    case ForwardCheckStructure: {
        // FIXME: We should be able to propagate the structure sets of constants (i.e. prototypes).
        AbstractValue& value = forNode(node.child1());
        // If this structure check is attempting to prove knowledge already held in
        // the futurePossibleStructure set then the constant folding phase should
        // turn this into a watchpoint instead.
        StructureSet& set = node.structureSet();
        if (value.m_futurePossibleStructure.isSubsetOf(set)
            || value.m_currentKnownStructure.isSubsetOf(set))
            m_foundConstants = true;
        node.setCanExit(
            !value.m_currentKnownStructure.isSubsetOf(set)
            || !isCellSpeculation(value.m_type));
        value.filter(set);
        m_haveStructures = true;
        break;
    }
        
    case StructureTransitionWatchpoint:
    case ForwardStructureTransitionWatchpoint: {
        AbstractValue& value = forNode(node.child1());

        // It's only valid to issue a structure transition watchpoint if we already
        // know that the watchpoint covers a superset of the structures known to
        // belong to the set of future structures that this value may have.
        // Currently, we only issue singleton watchpoints (that check one structure)
        // and our futurePossibleStructure set can only contain zero, one, or an
        // infinity of structures.
        ASSERT(value.m_futurePossibleStructure.isSubsetOf(StructureSet(node.structure())));
        
        ASSERT(value.isClear() || isCellSpeculation(value.m_type)); // Value could be clear if we've proven must-exit due to a speculation statically known to be bad.
        value.filter(node.structure());
        m_haveStructures = true;
        node.setCanExit(true);
        break;
    }
            
    case PutStructure:
    case PhantomPutStructure:
        node.setCanExit(false);
        if (!forNode(node.child1()).m_currentKnownStructure.isClear()) {
            clobberStructures(indexInBlock);
            forNode(node.child1()).set(node.structureTransitionData().newStructure);
            m_haveStructures = true;
        }
        break;
    case GetButterfly:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
        node.setCanExit(!isCellSpeculation(forNode(node.child1()).m_type));
        forNode(node.child1()).filter(SpecCell);
        forNode(nodeIndex).clear(); // The result is not a JS value.
        break;
    case CheckArray: {
        if (node.arrayMode().alreadyChecked(m_graph, node, forNode(node.child1()))) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        node.setCanExit(true); // Lies, but this is followed by operations (like GetByVal) that always exit, so there is no point in us trying to be clever here.
        switch (node.arrayMode().type()) {
        case Array::String:
            forNode(node.child1()).filter(SpecString);
            break;
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            forNode(node.child1()).filter(SpecCell);
            break;
        case Array::Arguments:
            forNode(node.child1()).filter(SpecArguments);
            break;
        case Array::Int8Array:
            forNode(node.child1()).filter(SpecInt8Array);
            break;
        case Array::Int16Array:
            forNode(node.child1()).filter(SpecInt16Array);
            break;
        case Array::Int32Array:
            forNode(node.child1()).filter(SpecInt32Array);
            break;
        case Array::Uint8Array:
            forNode(node.child1()).filter(SpecUint8Array);
            break;
        case Array::Uint8ClampedArray:
            forNode(node.child1()).filter(SpecUint8ClampedArray);
            break;
        case Array::Uint16Array:
            forNode(node.child1()).filter(SpecUint16Array);
            break;
        case Array::Uint32Array:
            forNode(node.child1()).filter(SpecUint32Array);
            break;
        case Array::Float32Array:
            forNode(node.child1()).filter(SpecFloat32Array);
            break;
        case Array::Float64Array:
            forNode(node.child1()).filter(SpecFloat64Array);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        forNode(node.child1()).filterArrayModes(node.arrayMode().arrayModesThatPassFiltering());
        m_haveStructures = true;
        break;
    }
    case Arrayify: {
        if (node.arrayMode().alreadyChecked(m_graph, node, forNode(node.child1()))) {
            m_foundConstants = true;
            node.setCanExit(false);
            break;
        }
        ASSERT(node.arrayMode().conversion() == Array::Convert
            || node.arrayMode().conversion() == Array::RageConvert);
        node.setCanExit(true);
        forNode(node.child1()).filter(SpecCell);
        if (node.child2())
            forNode(node.child2()).filter(SpecInt32);
        clobberStructures(indexInBlock);
        forNode(node.child1()).filterArrayModes(node.arrayMode().arrayModesThatPassFiltering());
        m_haveStructures = true;
        break;
    }
    case ArrayifyToStructure: {
        AbstractValue& value = forNode(node.child1());
        StructureSet set = node.structure();
        if (value.m_futurePossibleStructure.isSubsetOf(set)
            || value.m_currentKnownStructure.isSubsetOf(set))
            m_foundConstants = true;
        node.setCanExit(true);
        if (node.child2())
            forNode(node.child2()).filter(SpecInt32);
        clobberStructures(indexInBlock);
        value.filter(set);
        m_haveStructures = true;
        break;
    }
    case GetIndexedPropertyStorage: {
        node.setCanExit(false);
        forNode(nodeIndex).clear();
        break; 
    }
    case GetByOffset:
        if (!m_graph[node.child1()].hasStorageResult()) {
            node.setCanExit(!isCellSpeculation(forNode(node.child1()).m_type));
            forNode(node.child1()).filter(SpecCell);
        }
        forNode(nodeIndex).makeTop();
        break;
            
    case PutByOffset: {
        bool canExit = false;
        if (!m_graph[node.child1()].hasStorageResult()) {
            canExit |= !isCellSpeculation(forNode(node.child1()).m_type);
            forNode(node.child1()).filter(SpecCell);
        }
        canExit |= !isCellSpeculation(forNode(node.child2()).m_type);
        forNode(node.child2()).filter(SpecCell);
        node.setCanExit(canExit);
        break;
    }
            
    case CheckFunction: {
        JSValue value = forNode(node.child1()).value();
        if (value == node.function()) {
            m_foundConstants = true;
            ASSERT(value);
            node.setCanExit(false);
            break;
        }
        
        node.setCanExit(true); // Lies! We can do better.
        if (!forNode(node.child1()).filterByValue(node.function())) {
            m_isValid = false;
            break;
        }
        break;
    }
        
    case PutById:
    case PutByIdDirect:
        node.setCanExit(true);
        if (Structure* structure = forNode(node.child1()).bestProvenStructure()) {
            PutByIdStatus status = PutByIdStatus::computeFor(
                m_graph.m_globalData,
                m_graph.globalObjectFor(node.codeOrigin),
                structure,
                m_graph.m_codeBlock->identifier(node.identifierNumber()),
                node.op() == PutByIdDirect);
            if (status.isSimpleReplace()) {
                forNode(node.child1()).filter(structure);
                m_foundConstants = true;
                break;
            }
            if (status.isSimpleTransition()) {
                clobberStructures(indexInBlock);
                forNode(node.child1()).set(status.newStructure());
                m_haveStructures = true;
                m_foundConstants = true;
                break;
            }
        }
        forNode(node.child1()).filter(SpecCell);
        clobberWorld(node.codeOrigin, indexInBlock);
        break;
            
    case GetGlobalVar:
        node.setCanExit(false);
        forNode(nodeIndex).makeTop();
        break;
        
    case GlobalVarWatchpoint:
        node.setCanExit(true);
        break;
            
    case PutGlobalVar:
    case PutGlobalVarCheck:
        node.setCanExit(false);
        break;
            
    case CheckHasInstance:
        node.setCanExit(true);
        forNode(node.child1()).filter(SpecCell);
        // Sadly, we don't propagate the fact that we've done CheckHasInstance
        break;
            
    case InstanceOf:
        node.setCanExit(true);
        // Again, sadly, we don't propagate the fact that we've done InstanceOf
        if (!(m_graph[node.child1()].prediction() & ~SpecCell) && !(forNode(node.child1()).m_type & ~SpecCell))
            forNode(node.child1()).filter(SpecCell);
        forNode(node.child2()).filter(SpecCell);
        forNode(nodeIndex).set(SpecBoolean);
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

    case GarbageValue:
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
    case CountExecution:
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
    clobberCapturedVars(codeOrigin);
    clobberStructures(indexInBlock);
}

inline void AbstractState::clobberCapturedVars(const CodeOrigin& codeOrigin)
{
    if (codeOrigin.inlineCallFrame) {
        const BitVector& capturedVars = codeOrigin.inlineCallFrame->capturedVars;
        for (size_t i = capturedVars.size(); i--;) {
            if (!capturedVars.quickGet(i))
                continue;
            m_variables.local(i).makeTop();
        }
    } else {
        for (size_t i = m_codeBlock->m_numVars; i--;) {
            if (m_codeBlock->isCaptured(i))
                m_variables.local(i).makeTop();
        }
    }

    for (size_t i = m_variables.numberOfArguments(); i--;) {
        if (m_codeBlock->isCaptured(argumentToOperand(i)))
            m_variables.argument(i).makeTop();
    }
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
    m_didClobber = true;
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
    dataLogF("          It's live, node @%u.\n", nodeIndex);
#endif
    
    if (node.variableAccessData()->isCaptured()) {
        source = inVariable;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("          Transfering ");
        source.dump(WTF::dataFile());
        dataLogF(" from last access due to captured variable.\n");
#endif
    } else {
        switch (node.op()) {
        case Phi:
        case SetArgument:
        case Flush:
            // The block transfers the value from head to tail.
            source = inVariable;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLogF("          Transfering ");
            source.dump(WTF::dataFile());
            dataLogF(" from head to tail.\n");
#endif
            break;
            
        case GetLocal:
            // The block refines the value with additional speculations.
            source = forNode(nodeIndex);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLogF("          Refining to ");
            source.dump(WTF::dataFile());
            dataLogF("\n");
#endif
            break;
            
        case SetLocal:
            // The block sets the variable, and potentially refines it, both
            // before and after setting it.
            if (node.variableAccessData()->shouldUseDoubleFormat()) {
                // FIXME: This unnecessarily loses precision.
                source.set(SpecDouble);
            } else
                source = forNode(node.child1());
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLogF("          Setting to ");
            source.dump(WTF::dataFile());
            dataLogF("\n");
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
        dataLogF("          Not changed!\n");
#endif
        return false;
    }
    
    // Abstract execution reached a new conclusion about the speculations reached about
    // this variable after execution of this basic block. Update the state, and return
    // true to indicate that the fixpoint must go on!
    destination = source;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
    dataLogF("          Changed!\n");
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
    Graph& graph, BasicBlock* basicBlock)
{
    Node& terminal = graph[basicBlock->last()];
    
    ASSERT(terminal.isTerminal());
    
    switch (terminal.op()) {
    case Jump: {
        ASSERT(basicBlock->cfaBranchDirection == InvalidBranchDirection);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("        Merging to block #%u.\n", terminal.takenBlockIndex());
#endif
        return merge(basicBlock, graph.m_blocks[terminal.takenBlockIndex()].get());
    }
        
    case Branch: {
        ASSERT(basicBlock->cfaBranchDirection != InvalidBranchDirection);
        bool changed = false;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("        Merging to block #%u.\n", terminal.takenBlockIndex());
#endif
        if (basicBlock->cfaBranchDirection != TakeFalse)
            changed |= merge(basicBlock, graph.m_blocks[terminal.takenBlockIndex()].get());
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("        Merging to block #%u.\n", terminal.notTakenBlockIndex());
#endif
        if (basicBlock->cfaBranchDirection != TakeTrue)
            changed |= merge(basicBlock, graph.m_blocks[terminal.notTakenBlockIndex()].get());
        return changed;
    }
        
    case Return:
    case Throw:
    case ThrowReferenceError:
        ASSERT(basicBlock->cfaBranchDirection == InvalidBranchDirection);
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

void AbstractState::dump(PrintStream& out)
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
            out.printf(" ");
        out.printf("@%lu:", static_cast<unsigned long>(index));
        value.dump(out);
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

