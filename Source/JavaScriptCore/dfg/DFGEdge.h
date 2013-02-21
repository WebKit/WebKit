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

#ifndef DFGEdge_h
#define DFGEdge_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "DFGCommon.h"
#include "DFGUseKind.h"

namespace JSC { namespace DFG {

class AdjacencyList;

class Edge {
public:
    Edge()
        : m_encodedWord(makeWord(0, UntypedUse))
    {
    }
    
    explicit Edge(Node* node)
        : m_encodedWord(makeWord(node, UntypedUse))
    {
    }
    
    Edge(Node* node, UseKind useKind)
        : m_encodedWord(makeWord(node, useKind))
    {
    }
    
    Node* node() const { return bitwise_cast<Node*>(m_encodedWord & ~((1 << shift()) - 1)); }
    Node& operator*() const { return *node(); }
    Node* operator->() const { return node(); }
    
    void setNode(Node* node)
    {
        m_encodedWord = makeWord(node, useKind());
    }
    
    UseKind useKindUnchecked() const
    {
        unsigned masked = m_encodedWord & (((1 << shift()) - 1));
        ASSERT(masked < LastUseKind);
        UseKind result = static_cast<UseKind>(masked);
        ASSERT(node() || result == UntypedUse);
        return result;
    }
    UseKind useKind() const
    {
        ASSERT(node());
        return useKindUnchecked();
    }
    void setUseKind(UseKind useKind)
    {
        ASSERT(node());
        m_encodedWord = makeWord(node(), useKind);
    }
    
    bool isSet() const { return !!node(); }
    
    typedef void* Edge::*UnspecifiedBoolType;
    operator UnspecifiedBoolType*() const { return reinterpret_cast<UnspecifiedBoolType*>(isSet()); }
    
    bool operator!() const { return !isSet(); }
    
    bool operator==(Edge other) const
    {
        return m_encodedWord == other.m_encodedWord;
    }
    bool operator!=(Edge other) const
    {
        return m_encodedWord != other.m_encodedWord;
    }
    
    void dump(PrintStream&) const;

private:
    friend class AdjacencyList;
    
    static uint32_t shift() { return 4; }
    
    static uintptr_t makeWord(Node* node, UseKind useKind)
    {
        uintptr_t value = bitwise_cast<uintptr_t>(node);
        ASSERT(!(value & ((1 << shift()) - 1)));
        ASSERT(useKind >= 0 && useKind < LastUseKind);
        ASSERT(LastUseKind <= (1 << shift()));
        return value | static_cast<uintptr_t>(useKind);
    }
    
    uintptr_t m_encodedWord;
};

inline bool operator==(Edge edge, Node* node)
{
    return edge.node() == node;
}
inline bool operator==(Node* node, Edge edge)
{
    return edge.node() == node;
}
inline bool operator!=(Edge edge, Node* node)
{
    return edge.node() != node;
}
inline bool operator!=(Node* node, Edge edge)
{
    return edge.node() != node;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGEdge_h

