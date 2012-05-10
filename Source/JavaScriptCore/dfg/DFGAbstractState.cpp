/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#define CFA_PROFILING 0

#if CFA_PROFILING
#define PROFILE(flag) SamplingFlags::ScopedFlag scopedFlag(flag)
#else
#define PROFILE(flag) do { } while (false)
#endif

// Profiling flags
#define FLAG_FOR_BLOCK_INITIALIZATION  17
#define FLAG_FOR_BLOCK_END             18
#define FLAG_FOR_EXECUTION             19
#define FLAG_FOR_MERGE_TO_SUCCESSORS   20
#define FLAG_FOR_STRUCTURE_CLOBBERING  21

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
    PROFILE(FLAG_FOR_BLOCK_INITIALIZATION);
    
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
}

void AbstractState::initialize(Graph& graph)
{
    PROFILE(FLAG_FOR_BLOCK_INITIALIZATION);
    BasicBlock* root = graph.m_blocks[0].get();
    root->cfaShouldRevisit = true;
    for (size_t i = 0; i < root->valuesAtHead.numberOfArguments(); ++i) {
        Node& node = graph[root->variablesAtHead.argument(i)];
        ASSERT(node.op() == SetArgument);
        if (!node.shouldGenerate()) {
            // The argument is dead. We don't do any checks for such arguments, and so
            // for the purpose of the analysis, they contain no value.
            root->valuesAtHead.argument(i).clear();
            continue;
        }
        
        if (graph.argumentIsCaptured(i)) {
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
    }
    for (size_t i = 0; i < root->valuesAtHead.numberOfLocals(); ++i) {
        if (!graph.localIsCaptured(i))
            continue;
        root->valuesAtHead.local(i).makeTop();
    }
}

bool AbstractState::endBasicBlock(MergeMode mergeMode)
{
    PROFILE(FLAG_FOR_BLOCK_END);
    ASSERT(m_block);
    
    BasicBlock* block = m_block; // Save the block for successor merging.
    
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
            if (m_graph.argumentIsCaptured(argument)) {
                if (!destination.isTop()) {
                    destination.makeTop();
                    changed = true;
                }
            } else
                changed |= mergeStateAtTail(destination, m_variables.argument(argument), block->variablesAtTail.argument(argument));
        }
        
        for (size_t local = 0; local < block->variablesAtTail.numberOfLocals(); ++local) {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("        Merging state for local %zu.\n", local);
#endif
            AbstractValue& destination = block->valuesAtTail.local(local);
            if (m_graph.localIsCaptured(local)) {
                if (!destination.isTop()) {
                    destination.makeTop();
                    changed = true;
                }
            } else
                changed |= mergeStateAtTail(destination, m_variables.local(local), block->variablesAtTail.local(local));
        }
    }
    
    ASSERT(mergeMode != DontMerge || !changed);
    
    reset();
    
    if (mergeMode != MergeToSuccessors)
        return changed;
    
    return mergeToSuccessors(m_graph, block);
}

void AbstractState::reset()
{
    m_block = 0;
    m_isValid = false;
}

