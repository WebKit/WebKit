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
#include <wtf/BitVector.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

class CodeBlock;
class ExecState;

namespace DFG {

// helper function to distinguish vars & temporaries from arguments.
inline bool operandIsArgument(int operand) { return operand < 0; }

typedef uint32_t BlockIndex;

// For every local variable we track any existing get or set of the value.
// We track the get so that these may be shared, and we track the set to
// retrieve the current value, and to reference the final definition.
struct VariableRecord {
    VariableRecord()
        : value(NoNode)
    {
    }
    
    void setFirstTime(NodeIndex nodeIndex)
    {
        ASSERT(value == NoNode);
        value = nodeIndex;
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
    
    bool operator==(const MethodCheckData& other) const
    {
        if (structure != other.structure)
            return false;
        if (prototypeStructure != other.prototypeStructure)
            return false;
        if (function != other.function)
            return false;
        if (prototype != other.prototype)
            return false;
        return true;
    }
    
    bool operator!=(const MethodCheckData& other) const
    {
        return !(*this == other);
    }
};

struct StorageAccessData {
    size_t offset;
    unsigned identifierNumber;
    
    // NOTE: the offset and identifierNumber do not by themselves
    // uniquely identify a property. The identifierNumber and a
    // Structure* do. If those two match, then the offset should
    // be the same, as well. For any Node that has a StorageAccessData,
    // it is possible to retrieve the Structure* by looking at the
    // first child. It should be a CheckStructure, which has the
    // Structure*.
};

struct ResolveGlobalData {
    unsigned identifierNumber;
    unsigned resolveInfoIndex;
};

typedef Vector <BlockIndex, 2> PredecessorList;

struct BasicBlock {
    BasicBlock(unsigned bytecodeBegin, NodeIndex begin, unsigned numArguments, unsigned numLocals)
        : bytecodeBegin(bytecodeBegin)
        , begin(begin)
        , end(NoNode)
        , isOSRTarget(false)
        , m_argumentsAtHead(numArguments)
        , m_localsAtHead(numLocals)
        , m_argumentsAtTail(numArguments)
        , m_localsAtTail(numLocals)
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
    
    Vector<VariableRecord, 8> m_argumentsAtHead;
    Vector<VariableRecord, 16> m_localsAtHead;
    
    Vector<VariableRecord, 8> m_argumentsAtTail;
    Vector<VariableRecord, 16> m_localsAtTail;
};

// 
// === Graph ===
//
// The dataflow graph is an ordered vector of nodes.
// The order may be significant for nodes with side-effects (property accesses, value conversions).
// Nodes that are 'dead' remain in the vector with refCount 0.
class Graph : public Vector<Node, 64> {
public:
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
    
    bool predictGlobalVar(unsigned varNumber, PredictedType prediction)
    {
        return m_predictions.predictGlobalVar(varNumber, prediction);
    }
    
    PredictedType getGlobalVarPrediction(unsigned varNumber)
    {
        return m_predictions.getGlobalVarPrediction(varNumber);
    }
    
    PredictedType getMethodCheckPrediction(Node& node)
    {
        return predictionFromCell(m_methodCheckData[node.methodCheckDataIndex()].function);
    }
    
    PredictedType getJSConstantPrediction(Node& node, CodeBlock* codeBlock)
    {
        return predictionFromValue(node.valueOfJSConstantNode(codeBlock));
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
    bool isFunctionConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        if (!isJSConstant(nodeIndex))
            return false;
        if (!getJSFunction(valueOfJSConstant(codeBlock, nodeIndex)))
            return false;
        return true;
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
        return valueOfJSConstantNode(codeBlock, nodeIndex).asNumber();
    }
    bool valueOfBooleanConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return valueOfJSConstantNode(codeBlock, nodeIndex).asBoolean();
    }
    JSFunction* valueOfFunctionConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        JSCell* function = getJSFunction(valueOfJSConstant(codeBlock, nodeIndex));
        ASSERT(function);
        return asFunction(function);
    }

#ifndef NDEBUG
    static const char *opName(NodeType);
    
    // This is O(n), and should only be used for verbose dumps.
    const char* nameOfVariableAccessData(VariableAccessData*);
#endif

    void predictArgumentTypes(ExecState*, CodeBlock*);
    
    StructureSet* addStructureSet(const StructureSet& structureSet)
    {
        ASSERT(structureSet.size());
        m_structureSet.append(structureSet);
        return &m_structureSet.last();
    }
    
    StructureTransitionData* addStructureTransitionData(const StructureTransitionData& structureTransitionData)
    {
        m_structureTransitionData.append(structureTransitionData);
        return &m_structureTransitionData.last();
    }

    Vector< OwnPtr<BasicBlock> , 8> m_blocks;
    Vector<NodeIndex, 16> m_varArgChildren;
    Vector<MethodCheckData> m_methodCheckData;
    Vector<StorageAccessData> m_storageAccessData;
    Vector<ResolveGlobalData> m_resolveGlobalData;
    Vector<NodeIndex, 8> m_arguments;
    SegmentedVector<VariableAccessData, 16> m_variableAccessData;
    SegmentedVector<StructureSet, 16> m_structureSet;
    SegmentedVector<StructureTransitionData, 8> m_structureTransitionData;
    BitVector m_preservedVars;
    unsigned m_localVars;
    unsigned m_parameterSlots;
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
