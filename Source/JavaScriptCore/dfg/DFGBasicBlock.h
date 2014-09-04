/*
 * Copyright (C) 2011, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef DFGBasicBlock_h
#define DFGBasicBlock_h

#if ENABLE(DFG_JIT)

#include "DFGAbstractValue.h"
#include "DFGAvailability.h"
#include "DFGBranchDirection.h"
#include "DFGFlushedAt.h"
#include "DFGNode.h"
#include "DFGStructureClobberState.h"
#include "Operands.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace JSC { namespace DFG {

class Graph;
class InsertionSet;

typedef Vector<BasicBlock*, 2> PredecessorList;

struct BasicBlock : RefCounted<BasicBlock> {
    BasicBlock(
        unsigned bytecodeBegin, unsigned numArguments, unsigned numLocals,
        float executionCount);
    ~BasicBlock();
    
    void ensureLocals(unsigned newNumLocals);
    
    size_t size() const { return m_nodes.size(); }
    bool isEmpty() const { return !size(); }
    Node*& at(size_t i) { return m_nodes[i]; }
    Node* at(size_t i) const { return m_nodes[i]; }
    Node*& operator[](size_t i) { return at(i); }
    Node* operator[](size_t i) const { return at(i); }
    Node* last() const { return at(size() - 1); }
    Node* takeLast() { return m_nodes.takeLast(); }
    void resize(size_t size) { m_nodes.resize(size); }
    void grow(size_t size) { m_nodes.grow(size); }
    
    void append(Node* node) { m_nodes.append(node); }
    void insertBeforeLast(Node* node)
    {
        append(last());
        at(size() - 2) = node;
    }
    
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
    
    unsigned numSuccessors() { return last()->numSuccessors(); }
    
    BasicBlock*& successor(unsigned index)
    {
        return last()->successor(index);
    }
    BasicBlock*& successorForCondition(bool condition)
    {
        return last()->successorForCondition(condition);
    }
    
    void removePredecessor(BasicBlock* block);
    void replacePredecessor(BasicBlock* from, BasicBlock* to);

    template<typename... Params>
    Node* appendNode(Graph&, SpeculatedType, Params...);
    
    template<typename... Params>
    Node* appendNonTerminal(Graph&, SpeculatedType, Params...);
    
    void dump(PrintStream& out) const;
    
    void didLink()
    {
#if !ASSERT_DISABLED
        isLinked = true;
#endif
    }
    
    // This value is used internally for block linking and OSR entry. It is mostly meaningless
    // for other purposes due to inlining.
    unsigned bytecodeBegin;
    
    BlockIndex index;
    
    bool isOSRTarget;
    bool cfaHasVisited;
    bool cfaShouldRevisit;
    bool cfaFoundConstants;
    bool cfaDidFinish;
    StructureClobberState cfaStructureClobberStateAtHead;
    StructureClobberState cfaStructureClobberStateAtTail;
    BranchDirection cfaBranchDirection;
#if !ASSERT_DISABLED
    bool isLinked;
#endif
    bool isReachable;
    
    Vector<Node*> phis;
    PredecessorList predecessors;
    
    Operands<Node*, NodePointerTraits> variablesAtHead;
    Operands<Node*, NodePointerTraits> variablesAtTail;
    
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
    bool intersectionOfCFAHasVisited;
    
    float executionCount;
    
    // These fields are reserved for NaturalLoops.
    static const unsigned numberOfInnerMostLoopIndices = 2;
    unsigned innerMostLoopIndices[numberOfInnerMostLoopIndices];

    struct SSAData {
        Operands<Availability> availabilityAtHead;
        Operands<Availability> availabilityAtTail;
        HashSet<Node*> liveAtHead;
        HashSet<Node*> liveAtTail;
        HashMap<Node*, AbstractValue> valuesAtHead;
        HashMap<Node*, AbstractValue> valuesAtTail;
        
        SSAData(BasicBlock*);
        ~SSAData();
    };
    std::unique_ptr<SSAData> ssa;
    
private:
    friend class InsertionSet;
    Vector<Node*, 8> m_nodes;
};

typedef Vector<BasicBlock*, 5> BlockList;

struct UnlinkedBlock {
    BasicBlock* m_block;
    bool m_needsNormalLinking;
    bool m_needsEarlyReturnLinking;
    
    UnlinkedBlock() { }
    
    explicit UnlinkedBlock(BasicBlock* block)
        : m_block(block)
        , m_needsNormalLinking(true)
        , m_needsEarlyReturnLinking(false)
    {
    }
};
    
static inline unsigned getBytecodeBeginForBlock(BasicBlock** basicBlock)
{
    return (*basicBlock)->bytecodeBegin;
}

static inline BasicBlock* blockForBytecodeOffset(Vector<BasicBlock*>& linkingTargets, unsigned bytecodeBegin)
{
    return *binarySearch<BasicBlock*, unsigned>(linkingTargets, linkingTargets.size(), bytecodeBegin, getBytecodeBeginForBlock);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGBasicBlock_h

