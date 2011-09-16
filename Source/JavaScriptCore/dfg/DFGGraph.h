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

#ifndef DFGGraph_h
#define DFGGraph_h

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGNode.h"
#include "PredictionTracker.h"
#include "RegisterFile.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

class CodeBlock;
class ExecState;

namespace DFG {

typedef uint32_t BlockIndex;

// For every local variable we track any existing get or set of the value.
// We track the get so that these may be shared, and we track the set to
// retrieve the current value, and to reference the final definition.
struct VariableRecord {
    VariableRecord()
        : value(NoNode)
    {
    }

    NodeIndex value;
};

struct MethodCheckData {
    // It is safe to refer to these directly because they are shadowed by
    // the old JIT's CodeBlock's MethodCallLinkInfo.
    Structure* structure;
    Structure* prototypeStructure;
    JSObject* function;
    JSObject* prototype;
};

typedef Vector <BlockIndex, 2> PredecessorList;

struct BasicBlock {
    BasicBlock(unsigned bytecodeBegin, NodeIndex begin, unsigned numArguments, unsigned numLocals)
        : bytecodeBegin(bytecodeBegin)
        , begin(begin)
        , end(NoNode)
        , isOSRTarget(false)
        , m_arguments(numArguments)
        , m_locals(numLocals)
    {
    }

    static inline BlockIndex getBytecodeBegin(OwnPtr<BasicBlock>* block)
    {
        return (*block)->bytecodeBegin;
    }

    unsigned bytecodeBegin;
    NodeIndex begin;
    NodeIndex end;
    bool isOSRTarget;

    PredecessorList m_predecessors;
    Vector <VariableRecord, 8> m_arguments;
    Vector <VariableRecord, 16> m_locals;
};

// 
// === Graph ===
//
// The dataflow graph is an ordered vector of nodes.
// The order may be significant for nodes with side-effects (property accesses, value conversions).
// Nodes that are 'dead' remain in the vector with refCount 0.
class Graph : public Vector<Node, 64> {
public:
    Graph(unsigned numArguments, unsigned numVariables)
        : m_predictions(numArguments, numVariables)
    {
    }

    // Mark a node as being referenced.
    void ref(NodeIndex nodeIndex)
    {
        Node& node = at(nodeIndex);
        // If the value (before incrementing) was at refCount zero then we need to ref its children.
        if (node.ref())
            refChildren(nodeIndex);
    }

#ifndef NDEBUG
    // CodeBlock is optional, but may allow additional information to be dumped (e.g. Identifier names).
    void dump(CodeBlock* = 0);
    void dump(NodeIndex, CodeBlock* = 0);
#endif

    BlockIndex blockIndexForBytecodeOffset(unsigned bytecodeBegin)
    {
        OwnPtr<BasicBlock>* begin = m_blocks.begin();
        OwnPtr<BasicBlock>* block = binarySearch<OwnPtr<BasicBlock>, unsigned, BasicBlock::getBytecodeBegin>(begin, m_blocks.size(), bytecodeBegin);
        ASSERT(block >= m_blocks.begin() && block < m_blocks.end());
        return static_cast<BlockIndex>(block - begin);
    }

    BasicBlock& blockForBytecodeOffset(unsigned bytecodeBegin)
    {
        return *m_blocks[blockIndexForBytecodeOffset(bytecodeBegin)];
    }
    
    PredictionTracker& predictions()
    {
        return m_predictions;
    }
    
    bool predict(int operand, PredictedType prediction, PredictionSource source)
    {
        return m_predictions.predict(operand, prediction, source);
    }
    
    bool predictGlobalVar(unsigned varNumber, PredictedType prediction, PredictionSource source)
    {
        return m_predictions.predictGlobalVar(varNumber, prediction, source);
    }
    
    bool predict(Node& node, PredictedType prediction, PredictionSource source)
    {
        switch (node.op) {
        case GetLocal:
            return predict(node.local(), prediction, source);
            break;
        case GetGlobalVar:
            return predictGlobalVar(node.varNumber(), prediction, source);
            break;
        case GetById:
        case GetMethod:
        case GetByVal:
        case Call:
        case Construct:
            return node.predict(prediction, source);
        default:
            return false;
        }
    }

    PredictedType getPrediction(int operand)
    {
        return m_predictions.getPrediction(operand);
    }
    
    PredictedType getGlobalVarPrediction(unsigned varNumber)
    {
        return m_predictions.getGlobalVarPrediction(varNumber);
    }
    
    PredictedType getMethodCheckPrediction(Node& node)
    {
        return makePrediction(predictionFromCell(m_methodCheckData[node.methodCheckDataIndex()].function), StrongPrediction);
    }
    
    PredictedType getPrediction(Node& node)
    {
        Node* nodePtr = &node;
        
        if (nodePtr->op == ValueToNumber)
            nodePtr = &(*this)[nodePtr->child1()];

        if (nodePtr->op == ValueToInt32)
            nodePtr = &(*this)[nodePtr->child1()];
        
        switch (nodePtr->op) {
        case GetLocal:
            return getPrediction(nodePtr->local());
        case GetGlobalVar:
            return getGlobalVarPrediction(nodePtr->varNumber());
        case GetById:
        case GetMethod:
        case GetByVal:
        case Call:
        case Construct:
            return nodePtr->getPrediction();
        case CheckMethod:
            return getMethodCheckPrediction(*nodePtr);
        default:
            return PredictNone;
        }
    }
    
    // Helper methods to check nodes for constants.
    bool isConstant(NodeIndex nodeIndex)
    {
        return at(nodeIndex).hasConstant();
    }
    bool isJSConstant(NodeIndex nodeIndex)
    {
        return at(nodeIndex).hasConstant();
    }
    bool isInt32Constant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return at(nodeIndex).isInt32Constant(codeBlock);
    }
    bool isDoubleConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return at(nodeIndex).isDoubleConstant(codeBlock);
    }
    bool isNumberConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return at(nodeIndex).isNumberConstant(codeBlock);
    }
    bool isBooleanConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return at(nodeIndex).isBooleanConstant(codeBlock);
    }
    // Helper methods get constant values from nodes.
    JSValue valueOfJSConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        if (at(nodeIndex).hasMethodCheckData())
            return JSValue(m_methodCheckData[at(nodeIndex).methodCheckDataIndex()].function);
        return valueOfJSConstantNode(codeBlock, nodeIndex);
    }
    int32_t valueOfInt32Constant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return valueOfJSConstantNode(codeBlock, nodeIndex).asInt32();
    }
    double valueOfNumberConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return valueOfJSConstantNode(codeBlock, nodeIndex).uncheckedGetNumber();
    }
    bool valueOfBooleanConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return valueOfJSConstantNode(codeBlock, nodeIndex).getBoolean();
    }

#ifndef NDEBUG
    static const char *opName(NodeType);
#endif

    void predictArgumentTypes(ExecState*, CodeBlock*);

    Vector< OwnPtr<BasicBlock> , 8> m_blocks;
    Vector<NodeIndex, 16> m_varArgChildren;
    Vector<MethodCheckData> m_methodCheckData;
private:
    
    JSValue valueOfJSConstantNode(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return codeBlock->constantRegister(FirstConstantRegisterIndex + at(nodeIndex).constantNumber()).get();
    }

    // When a node's refCount goes from 0 to 1, it must (logically) recursively ref all of its children, and vice versa.
    void refChildren(NodeIndex);

    PredictionTracker m_predictions;
};

} } // namespace JSC::DFG

#endif
#endif
