/*
 * Copyright (C) 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "AssemblyHelpers.h"
#include "CodeBlock.h"
#include "DFGArgumentPosition.h"
#include "DFGBasicBlock.h"
#include "DFGDominators.h"
#include "DFGFrozenValue.h"
#include "DFGLongLivedState.h"
#include "DFGNaturalLoops.h"
#include "DFGNode.h"
#include "DFGNodeAllocator.h"
#include "DFGPlan.h"
#include "DFGPrePostNumbering.h"
#include "DFGScannable.h"
#include "JSStack.h"
#include "MethodOfGettingAValueProfile.h"
#include <unordered_map>
#include <wtf/BitVector.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

class CodeBlock;
class ExecState;

namespace DFG {

#define DFG_NODE_DO_TO_CHILDREN(graph, node, thingToDo) do {            \
        Node* _node = (node);                                           \
        if (_node->flags() & NodeHasVarArgs) {                          \
            for (unsigned _childIdx = _node->firstChild();              \
                _childIdx < _node->firstChild() + _node->numChildren(); \
                _childIdx++) {                                          \
                if (!!(graph).m_varArgChildren[_childIdx])              \
                    thingToDo(_node, (graph).m_varArgChildren[_childIdx]); \
            }                                                           \
        } else {                                                        \
            if (!_node->child1()) {                                     \
                ASSERT(                                                 \
                    !_node->child2()                                    \
                    && !_node->child3());                               \
                break;                                                  \
            }                                                           \
            thingToDo(_node, _node->child1());                          \
                                                                        \
            if (!_node->child2()) {                                     \
                ASSERT(!_node->child3());                               \
                break;                                                  \
            }                                                           \
            thingToDo(_node, _node->child2());                          \
                                                                        \
            if (!_node->child3())                                       \
                break;                                                  \
            thingToDo(_node, _node->child3());                          \
        }                                                               \
    } while (false)

#define DFG_ASSERT(graph, node, assertion) do {                         \
        if (!!(assertion))                                              \
            break;                                                      \
        (graph).handleAssertionFailure(                                 \
            (node), __FILE__, __LINE__, WTF_PRETTY_FUNCTION, #assertion); \
    } while (false)

#define DFG_CRASH(graph, node, reason)                                  \
    (graph).handleAssertionFailure(                                     \
        (node), __FILE__, __LINE__, WTF_PRETTY_FUNCTION, (reason));

struct InlineVariableData {
    InlineCallFrame* inlineCallFrame;
    unsigned argumentPositionStart;
    VariableAccessData* calleeVariable;
};

enum AddSpeculationMode {
    DontSpeculateInt32,
    SpeculateInt32AndTruncateConstants,
    SpeculateInt32
};

//
// === Graph ===
//
// The order may be significant for nodes with side-effects (property accesses, value conversions).
// Nodes that are 'dead' remain in the vector with refCount 0.
class Graph : public virtual Scannable {
public:
    Graph(VM&, Plan&, LongLivedState&);
    ~Graph();
    
    void changeChild(Edge& edge, Node* newNode)
    {
        edge.setNode(newNode);
    }
    
    void changeEdge(Edge& edge, Edge newEdge)
    {
        edge = newEdge;
    }
    
    void compareAndSwap(Edge& edge, Node* oldNode, Node* newNode)
    {
        if (edge.node() != oldNode)
            return;
        changeChild(edge, newNode);
    }
    
    void compareAndSwap(Edge& edge, Edge oldEdge, Edge newEdge)
    {
        if (edge != oldEdge)
            return;
        changeEdge(edge, newEdge);
    }
    
    void performSubstitution(Node* node)
    {
        if (node->flags() & NodeHasVarArgs) {
            for (unsigned childIdx = node->firstChild(); childIdx < node->firstChild() + node->numChildren(); childIdx++)
                performSubstitutionForEdge(m_varArgChildren[childIdx]);
        } else {
            performSubstitutionForEdge(node->child1());
            performSubstitutionForEdge(node->child2());
            performSubstitutionForEdge(node->child3());
        }
    }
    
    void performSubstitutionForEdge(Edge& child)
    {
        // Check if this operand is actually unused.
        if (!child)
            return;
        
        // Check if there is any replacement.
        Node* replacement = child->replacement;
        if (!replacement)
            return;
        
        child.setNode(replacement);
        
        // There is definitely a replacement. Assert that the replacement does not
        // have a replacement.
        ASSERT(!child->replacement);
    }
    
    template<typename... Params>
    Node* addNode(SpeculatedType type, Params... params)
    {
        Node* node = new (m_allocator) Node(params...);
        node->predict(type);
        return node;
    }

    void dethread();
    
    FrozenValue* freezeFragile(JSValue value);
    FrozenValue* freeze(JSValue value); // We use weak freezing by default. Shorthand for freezeFragile(value)->strengthenTo(WeakValue);
    FrozenValue* freezeStrong(JSValue value); // Shorthand for freezeFragile(value)->strengthenTo(StrongValue).
    
    void convertToConstant(Node* node, FrozenValue* value);
    void convertToConstant(Node* node, JSValue value);
    void convertToStrongConstant(Node* node, JSValue value);
    
    StructureRegistrationResult registerStructure(Structure* structure);
    void assertIsRegistered(Structure* structure);
    
    // CodeBlock is optional, but may allow additional information to be dumped (e.g. Identifier names).
    void dump(PrintStream& = WTF::dataFile(), DumpContext* = 0);
    enum PhiNodeDumpMode { DumpLivePhisOnly, DumpAllPhis };
    void dumpBlockHeader(PrintStream&, const char* prefix, BasicBlock*, PhiNodeDumpMode, DumpContext*);
    void dump(PrintStream&, Edge);
    void dump(PrintStream&, const char* prefix, Node*, DumpContext* = 0);
    static int amountOfNodeWhiteSpace(Node*);
    static void printNodeWhiteSpace(PrintStream&, Node*);

    // Dump the code origin of the given node as a diff from the code origin of the
    // preceding node. Returns true if anything was printed.
    bool dumpCodeOrigin(PrintStream&, const char* prefix, Node* previousNode, Node* currentNode, DumpContext*);

    AddSpeculationMode addSpeculationMode(Node* add, bool leftShouldSpeculateInt32, bool rightShouldSpeculateInt32, PredictionPass pass)
    {
        ASSERT(add->op() == ValueAdd || add->op() == ArithAdd || add->op() == ArithSub);
        
        RareCaseProfilingSource source = add->sourceFor(pass);
        
        Node* left = add->child1().node();
        Node* right = add->child2().node();
        
        if (left->hasConstant())
            return addImmediateShouldSpeculateInt32(add, rightShouldSpeculateInt32, left, source);
        if (right->hasConstant())
            return addImmediateShouldSpeculateInt32(add, leftShouldSpeculateInt32, right, source);
        
        return (leftShouldSpeculateInt32 && rightShouldSpeculateInt32 && add->canSpeculateInt32(source)) ? SpeculateInt32 : DontSpeculateInt32;
    }
    
    AddSpeculationMode valueAddSpeculationMode(Node* add, PredictionPass pass)
    {
        return addSpeculationMode(
            add,
            add->child1()->shouldSpeculateInt32OrBooleanExpectingDefined(),
            add->child2()->shouldSpeculateInt32OrBooleanExpectingDefined(),
            pass);
    }
    
    AddSpeculationMode arithAddSpeculationMode(Node* add, PredictionPass pass)
    {
        return addSpeculationMode(
            add,
            add->child1()->shouldSpeculateInt32OrBooleanForArithmetic(),
            add->child2()->shouldSpeculateInt32OrBooleanForArithmetic(),
            pass);
    }
    
    AddSpeculationMode addSpeculationMode(Node* add, PredictionPass pass)
    {
        if (add->op() == ValueAdd)
            return valueAddSpeculationMode(add, pass);
        
        return arithAddSpeculationMode(add, pass);
    }
    
    bool addShouldSpeculateInt32(Node* add, PredictionPass pass)
    {
        return addSpeculationMode(add, pass) != DontSpeculateInt32;
    }
    
    bool addShouldSpeculateMachineInt(Node* add)
    {
        if (!enableInt52())
            return false;
        
        Node* left = add->child1().node();
        Node* right = add->child2().node();

        bool speculation;
        if (add->op() == ValueAdd)
            speculation = Node::shouldSpeculateMachineInt(left, right);
        else
            speculation = Node::shouldSpeculateMachineInt(left, right);

        return speculation && !hasExitSite(add, Int52Overflow);
    }
    
    bool mulShouldSpeculateInt32(Node* mul, PredictionPass pass)
    {
        ASSERT(mul->op() == ArithMul);
        
        Node* left = mul->child1().node();
        Node* right = mul->child2().node();
        
        return Node::shouldSpeculateInt32OrBooleanForArithmetic(left, right)
            && mul->canSpeculateInt32(mul->sourceFor(pass));
    }
    
    bool mulShouldSpeculateMachineInt(Node* mul, PredictionPass pass)
    {
        ASSERT(mul->op() == ArithMul);
        
        if (!enableInt52())
            return false;
        
        Node* left = mul->child1().node();
        Node* right = mul->child2().node();

        return Node::shouldSpeculateMachineInt(left, right)
            && mul->canSpeculateInt52(pass)
            && !hasExitSite(mul, Int52Overflow);
    }
    
    bool negateShouldSpeculateInt32(Node* negate, PredictionPass pass)
    {
        ASSERT(negate->op() == ArithNegate);
        return negate->child1()->shouldSpeculateInt32OrBooleanForArithmetic()
            && negate->canSpeculateInt32(pass);
    }
    
    bool negateShouldSpeculateMachineInt(Node* negate, PredictionPass pass)
    {
        ASSERT(negate->op() == ArithNegate);
        if (!enableInt52())
            return false;
        return negate->child1()->shouldSpeculateMachineInt()
            && !hasExitSite(negate, Int52Overflow)
            && negate->canSpeculateInt52(pass);
    }
    
    VirtualRegister bytecodeRegisterForArgument(CodeOrigin codeOrigin, int argument)
    {
        return VirtualRegister(
            codeOrigin.inlineCallFrame->stackOffset +
            baselineCodeBlockFor(codeOrigin)->argumentIndexAfterCapture(argument));
    }
    
    static const char *opName(NodeType);
    
    StructureSet* addStructureSet(const StructureSet& structureSet)
    {
        ASSERT(structureSet.size());
        m_structureSet.append(structureSet);
        return &m_structureSet.last();
    }
    
    JSGlobalObject* globalObjectFor(CodeOrigin codeOrigin)
    {
        return m_codeBlock->globalObjectFor(codeOrigin);
    }
    
    JSObject* globalThisObjectFor(CodeOrigin codeOrigin)
    {
        JSGlobalObject* object = globalObjectFor(codeOrigin);
        return jsCast<JSObject*>(object->methodTable()->toThis(object, object->globalExec(), NotStrictMode));
    }
    
    ScriptExecutable* executableFor(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return m_codeBlock->ownerExecutable();
        
        return inlineCallFrame->executable.get();
    }
    
    ScriptExecutable* executableFor(const CodeOrigin& codeOrigin)
    {
        return executableFor(codeOrigin.inlineCallFrame);
    }
    
    CodeBlock* baselineCodeBlockFor(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return m_profiledBlock;
        return baselineCodeBlockForInlineCallFrame(inlineCallFrame);
    }
    
    CodeBlock* baselineCodeBlockFor(const CodeOrigin& codeOrigin)
    {
        return baselineCodeBlockForOriginAndBaselineCodeBlock(codeOrigin, m_profiledBlock);
    }
    
    const BitVector& capturedVarsFor(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return m_outermostCapturedVars;
        return inlineCallFrame->capturedVars;
    }
    
    bool isStrictModeFor(CodeOrigin codeOrigin)
    {
        if (!codeOrigin.inlineCallFrame)
            return m_codeBlock->isStrictMode();
        return jsCast<FunctionExecutable*>(codeOrigin.inlineCallFrame->executable.get())->isStrictMode();
    }
    
    ECMAMode ecmaModeFor(CodeOrigin codeOrigin)
    {
        return isStrictModeFor(codeOrigin) ? StrictMode : NotStrictMode;
    }
    
    bool masqueradesAsUndefinedWatchpointIsStillValid(const CodeOrigin& codeOrigin)
    {
        return globalObjectFor(codeOrigin)->masqueradesAsUndefinedWatchpoint()->isStillValid();
    }
    
    bool hasGlobalExitSite(const CodeOrigin& codeOrigin, ExitKind exitKind)
    {
        return baselineCodeBlockFor(codeOrigin)->hasExitSite(FrequentExitSite(exitKind));
    }
    
    bool hasExitSite(const CodeOrigin& codeOrigin, ExitKind exitKind)
    {
        return baselineCodeBlockFor(codeOrigin)->hasExitSite(FrequentExitSite(codeOrigin.bytecodeIndex, exitKind));
    }
    
    bool hasExitSite(Node* node, ExitKind exitKind)
    {
        return hasExitSite(node->origin.semantic, exitKind);
    }
    
    bool usesArguments(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return m_profiledBlock->usesArguments();
        
        return baselineCodeBlockForInlineCallFrame(inlineCallFrame)->usesArguments();
    }
    
    VirtualRegister argumentsRegisterFor(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return m_profiledBlock->argumentsRegister();
        
        return VirtualRegister(baselineCodeBlockForInlineCallFrame(
            inlineCallFrame)->argumentsRegister().offset() +
            inlineCallFrame->stackOffset);
    }
    
    VirtualRegister argumentsRegisterFor(const CodeOrigin& codeOrigin)
    {
        return argumentsRegisterFor(codeOrigin.inlineCallFrame);
    }
    
    VirtualRegister machineArgumentsRegisterFor(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return m_codeBlock->argumentsRegister();
        
        return inlineCallFrame->argumentsRegister;
    }
    
    VirtualRegister machineArgumentsRegisterFor(const CodeOrigin& codeOrigin)
    {
        return machineArgumentsRegisterFor(codeOrigin.inlineCallFrame);
    }
    
    VirtualRegister uncheckedArgumentsRegisterFor(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return m_profiledBlock->uncheckedArgumentsRegister();
        
        CodeBlock* codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);
        if (!codeBlock->usesArguments())
            return VirtualRegister();
        
        return VirtualRegister(codeBlock->argumentsRegister().offset() +
            inlineCallFrame->stackOffset);
    }
    
    VirtualRegister uncheckedArgumentsRegisterFor(const CodeOrigin& codeOrigin)
    {
        return uncheckedArgumentsRegisterFor(codeOrigin.inlineCallFrame);
    }
    
    VirtualRegister activationRegister()
    {
        return m_profiledBlock->activationRegister();
    }
    
    VirtualRegister uncheckedActivationRegister()
    {
        return m_profiledBlock->uncheckedActivationRegister();
    }
    
    VirtualRegister machineActivationRegister()
    {
        return m_profiledBlock->activationRegister();
    }
    
    VirtualRegister uncheckedMachineActivationRegister()
    {
        return m_profiledBlock->uncheckedActivationRegister();
    }
    
    ValueProfile* valueProfileFor(Node* node)
    {
        if (!node)
            return nullptr;
        
        CodeBlock* profiledBlock = baselineCodeBlockFor(node->origin.semantic);
        
        if (node->hasLocal(*this)) {
            if (!node->local().isArgument())
                return 0;
            int argument = node->local().toArgument();
            Node* argumentNode = m_arguments[argument];
            if (!argumentNode)
                return nullptr;
            if (node->variableAccessData() != argumentNode->variableAccessData())
                return nullptr;
            return profiledBlock->valueProfileForArgument(argument);
        }
        
        if (node->hasHeapPrediction())
            return profiledBlock->valueProfileForBytecodeOffset(node->origin.semantic.bytecodeIndex);
        
        return 0;
    }
    
    MethodOfGettingAValueProfile methodOfGettingAValueProfileFor(Node* node)
    {
        if (!node)
            return MethodOfGettingAValueProfile();
        
        if (ValueProfile* valueProfile = valueProfileFor(node))
            return MethodOfGettingAValueProfile(valueProfile);
        
        if (node->op() == GetLocal) {
            CodeBlock* profiledBlock = baselineCodeBlockFor(node->origin.semantic);
        
            return MethodOfGettingAValueProfile::fromLazyOperand(
                profiledBlock,
                LazyOperandValueProfileKey(
                    node->origin.semantic.bytecodeIndex, node->local()));
        }
        
        return MethodOfGettingAValueProfile();
    }
    
    bool usesArguments() const
    {
        return m_codeBlock->usesArguments();
    }
    
    BlockIndex numBlocks() const { return m_blocks.size(); }
    BasicBlock* block(BlockIndex blockIndex) const { return m_blocks[blockIndex].get(); }
    BasicBlock* lastBlock() const { return block(numBlocks() - 1); }

    void appendBlock(PassRefPtr<BasicBlock> basicBlock)
    {
        basicBlock->index = m_blocks.size();
        m_blocks.append(basicBlock);
    }
    
    void killBlock(BlockIndex blockIndex)
    {
        m_blocks[blockIndex].clear();
    }
    
    void killBlock(BasicBlock* basicBlock)
    {
        killBlock(basicBlock->index);
    }
    
    void killBlockAndItsContents(BasicBlock*);
    
    void killUnreachableBlocks();
    
    void determineReachability();
    void resetReachability();
    
    void mergeRelevantToOSR();
    
    void computeRefCounts();
    
    unsigned varArgNumChildren(Node* node)
    {
        ASSERT(node->flags() & NodeHasVarArgs);
        return node->numChildren();
    }
    
    unsigned numChildren(Node* node)
    {
        if (node->flags() & NodeHasVarArgs)
            return varArgNumChildren(node);
        return AdjacencyList::Size;
    }
    
    Edge& varArgChild(Node* node, unsigned index)
    {
        ASSERT(node->flags() & NodeHasVarArgs);
        return m_varArgChildren[node->firstChild() + index];
    }
    
    Edge& child(Node* node, unsigned index)
    {
        if (node->flags() & NodeHasVarArgs)
            return varArgChild(node, index);
        return node->children.child(index);
    }
    
    void voteNode(Node* node, unsigned ballot, float weight = 1)
    {
        switch (node->op()) {
        case ValueToInt32:
        case UInt32ToNumber:
            node = node->child1().node();
            break;
        default:
            break;
        }
        
        if (node->op() == GetLocal)
            node->variableAccessData()->vote(ballot, weight);
    }
    
    void voteNode(Edge edge, unsigned ballot, float weight = 1)
    {
        voteNode(edge.node(), ballot, weight);
    }
    
    void voteChildren(Node* node, unsigned ballot, float weight = 1)
    {
        if (node->flags() & NodeHasVarArgs) {
            for (unsigned childIdx = node->firstChild();
                childIdx < node->firstChild() + node->numChildren();
                childIdx++) {
                if (!!m_varArgChildren[childIdx])
                    voteNode(m_varArgChildren[childIdx], ballot, weight);
            }
            return;
        }
        
        if (!node->child1())
            return;
        voteNode(node->child1(), ballot, weight);
        if (!node->child2())
            return;
        voteNode(node->child2(), ballot, weight);
        if (!node->child3())
            return;
        voteNode(node->child3(), ballot, weight);
    }
    
    template<typename T> // T = Node* or Edge
    void substitute(BasicBlock& block, unsigned startIndexInBlock, T oldThing, T newThing)
    {
        for (unsigned indexInBlock = startIndexInBlock; indexInBlock < block.size(); ++indexInBlock) {
            Node* node = block[indexInBlock];
            if (node->flags() & NodeHasVarArgs) {
                for (unsigned childIdx = node->firstChild(); childIdx < node->firstChild() + node->numChildren(); ++childIdx) {
                    if (!!m_varArgChildren[childIdx])
                        compareAndSwap(m_varArgChildren[childIdx], oldThing, newThing);
                }
                continue;
            }
            if (!node->child1())
                continue;
            compareAndSwap(node->children.child1(), oldThing, newThing);
            if (!node->child2())
                continue;
            compareAndSwap(node->children.child2(), oldThing, newThing);
            if (!node->child3())
                continue;
            compareAndSwap(node->children.child3(), oldThing, newThing);
        }
    }
    
    // Use this if you introduce a new GetLocal and you know that you introduced it *before*
    // any GetLocals in the basic block.
    // FIXME: it may be appropriate, in the future, to generalize this to handle GetLocals
    // introduced anywhere in the basic block.
    void substituteGetLocal(BasicBlock& block, unsigned startIndexInBlock, VariableAccessData* variableAccessData, Node* newGetLocal);
    
    void invalidateCFG();
    
    void clearFlagsOnAllNodes(NodeFlags);
    
    void clearReplacements();
    void initializeNodeOwners();
    
    BlockList blocksInPreOrder();
    BlockList blocksInPostOrder();
    
    class NaturalBlockIterable {
    public:
        NaturalBlockIterable()
            : m_graph(nullptr)
        {
        }
        
        NaturalBlockIterable(Graph& graph)
            : m_graph(&graph)
        {
        }
        
        class iterator {
        public:
            iterator()
                : m_graph(nullptr)
                , m_index(0)
            {
            }
            
            iterator(Graph& graph, BlockIndex index)
                : m_graph(&graph)
                , m_index(findNext(index))
            {
            }
            
            BasicBlock *operator*()
            {
                return m_graph->block(m_index);
            }
            
            iterator& operator++()
            {
                m_index = findNext(m_index + 1);
                return *this;
            }
            
            bool operator==(const iterator& other) const
            {
                return m_index == other.m_index;
            }
            
            bool operator!=(const iterator& other) const
            {
                return !(*this == other);
            }
            
        private:
            BlockIndex findNext(BlockIndex index)
            {
                while (index < m_graph->numBlocks() && !m_graph->block(index))
                    index++;
                return index;
            }
            
            Graph* m_graph;
            BlockIndex m_index;
        };
        
        iterator begin()
        {
            return iterator(*m_graph, 0);
        }
        
        iterator end()
        {
            return iterator(*m_graph, m_graph->numBlocks());
        }
        
    private:
        Graph* m_graph;
    };
    
    NaturalBlockIterable blocksInNaturalOrder()
    {
        return NaturalBlockIterable(*this);
    }
    
    template<typename ChildFunctor>
    void doToChildrenWithNode(Node* node, const ChildFunctor& functor)
    {
        DFG_NODE_DO_TO_CHILDREN(*this, node, functor);
    }
    
    template<typename ChildFunctor>
    void doToChildren(Node* node, const ChildFunctor& functor)
    {
        doToChildrenWithNode(
            node,
            [&functor] (Node*, Edge& edge) {
                functor(edge);
            });
    }
    
    Profiler::Compilation* compilation() { return m_plan.compilation.get(); }
    
    DesiredIdentifiers& identifiers() { return m_plan.identifiers; }
    DesiredWatchpoints& watchpoints() { return m_plan.watchpoints; }
    
    FullBytecodeLiveness& livenessFor(CodeBlock*);
    FullBytecodeLiveness& livenessFor(InlineCallFrame*);
    bool isLiveInBytecode(VirtualRegister, CodeOrigin);
    
    unsigned frameRegisterCount();
    unsigned stackPointerOffset();
    unsigned requiredRegisterCountForExit();
    unsigned requiredRegisterCountForExecutionAndExit();
    
    JSValue tryGetConstantProperty(JSValue base, const StructureSet&, PropertyOffset);
    JSValue tryGetConstantProperty(JSValue base, Structure*, PropertyOffset);
    JSValue tryGetConstantProperty(JSValue base, const StructureAbstractValue&, PropertyOffset);
    JSValue tryGetConstantProperty(const AbstractValue&, PropertyOffset);
    
    JSLexicalEnvironment* tryGetActivation(Node*);
    WriteBarrierBase<Unknown>* tryGetRegisters(Node*);
    
    JSArrayBufferView* tryGetFoldableView(Node*);
    JSArrayBufferView* tryGetFoldableView(Node*, ArrayMode);
    JSArrayBufferView* tryGetFoldableViewForChild1(Node*);
    
    void registerFrozenValues();
    
    virtual void visitChildren(SlotVisitor&) override;
    
    NO_RETURN_DUE_TO_CRASH void handleAssertionFailure(
        std::nullptr_t, const char* file, int line, const char* function,
        const char* assertion);
    NO_RETURN_DUE_TO_CRASH void handleAssertionFailure(
        Node*, const char* file, int line, const char* function,
        const char* assertion);
    NO_RETURN_DUE_TO_CRASH void handleAssertionFailure(
        BasicBlock*, const char* file, int line, const char* function,
        const char* assertion);
    
    VM& m_vm;
    Plan& m_plan;
    CodeBlock* m_codeBlock;
    CodeBlock* m_profiledBlock;
    
    NodeAllocator& m_allocator;

    Operands<FrozenValue*> m_mustHandleValues;
    
    Vector< RefPtr<BasicBlock> , 8> m_blocks;
    Vector<Edge, 16> m_varArgChildren;

    HashMap<EncodedJSValue, FrozenValue*, EncodedJSValueHash, EncodedJSValueHashTraits> m_frozenValueMap;
    Bag<FrozenValue> m_frozenValues;
    
    Bag<StorageAccessData> m_storageAccessData;
    
    // In CPS, this is all of the SetArgument nodes for the arguments in the machine code block
    // that survived DCE. All of them except maybe "this" will survive DCE, because of the Flush
    // nodes.
    //
    // In SSA, this is all of the GetLocal nodes for the arguments in the machine code block that
    // may have some speculation in the prologue and survived DCE. Note that to get the speculation
    // for an argument in SSA, you must use m_argumentFormats, since we still have to speculate
    // even if the argument got killed. For example:
    //
    //     function foo(x) {
    //        var tmp = x + 1;
    //     }
    //
    // Assume that x is always int during profiling. The ArithAdd for "x + 1" will be dead and will
    // have a proven check for the edge to "x". So, we will not insert a Check node and we will
    // kill the GetLocal for "x". But, we must do the int check in the progolue, because that's the
    // thing we used to allow DCE of ArithAdd. Otherwise the add could be impure:
    //
    //     var o = {
    //         valueOf: function() { do side effects; }
    //     };
    //     foo(o);
    //
    // If we DCE the ArithAdd and we remove the int check on x, then this won't do the side
    // effects.
    Vector<Node*, 8> m_arguments;
    
    // In CPS, this is meaningless. In SSA, this is the argument speculation that we've locked in.
    Vector<FlushFormat> m_argumentFormats;
    
    SegmentedVector<VariableAccessData, 16> m_variableAccessData;
    SegmentedVector<ArgumentPosition, 8> m_argumentPositions;
    SegmentedVector<StructureSet, 16> m_structureSet;
    Bag<Transition> m_transitions;
    SegmentedVector<NewArrayBufferData, 4> m_newArrayBufferData;
    Bag<BranchData> m_branchData;
    Bag<SwitchData> m_switchData;
    Bag<MultiGetByOffsetData> m_multiGetByOffsetData;
    Bag<MultiPutByOffsetData> m_multiPutByOffsetData;
    Bag<ObjectMaterializationData> m_objectMaterializationData;
    Vector<InlineVariableData, 4> m_inlineVariableData;
    HashMap<CodeBlock*, std::unique_ptr<FullBytecodeLiveness>> m_bytecodeLiveness;
    bool m_hasArguments;
    HashSet<ExecutableBase*> m_executablesWhoseArgumentsEscaped;
    BitVector m_lazyVars;
    Dominators m_dominators;
    PrePostNumbering m_prePostNumbering;
    NaturalLoops m_naturalLoops;
    unsigned m_localVars;
    unsigned m_nextMachineLocal;
    unsigned m_parameterSlots;
    int m_machineCaptureStart;
    std::unique_ptr<SlowArgument[]> m_slowArguments;
    BitVector m_outermostCapturedVars;

#if USE(JSVALUE32_64)
    std::unordered_map<int64_t, double*> m_doubleConstantsMap;
    std::unique_ptr<Bag<double>> m_doubleConstants;
#endif
    
    OptimizationFixpointState m_fixpointState;
    StructureRegistrationState m_structureRegistrationState;
    GraphForm m_form;
    UnificationState m_unificationState;
    RefCountState m_refCountState;
private:
    
    void handleSuccessor(Vector<BasicBlock*, 16>& worklist, BasicBlock*, BasicBlock* successor);
    
    AddSpeculationMode addImmediateShouldSpeculateInt32(Node* add, bool variableShouldSpeculateInt32, Node* immediate, RareCaseProfilingSource source)
    {
        ASSERT(immediate->hasConstant());
        
        JSValue immediateValue = immediate->asJSValue();
        if (!immediateValue.isNumber() && !immediateValue.isBoolean())
            return DontSpeculateInt32;
        
        if (!variableShouldSpeculateInt32)
            return DontSpeculateInt32;
        
        if (immediateValue.isInt32() || immediateValue.isBoolean())
            return add->canSpeculateInt32(source) ? SpeculateInt32 : DontSpeculateInt32;
        
        double doubleImmediate = immediateValue.asDouble();
        const double twoToThe48 = 281474976710656.0;
        if (doubleImmediate < -twoToThe48 || doubleImmediate > twoToThe48)
            return DontSpeculateInt32;
        
        return bytecodeCanTruncateInteger(add->arithNodeFlags()) ? SpeculateInt32AndTruncateConstants : DontSpeculateInt32;
    }
};

} } // namespace JSC::DFG

#endif
#endif
