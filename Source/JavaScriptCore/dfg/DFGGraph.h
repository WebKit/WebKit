/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(DFG_JIT)

#include "AssemblyHelpers.h"
#include "BytecodeLivenessAnalysisInlines.h"
#include "CodeBlock.h"
#include "DFGArgumentPosition.h"
#include "DFGBasicBlock.h"
#include "DFGFrozenValue.h"
#include "DFGNode.h"
#include "DFGPlan.h"
#include "DFGPropertyTypeKey.h"
#include "DFGScannable.h"
#include "FullBytecodeLiveness.h"
#include "MethodOfGettingAValueProfile.h"
#include <wtf/BitVector.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StdUnorderedMap.h>

namespace WTF {
template <typename T> class SingleRootGraph;
}

namespace JSC {

class CodeBlock;
class ExecState;

namespace DFG {

class BackwardsCFG;
class BackwardsDominators;
class CFG;
class CPSCFG;
class ControlEquivalenceAnalysis;
template <typename T> class Dominators;
template <typename T> class NaturalLoops;
class FlowIndexing;
template<typename> class FlowMap;

using ArgumentsVector = Vector<Node*, 8>;

using SSACFG = CFG;
using CPSDominators = Dominators<CPSCFG>;
using SSADominators = Dominators<SSACFG>;
using CPSNaturalLoops = NaturalLoops<CPSCFG>;
using SSANaturalLoops = NaturalLoops<SSACFG>;

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
            for (unsigned _edgeIndex = 0; _edgeIndex < AdjacencyList::Size; _edgeIndex++) { \
                Edge& _edge = _node->children.child(_edgeIndex);        \
                if (!_edge)                                             \
                    break;                                              \
                thingToDo(_node, _edge);                                \
            }                                                           \
        }                                                               \
    } while (false)

#define DFG_ASSERT(graph, node, assertion, ...) do {                    \
        if (!!(assertion))                                              \
            break;                                                      \
        (graph).logAssertionFailure(                                    \
            (node), __FILE__, __LINE__, WTF_PRETTY_FUNCTION, #assertion); \
        CRASH_WITH_SECURITY_IMPLICATION_AND_INFO(__VA_ARGS__);          \
    } while (false)

