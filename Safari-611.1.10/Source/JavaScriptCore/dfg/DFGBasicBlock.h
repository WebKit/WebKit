/*
 * Copyright (C) 2011, 2013-2016 Apple Inc. All rights reserved.
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

#include "DFGAbstractValue.h"
#include "DFGAvailabilityMap.h"
#include "DFGBranchDirection.h"
#include "DFGNode.h"
#include "DFGNodeAbstractValuePair.h"
#include "DFGStructureClobberState.h"
#include "Operands.h"
#include <wtf/Vector.h>

namespace JSC { namespace DFG {

class Graph;
class InsertionSet;

typedef Vector<BasicBlock*, 2> PredecessorList;
typedef Vector<Node*, 8> BlockNodeList;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BasicBlock);

struct BasicBlock : RefCounted<BasicBlock> {
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(BasicBlock);
    BasicBlock(
        BytecodeIndex bytecodeBegin, unsigned numArguments, unsigned numLocals, unsigned numTmps,
        float executionCount);
    ~BasicBlock();
    
    void ensureLocals(unsigned newNumLocals);
    void ensureTmps(unsigned newNumTmps);
    
    size_t size() const { return m_nodes.size(); }
    bool isEmpty() const { return !size(); }
    Node*& at(size_t i) { return m_nodes[i]; }
    Node* at(size_t i) const { return m_nodes[i]; }
    Node* tryAt(size_t i) const
    {
        if (i >= size())
            return nullptr;
        return at(i);
    }
    Node*& operator[](size_t i) { return at(i); }
    Node* operator[](size_t i) const { return at(i); }
    Node* last() const
    {
        RELEASE_ASSERT(!!size());
        return at(size() - 1);
    }
    
    // Use this to find both the index of the terminal and the terminal itself in one go. May
    // return a clear NodeAndIndex if the basic block currently lacks a terminal. That may happen
    // in the middle of IR transformations within a phase but should never be the case in between
    // phases.
    //
    // The reason why this is more than just "at(size() - 1)" is that we may place non-terminal
    // liveness marking instructions after the terminal. This is supposed to happen infrequently
    // but some basic blocks - most notably return blocks - will have liveness markers for all of
    // the flushed variables right after the return.
    //
    // It turns out that doing this linear search is basically perf-neutral, so long as we force
    // the method to be inlined. Hence the ALWAYS_INLINE.
    ALWAYS_INLINE NodeAndIndex findTerminal() const
    {
        size_t i = size();
        while (i--) {
            Node* node = at(i);
            if (node->isTerminal())
                return NodeAndIndex(node, i);
            switch (node->op()) {
            // The bitter end can contain Phantoms and the like. There will probably only be one or two nodes after the terminal. They are all no-ops and will not have any checked children.
            case Check: // This is here because it's our universal no-op.
            case CheckVarargs:
            case Phantom:
            case PhantomLocal:
            case Flush:
                break;
            default:
                return NodeAndIndex();
            }
        }
        return NodeAndIndex();
    }
    
    ALWAYS_INLINE Node* terminal() const
    {
        return findTerminal().node;
    }
    
    void resize(size_t size) { m_nodes.resize(size); }
    void grow(size_t size) { m_nodes.grow(size); }
    
    void append(Node* node) { m_nodes.append(node); }
    void insertBeforeTerminal(Node* node)
    {
        NodeAndIndex result = findTerminal();
        if (!result)
            append(node);
        else
            m_nodes.insert(result.index, node);
    }
    
    void replaceTerminal(Graph&, Node*);
    
    size_t numNodes() const { return phis.size() + size(); }
    Node* node(size_t i) const
    {
        if (i < phis.size())
            return phis[i];
        return at(i - phis.size());
    }
    bool isPhiIndex(size_t i) const { return i < phis.size(); }
    
    bool isInPhis(Node* node) const;
    bool isInBlock(Node* myNode) const;
    
    BlockNodeList::iterator begin() { return m_nodes.begin(); }
    BlockNodeList::iterator end() { return m_nodes.end(); }

    unsigned numSuccessors() { return terminal()->numSuccessors(); }
    
    BasicBlock*& successor(unsigned index)
    {
        return terminal()->successor(index);
    }
    BasicBlock*& successorForCondition(bool condition)
    {
        return terminal()->successorForCondition(condition);
    }

    Node::SuccessorsIterable successors()
    {
        return terminal()->successors();
    }
    
    void removePredecessor(BasicBlock* block);
    void replacePredecessor(BasicBlock* from, BasicBlock* to);

    template<typename... Params>
    Node* appendNode(Graph&, SpeculatedType, Params...);
    
    template<typename... Params>
    Node* appendNonTerminal(Graph&, SpeculatedType, Params...);
    
    template<typename... Params>
    Node* replaceTerminal(Graph&, SpeculatedType, Params...);
    
    void dump(PrintStream& out) const;
    
    void didLink()
    {
#if ASSERT_ENABLED
        isLinked = true;
#endif
    }
    
    // This value is used internally for block linking and OSR entry. It is mostly meaningless
    // for other purposes due to inlining.
    BytecodeIndex bytecodeBegin;
    
    BlockIndex index;

    StructureClobberState cfaStructureClobberStateAtHead;
    StructureClobberState cfaStructureClobberStateAtTail;
    BranchDirection cfaBranchDirection;
    bool cfaHasVisited;
    bool cfaShouldRevisit;
    bool cfaThinksShouldTryConstantFolding { false };
    bool cfaDidFinish;
    bool intersectionOfCFAHasVisited;
    bool isOSRTarget;
    bool isCatchEntrypoint;

#if ASSERT_ENABLED
    bool isLinked;
#endif
    bool isReachable;
    
    Vector<Node*> phis;
    PredecessorList predecessors;
    
    Operands<Node*> variablesAtHead;
    Operands<Node*> variablesAtTail;
    
    Operands<AbstractValue> valuesAtHead;
    Operands<AbstractValue> valuesAtTail;
    
    // The intersection of assumptions we have made previously at the head of this block. Note
    // that under normal circumstances, each time we run the CFA, we will get strictly more precise
    // results. But we don't actually require this to be the case. It's fine for the CFA to loosen
    // up for any odd reason. It's fine when this happens, because anything that the CFA proves
    // must be true from that point forward, except if some registered watchpoint fires, in which
    // case the code won't ever run. So, the CFA proving something less precise later on is just an
    // outcome of the CFA being imperfect; the more precise thing that it had proved earlier is no
    // less true.
    //
    // But for the purpose of OSR entry, we need to make sure that we remember what assumptions we
    // had used for optimizing any given basic block. That's what this is for.
    //
    // It's interesting that we could use this to make the CFA more precise: all future CFAs could
    // filter their results with this thing to sort of maintain maximal precision. Because we
    // expect CFA to usually be monotonically more precise each time we run it to fixpoint, this
    // would not be a productive optimization: it would make setting up a basic block more
    // expensive and would only benefit bizarre pathological cases.
    Operands<AbstractValue> intersectionOfPastValuesAtHead;
    
    float executionCount;
    
    struct SSAData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        void invalidate()
        {
            liveAtTail.clear();
            liveAtHead.clear();
            valuesAtHead.clear();
            valuesAtTail.clear();
        }

        AvailabilityMap availabilityAtHead;
        AvailabilityMap availabilityAtTail;

        Vector<NodeFlowProjection> liveAtHead;
        Vector<NodeFlowProjection> liveAtTail;
        Vector<NodeAbstractValuePair> valuesAtHead;
        Vector<NodeAbstractValuePair> valuesAtTail;
        
        SSAData(BasicBlock*);
        ~SSAData();
    };
    std::unique_ptr<SSAData> ssa;
    
private:
    friend class InsertionSet;
    BlockNodeList m_nodes;
};

typedef Vector<BasicBlock*> BlockList;
    
static inline BytecodeIndex getBytecodeBeginForBlock(BasicBlock** basicBlock)
{
    return (*basicBlock)->bytecodeBegin;
}

static inline BasicBlock* blockForBytecodeIndex(Vector<BasicBlock*>& linkingTargets, BytecodeIndex bytecodeBegin)
{
    return *binarySearch<BasicBlock*, BytecodeIndex>(linkingTargets, linkingTargets.size(), bytecodeBegin, getBytecodeBeginForBlock);
}

} } // namespace JSC::DFG

namespace WTF {
void printInternal(PrintStream&, JSC::DFG::BasicBlock*);
}

#endif // ENABLE(DFG_JIT)
