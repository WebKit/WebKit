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
#include "DFGBasicBlock.h"
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
    
    void deref(NodeIndex nodeIndex)
    {
        if (at(nodeIndex).deref())
            derefChildren(nodeIndex);
    }
    
    void clearAndDerefChild1(Node& node)
    {
        if (node.children.fixed.child1 == NoNode)
            return;
        deref(node.children.fixed.child1);
        node.children.fixed.child1 = NoNode;
    }

    void clearAndDerefChild2(Node& node)
    {
        if (node.children.fixed.child2 == NoNode)
            return;
        deref(node.children.fixed.child2);
        node.children.fixed.child2 = NoNode;
    }

    void clearAndDerefChild3(Node& node)
    {
        if (node.children.fixed.child3 == NoNode)
            return;
        deref(node.children.fixed.child3);
        node.children.fixed.child3 = NoNode;
    }

    // CodeBlock is optional, but may allow additional information to be dumped (e.g. Identifier names).
    void dump(CodeBlock* = 0);
    void dump(NodeIndex, CodeBlock* = 0);

    // Dump the code origin of the given node as a diff from the code origin of the
    // preceding node.
    void dumpCodeOrigin(NodeIndex);

    BlockIndex blockIndexForBytecodeOffset(Vector<BlockIndex>& blocks, unsigned bytecodeBegin);

    bool predictGlobalVar(unsigned varNumber, PredictedType prediction)
    {
        return m_predictions.predictGlobalVar(varNumber, prediction);
    }
    
    PredictedType getGlobalVarPrediction(unsigned varNumber)
    {
        return m_predictions.getGlobalVarPrediction(varNumber);
    }
    
    PredictedType getJSConstantPrediction(Node& node, CodeBlock* codeBlock)
    {
        return predictionFromValue(node.valueOfJSConstant(codeBlock));
    }
    
    bool addShouldSpeculateInteger(Node& add, CodeBlock* codeBlock)
    {
        ASSERT(add.op == ValueAdd || add.op == ArithAdd || add.op == ArithSub);
        
        Node& left = at(add.child1());
        Node& right = at(add.child2());
        
        if (left.hasConstant())
            return addImmediateShouldSpeculateInteger(codeBlock, add, right, left);
        if (right.hasConstant())
            return addImmediateShouldSpeculateInteger(codeBlock, add, left, right);
        
        return Node::shouldSpeculateInteger(left, right) && add.canSpeculateInteger();
    }
    
    bool addShouldSpeculateInteger(NodeIndex nodeIndex, CodeBlock* codeBlock)
    {
        return addShouldSpeculateInteger(at(nodeIndex), codeBlock);
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
        return at(nodeIndex).valueOfJSConstant(codeBlock);
    }
    int32_t valueOfInt32Constant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return valueOfJSConstant(codeBlock, nodeIndex).asInt32();
    }
    double valueOfNumberConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return valueOfJSConstant(codeBlock, nodeIndex).asNumber();
    }
    bool valueOfBooleanConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        return valueOfJSConstant(codeBlock, nodeIndex).asBoolean();
    }
    JSFunction* valueOfFunctionConstant(CodeBlock* codeBlock, NodeIndex nodeIndex)
    {
        JSCell* function = getJSFunction(valueOfJSConstant(codeBlock, nodeIndex));
        ASSERT(function);
        return asFunction(function);
    }

    static const char *opName(NodeType);
    
    // This is O(n), and should only be used for verbose dumps.
    const char* nameOfVariableAccessData(VariableAccessData*);

    void predictArgumentTypes(CodeBlock*);
    
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
    
    ValueProfile* valueProfileFor(NodeIndex nodeIndex, CodeBlock* profiledBlock)
    {
        if (nodeIndex == NoNode)
            return 0;
        
        Node& node = at(nodeIndex);
        
        if (node.op == GetLocal) {
            if (!operandIsArgument(node.local()))
                return 0;
            int argument = operandToArgument(node.local());
            if (node.variableAccessData() != at(m_arguments[argument]).variableAccessData())
                return 0;
            return profiledBlock->valueProfileForArgument(argument);
        }
        
        if (node.hasHeapPrediction())
            return profiledBlock->valueProfileForBytecodeOffset(node.codeOrigin.bytecodeIndexForValueProfile());
        
        return 0;
    }

    Vector< OwnPtr<BasicBlock> , 8> m_blocks;
    Vector<NodeIndex, 16> m_varArgChildren;
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
    
    bool addImmediateShouldSpeculateInteger(CodeBlock* codeBlock, Node& add, Node& variable, Node& immediate)
    {
        ASSERT(immediate.hasConstant());
        
        JSValue immediateValue = immediate.valueOfJSConstant(codeBlock);
        if (!immediateValue.isNumber())
            return false;
        
        if (!variable.shouldSpeculateInteger())
            return false;
        
        if (immediateValue.isInt32())
            return add.canSpeculateInteger();
        
        double doubleImmediate = immediateValue.asDouble();
        const double twoToThe48 = 281474976710656.0;
        if (doubleImmediate < -twoToThe48 || doubleImmediate > twoToThe48)
            return false;
        
        return nodeCanTruncateInteger(add.arithNodeFlags());
    }
    
    // When a node's refCount goes from 0 to 1, it must (logically) recursively ref all of its children, and vice versa.
    void refChildren(NodeIndex);
    void derefChildren(NodeIndex);

    PredictionTracker m_predictions;
};

class GetBytecodeBeginForBlock {
public:
    GetBytecodeBeginForBlock(Graph& graph)
        : m_graph(graph)
    {
    }
    
    unsigned operator()(BlockIndex* blockIndex) const
    {
        return m_graph.m_blocks[*blockIndex]->bytecodeBegin;
    }

private:
    Graph& m_graph;
};

inline BlockIndex Graph::blockIndexForBytecodeOffset(Vector<BlockIndex>& linkingTargets, unsigned bytecodeBegin)
{
    return *WTF::binarySearchWithFunctor<BlockIndex, unsigned>(linkingTargets.begin(), linkingTargets.size(), bytecodeBegin, WTF::KeyMustBePresentInArray, GetBytecodeBeginForBlock(*this));
}

} } // namespace JSC::DFG

#endif
#endif
