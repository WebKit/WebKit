/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#include "DFGAbstractInterpreterClobberState.h"
#include "DFGAbstractValue.h"
#include "DFGBasicBlock.h"
#include "DFGBlockMap.h"
#include "DFGGraph.h"
#include "DFGNodeFlowProjection.h"

namespace JSC { namespace DFG { 

class AtTailAbstractState {
public:
    AtTailAbstractState(Graph&);
    
    ~AtTailAbstractState();
    
    explicit operator bool() const { return true; }
    
    void initializeTo(BasicBlock* block)
    {
        m_block = block;
    }
    
    void createValueForNode(NodeFlowProjection);
    
    AbstractValue& fastForward(AbstractValue& value) { return value; }
    
    AbstractValue& forNode(NodeFlowProjection);
    AbstractValue& forNode(Edge edge) { return forNode(edge.node()); }
    
    ALWAYS_INLINE AbstractValue& forNodeWithoutFastForward(NodeFlowProjection node)
    {
        return forNode(node);
    }
    
    ALWAYS_INLINE AbstractValue& forNodeWithoutFastForward(Edge edge)
    {
        return forNode(edge);
    }
    
    ALWAYS_INLINE void fastForwardAndFilterUnproven(AbstractValue& value, SpeculatedType type)
    {
        value.filter(type);
    }
    
    ALWAYS_INLINE void clearForNode(NodeFlowProjection node)
    {
        forNode(node).clear();
    }
    
    ALWAYS_INLINE void clearForNode(Edge edge)
    {
        clearForNode(edge.node());
    }
    
    template<typename... Arguments>
    ALWAYS_INLINE void setForNode(NodeFlowProjection node, Arguments&&... arguments)
    {
        forNode(node).set(m_graph, std::forward<Arguments>(arguments)...);
    }

    template<typename... Arguments>
    ALWAYS_INLINE void setForNode(Edge edge, Arguments&&... arguments)
    {
        setForNode(edge.node(), std::forward<Arguments>(arguments)...);
    }
    
    template<typename... Arguments>
    ALWAYS_INLINE void setTypeForNode(NodeFlowProjection node, Arguments&&... arguments)
    {
        forNode(node).setType(m_graph, std::forward<Arguments>(arguments)...);
    }

    template<typename... Arguments>
    ALWAYS_INLINE void setTypeForNode(Edge edge, Arguments&&... arguments)
    {
        setTypeForNode(edge.node(), std::forward<Arguments>(arguments)...);
    }
    
    template<typename... Arguments>
    ALWAYS_INLINE void setNonCellTypeForNode(NodeFlowProjection node, Arguments&&... arguments)
    {
        forNode(node).setNonCellType(std::forward<Arguments>(arguments)...);
    }

    template<typename... Arguments>
    ALWAYS_INLINE void setNonCellTypeForNode(Edge edge, Arguments&&... arguments)
    {
        setNonCellTypeForNode(edge.node(), std::forward<Arguments>(arguments)...);
    }
    
    ALWAYS_INLINE void makeBytecodeTopForNode(NodeFlowProjection node)
    {
        forNode(node).makeBytecodeTop();
    }
    
    ALWAYS_INLINE void makeBytecodeTopForNode(Edge edge)
    {
        makeBytecodeTopForNode(edge.node());
    }
    
    ALWAYS_INLINE void makeHeapTopForNode(NodeFlowProjection node)
    {
        forNode(node).makeHeapTop();
    }
    
    ALWAYS_INLINE void makeHeapTopForNode(Edge edge)
    {
        makeHeapTopForNode(edge.node());
    }
    
    unsigned numberOfArguments() const { return m_block->valuesAtTail.numberOfArguments(); }
    unsigned numberOfLocals() const { return m_block->valuesAtTail.numberOfLocals(); }
    AbstractValue& operand(int operand) { return m_block->valuesAtTail.operand(operand); }
    AbstractValue& operand(VirtualRegister operand) { return m_block->valuesAtTail.operand(operand); }
    AbstractValue& local(size_t index) { return m_block->valuesAtTail.local(index); }
    AbstractValue& argument(size_t index) { return m_block->valuesAtTail.argument(index); }
    
    void clobberStructures()
    {
        UNREACHABLE_FOR_PLATFORM();
    }
    
    void observeInvalidationPoint()
    {
        UNREACHABLE_FOR_PLATFORM();
    }
    
    BasicBlock* block() const { return m_block; }
    
    bool isValid() { return m_block->cfaDidFinish; }
    
    StructureClobberState structureClobberState() const { return m_block->cfaStructureClobberStateAtTail; }
    
    void setClobberState(AbstractInterpreterClobberState) { }
    void mergeClobberState(AbstractInterpreterClobberState) { }
    void setStructureClobberState(StructureClobberState state) { RELEASE_ASSERT(state == m_block->cfaStructureClobberStateAtTail); }
    void setIsValid(bool isValid) { m_block->cfaDidFinish = isValid; }
    void setBranchDirection(BranchDirection) { }
    void setFoundConstants(bool) { }

    void trustEdgeProofs() { m_trustEdgeProofs = true; }
    void dontTrustEdgeProofs() { m_trustEdgeProofs = false; }
    void setProofStatus(Edge& edge, ProofStatus status)
    {
        if (m_trustEdgeProofs)
            edge.setProofStatus(status);
    }

private:
    Graph& m_graph;
    BlockMap<HashMap<NodeFlowProjection, AbstractValue>> m_valuesAtTailMap;
    BasicBlock* m_block { nullptr };
    bool m_trustEdgeProofs { false };
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