#define DFG_CRASH(graph, node, reason, ...) do {                        \
        (graph).logAssertionFailure(                                    \
            (node), __FILE__, __LINE__, WTF_PRETTY_FUNCTION, (reason)); \
        CRASH_WITH_SECURITY_IMPLICATION_AND_INFO(__VA_ARGS__);          \
    } while (false)

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
    Graph(VM&, Plan&);
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
        Node* replacement = child->replacement();
        if (!replacement)
            return;
        
        child.setNode(replacement);
        
        // There is definitely a replacement. Assert that the replacement does not
        // have a replacement.
        ASSERT(!child->replacement());
    }
    
    template<typename... Params>
    Node* addNode(Params... params)
    {
        return m_nodes.addNew(params...);
    }

    template<typename... Params>
    Node* addNode(SpeculatedType type, Params... params)
    {
        Node* node = m_nodes.addNew(params...);
        node->predict(type);
        return node;
    }

    void deleteNode(Node*);
    unsigned maxNodeCount() const { return m_nodes.size(); }
    Node* nodeAt(unsigned index) const { return m_nodes[index]; }
    void packNodeIndices();

    void dethread();
    
    FrozenValue* freeze(JSValue); // We use weak freezing by default.
    FrozenValue* freezeStrong(JSValue); // Shorthand for freeze(value)->strengthenTo(StrongValue).
    
    void convertToConstant(Node* node, FrozenValue* value);
    void convertToConstant(Node* node, JSValue value);
    void convertToStrongConstant(Node* node, JSValue value);
    
    RegisteredStructure registerStructure(Structure* structure)
    {
        StructureRegistrationResult ignored;
        return registerStructure(structure, ignored);
    }
    RegisteredStructure registerStructure(Structure*, StructureRegistrationResult&);
    void registerAndWatchStructureTransition(Structure*);
    void assertIsRegistered(Structure* structure);
    
    // CodeBlock is optional, but may allow additional information to be dumped (e.g. Identifier names).
    void dump(PrintStream& = WTF::dataFile(), DumpContext* = 0);
    
    bool terminalsAreValid();
    
    enum PhiNodeDumpMode { DumpLivePhisOnly, DumpAllPhis };
    void dumpBlockHeader(PrintStream&, const char* prefix, BasicBlock*, PhiNodeDumpMode, DumpContext*);
    void dump(PrintStream&, Edge);
    void dump(PrintStream&, const char* prefix, Node*, DumpContext* = 0);
    static int amountOfNodeWhiteSpace(Node*);
    static void printNodeWhiteSpace(PrintStream&, Node*);

    // Dump the code origin of the given node as a diff from the code origin of the
    // preceding node. Returns true if anything was printed.
    bool dumpCodeOrigin(PrintStream&, const char* prefix, Node*& previousNode, Node* currentNode, DumpContext*);

    AddSpeculationMode addSpeculationMode(Node* add, bool leftShouldSpeculateInt32, bool rightShouldSpeculateInt32, PredictionPass pass)
    {
        ASSERT(add->op() == ValueAdd || add->op() == ValueSub || add->op() == ArithAdd || add->op() == ArithSub);
        
        RareCaseProfilingSource source = add->sourceFor(pass);
        
        Node* left = add->child1().node();
        Node* right = add->child2().node();
        
        if (left->hasConstant())
            return addImmediateShouldSpeculateInt32(add, rightShouldSpeculateInt32, right, left, source);
        if (right->hasConstant())
            return addImmediateShouldSpeculateInt32(add, leftShouldSpeculateInt32, left, right, source);
        
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
    
    bool addShouldSpeculateAnyInt(Node* add)
    {
        if (!enableInt52())
            return false;
        
        Node* left = add->child1().node();
        Node* right = add->child2().node();

        if (hasExitSite(add, Int52Overflow))
            return false;

        if (Node::shouldSpeculateAnyInt(left, right))
            return true;

        auto shouldSpeculateAnyIntForAdd = [](Node* node) {
            auto isAnyIntSpeculationForAdd = [](SpeculatedType value) {
                return !!value && (value & (SpecAnyInt | SpecAnyIntAsDouble)) == value;
            };

            // When DoubleConstant node appears, it means that users explicitly write a constant in their code with double form instead of integer form (1.0 instead of 1).
            // In that case, we should honor this decision: using it as integer is not appropriate.
            if (node->op() == DoubleConstant)
                return false;
            return isAnyIntSpeculationForAdd(node->prediction());
        };

        // Allow AnyInt ArithAdd only when the one side of the binary operation should be speculated AnyInt. It is a bit conservative
        // decision. This is because Double to Int52 conversion is not so cheap. Frequent back-and-forth conversions between Double and Int52
        // rather hurt the performance. If the one side of the operation is already Int52, the cost for constructing ArithAdd becomes
        // cheap since only one Double to Int52 conversion could be required.
        // This recovers some regression in assorted tests while keeping kraken crypto improvements.
        if (!left->shouldSpeculateAnyInt() && !right->shouldSpeculateAnyInt())
            return false;

        auto usesAsNumbers = [](Node* node) {
            NodeFlags flags = node->flags() & NodeBytecodeBackPropMask;
            if (!flags)
                return false;
            return (flags & (NodeBytecodeUsesAsNumber | NodeBytecodeNeedsNegZero | NodeBytecodeUsesAsInt | NodeBytecodeUsesAsArrayIndex)) == flags;
        };

        // Wrapping Int52 to Value is also not so cheap. Thus, we allow Int52 addition only when the node is used as number.
        if (!usesAsNumbers(add))
            return false;

        return shouldSpeculateAnyIntForAdd(left) && shouldSpeculateAnyIntForAdd(right);
    }
    
    bool binaryArithShouldSpeculateInt32(Node* node, PredictionPass pass)
    {
        Node* left = node->child1().node();
        Node* right = node->child2().node();
        
        return Node::shouldSpeculateInt32OrBooleanForArithmetic(left, right)
            && node->canSpeculateInt32(node->sourceFor(pass));
    }
    
    bool binaryArithShouldSpeculateAnyInt(Node* node, PredictionPass pass)
    {
        if (!enableInt52())
            return false;
        
        Node* left = node->child1().node();
        Node* right = node->child2().node();

        return Node::shouldSpeculateAnyInt(left, right)
            && node->canSpeculateInt52(pass)
            && !hasExitSite(node, Int52Overflow);
    }
    
    bool unaryArithShouldSpeculateInt32(Node* node, PredictionPass pass)
    {
        return node->child1()->shouldSpeculateInt32OrBooleanForArithmetic()
            && node->canSpeculateInt32(pass);
    }
    
    bool unaryArithShouldSpeculateAnyInt(Node* node, PredictionPass pass)
    {
        if (!enableInt52())
            return false;
        return node->child1()->shouldSpeculateAnyInt()
            && node->canSpeculateInt52(pass)
            && !hasExitSite(node, Int52Overflow);
    }

    bool canOptimizeStringObjectAccess(const CodeOrigin&);

    bool getRegExpPrototypeProperty(JSObject* regExpPrototype, Structure* regExpPrototypeStructure, UniquedStringImpl* uid, JSValue& returnJSValue);

    bool roundShouldSpeculateInt32(Node* arithRound, PredictionPass pass)
    {
        ASSERT(arithRound->op() == ArithRound || arithRound->op() == ArithFloor || arithRound->op() == ArithCeil || arithRound->op() == ArithTrunc);
        return arithRound->canSpeculateInt32(pass) && !hasExitSite(arithRound->origin.semantic, Overflow) && !hasExitSite(arithRound->origin.semantic, NegativeZero);
    }
    
    static const char *opName(NodeType);
    
    RegisteredStructureSet* addStructureSet(const StructureSet& structureSet)
    {
        m_structureSets.append();
        RegisteredStructureSet* result = &m_structureSets.last();

        for (Structure* structure : structureSet)
            result->add(registerStructure(structure));

        return result;
    }

    RegisteredStructureSet* addStructureSet(const RegisteredStructureSet& structureSet)
    {
        m_structureSets.append();
        RegisteredStructureSet* result = &m_structureSets.last();

        for (RegisteredStructure structure : structureSet)
            result->add(structure);

        return result;
    }
    
    JSGlobalObject* globalObjectFor(CodeOrigin codeOrigin)
    {
        return m_codeBlock->globalObjectFor(codeOrigin);
    }
    
    JSObject* globalThisObjectFor(CodeOrigin codeOrigin)
    {
        JSGlobalObject* object = globalObjectFor(codeOrigin);
        return jsCast<JSObject*>(object->methodTable(m_vm)->toThis(object, object->globalExec(), NotStrictMode));
    }
    
    ScriptExecutable* executableFor(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return m_codeBlock->ownerScriptExecutable();
        
        return inlineCallFrame->baselineCodeBlock->ownerScriptExecutable();
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
    
    bool isStrictModeFor(CodeOrigin codeOrigin)
    {
        if (!codeOrigin.inlineCallFrame)
            return m_codeBlock->isStrictMode();
        return codeOrigin.inlineCallFrame->isStrictMode();
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
        return baselineCodeBlockFor(codeOrigin)->unlinkedCodeBlock()->hasExitSite(FrequentExitSite(exitKind));
    }
    
    bool hasExitSite(const CodeOrigin& codeOrigin, ExitKind exitKind)
    {
        return baselineCodeBlockFor(codeOrigin)->unlinkedCodeBlock()->hasExitSite(FrequentExitSite(codeOrigin.bytecodeIndex, exitKind));
    }
    
    bool hasExitSite(Node* node, ExitKind exitKind)
    {
        return hasExitSite(node->origin.semantic, exitKind);
    }
    
    MethodOfGettingAValueProfile methodOfGettingAValueProfileFor(Node* currentNode, Node* operandNode);
    
    BlockIndex numBlocks() const { return m_blocks.size(); }
    BasicBlock* block(BlockIndex blockIndex) const { return m_blocks[blockIndex].get(); }
    BasicBlock* lastBlock() const { return block(numBlocks() - 1); }

    void appendBlock(Ref<BasicBlock>&& basicBlock)
    {
        basicBlock->index = m_blocks.size();
        m_blocks.append(WTFMove(basicBlock));
    }
    
    void killBlock(BlockIndex blockIndex)
    {
        m_blocks[blockIndex] = nullptr;
    }
    
    void killBlock(BasicBlock* basicBlock)
    {
        killBlock(basicBlock->index);
    }
    
    void killBlockAndItsContents(BasicBlock*);
    
    void killUnreachableBlocks();
    
    void determineReachability();
    void resetReachability();
    
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

    template <typename Function = bool(*)(Edge)>
    AdjacencyList copyVarargChildren(Node* node, Function filter = [] (Edge) { return true; })
    {
        ASSERT(node->flags() & NodeHasVarArgs);
        unsigned firstChild = m_varArgChildren.size();
        unsigned numChildren = 0;
        doToChildren(node, [&] (Edge edge) {
            if (filter(edge)) {
                ++numChildren;
                m_varArgChildren.append(edge);
            }
        });

        return AdjacencyList(AdjacencyList::Variable, firstChild, numChildren);
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
    void invalidateNodeLiveness();
    
    void clearFlagsOnAllNodes(NodeFlags);
    
    void clearReplacements();
    void clearEpochs();
    void initializeNodeOwners();
    
    BlockList blocksInPreOrder();
    BlockList blocksInPostOrder(bool isSafeToValidate = true);
    
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
    ALWAYS_INLINE void doToChildrenWithNode(Node* node, const ChildFunctor& functor)
    {
        DFG_NODE_DO_TO_CHILDREN(*this, node, functor);
    }
    
    template<typename ChildFunctor>
    ALWAYS_INLINE void doToChildren(Node* node, const ChildFunctor& functor)
    {
        class ForwardingFunc {
        public:
            ForwardingFunc(const ChildFunctor& functor)
                : m_functor(functor)
            {
            }
            
            // This is a manually written func because we want ALWAYS_INLINE.
            ALWAYS_INLINE void operator()(Node*, Edge& edge) const
            {
                m_functor(edge);
            }
        
        private:
            const ChildFunctor& m_functor;
        };
    
        doToChildrenWithNode(node, ForwardingFunc(functor));
    }
    
    bool uses(Node* node, Node* child)
    {
        bool result = false;
        doToChildren(node, [&] (Edge edge) { result |= edge == child; });
        return result;
    }

    bool isWatchingHavingABadTimeWatchpoint(Node* node)
    {
        JSGlobalObject* globalObject = globalObjectFor(node->origin.semantic);
        return watchpoints().isWatched(globalObject->havingABadTimeWatchpoint());
    }

    bool isWatchingGlobalObjectWatchpoint(JSGlobalObject* globalObject, InlineWatchpointSet& set)
    {
        if (watchpoints().isWatched(set))
            return true;

        if (set.isStillValid()) {
            // Since the global object owns this watchpoint, we make ourselves have a weak pointer to it.
            // If the global object got deallocated, it wouldn't fire the watchpoint. It's unlikely the
            // global object would get deallocated without this code ever getting thrown away, however,
            // it's more sound logically to depend on the global object lifetime weakly.
            freeze(globalObject);
            watchpoints().addLazily(set);
            return true;
        }

        return false;
    }

    bool isWatchingArrayIteratorProtocolWatchpoint(Node* node)
    {
        JSGlobalObject* globalObject = globalObjectFor(node->origin.semantic);
        InlineWatchpointSet& set = globalObject->arrayIteratorProtocolWatchpoint();
        return isWatchingGlobalObjectWatchpoint(globalObject, set);
    }

    bool isWatchingNumberToStringWatchpoint(Node* node)
    {
        JSGlobalObject* globalObject = globalObjectFor(node->origin.semantic);
        InlineWatchpointSet& set = globalObject->numberToStringWatchpoint();
        return isWatchingGlobalObjectWatchpoint(globalObject, set);
    }

    Profiler::Compilation* compilation() { return m_plan.compilation(); }

    DesiredIdentifiers& identifiers() { return m_plan.identifiers(); }
    DesiredWatchpoints& watchpoints() { return m_plan.watchpoints(); }
    DesiredGlobalProperties& globalProperties() { return m_plan.globalProperties(); }

    // Returns false if the key is already invalid or unwatchable. If this is a Presence condition,
    // this also makes it cheap to query if the condition holds. Also makes sure that the GC knows
    // what's going on.
    bool watchCondition(const ObjectPropertyCondition&);
    bool watchConditions(const ObjectPropertyConditionSet&);

    // Checks if it's known that loading from the given object at the given offset is fine. This is
    // computed by tracking which conditions we track with watchCondition().
    bool isSafeToLoad(JSObject* base, PropertyOffset);

    void registerInferredType(const InferredType::Descriptor& type)
    {
        if (type.structure())
            registerStructure(type.structure());
    }

    // Tells us what inferred type we are able to prove the property to have now and in the future.
    InferredType::Descriptor inferredTypeFor(const PropertyTypeKey&);
    InferredType::Descriptor inferredTypeForProperty(Structure* structure, UniquedStringImpl* uid)
    {
        return inferredTypeFor(PropertyTypeKey(structure, uid));
    }

    AbstractValue inferredValueForProperty(
        const RegisteredStructureSet& base, UniquedStringImpl* uid, StructureClobberState = StructuresAreWatched);

    // This uses either constant property inference or property type inference to derive a good abstract
    // value for some property accessed with the given abstract value base.
    AbstractValue inferredValueForProperty(
        const AbstractValue& base, UniquedStringImpl* uid, PropertyOffset, StructureClobberState);
    
    FullBytecodeLiveness& livenessFor(CodeBlock*);
    FullBytecodeLiveness& livenessFor(InlineCallFrame*);
    
    // Quickly query if a single local is live at the given point. This is faster than calling
    // forAllLiveInBytecode() if you will only query one local. But, if you want to know all of the
    // locals live, then calling this for each local is much slower than forAllLiveInBytecode().
    bool isLiveInBytecode(VirtualRegister, CodeOrigin);
    
    // Quickly get all of the non-argument locals live at the given point. This doesn't give you
    // any arguments because those are all presumed live. You can call forAllLiveInBytecode() to
    // also get the arguments. This is much faster than calling isLiveInBytecode() for each local.
    template<typename Functor>
    void forAllLocalsLiveInBytecode(CodeOrigin codeOrigin, const Functor& functor)
    {
        // Support for not redundantly reporting arguments. Necessary because in case of a varargs
        // call, only the callee knows that arguments are live while in the case of a non-varargs
        // call, both callee and caller will see the variables live.
        VirtualRegister exclusionStart;
        VirtualRegister exclusionEnd;

        CodeOrigin* codeOriginPtr = &codeOrigin;
        
        for (;;) {
            InlineCallFrame* inlineCallFrame = codeOriginPtr->inlineCallFrame;
            VirtualRegister stackOffset(inlineCallFrame ? inlineCallFrame->stackOffset : 0);
            
            if (inlineCallFrame) {
                if (inlineCallFrame->isClosureCall)
                    functor(stackOffset + CallFrameSlot::callee);
                if (inlineCallFrame->isVarargs())
                    functor(stackOffset + CallFrameSlot::argumentCount);
            }
            
            CodeBlock* codeBlock = baselineCodeBlockFor(inlineCallFrame);
            FullBytecodeLiveness& fullLiveness = livenessFor(codeBlock);
            const FastBitVector& liveness = fullLiveness.getLiveness(codeOriginPtr->bytecodeIndex);
            for (unsigned relativeLocal = codeBlock->numCalleeLocals(); relativeLocal--;) {
                VirtualRegister reg = stackOffset + virtualRegisterForLocal(relativeLocal);
                
                // Don't report if our callee already reported.
                if (reg >= exclusionStart && reg < exclusionEnd)
                    continue;
                
                if (liveness[relativeLocal])
                    functor(reg);
            }
            
            if (!inlineCallFrame)
                break;

            // Arguments are always live. This would be redundant if it wasn't for our
            // op_call_varargs inlining. See the comment above.
            exclusionStart = stackOffset + CallFrame::argumentOffsetIncludingThis(0);
            exclusionEnd = stackOffset + CallFrame::argumentOffsetIncludingThis(inlineCallFrame->argumentsWithFixup.size());
            
            // We will always have a "this" argument and exclusionStart should be a smaller stack
            // offset than exclusionEnd.
            ASSERT(exclusionStart < exclusionEnd);

            for (VirtualRegister reg = exclusionStart; reg < exclusionEnd; reg += 1)
                functor(reg);
            
            codeOriginPtr = inlineCallFrame->getCallerSkippingTailCalls();

            // The first inline call frame could be an inline tail call
            if (!codeOriginPtr)
                break;
        }
    }
    
    // Get a BitVector of all of the non-argument locals live right now. This is mostly useful if
    // you want to compare two sets of live locals from two different CodeOrigins.
    BitVector localsLiveInBytecode(CodeOrigin);
    
    // Tells you all of the arguments and locals live at the given CodeOrigin. This is a small
    // extension to forAllLocalsLiveInBytecode(), since all arguments are always presumed live.
    template<typename Functor>
    void forAllLiveInBytecode(CodeOrigin codeOrigin, const Functor& functor)
    {
        forAllLocalsLiveInBytecode(codeOrigin, functor);
        
        // Report all arguments as being live.
        for (unsigned argument = block(0)->variablesAtHead.numberOfArguments(); argument--;)
            functor(virtualRegisterForArgument(argument));
    }
    
    BytecodeKills& killsFor(CodeBlock*);
    BytecodeKills& killsFor(InlineCallFrame*);
    
    static unsigned parameterSlotsForArgCount(unsigned);
    
    unsigned frameRegisterCount();
    unsigned stackPointerOffset();
    unsigned requiredRegisterCountForExit();
    unsigned requiredRegisterCountForExecutionAndExit();
    
    JSValue tryGetConstantProperty(JSValue base, const RegisteredStructureSet&, PropertyOffset);
    JSValue tryGetConstantProperty(JSValue base, Structure*, PropertyOffset);
    JSValue tryGetConstantProperty(JSValue base, const StructureAbstractValue&, PropertyOffset);
    JSValue tryGetConstantProperty(const AbstractValue&, PropertyOffset);
    
    JSValue tryGetConstantClosureVar(JSValue base, ScopeOffset);
    JSValue tryGetConstantClosureVar(const AbstractValue&, ScopeOffset);
    JSValue tryGetConstantClosureVar(Node*, ScopeOffset);
    
    JSArrayBufferView* tryGetFoldableView(JSValue);
    JSArrayBufferView* tryGetFoldableView(JSValue, ArrayMode arrayMode);

    bool canDoFastSpread(Node*, const AbstractValue&);
    
    void registerFrozenValues();
    
    void visitChildren(SlotVisitor&) override;
    
    void logAssertionFailure(
        std::nullptr_t, const char* file, int line, const char* function,
        const char* assertion);
    void logAssertionFailure(
        Node*, const char* file, int line, const char* function,
        const char* assertion);
    void logAssertionFailure(
        BasicBlock*, const char* file, int line, const char* function,
        const char* assertion);

    bool hasDebuggerEnabled() const { return m_hasDebuggerEnabled; }

    CPSDominators& ensureCPSDominators();
    SSADominators& ensureSSADominators();
    CPSNaturalLoops& ensureCPSNaturalLoops();
    SSANaturalLoops& ensureSSANaturalLoops();
    BackwardsCFG& ensureBackwardsCFG();
    BackwardsDominators& ensureBackwardsDominators();
    ControlEquivalenceAnalysis& ensureControlEquivalenceAnalysis();
    CPSCFG& ensureCPSCFG();

    // These functions only makes sense to call after bytecode parsing
    // because it queries the m_hasExceptionHandlers boolean whose value
    // is only fully determined after bytcode parsing.
    bool willCatchExceptionInMachineFrame(CodeOrigin codeOrigin)
    {
        CodeOrigin ignored;
        HandlerInfo* ignored2;
        return willCatchExceptionInMachineFrame(codeOrigin, ignored, ignored2);
    }
    bool willCatchExceptionInMachineFrame(CodeOrigin, CodeOrigin& opCatchOriginOut, HandlerInfo*& catchHandlerOut);
    
    bool needsScopeRegister() const { return m_hasDebuggerEnabled || m_codeBlock->usesEval(); }
    bool needsFlushedThis() const { return m_codeBlock->usesEval(); }

    void clearCPSCFGData();

    bool isRoot(BasicBlock* block) const
    {
        ASSERT_WITH_MESSAGE(!m_isInSSAConversion, "This is not written to work during SSA conversion.");

        if (m_form == SSA) {
            ASSERT(m_roots.size() == 1);
            ASSERT(m_roots.contains(this->block(0)));
            return block == this->block(0);
        }

        if (m_roots.size() <= 4) {
            bool result = m_roots.contains(block);
            ASSERT(result == m_rootToArguments.contains(block));
            return result;
        }
        bool result = m_rootToArguments.contains(block);
        ASSERT(result == m_roots.contains(block));
        return result;
    }

    VM& m_vm;
    Plan& m_plan;
    CodeBlock* m_codeBlock;
    CodeBlock* m_profiledBlock;
    
    Vector<RefPtr<BasicBlock>, 8> m_blocks;
    Vector<BasicBlock*, 1> m_roots;
    Vector<Edge, 16> m_varArgChildren;

    HashMap<EncodedJSValue, FrozenValue*, EncodedJSValueHash, EncodedJSValueHashTraits> m_frozenValueMap;
    Bag<FrozenValue> m_frozenValues;

    Vector<uint32_t> m_uint32ValuesInUse;
    
    Bag<StorageAccessData> m_storageAccessData;
    
    // In CPS, this is all of the SetArgument nodes for the arguments in the machine code block
    // that survived DCE. All of them except maybe "this" will survive DCE, because of the Flush
    // nodes. In SSA, this has no meaning. It's empty.
    HashMap<BasicBlock*, ArgumentsVector> m_rootToArguments;

    // In SSA, this is the argument speculation that we've locked in for an entrypoint block.
    //
    // We must speculate on the argument types at each entrypoint even if operations involving
    // arguments get killed. For example:
    //
    //     function foo(x) {
    //        var tmp = x + 1;
    //     }
    //
    // Assume that x is always int during profiling. The ArithAdd for "x + 1" will be dead and will
    // have a proven check for the edge to "x". So, we will not insert a Check node and we will
    // kill the GetStack for "x". But, we must do the int check in the progolue, because that's the
    // thing we used to allow DCE of ArithAdd. Otherwise the add could be impure:
    //
    //     var o = {
    //         valueOf: function() { do side effects; }
    //     };
    //     foo(o);
    //
    // If we DCE the ArithAdd and we remove the int check on x, then this won't do the side
    // effects.
    //
    // By convention, entrypoint index 0 is used for the CodeBlock's op_enter entrypoint.
    // So argumentFormats[0] are the argument formats for the normal call entrypoint.
    Vector<Vector<FlushFormat>> m_argumentFormats;

    // This maps an entrypoint index to a particular op_catch bytecode offset. By convention,
    // it'll never have zero as a key because we use zero to mean the op_enter entrypoint.
    HashMap<unsigned, unsigned> m_entrypointIndexToCatchBytecodeOffset;

    // This is the number of logical entrypoints that we're compiling. This is only used
    // in SSA. Each EntrySwitch node must have m_numberOfEntrypoints cases. Note, this is
    // not the same as m_roots.size(). m_roots.size() represents the number of roots in
    // the CFG. In SSA, m_roots.size() == 1 even if we're compiling more than one entrypoint.
    unsigned m_numberOfEntrypoints { UINT_MAX };

    SegmentedVector<VariableAccessData, 16> m_variableAccessData;
    SegmentedVector<ArgumentPosition, 8> m_argumentPositions;
    Bag<Transition> m_transitions;
    Bag<BranchData> m_branchData;
    Bag<SwitchData> m_switchData;
    Bag<MultiGetByOffsetData> m_multiGetByOffsetData;
    Bag<MultiPutByOffsetData> m_multiPutByOffsetData;
    Bag<MatchStructureData> m_matchStructureData;
    Bag<ObjectMaterializationData> m_objectMaterializationData;
    Bag<CallVarargsData> m_callVarargsData;
    Bag<LoadVarargsData> m_loadVarargsData;
    Bag<StackAccessData> m_stackAccessData;
    Bag<LazyJSValue> m_lazyJSValues;
    Bag<CallDOMGetterData> m_callDOMGetterData;
    Bag<BitVector> m_bitVectors;
    Vector<InlineVariableData, 4> m_inlineVariableData;
    HashMap<CodeBlock*, std::unique_ptr<FullBytecodeLiveness>> m_bytecodeLiveness;
    HashMap<CodeBlock*, std::unique_ptr<BytecodeKills>> m_bytecodeKills;
    HashSet<std::pair<JSObject*, PropertyOffset>> m_safeToLoad;
    HashMap<PropertyTypeKey, InferredType::Descriptor> m_inferredTypes;
    Vector<Ref<Snippet>> m_domJITSnippets;
    std::unique_ptr<CPSDominators> m_cpsDominators;
    std::unique_ptr<SSADominators> m_ssaDominators;
    std::unique_ptr<CPSNaturalLoops> m_cpsNaturalLoops;
    std::unique_ptr<SSANaturalLoops> m_ssaNaturalLoops;
    std::unique_ptr<SSACFG> m_ssaCFG;
    std::unique_ptr<CPSCFG> m_cpsCFG;
    std::unique_ptr<BackwardsCFG> m_backwardsCFG;
    std::unique_ptr<BackwardsDominators> m_backwardsDominators;
    std::unique_ptr<ControlEquivalenceAnalysis> m_controlEquivalenceAnalysis;
    unsigned m_localVars;
    unsigned m_nextMachineLocal;
    unsigned m_parameterSlots;
    
    HashSet<String> m_localStrings;
    HashMap<const StringImpl*, String> m_copiedStrings;

#if USE(JSVALUE32_64)
    StdUnorderedMap<int64_t, double*> m_doubleConstantsMap;
    std::unique_ptr<Bag<double>> m_doubleConstants;
#endif
    
    OptimizationFixpointState m_fixpointState;
    StructureRegistrationState m_structureRegistrationState;
    GraphForm m_form;
    UnificationState m_unificationState;
    PlanStage m_planStage { PlanStage::Initial };
    RefCountState m_refCountState;
    bool m_hasDebuggerEnabled;
    bool m_hasExceptionHandlers { false };
    bool m_isInSSAConversion { false };
    Optional<uint32_t> m_maxLocalsForCatchOSREntry;
    std::unique_ptr<FlowIndexing> m_indexingCache;
    std::unique_ptr<FlowMap<AbstractValue>> m_abstractValuesCache;
    Bag<EntrySwitchData> m_entrySwitchData;

    RegisteredStructure stringStructure;
    RegisteredStructure symbolStructure;

private:
    bool isStringPrototypeMethodSane(JSGlobalObject*, UniquedStringImpl*);

    void handleSuccessor(Vector<BasicBlock*, 16>& worklist, BasicBlock*, BasicBlock* successor);
    
    AddSpeculationMode addImmediateShouldSpeculateInt32(Node* add, bool variableShouldSpeculateInt32, Node* operand, Node*immediate, RareCaseProfilingSource source)
    {
        ASSERT(immediate->hasConstant());
        
        JSValue immediateValue = immediate->asJSValue();
        if (!immediateValue.isNumber() && !immediateValue.isBoolean())
            return DontSpeculateInt32;
        
        if (!variableShouldSpeculateInt32)
            return DontSpeculateInt32;

        // Integer constants can be typed Double if they are written like a double in the source code (e.g. 42.0).
        // In that case, we stay conservative unless the other operand was explicitly typed as integer.
        NodeFlags operandResultType = operand->result();
        if (operandResultType != NodeResultInt32 && immediateValue.isDouble())
            return DontSpeculateInt32;
        
        if (immediateValue.isBoolean() || jsNumber(immediateValue.asNumber()).isInt32())
            return add->canSpeculateInt32(source) ? SpeculateInt32 : DontSpeculateInt32;
        
        double doubleImmediate = immediateValue.asDouble();
        const double twoToThe48 = 281474976710656.0;
        if (doubleImmediate < -twoToThe48 || doubleImmediate > twoToThe48)
            return DontSpeculateInt32;
        
        return bytecodeCanTruncateInteger(add->arithNodeFlags()) ? SpeculateInt32AndTruncateConstants : DontSpeculateInt32;
    }

    B3::SparseCollection<Node> m_nodes;
    SegmentedVector<RegisteredStructureSet, 16> m_structureSets;
};

} } // namespace JSC::DFG

#endif
