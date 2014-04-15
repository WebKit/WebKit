/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef DFGInsertionSet_h
#define DFGInsertionSet_h

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include <wtf/Insertion.h>
#include <wtf/Vector.h>

namespace JSC { namespace DFG {

typedef WTF::Insertion<Node*> Insertion;

class InsertionSet {
public:
    InsertionSet(Graph& graph)
        : m_graph(graph)
    {
    }
    
    Node* insert(const Insertion& insertion)
    {
        ASSERT(!m_insertions.size() || m_insertions.last().index() <= insertion.index());
        m_insertions.append(insertion);
        return insertion.element();
    }
    
    Node* insert(size_t index, Node* element)
    {
        return insert(Insertion(index, element));
    }

    template<typename... Params>
    Node* insertNode(size_t index, SpeculatedType type, Params... params)
    {
        return insert(index, m_graph.addNode(type, params...));
    }
    
    Node* insertConstant(
        size_t index, NodeOrigin origin, JSValue value,
        NodeType op = JSConstant)
    {
        unsigned constantReg =
            m_graph.constantRegisterForConstant(value);
        return insertNode(
            index, speculationFromValue(value), op, origin, OpInfo(constantReg));
    }
    
    Node* insertConstant(
        size_t index, CodeOrigin origin, JSValue value, NodeType op = JSConstant)
    {
        return insertConstant(index, NodeOrigin(origin), value, op);
    }
    
    Edge insertConstantForUse(
        size_t index, NodeOrigin origin, JSValue value, UseKind useKind)
    {
        NodeType op;
        if (isDouble(useKind))
            op = DoubleConstant;
        else if (useKind == Int52RepUse)
            op = Int52Constant;
        else
            op = JSConstant;
        return Edge(insertConstant(index, origin, value, op), useKind);
    }
    
    Edge insertConstantForUse(
        size_t index, CodeOrigin origin, JSValue value, UseKind useKind)
    {
        return insertConstantForUse(index, NodeOrigin(origin), value, useKind);
    }

    void execute(BasicBlock* block)
    {
        executeInsertions(*block, m_insertions);
    }
private:
    Graph& m_graph;
    Vector<Insertion, 8> m_insertions;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGInsertionSet_h

