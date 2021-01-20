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

#include "DFGAbstractValue.h"
#include "DFGGraph.h"
#include "DFGNode.h"
#include "DFGNodeFlowProjection.h"
#include "DFGPhiChildren.h"
#include <wtf/TriState.h>

namespace JSC { namespace DFG {

template<typename AbstractStateType>
class AbstractInterpreter {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AbstractInterpreter(Graph&, AbstractStateType&);
    ~AbstractInterpreter();
    
    ALWAYS_INLINE AbstractValue& forNode(NodeFlowProjection node)
    {
        return m_state.forNode(node);
    }
    
    ALWAYS_INLINE AbstractValue& forNode(Edge edge)
    {
        return forNode(edge.node());
    }
    
    ALWAYS_INLINE void clearForNode(NodeFlowProjection node)
    {
        m_state.clearForNode(node);
    }
    
    ALWAYS_INLINE void clearForNode(Edge edge)
    {
        clearForNode(edge.node());
    }

    template<typename... Arguments>
    ALWAYS_INLINE void setForNode(NodeFlowProjection node, Arguments&&... arguments)
    {
        m_state.setForNode(node, std::forward<Arguments>(arguments)...);
    }

    template<typename... Arguments>
    ALWAYS_INLINE void setForNode(Edge edge, Arguments&&... arguments)
    {
        setForNode(edge.node(), std::forward<Arguments>(arguments)...);
    }

    template<typename... Arguments>
    ALWAYS_INLINE void setTypeForNode(NodeFlowProjection node, Arguments&&... arguments)
    {
        m_state.setTypeForNode(node, std::forward<Arguments>(arguments)...);
    }

    template<typename... Arguments>
    ALWAYS_INLINE void setTypeForNode(Edge edge, Arguments&&... arguments)
    {
        setTypeForNode(edge.node(), std::forward<Arguments>(arguments)...);
    }
    
    template<typename... Arguments>
    ALWAYS_INLINE void setNonCellTypeForNode(NodeFlowProjection node, Arguments&&... arguments)
    {
        m_state.setNonCellTypeForNode(node, std::forward<Arguments>(arguments)...);
    }

    template<typename... Arguments>
    ALWAYS_INLINE void setNonCellTypeForNode(Edge edge, Arguments&&... arguments)
    {
        setNonCellTypeForNode(edge.node(), std::forward<Arguments>(arguments)...);
    }
    
    ALWAYS_INLINE void makeBytecodeTopForNode(NodeFlowProjection node)
    {
        m_state.makeBytecodeTopForNode(node);
    }
    
    ALWAYS_INLINE void makeBytecodeTopForNode(Edge edge)
    {
        makeBytecodeTopForNode(edge.node());
    }
    
    ALWAYS_INLINE void makeHeapTopForNode(NodeFlowProjection node)
    {
        m_state.makeHeapTopForNode(node);
    }
    
    ALWAYS_INLINE void makeHeapTopForNode(Edge edge)
    {
        makeHeapTopForNode(edge.node());
    }
    
    bool needsTypeCheck(Node* node, SpeculatedType typesPassedThrough)
    {
        return !forNode(node).isType(typesPassedThrough);
    }
    
    bool needsTypeCheck(Edge edge, SpeculatedType typesPassedThrough)
    {
        return needsTypeCheck(edge.node(), typesPassedThrough);
    }
    
    bool needsTypeCheck(Edge edge)
    {
        return needsTypeCheck(edge, typeFilterFor(edge.useKind()));
    }
    
    // Abstractly executes the given node. The new abstract state is stored into an
    // abstract stack stored in *this. Loads of local variables (that span
    // basic blocks) interrogate the basic block's notion of the state at the head.
    // Stores to local variables are handled in endBasicBlock(). This returns true
    // if execution should continue past this node. Notably, it will return true
    // for block terminals, so long as those terminals are not Return or Unreachable.
    //
    // This is guaranteed to be equivalent to doing:
    //
    // state.startExecuting()
    // state.executeEdges(node);
    // result = state.executeEffects(index);
    bool execute(unsigned indexInBlock);
    bool execute(Node*);
    
