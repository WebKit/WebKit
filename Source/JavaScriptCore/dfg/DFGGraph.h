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
#include "DFGArgumentPosition.h"
#include "DFGAssemblyHelpers.h"
#include "DFGBasicBlock.h"
#include "DFGNode.h"
#include "MethodOfGettingAValueProfile.h"
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
    Graph(JSGlobalData& globalData, CodeBlock* codeBlock)
        : m_globalData(globalData)
        , m_codeBlock(codeBlock)
        , m_profiledBlock(codeBlock->alternative())
    {
        ASSERT(m_profiledBlock);
    }
    
    using Vector<Node, 64>::operator[];
    using Vector<Node, 64>::at;
    
    Node& operator[](Edge nodeUse) { return at(nodeUse.index()); }
    const Node& operator[](Edge nodeUse) const { return at(nodeUse.index()); }
    
    Node& at(Edge nodeUse) { return at(nodeUse.index()); }
    const Node& at(Edge nodeUse) const { return at(nodeUse.index()); }
    
    // Mark a node as being referenced.
    void ref(NodeIndex nodeIndex)
    {
        Node& node = at(nodeIndex);
        // If the value (before incrementing) was at refCount zero then we need to ref its children.
        if (node.ref())
            refChildren(nodeIndex);
    }
    void ref(Edge nodeUse)
    {
        ref(nodeUse.index());
    }
    
    void deref(NodeIndex nodeIndex)
    {
        if (at(nodeIndex).deref())
            derefChildren(nodeIndex);
    }
    void deref(Edge nodeUse)
    {
        deref(nodeUse.index());
    }
    
    void clearAndDerefChild1(Node& node)
    {
        if (!node.child1())
            return;
        deref(node.child1());
        node.children.child1() = Edge();
    }

    void clearAndDerefChild2(Node& node)
    {
        if (!node.child2())
            return;
        deref(node.child2());
        node.children.child2() = Edge();
    }

    void clearAndDerefChild3(Node& node)
    {
        if (!node.child3())
            return;
        deref(node.child3());
        node.children.child3() = Edge();
    }

    // CodeBlock is optional, but may allow additional information to be dumped (e.g. Identifier names).
    void dump();
    void dump(NodeIndex);

    // Dump the code origin of the given node as a diff from the code origin of the
    // preceding node.
    void dumpCodeOrigin(NodeIndex, NodeIndex);

    BlockIndex blockIndexForBytecodeOffset(Vector<BlockIndex>& blocks, unsigned bytecodeBegin);

    PredictedType getJSConstantPrediction(Node& node)
    {
        return predictionFromValue(node.valueOfJSConstant(m_codeBlock));
    }
    
    bool addShouldSpeculateInteger(Node& add)
    {
        ASSERT(add.op() == ValueAdd || add.op() == ArithAdd || add.op() == ArithSub);
        
        Node& left = at(add.child1());
        Node& right = at(add.child2());
        
        if (left.hasConstant())
            return addImmediateShouldSpeculateInteger(add, right, left);
        if (right.hasConstant())
            return addImmediateShouldSpeculateInteger(add, left, right);
        
        return Node::shouldSpeculateInteger(left, right) && add.canSpeculateInteger();
    }
    
    bool negateShouldSpeculateInteger(Node& negate)
    {
        ASSERT(negate.op() == ArithNegate);
        return at(negate.child1()).shouldSpeculateInteger() && negate.canSpeculateInteger();
    }
    
    bool addShouldSpeculateInteger(NodeIndex nodeIndex)
    {
        return addShouldSpeculateInteger(at(nodeIndex));
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
    bool isInt32Constant(NodeIndex nodeIndex)
    {
        return at(nodeIndex).isInt32Constant(m_codeBlock);
    }
    bool isDoubleConstant(NodeIndex nodeIndex)
    {
        return at(nodeIndex).isDoubleConstant(m_codeBlock);
    }
    bool isNumberConstant(NodeIndex nodeIndex)
    {
        return at(nodeIndex).isNumberConstant(m_codeBlock);
    }
    bool isBooleanConstant(NodeIndex nodeIndex)
    {
        return at(nodeIndex).isBooleanConstant(m_codeBlock);
    }
    bool isFunctionConstant(NodeIndex nodeIndex)
    {
        if (!isJSConstant(nodeIndex))
            return false;
        if (!getJSFunction(valueOfJSConstant(nodeIndex)))
            return false;
        return true;
    }
    // Helper methods get constant values from nodes.
    JSValue valueOfJSConstant(NodeIndex nodeIndex)
    {
        return at(nodeIndex).valueOfJSConstant(m_codeBlock);
    }
    int32_t valueOfInt32Constant(NodeIndex nodeIndex)
    {
        return valueOfJSConstant(nodeIndex).asInt32();
    }
    double valueOfNumberConstant(NodeIndex nodeIndex)
    {
        return valueOfJSConstant(nodeIndex).asNumber();
    }
    bool valueOfBooleanConstant(NodeIndex nodeIndex)
    {
        return valueOfJSConstant(nodeIndex).asBoolean();
    }
    JSFunction* valueOfFunctionConstant(NodeIndex nodeIndex)
    {
        JSCell* function = getJSFunction(valueOfJSConstant(nodeIndex));
        ASSERT(function);
        return jsCast<JSFunction*>(function);
    }

    static const char *opName(NodeType);
    
    // This is O(n), and should only be used for verbose dumps.
    const char* nameOfVariableAccessData(VariableAccessData*);

    void predictArgumentTypes();
    
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
    
    CodeBlock* baselineCodeBlockFor(const CodeOrigin& codeOrigin)
    {
        return baselineCodeBlockForOriginAndBaselineCodeBlock(codeOrigin, m_profiledBlock);
    }
    
    ValueProfile* valueProfileFor(NodeIndex nodeIndex)
    {
        if (nodeIndex == NoNode)
            return 0;
        
        Node& node = at(nodeIndex);
        CodeBlock* profiledBlock = baselineCodeBlockFor(node.codeOrigin);
        
        if (node.hasLocal()) {
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
    
    MethodOfGettingAValueProfile methodOfGettingAValueProfileFor(NodeIndex nodeIndex)
    {
        if (nodeIndex == NoNode)
            return MethodOfGettingAValueProfile();
        
        Node& node = at(nodeIndex);
        CodeBlock* profiledBlock = baselineCodeBlockFor(node.codeOrigin);
        
        if (node.op() == GetLocal) {
            return MethodOfGettingAValueProfile::fromLazyOperand(
                profiledBlock,
                LazyOperandValueProfileKey(
                    node.codeOrigin.bytecodeIndex, node.local()));
        }
        
        return MethodOfGettingAValueProfile(valueProfileFor(nodeIndex));
    }
    
    bool needsActivation() const
    {
#if DFG_ENABLE(ALL_VARIABLES_CAPTURED)
        return true;
#else
        return m_codeBlock->needsFullScopeChain() && m_codeBlock->codeType() != GlobalCode;
#endif
    }
    
    // Pass an argument index. Currently it's ignored, but that's somewhat
    // of a bug.
    bool argumentIsCaptured(int) const
    {
        return needsActivation();
    }
    bool localIsCaptured(int operand) const
    {
#if DFG_ENABLE(ALL_VARIABLES_CAPTURED)
        return operand < m_codeBlock->m_numVars;
#else
        return operand < m_codeBlock->m_numCapturedVars;
#endif
    }
    
    bool isCaptured(int operand) const
    {
        if (operandIsArgument(operand))
            return argumentIsCaptured(operandToArgument(operand));
        return localIsCaptured(operand);
    }
    bool isCaptured(VirtualRegister virtualRegister) const
    {
        return isCaptured(static_cast<int>(virtualRegister));
    }
    
    JSGlobalData& m_globalData;
    CodeBlock* m_codeBlock;
    CodeBlock* m_profiledBlock;

    Vector< OwnPtr<BasicBlock> , 8> m_blocks;
    Vector<Edge, 16> m_varArgChildren;
    Vector<StorageAccessData> m_storageAccessData;
    Vector<ResolveGlobalData> m_resolveGlobalData;
    Vector<NodeIndex, 8> m_arguments;
    SegmentedVector<VariableAccessData, 16> m_variableAccessData;
    SegmentedVector<ArgumentPosition, 8> m_argumentPositions;
    SegmentedVector<StructureSet, 16> m_structureSet;
    SegmentedVector<StructureTransitionData, 8> m_structureTransitionData;
    BitVector m_preservedVars;
    unsigned m_localVars;
    unsigned m_parameterSlots;
private:
    
    bool addImmediateShouldSpeculateInteger(Node& add, Node& variable, Node& immediate)
    {
        ASSERT(immediate.hasConstant());
        
        JSValue immediateValue = immediate.valueOfJSConstant(m_codeBlock);
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
