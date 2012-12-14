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

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGArgumentPosition.h"
#include "DFGAssemblyHelpers.h"
#include "DFGBasicBlock.h"
#include "DFGDominators.h"
#include "DFGNode.h"
#include "JSStack.h"
#include "MethodOfGettingAValueProfile.h"
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
};

struct ResolveGlobalData {
    unsigned identifierNumber;
    unsigned resolveOperationsIndex;
    unsigned putToBaseOperationIndex;
    unsigned resolvePropertyIndex;
};

struct ResolveOperationData {
    unsigned identifierNumber;
    unsigned resolveOperationsIndex;
    unsigned putToBaseOperationIndex;
};

struct PutToBaseOperationData {
    unsigned putToBaseOperationIndex;
};


//
// === Graph ===
//
// The dataflow graph is an ordered vector of nodes.
// The order may be significant for nodes with side-effects (property accesses, value conversions).
// Nodes that are 'dead' remain in the vector with refCount 0.
class Graph : public Vector<Node, 64> {
public:
    Graph(JSGlobalData&, CodeBlock*, unsigned osrEntryBytecodeIndex, const Operands<JSValue>& mustHandleValues);
    
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
        if (!at(nodeIndex).refCount())
            dump();
        if (at(nodeIndex).deref())
            derefChildren(nodeIndex);
    }
    void deref(Edge nodeUse)
    {
        deref(nodeUse.index());
    }
    
    void changeIndex(Edge& edge, NodeIndex newIndex, bool changeRef = true)
    {
        if (changeRef) {
            ref(newIndex);
            deref(edge.index());
        }
        edge.setIndex(newIndex);
    }
    
    void changeEdge(Edge& edge, Edge newEdge, bool changeRef = true)
    {
        if (changeRef) {
            ref(newEdge);
            deref(edge);
        }
        edge = newEdge;
    }
    
    void compareAndSwap(Edge& edge, NodeIndex oldIndex, NodeIndex newIndex, bool changeRef)
    {
        if (edge.index() != oldIndex)
            return;
        changeIndex(edge, newIndex, changeRef);
    }
    
    void compareAndSwap(Edge& edge, Edge oldEdge, Edge newEdge, bool changeRef)
    {
        if (edge != oldEdge)
            return;
        changeEdge(edge, newEdge, changeRef);
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
    
    // Call this if you've modified the reference counts of nodes that deal with
    // local variables. This is necessary because local variable references can form
    // cycles, and hence reference counting is not enough. This will reset the
    // reference counts according to reachability.
    void collectGarbage();
    
    void convertToConstant(NodeIndex nodeIndex, unsigned constantNumber)
    {
        at(nodeIndex).convertToConstant(constantNumber);
    }
    
    void convertToConstant(NodeIndex nodeIndex, JSValue value)
    {
        convertToConstant(nodeIndex, m_codeBlock->addOrFindConstant(value));
    }

    // CodeBlock is optional, but may allow additional information to be dumped (e.g. Identifier names).
    void dump(PrintStream& = WTF::dataFile());
    enum PhiNodeDumpMode { DumpLivePhisOnly, DumpAllPhis };
    void dumpBlockHeader(PrintStream&, const char* prefix, BlockIndex, PhiNodeDumpMode);
    void dump(PrintStream&, Edge);
    void dump(PrintStream&, const char* prefix, NodeIndex);
    static int amountOfNodeWhiteSpace(Node&);
    static void printNodeWhiteSpace(PrintStream&, Node&);

    // Dump the code origin of the given node as a diff from the code origin of the
    // preceding node. Returns true if anything was printed.
    bool dumpCodeOrigin(PrintStream&, const char* prefix, NodeIndex, NodeIndex);

    BlockIndex blockIndexForBytecodeOffset(Vector<BlockIndex>& blocks, unsigned bytecodeBegin);

    SpeculatedType getJSConstantSpeculation(Node& node)
    {
        return speculationFromValue(node.valueOfJSConstant(m_codeBlock));
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
        
        return Node::shouldSpeculateIntegerExpectingDefined(left, right) && add.canSpeculateInteger();
    }
    
    bool mulShouldSpeculateInteger(Node& mul)
    {
        ASSERT(mul.op() == ArithMul);
        
        Node& left = at(mul.child1());
        Node& right = at(mul.child2());
        
        return Node::shouldSpeculateIntegerForArithmetic(left, right) && mul.canSpeculateInteger();
    }
    
    bool negateShouldSpeculateInteger(Node& negate)
    {
        ASSERT(negate.op() == ArithNegate);
        return at(negate.child1()).shouldSpeculateIntegerForArithmetic() && negate.canSpeculateInteger();
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
    bool isCellConstant(NodeIndex nodeIndex)
    {
        if (!isJSConstant(nodeIndex))
            return false;
        JSValue value = valueOfJSConstant(nodeIndex);
        return value.isCell() && !!value;
    }
    bool isFunctionConstant(NodeIndex nodeIndex)
    {
        if (!isJSConstant(nodeIndex))
            return false;
        if (!getJSFunction(valueOfJSConstant(nodeIndex)))
            return false;
        return true;
    }
    bool isInternalFunctionConstant(NodeIndex nodeIndex)
    {
        if (!isJSConstant(nodeIndex))
            return false;
        JSValue value = valueOfJSConstant(nodeIndex);
        if (!value.isCell() || !value)
            return false;
        JSCell* cell = value.asCell();
        if (!cell->inherits(&InternalFunction::s_info))
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
    InternalFunction* valueOfInternalFunctionConstant(NodeIndex nodeIndex)
    {
        return jsCast<InternalFunction*>(valueOfJSConstant(nodeIndex).asCell());
    }

    static const char *opName(NodeType);
    
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
    
    JSGlobalObject* globalObjectFor(CodeOrigin codeOrigin)
    {
        return m_codeBlock->globalObjectFor(codeOrigin);
    }
    
    ExecutableBase* executableFor(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return m_codeBlock->ownerExecutable();
        
        return inlineCallFrame->executable.get();
    }
    
    ExecutableBase* executableFor(const CodeOrigin& codeOrigin)
    {
        return executableFor(codeOrigin.inlineCallFrame);
    }
    
    CodeBlock* baselineCodeBlockFor(const CodeOrigin& codeOrigin)
    {
        return baselineCodeBlockForOriginAndBaselineCodeBlock(codeOrigin, m_profiledBlock);
    }
    
    int argumentsRegisterFor(const CodeOrigin& codeOrigin)
    {
        if (!codeOrigin.inlineCallFrame)
            return m_codeBlock->argumentsRegister();
        
        return baselineCodeBlockForInlineCallFrame(
            codeOrigin.inlineCallFrame)->argumentsRegister() +
            codeOrigin.inlineCallFrame->stackOffset;
    }
    
    int uncheckedArgumentsRegisterFor(const CodeOrigin& codeOrigin)
    {
        if (!codeOrigin.inlineCallFrame)
            return m_codeBlock->uncheckedArgumentsRegister();
        
        CodeBlock* codeBlock = baselineCodeBlockForInlineCallFrame(
            codeOrigin.inlineCallFrame);
        if (!codeBlock->usesArguments())
            return InvalidVirtualRegister;
        
        return codeBlock->argumentsRegister() +
            codeOrigin.inlineCallFrame->stackOffset;
    }
    
    int uncheckedActivationRegisterFor(const CodeOrigin& codeOrigin)
    {
        ASSERT_UNUSED(codeOrigin, !codeOrigin.inlineCallFrame);
        return m_codeBlock->uncheckedActivationRegister();
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
        return m_codeBlock->needsFullScopeChain() && m_codeBlock->codeType() != GlobalCode;
    }
    
    bool usesArguments() const
    {
        return m_codeBlock->usesArguments();
    }
    
    bool isCreatedThisArgument(int operand)
    {
        if (!operandIsArgument(operand))
            return false;
        if (operandToArgument(operand))
            return false;
        return m_codeBlock->specializationKind() == CodeForConstruct;
    }
    
    unsigned numSuccessors(BasicBlock* block)
    {
        return at(block->last()).numSuccessors();
    }
    BlockIndex successor(BasicBlock* block, unsigned index)
    {
        return at(block->last()).successor(index);
    }
    BlockIndex successorForCondition(BasicBlock* block, bool condition)
    {
        return at(block->last()).successorForCondition(condition);
    }
    
    bool isPredictedNumerical(Node& node)
    {
        SpeculatedType left = at(node.child1()).prediction();
        SpeculatedType right = at(node.child2()).prediction();
        return isNumberSpeculation(left) && isNumberSpeculation(right);
    }
    
    // Note that a 'true' return does not actually mean that the ByVal access clobbers nothing.
    // It really means that it will not clobber the entire world. It's still up to you to
    // carefully consider things like:
    // - PutByVal definitely changes the array it stores to, and may even change its length.
    // - PutByOffset definitely changes the object it stores to.
    // - and so on.
    bool byValIsPure(Node& node)
    {
        switch (node.arrayMode().type()) {
        case Array::Generic:
            return false;
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage:
            return !node.arrayMode().isOutOfBounds();
        case Array::SlowPutArrayStorage:
            return !node.arrayMode().mayStoreToHole();
        case Array::String:
            return node.op() == GetByVal;
#if USE(JSVALUE32_64)
        case Array::Arguments:
            if (node.op() == GetByVal)
                return true;
            return false;
#endif // USE(JSVALUE32_64)
        default:
            return true;
        }
    }
    
    bool clobbersWorld(Node& node)
    {
        if (node.flags() & NodeClobbersWorld)
            return true;
        if (!(node.flags() & NodeMightClobber))
            return false;
        switch (node.op()) {
        case ValueAdd:
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq:
        case CompareEq:
            return !isPredictedNumerical(node);
        case GetByVal:
        case PutByVal:
        case PutByValAlias:
            return !byValIsPure(node);
        default:
            ASSERT_NOT_REACHED();
            return true; // If by some oddity we hit this case in release build it's safer to have CSE assume the worst.
        }
    }
    
    bool clobbersWorld(NodeIndex nodeIndex)
    {
        return clobbersWorld(at(nodeIndex));
    }
    
    void determineReachability();
    void resetReachability();
    
    void resetExitStates();
    
    unsigned varArgNumChildren(Node& node)
    {
        ASSERT(node.flags() & NodeHasVarArgs);
        return node.numChildren();
    }
    
    unsigned numChildren(Node& node)
    {
        if (node.flags() & NodeHasVarArgs)
            return varArgNumChildren(node);
        return AdjacencyList::Size;
    }
    
    Edge& varArgChild(Node& node, unsigned index)
    {
        ASSERT(node.flags() & NodeHasVarArgs);
        return m_varArgChildren[node.firstChild() + index];
    }
    
    Edge& child(Node& node, unsigned index)
    {
        if (node.flags() & NodeHasVarArgs)
            return varArgChild(node, index);
        return node.children.child(index);
    }
    
    void vote(Edge edge, unsigned ballot)
    {
        switch (at(edge).op()) {
        case ValueToInt32:
        case UInt32ToNumber:
            edge = at(edge).child1();
            break;
        default:
            break;
        }
        
        if (at(edge).op() == GetLocal)
            at(edge).variableAccessData()->vote(ballot);
    }
    
    void vote(Node& node, unsigned ballot)
    {
        if (node.flags() & NodeHasVarArgs) {
            for (unsigned childIdx = node.firstChild();
                 childIdx < node.firstChild() + node.numChildren();
                 childIdx++) {
                if (!!m_varArgChildren[childIdx])
                    vote(m_varArgChildren[childIdx], ballot);
            }
            return;
        }
        
        if (!node.child1())
            return;
        vote(node.child1(), ballot);
        if (!node.child2())
            return;
        vote(node.child2(), ballot);
        if (!node.child3())
            return;
        vote(node.child3(), ballot);
    }
    
    template<typename T> // T = NodeIndex or Edge
    void substitute(BasicBlock& block, unsigned startIndexInBlock, T oldThing, T newThing)
    {
        for (unsigned indexInBlock = startIndexInBlock; indexInBlock < block.size(); ++indexInBlock) {
            NodeIndex nodeIndex = block[indexInBlock];
            Node& node = at(nodeIndex);
            if (node.flags() & NodeHasVarArgs) {
                for (unsigned childIdx = node.firstChild(); childIdx < node.firstChild() + node.numChildren(); ++childIdx) {
                    if (!!m_varArgChildren[childIdx])
                        compareAndSwap(m_varArgChildren[childIdx], oldThing, newThing, node.shouldGenerate());
                }
                continue;
            }
            if (!node.child1())
                continue;
            compareAndSwap(node.children.child1(), oldThing, newThing, node.shouldGenerate());
            if (!node.child2())
                continue;
            compareAndSwap(node.children.child2(), oldThing, newThing, node.shouldGenerate());
            if (!node.child3())
                continue;
            compareAndSwap(node.children.child3(), oldThing, newThing, node.shouldGenerate());
        }
    }
    
    // Use this if you introduce a new GetLocal and you know that you introduced it *before*
    // any GetLocals in the basic block.
    // FIXME: it may be appropriate, in the future, to generalize this to handle GetLocals
    // introduced anywhere in the basic block.
    void substituteGetLocal(BasicBlock& block, unsigned startIndexInBlock, VariableAccessData* variableAccessData, NodeIndex newGetLocal)
    {
        if (variableAccessData->isCaptured()) {
            // Let CSE worry about this one.
            return;
        }
        for (unsigned indexInBlock = startIndexInBlock; indexInBlock < block.size(); ++indexInBlock) {
            NodeIndex nodeIndex = block[indexInBlock];
            Node& node = at(nodeIndex);
            bool shouldContinue = true;
            switch (node.op()) {
            case SetLocal: {
                if (node.local() == variableAccessData->local())
                    shouldContinue = false;
                break;
            }
                
            case GetLocal: {
                if (node.variableAccessData() != variableAccessData)
                    continue;
                substitute(block, indexInBlock, nodeIndex, newGetLocal);
                NodeIndex oldTailIndex = block.variablesAtTail.operand(variableAccessData->local());
                if (oldTailIndex == nodeIndex)
                    block.variablesAtTail.operand(variableAccessData->local()) = newGetLocal;
                shouldContinue = false;
                break;
            }
                
            default:
                break;
            }
            if (!shouldContinue)
                break;
        }
    }
    
    JSGlobalData& m_globalData;
    CodeBlock* m_codeBlock;
    RefPtr<Profiler::Compilation> m_compilation;
    CodeBlock* m_profiledBlock;

    Vector< OwnPtr<BasicBlock> , 8> m_blocks;
    Vector<Edge, 16> m_varArgChildren;
    Vector<StorageAccessData> m_storageAccessData;
    Vector<ResolveGlobalData> m_resolveGlobalData;
    Vector<ResolveOperationData> m_resolveOperationsData;
    Vector<PutToBaseOperationData> m_putToBaseOperationData;
    Vector<NodeIndex, 8> m_arguments;
    SegmentedVector<VariableAccessData, 16> m_variableAccessData;
    SegmentedVector<ArgumentPosition, 8> m_argumentPositions;
    SegmentedVector<StructureSet, 16> m_structureSet;
    SegmentedVector<StructureTransitionData, 8> m_structureTransitionData;
    SegmentedVector<NewArrayBufferData, 4> m_newArrayBufferData;
    bool m_hasArguments;
    HashSet<ExecutableBase*> m_executablesWhoseArgumentsEscaped;
    BitVector m_preservedVars;
    Dominators m_dominators;
    unsigned m_localVars;
    unsigned m_parameterSlots;
    unsigned m_osrEntryBytecodeIndex;
    Operands<JSValue> m_mustHandleValues;
    
    OptimizationFixpointState m_fixpointState;
private:
    
    void handleSuccessor(Vector<BlockIndex, 16>& worklist, BlockIndex blockIndex, BlockIndex successorIndex);
    
    bool addImmediateShouldSpeculateInteger(Node& add, Node& variable, Node& immediate)
    {
        ASSERT(immediate.hasConstant());
        
        JSValue immediateValue = immediate.valueOfJSConstant(m_codeBlock);
        if (!immediateValue.isNumber())
            return false;
        
        if (!variable.shouldSpeculateIntegerExpectingDefined())
            return false;
        
        if (immediateValue.isInt32())
            return add.canSpeculateInteger();
        
        double doubleImmediate = immediateValue.asDouble();
        const double twoToThe48 = 281474976710656.0;
        if (doubleImmediate < -twoToThe48 || doubleImmediate > twoToThe48)
            return false;
        
        return nodeCanTruncateInteger(add.arithNodeFlags());
    }
    
    bool mulImmediateShouldSpeculateInteger(Node& mul, Node& variable, Node& immediate)
    {
        ASSERT(immediate.hasConstant());
        
        JSValue immediateValue = immediate.valueOfJSConstant(m_codeBlock);
        if (!immediateValue.isInt32())
            return false;
        
        if (!variable.shouldSpeculateIntegerForArithmetic())
            return false;
        
        int32_t intImmediate = immediateValue.asInt32();
        // Doubles have a 53 bit mantissa so we expect a multiplication of 2^31 (the highest
        // magnitude possible int32 value) and any value less than 2^22 to not result in any
        // rounding in a double multiplication - hence it will be equivalent to an integer
        // multiplication, if we are doing int32 truncation afterwards (which is what
        // canSpeculateInteger() implies).
        const int32_t twoToThe22 = 1 << 22;
        if (intImmediate <= -twoToThe22 || intImmediate >= twoToThe22)
            return mul.canSpeculateInteger() && !nodeMayOverflow(mul.arithNodeFlags());

        return mul.canSpeculateInteger();
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
    return *binarySearch<BlockIndex, unsigned>(linkingTargets, linkingTargets.size(), bytecodeBegin, GetBytecodeBeginForBlock(*this));
}

} } // namespace JSC::DFG

#endif
#endif
