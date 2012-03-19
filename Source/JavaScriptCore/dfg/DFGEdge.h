/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

namespace JSC { namespace DFG {

class AdjacencyList;

class Edge {
public:
    Edge()
        : m_encodedWord(makeWord(NoNode, UntypedUse))
    {
    }
    
    explicit Edge(NodeIndex nodeIndex)
        : m_encodedWord(makeWord(nodeIndex, UntypedUse))
    {
    }
    
    Edge(NodeIndex nodeIndex, UseKind useKind)
        : m_encodedWord(makeWord(nodeIndex, useKind))
    {
    }
    
    NodeIndex indexUnchecked() const { return m_encodedWord >> shift(); }
    NodeIndex index() const
    {
        ASSERT(isSet());
        return m_encodedWord >> shift();
    }
    void setIndex(NodeIndex nodeIndex)
    {
        m_encodedWord = makeWord(nodeIndex, useKind());
    }
    
    UseKind useKind() const
    {
        ASSERT(isSet());
        unsigned masked = m_encodedWord & (((1 << shift()) - 1));
        ASSERT(masked < LastUseKind);
        return static_cast<UseKind>(masked);
    }
    void setUseKind(UseKind useKind)
    {
        ASSERT(isSet());
        m_encodedWord = makeWord(index(), useKind);
    }
    
    bool isSet() const { return indexUnchecked() != NoNode; }
    bool operator!() const { return !isSet(); }
    
    bool operator==(Edge other) const
    {
        return m_encodedWord == other.m_encodedWord;
    }
    bool operator!=(Edge other) const
    {
        return m_encodedWord != other.m_encodedWord;
    }

private:
    friend class AdjacencyList;
    
    static uint32_t shift() { return 4; }
    
    static int32_t makeWord(NodeIndex nodeIndex, UseKind useKind)
    {
        ASSERT(static_cast<uint32_t>(((static_cast<int32_t>(nodeIndex) << shift()) >> shift())) == nodeIndex);
        ASSERT(useKind >= 0 && useKind < LastUseKind);
        ASSERT(LastUseKind <= (1 << shift()));
        return (nodeIndex << shift()) | useKind;
    }
    
    int32_t m_encodedWord;
};

inline bool operator==(Edge nodeUse, NodeIndex nodeIndex)
{
    return nodeUse.indexUnchecked() == nodeIndex;
}
inline bool operator==(NodeIndex nodeIndex, Edge nodeUse)
{
    return nodeUse.indexUnchecked() == nodeIndex;
}
inline bool operator!=(Edge nodeUse, NodeIndex nodeIndex)
{
    return nodeUse.indexUnchecked() != nodeIndex;
}
inline bool operator!=(NodeIndex nodeIndex, Edge nodeUse)
{
    return nodeUse.indexUnchecked() != nodeIndex;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGEdge_h

