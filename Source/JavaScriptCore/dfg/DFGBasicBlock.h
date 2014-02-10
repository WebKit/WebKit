/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

#include "DFGAvailability.h"
#include "DFGBranchDirection.h"
#include "DFGNode.h"
#include "DFGVariadicFunction.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace JSC { namespace DFG {

class Graph;
class InsertionSet;

typedef Vector<BasicBlock*, 2> PredecessorList;

struct BasicBlock : RefCounted<BasicBlock> {
    BasicBlock(unsigned bytecodeBegin, unsigned numArguments, unsigned numLocals);
    ~BasicBlock();
    
    void ensureLocals(unsigned newNumLocals);
    
    size_t size() const { return m_nodes.size(); }
    bool isEmpty() const { return !size(); }
    Node*& at(size_t i) { return m_nodes[i]; }
    Node* at(size_t i) const { return m_nodes[i]; }
    Node*& operator[](size_t i) { return at(i); }
    Node* operator[](size_t i) const { return at(i); }
    Node* last() const { return at(size() - 1); }
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

#define DFG_DEFINE_APPEND_NODE(templatePre, templatePost, typeParams, valueParamsComma, valueParams, valueArgs) \
    templatePre typeParams templatePost Node* appendNode(Graph&, SpeculatedType valueParamsComma valueParams);
    DFG_VARIADIC_TEMPLATE_FUNCTION(DFG_DEFINE_APPEND_NODE)
#undef DFG_DEFINE_APPEND_NODE
    
#define DFG_DEFINE_APPEND_NODE(templatePre, templatePost, typeParams, valueParamsComma, valueParams, valueArgs) \
    templatePre typeParams templatePost Node* appendNonTerminal(Graph&, SpeculatedType valueParamsComma valueParams);
    DFG_VARIADIC_TEMPLATE_FUNCTION(DFG_DEFINE_APPEND_NODE)
#undef DFG_DEFINE_APPEND_NODE
    
    void dump(PrintStream& out) const;
    
    // This value is used internally for block linking and OSR entry. It is mostly meaningless
    // for other purposes due to inlining.
    unsigned bytecodeBegin;
    
    BlockIndex index;
    
    bool isOSRTarget;
    bool cfaHasVisited;
    bool cfaShouldRevisit;
    bool cfaFoundConstants;
    bool cfaDidFinish;
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
    
    // These fields are reserved for NaturalLoops.
    static const unsigned numberOfInnerMostLoopIndices = 2;
    unsigned innerMostLoopIndices[numberOfInnerMostLoopIndices];

    struct SSAData {
        Operands<FlushedAt> flushAtHead;
        Operands<FlushedAt> flushAtTail;
        Operands<Availability> availabilityAtHead;
        Operands<Availability> availabilityAtTail;
        HashSet<Node*> liveAtHead;
        HashSet<Node*> liveAtTail;
        HashMap<Node*, AbstractValue> valuesAtHead;
        HashMap<Node*, AbstractValue> valuesAtTail;
        
        SSAData(BasicBlock*);
        ~SSAData();
    };
    OwnPtr<SSAData> ssa;

private:
    friend class InsertionSet;
    Vector<Node*, 8> m_nodes;
};

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