bool AbstractState::execute(unsigned indexInBlock)
{
    PROFILE(FLAG_FOR_EXECUTION);
    ASSERT(m_block);
    ASSERT(m_isValid);
        
    NodeIndex nodeIndex = m_block->at(indexInBlock);
    Node& node = m_graph[nodeIndex];
        
    if (!node.shouldGenerate())
        return true;
        
    switch (node.op()) {
    case JSConstant:
    case WeakJSConstant: {
        JSValue value = m_graph.valueOfJSConstant(nodeIndex);
        // Have to be careful here! It's tempting to call set(value), but
        // that would be wrong, since that would constitute a proof that this
        // value will always have the same structure. The whole point of a value
        // having a structure is that it may change in the future - for example
        // between when we compile the code and when we run it.
        forNode(nodeIndex).set(predictionFromValue(value));
        break;
    }
            
    case GetLocal: {
        if (m_graph.isCaptured(node.local()))
            forNode(nodeIndex).makeTop();
        else
            forNode(nodeIndex) = m_variables.operand(node.local());
        break;
    }
        
    case SetLocal: {
        if (m_graph.isCaptured(node.local()))
            break;
        
        if (node.variableAccessData()->shouldUseDoubleFormat()) {
            forNode(node.child1()).filter(PredictNumber);
            m_variables.operand(node.local()).set(PredictDouble);
            break;
        }
        
        PredictedType predictedType = node.variableAccessData()->argumentAwarePrediction();
        if (isInt32Prediction(predictedType))
            forNode(node.child1()).filter(PredictInt32);
        else if (isArrayPrediction(predictedType))
            forNode(node.child1()).filter(PredictArray);
        else if (isBooleanPrediction(predictedType))
            forNode(node.child1()).filter(PredictBoolean);
        
        m_variables.operand(node.local()) = forNode(node.child1());
        break;
    }
            
    case SetArgument:
        // Assert that the state of arguments has been set.
        ASSERT(!m_block->valuesAtHead.operand(node.local()).isClear());
        break;
            
    case BitAnd:
    case BitOr:
    case BitXor:
    case BitRShift:
    case BitLShift:
    case BitURShift:
        forNode(node.child1()).filter(PredictInt32);
        forNode(node.child2()).filter(PredictInt32);
        forNode(nodeIndex).set(PredictInt32);
        break;
        
    case UInt32ToNumber:
        if (!node.canSpeculateInteger())
            forNode(nodeIndex).set(PredictDouble);
        else
            forNode(nodeIndex).set(PredictInt32);
        break;
            
    case DoubleAsInt32:
        forNode(node.child1()).filter(PredictNumber);
        forNode(nodeIndex).set(PredictInt32);
        break;
            
    case ValueToInt32:
        if (m_graph[node.child1()].shouldSpeculateInteger())
            forNode(node.child1()).filter(PredictInt32);
        else if (m_graph[node.child1()].shouldSpeculateNumber())
            forNode(node.child1()).filter(PredictNumber);
        else if (m_graph[node.child1()].shouldSpeculateBoolean())
            forNode(node.child1()).filter(PredictBoolean);
        
        forNode(nodeIndex).set(PredictInt32);
        break;
        
    case Int32ToDouble:
        forNode(node.child1()).filter(PredictNumber);
        forNode(nodeIndex).set(PredictDouble);
        break;
        
    case CheckNumber:
        forNode(node.child1()).filter(PredictNumber);
        break;
            
    case ValueAdd:
    case ArithAdd: {
        if (m_graph.addShouldSpeculateInteger(node)) {
            forNode(node.child1()).filter(PredictInt32);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        if (Node::shouldSpeculateNumber(m_graph[node.child1()], m_graph[node.child2()])) {
            forNode(node.child1()).filter(PredictNumber);
            forNode(node.child2()).filter(PredictNumber);
            forNode(nodeIndex).set(PredictDouble);
            break;
        }
        if (node.op() == ValueAdd) {
            clobberStructures(indexInBlock);
            forNode(nodeIndex).set(PredictString | PredictInt32 | PredictNumber);
            break;
        }
        // We don't handle this yet. :-(
        m_isValid = false;
        break;
    }
            
    case ArithSub: {
        if (m_graph.addShouldSpeculateInteger(node)) {
            forNode(node.child1()).filter(PredictInt32);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        forNode(node.child1()).filter(PredictNumber);
        forNode(node.child2()).filter(PredictNumber);
        forNode(nodeIndex).set(PredictDouble);
        break;
    }
        
    case ArithNegate: {
        if (m_graph.negateShouldSpeculateInteger(node)) {
            forNode(node.child1()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        forNode(node.child1()).filter(PredictNumber);
        forNode(nodeIndex).set(PredictDouble);
        break;
    }
        
    case ArithMul:
    case ArithDiv:
    case ArithMin:
    case ArithMax:
    case ArithMod: {
        if (Node::shouldSpeculateInteger(m_graph[node.child1()], m_graph[node.child2()]) && node.canSpeculateInteger()) {
            forNode(node.child1()).filter(PredictInt32);
            forNode(node.child2()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        forNode(node.child1()).filter(PredictNumber);
        forNode(node.child2()).filter(PredictNumber);
        forNode(nodeIndex).set(PredictDouble);
        break;
    }
            
    case ArithAbs:
        if (m_graph[node.child1()].shouldSpeculateInteger() && node.canSpeculateInteger()) {
            forNode(node.child1()).filter(PredictInt32);
            forNode(nodeIndex).set(PredictInt32);
            break;
        }
        forNode(node.child1()).filter(PredictNumber);
        forNode(nodeIndex).set(PredictDouble);
        break;
            
    case ArithSqrt:
        forNode(node.child1()).filter(PredictNumber);
        forNode(nodeIndex).set(PredictDouble);
        break;
            
    case LogicalNot: {
        Node& child = m_graph[node.child1()];
        if (isBooleanPrediction(child.prediction()))
            forNode(node.child1()).filter(PredictBoolean);
        else if (child.shouldSpeculateFinalObjectOrOther())
            forNode(node.child1()).filter(PredictFinalObject | PredictOther);
        else if (child.shouldSpeculateArrayOrOther())
            forNode(node.child1()).filter(PredictArray | PredictOther);
        else if (child.shouldSpeculateInteger())
            forNode(node.child1()).filter(PredictInt32);
        else if (child.shouldSpeculateNumber())
            forNode(node.child1()).filter(PredictNumber);
        else
            clobberStructures(indexInBlock);
        forNode(nodeIndex).set(PredictBoolean);
        break;
    }
        
    case IsUndefined:
    case IsBoolean:
    case IsNumber:
    case IsString:
    case IsObject:
    case IsFunction: {
        forNode(nodeIndex).set(PredictBoolean);
        break;
    }
            
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareEq: {
        forNode(nodeIndex).set(PredictBoolean);
        
        Node& left = m_graph[node.child1()];
        Node& right = m_graph[node.child2()];
        PredictedType filter;
        if (Node::shouldSpeculateInteger(left, right))
            filter = PredictInt32;
        else if (Node::shouldSpeculateNumber(left, right))
            filter = PredictNumber;
        else if (node.op() == CompareEq) {
            if ((m_graph.isConstant(node.child1().index())
                 && m_graph.valueOfJSConstant(node.child1().index()).isNull())
                || (m_graph.isConstant(node.child2().index())
                    && m_graph.valueOfJSConstant(node.child2().index()).isNull())) {
                // We know that this won't clobber the world. But that's all we know.
                break;
            }
            
            if (Node::shouldSpeculateFinalObject(left, right))
                filter = PredictFinalObject;
            else if (Node::shouldSpeculateArray(left, right))
                filter = PredictArray;
            else if (left.shouldSpeculateFinalObject() && right.shouldSpeculateFinalObjectOrOther()) {
                forNode(node.child1()).filter(PredictFinalObject);
                forNode(node.child2()).filter(PredictFinalObject | PredictOther);
                break;
            } else if (right.shouldSpeculateFinalObject() && left.shouldSpeculateFinalObjectOrOther()) {
                forNode(node.child1()).filter(PredictFinalObject | PredictOther);
                forNode(node.child2()).filter(PredictFinalObject);
                break;
            } else if (left.shouldSpeculateArray() && right.shouldSpeculateArrayOrOther()) {
                forNode(node.child1()).filter(PredictFinalObject);
                forNode(node.child2()).filter(PredictFinalObject | PredictOther);
                break;
            } else if (right.shouldSpeculateArray() && left.shouldSpeculateArrayOrOther()) {
                forNode(node.child1()).filter(PredictFinalObject | PredictOther);
                forNode(node.child2()).filter(PredictFinalObject);
                break;
            } else {
                filter = PredictTop;
                clobberStructures(indexInBlock);
            }
        } else {
            filter = PredictTop;
            clobberStructures(indexInBlock);
        }
        forNode(node.child1()).filter(filter);
        forNode(node.child2()).filter(filter);
        break;
    }
            
    case CompareStrictEq:
        forNode(nodeIndex).set(PredictBoolean);
        break;
        
    case StringCharCodeAt:
        forNode(node.child1()).filter(PredictString);
        forNode(node.child2()).filter(PredictInt32);
        forNode(nodeIndex).set(PredictInt32);
        break;
        
    case StringCharAt:
        forNode(node.child1()).filter(PredictString);
        forNode(node.child2()).filter(PredictInt32);
        forNode(nodeIndex).set(PredictString);
        break;
            
    case GetByVal: {
        if (!node.prediction() || !m_graph[node.child1()].prediction() || !m_graph[node.child2()].prediction()) {
            m_isValid = false;
            break;
        }
        if (!isActionableArrayPrediction(m_graph[node.child1()].prediction()) || !m_graph[node.child2()].shouldSpeculateInteger()) {
            clobberStructures(indexInBlock);
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
        if (!m_graph[node.child1()].prediction() || !m_graph[node.child2()].prediction()) {
            m_isValid = false;
            break;
        }
        if (!m_graph[node.child2()].shouldSpeculateInteger() || !isActionableMutableArrayPrediction(m_graph[node.child1()].prediction())) {
            ASSERT(node.op() == PutByVal);
            clobberStructures(indexInBlock);
            forNode(nodeIndex).makeTop();
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
        break;
    }
            
    case ArrayPush:
        forNode(node.child1()).filter(PredictArray);
        forNode(nodeIndex).set(PredictNumber);
        break;
            
    case ArrayPop:
        forNode(node.child1()).filter(PredictArray);
        forNode(nodeIndex).makeTop();
        break;
            
    case RegExpExec:
    case RegExpTest:
        forNode(node.child1()).filter(PredictCell);
        forNode(node.child2()).filter(PredictCell);
        forNode(nodeIndex).makeTop();
        break;
            
    case Jump:
        break;
            
    case Branch: {
        // There is probably profit to be found in doing sparse conditional constant
        // propagation, and to take it one step further, where a variable's value
        // is specialized on each direction of a branch. For now, we don't do this.
        Node& child = m_graph[node.child1()];
        if (child.shouldSpeculateBoolean())
            forNode(node.child1()).filter(PredictBoolean);
        else if (child.shouldSpeculateFinalObjectOrOther())
            forNode(node.child1()).filter(PredictFinalObject | PredictOther);
        else if (child.shouldSpeculateArrayOrOther())
            forNode(node.child1()).filter(PredictArray | PredictOther);
        else if (child.shouldSpeculateInteger())
            forNode(node.child1()).filter(PredictInt32);
        else if (child.shouldSpeculateNumber())
            forNode(node.child1()).filter(PredictNumber);
        break;
    }
            
    case Return:
    case Throw:
    case ThrowReferenceError:
        m_isValid = false;
        break;
            
    case ToPrimitive: {
        Node& child = m_graph[node.child1()];
        if (child.shouldSpeculateInteger()) {
            forNode(node.child1()).filter(PredictInt32);
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
        break;
    }
            
    case StrCat:
        forNode(nodeIndex).set(PredictString);
        break;
            
    case NewArray:
    case NewArrayBuffer:
        forNode(nodeIndex).set(m_codeBlock->globalObject()->arrayStructure());
        m_haveStructures = true;
        break;
            
    case NewRegexp:
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
            break;
        }
        
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
            
        source.filter(PredictFunction);
        destination.set(PredictFinalObject);
        break;
    }

    case NewObject:
        forNode(nodeIndex).set(m_codeBlock->globalObjectFor(node.codeOrigin)->emptyObjectStructure());
        m_haveStructures = true;
        break;
        
    case CreateActivation:
        forNode(nodeIndex).set(m_graph.m_globalData.activationStructure.get());
        m_haveStructures = true;
        break;
        
    case TearOffActivation:
        // Does nothing that is user-visible.
        break;
        
    case NewFunction:
    case NewFunctionExpression:
    case NewFunctionNoCheck:
        forNode(nodeIndex).set(m_codeBlock->globalObjectFor(node.codeOrigin)->functionStructure());
        break;
        
    case GetCallee:
        forNode(nodeIndex).set(PredictFunction);
        break;
            
    case GetScopeChain:
        forNode(nodeIndex).set(PredictCellOther);
        break;
            
    case GetScopedVar:
        forNode(nodeIndex).makeTop();
        break;
            
    case PutScopedVar:
        clobberStructures(indexInBlock);
        break;
            
    case GetById:
    case GetByIdFlush:
        if (!node.prediction()) {
            m_isValid = false;
            break;
        }
        if (isCellPrediction(m_graph[node.child1()].prediction()))
            forNode(node.child1()).filter(PredictCell);
        clobberStructures(indexInBlock);
        forNode(nodeIndex).makeTop();
        break;
            
    case GetArrayLength:
        forNode(node.child1()).filter(PredictArray);
        forNode(nodeIndex).set(PredictInt32);
        break;

    case GetStringLength:
        forNode(node.child1()).filter(PredictString);
        forNode(nodeIndex).set(PredictInt32);
        break;
        
    case GetInt8ArrayLength:
        forNode(node.child1()).filter(PredictInt8Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetInt16ArrayLength:
        forNode(node.child1()).filter(PredictInt16Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetInt32ArrayLength:
        forNode(node.child1()).filter(PredictInt32Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetUint8ArrayLength:
        forNode(node.child1()).filter(PredictUint8Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetUint8ClampedArrayLength:
        forNode(node.child1()).filter(PredictUint8ClampedArray);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetUint16ArrayLength:
        forNode(node.child1()).filter(PredictUint16Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetUint32ArrayLength:
        forNode(node.child1()).filter(PredictUint32Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetFloat32ArrayLength:
        forNode(node.child1()).filter(PredictFloat32Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
    case GetFloat64ArrayLength:
        forNode(node.child1()).filter(PredictFloat64Array);
        forNode(nodeIndex).set(PredictInt32);
        break;
            
    case CheckStructure:
        // FIXME: We should be able to propagate the structure sets of constants (i.e. prototypes).
        forNode(node.child1()).filter(node.structureSet());
        m_haveStructures = true;
        break;
            
    case PutStructure:
        clobberStructures(indexInBlock);
        forNode(node.child1()).set(node.structureTransitionData().newStructure);
        m_haveStructures = true;
        break;
    case GetPropertyStorage:
        forNode(node.child1()).filter(PredictCell);
        forNode(nodeIndex).clear(); // The result is not a JS value.
        break;
    case GetIndexedPropertyStorage: {
        PredictedType basePrediction = m_graph[node.child2()].prediction();
        if (!(basePrediction & PredictInt32) && basePrediction) {
            forNode(nodeIndex).clear();
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
        forNode(node.child1()).filter(PredictCell);
        forNode(nodeIndex).makeTop();
        break;
            
    case PutByOffset:
        forNode(node.child1()).filter(PredictCell);
        break;
            
    case CheckFunction:
        forNode(node.child1()).filter(PredictFunction);
        // FIXME: Should be able to propagate the fact that we know what the function is.
        break;
            
    case PutById:
    case PutByIdDirect:
        forNode(node.child1()).filter(PredictCell);
        clobberStructures(indexInBlock);
        break;
            
    case GetGlobalVar:
        forNode(nodeIndex).makeTop();
        break;
            
    case PutGlobalVar:
        break;
            
    case CheckHasInstance:
        forNode(node.child1()).filter(PredictCell);
        // Sadly, we don't propagate the fact that we've done CheckHasInstance
        break;
            
    case InstanceOf:
        // Again, sadly, we don't propagate the fact that we've done InstanceOf
        if (!(m_graph[node.child1()].prediction() & ~PredictCell) && !(forNode(node.child1()).m_type & ~PredictCell))
            forNode(node.child1()).filter(PredictCell);
        forNode(node.child3()).filter(PredictCell);
        forNode(nodeIndex).set(PredictBoolean);
        break;
            
    case Phi:
    case Flush:
        break;
            
    case Breakpoint:
        break;
            
    case Call:
    case Construct:
    case Resolve:
    case ResolveBase:
    case ResolveBaseStrictPut:
    case ResolveGlobal:
        clobberStructures(indexInBlock);
        forNode(nodeIndex).makeTop();
        break;
            
    case ForceOSRExit:
        m_isValid = false;
        break;
            
    case Phantom:
    case InlineStart:
    case Nop:
        break;
        
    case LastNodeType:
        ASSERT_NOT_REACHED();
        break;
    }
    
    return m_isValid;
}

inline void AbstractState::clobberStructures(unsigned indexInBlock)
{
    PROFILE(FLAG_FOR_STRUCTURE_CLOBBERING);
    if (!m_haveStructures)
        return;
    for (size_t i = indexInBlock + 1; i--;)
        forNode(m_block->at(i)).clobberStructures();
    for (size_t i = 0; i < m_variables.numberOfArguments(); ++i)
        m_variables.argument(i).clobberStructures();
    for (size_t i = 0; i < m_variables.numberOfLocals(); ++i)
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
    
    switch (node.op()) {
    case Phi:
    case SetArgument:
    case Flush:
        // The block transfers the value from head to tail.
        source = inVariable;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("          Transfering from head to tail.\n");
#endif
        break;
            
    case GetLocal:
        // The block refines the value with additional speculations.
        source = forNode(nodeIndex);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("          Refining.\n");
#endif
        break;
            
    case SetLocal:
        // The block sets the variable, and potentially refines it, both
        // before and after setting it.
        if (node.variableAccessData()->shouldUseDoubleFormat())
            source.set(PredictDouble);
        else
            source = forNode(node.child1());
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("          Setting.\n");
#endif
        break;
        
    default:
        ASSERT_NOT_REACHED();
        break;
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
        if (m_graph.argumentIsCaptured(argument)) {
            if (destination.isTop())
                continue;
            destination.makeTop();
            changed = true;
            continue;
        }
        changed |= mergeVariableBetweenBlocks(destination, from->valuesAtTail.argument(argument), to->variablesAtHead.argument(argument), from->variablesAtTail.argument(argument));
    }
    
    for (size_t local = 0; local < from->variablesAtTail.numberOfLocals(); ++local) {
        AbstractValue& destination = to->valuesAtHead.local(local);
        if (m_graph.localIsCaptured(local)) {
            if (destination.isTop())
                continue;
            destination.makeTop();
            changed = true;
            continue;
        }
        changed |= mergeVariableBetweenBlocks(destination, from->valuesAtTail.local(local), to->variablesAtHead.local(local), from->variablesAtTail.local(local));
    }

    if (!to->cfaHasVisited)
        changed = true;
    
    to->cfaShouldRevisit |= changed;
    
    return changed;
}

inline bool AbstractState::mergeToSuccessors(Graph& graph, BasicBlock* basicBlock)
{
    PROFILE(FLAG_FOR_MERGE_TO_SUCCESSORS);

    Node& terminal = graph[basicBlock->last()];
    
    ASSERT(terminal.isTerminal());
    
    switch (terminal.op()) {
    case Jump:
        return merge(basicBlock, graph.m_blocks[terminal.takenBlockIndex()].get());
        
    case Branch:
        return merge(basicBlock, graph.m_blocks[terminal.takenBlockIndex()].get())
            | merge(basicBlock, graph.m_blocks[terminal.notTakenBlockIndex()].get());
        
    case Return:
    case Throw:
    case ThrowReferenceError:
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

#ifndef NDEBUG
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
#endif

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