    // Indicate the start of execution of a node. It resets any state in the node
    // that is progressively built up by executeEdges() and executeEffects().
    void startExecuting();
    
    // Abstractly execute the edges of the given node. This runs filterEdgeByUse()
    // on all edges of the node. You can skip this step, if you have already used
    // filterEdgeByUse() (or some equivalent) on each edge.
    void executeEdges(Node*);

    void executeKnownEdgeTypes(Node*);
    
    ALWAYS_INLINE void filterEdgeByUse(Edge& edge)
    {
        UseKind useKind = edge.useKind();
        if (useKind == UntypedUse)
            return;
        filterByType(edge, typeFilterFor(useKind));
    }
    
    // Abstractly execute the effects of the given node. This changes the abstract
    // state assuming that edges have already been filtered.
    bool executeEffects(unsigned indexInBlock);
    bool executeEffects(unsigned clobberLimit, Node*);
    
    void dump(PrintStream& out) const;
    void dump(PrintStream& out);
    
    template<typename T>
    FiltrationResult filter(T node, const RegisteredStructureSet& set, SpeculatedType admittedTypes = SpecNone)
    {
        return filter(forNode(node), set, admittedTypes);
    }
    
    template<typename T>
    FiltrationResult filterArrayModes(T node, ArrayModes arrayModes, SpeculatedType admittedTypes = SpecNone)
    {
        return filterArrayModes(forNode(node), arrayModes, admittedTypes);
    }
    
    template<typename T>
    FiltrationResult filter(T node, SpeculatedType type)
    {
        return filter(forNode(node), type);
    }
    
    template<typename T>
    FiltrationResult filterByValue(T node, FrozenValue value)
    {
        return filterByValue(forNode(node), value);
    }
    
    template<typename T>
    FiltrationResult filterClassInfo(T node, const ClassInfo* classInfo)
    {
        return filterClassInfo(forNode(node), classInfo);
    }

    FiltrationResult filter(AbstractValue&, const RegisteredStructureSet&, SpeculatedType admittedTypes = SpecNone);
    FiltrationResult filterArrayModes(AbstractValue&, ArrayModes, SpeculatedType admittedTypes = SpecNone);
    FiltrationResult filter(AbstractValue&, SpeculatedType);
    FiltrationResult filterByValue(AbstractValue&, FrozenValue);
    FiltrationResult filterClassInfo(AbstractValue&, const ClassInfo*);
    
    PhiChildren* phiChildren() { return m_phiChildren.get(); }
    
    void filterICStatus(Node*);
    
    void clobberWorld();
    void didFoldClobberWorld();
private:
    
    bool handleConstantBinaryBitwiseOp(Node*);

    template<typename Functor>
    void forAllValues(unsigned indexInBlock, Functor&);
    
    void clobberStructures();
    void didFoldClobberStructures();
    
    void observeTransition(unsigned indexInBlock, RegisteredStructure from, RegisteredStructure to);
public:
    void observeTransitions(unsigned indexInBlock, const TransitionVector&);
private:
    
    TriState booleanResult(Node*, AbstractValue&);
    
    void setBuiltInConstant(Node* node, FrozenValue value)
    {
        AbstractValue& abstractValue = forNode(node);
        abstractValue.set(m_graph, value, m_state.structureClobberState());
        abstractValue.fixTypeForRepresentation(m_graph, node);
    }
    
    void setConstant(Node* node, FrozenValue value)
    {
        setBuiltInConstant(node, value);
        m_state.setShouldTryConstantFolding(true);
    }
    
    ALWAYS_INLINE void filterByType(Edge& edge, SpeculatedType type);
    
    void verifyEdge(Node*, Edge);
    void verifyEdges(Node*);
    void executeDoubleUnaryOpEffects(Node*, double(*equivalentFunction)(double));
    
    bool handleConstantDivOp(Node*);

    CodeBlock* m_codeBlock;
    Graph& m_graph;
    VM& m_vm;
    AbstractStateType& m_state;
    std::unique_ptr<PhiChildren> m_phiChildren;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
