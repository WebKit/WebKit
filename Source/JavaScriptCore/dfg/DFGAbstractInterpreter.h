/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef DFGAbstractInterpreter_h
#define DFGAbstractInterpreter_h

#if ENABLE(DFG_JIT)

#include "DFGAbstractValue.h"
#include "DFGBranchDirection.h"
#include "DFGGraph.h"
#include "DFGNode.h"

namespace JSC { namespace DFG {

template<typename AbstractStateType>
class AbstractInterpreter {
public:
    AbstractInterpreter(Graph&, AbstractStateType& state);
    ~AbstractInterpreter();
    
    AbstractValue& forNode(Node* node)
    {
        return m_state.forNode(node);
    }
    
    AbstractValue& forNode(Edge edge)
    {
        return forNode(edge.node());
    }
    
    Operands<AbstractValue>& variables()
    {
        return m_state.variables();
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
    // if (state.startExecuting(index)) {
    //     state.executeEdges(index);
    //     result = state.executeEffects(index);
    // } else
    //     result = true;
    bool execute(unsigned indexInBlock);
    bool execute(Node*);
    
    // Indicate the start of execution of the node. It resets any state in the node,
    // that is progressively built up by executeEdges() and executeEffects(). In
    // particular, this resets canExit(), so if you want to "know" between calls of
    // startExecuting() and executeEdges()/Effects() whether the last run of the
    // analysis concluded that the node can exit, you should probably set that
    // information aside prior to calling startExecuting().
    bool startExecuting(Node*);
    bool startExecuting(unsigned indexInBlock);
    
    // Abstractly execute the edges of the given node. This runs filterEdgeByUse()
    // on all edges of the node. You can skip this step, if you have already used
    // filterEdgeByUse() (or some equivalent) on each edge.
    void executeEdges(Node*);
    void executeEdges(unsigned indexInBlock);
    
    ALWAYS_INLINE void filterEdgeByUse(Node* node, Edge& edge)
    {
        ASSERT(mayHaveTypeCheck(edge.useKind()) || !needsTypeCheck(edge));
        filterByType(node, edge, typeFilterFor(edge.useKind()));
    }
    
    // Abstractly execute the effects of the given node. This changes the abstract
    // state assuming that edges have already been filtered.
    bool executeEffects(unsigned indexInBlock);
    bool executeEffects(unsigned clobberLimit, Node*);
    
    void dump(PrintStream& out);
    
    template<typename T>
    FiltrationResult filter(T node, const StructureSet& set)
    {
        return filter(forNode(node), set);
    }
    
    template<typename T>
    FiltrationResult filterArrayModes(T node, ArrayModes arrayModes)
    {
        return filterArrayModes(forNode(node), arrayModes);
    }
    
    template<typename T>
    FiltrationResult filter(T node, SpeculatedType type)
    {
        return filter(forNode(node), type);
    }
    
    template<typename T>
    FiltrationResult filterByValue(T node, JSValue value)
    {
        return filterByValue(forNode(node), value);
    }
    
    FiltrationResult filter(AbstractValue&, const StructureSet&);
    FiltrationResult filterArrayModes(AbstractValue&, ArrayModes);
    FiltrationResult filter(AbstractValue&, SpeculatedType);
    FiltrationResult filterByValue(AbstractValue&, JSValue);
    
private:
    void clobberWorld(const CodeOrigin&, unsigned indexInBlock);
    void clobberCapturedVars(const CodeOrigin&);
    void clobberStructures(unsigned indexInBlock);
    
    enum BooleanResult {
        UnknownBooleanResult,
        DefinitelyFalse,
        DefinitelyTrue
    };
    BooleanResult booleanResult(Node*, AbstractValue&);
    
    void setConstant(Node* node, JSValue value)
    {
        forNode(node).set(m_graph, value);
        m_state.setFoundConstants(true);
    }
    
    ALWAYS_INLINE void filterByType(Node* node, Edge& edge, SpeculatedType type)
    {
        AbstractValue& value = forNode(edge);
        if (!value.isType(type)) {
            node->setCanExit(true);
            edge.setProofStatus(NeedsCheck);
        } else
            edge.setProofStatus(IsProved);
        
        filter(value, type);
    }
    
    void verifyEdge(Node*, Edge);
    void verifyEdges(Node*);
    
    CodeBlock* m_codeBlock;
    Graph& m_graph;
    AbstractStateType& m_state;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGAbstractInterpreter_h

