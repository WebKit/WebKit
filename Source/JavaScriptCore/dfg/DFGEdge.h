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

#pragma once

#if ENABLE(DFG_JIT)

#include "DFGCommon.h"
#include "DFGUseKind.h"

namespace JSC { namespace DFG {

class AdjacencyList;

class Edge {
public:
    explicit Edge(Node* node = nullptr, UseKind useKind = UntypedUse, ProofStatus proofStatus = NeedsCheck, KillStatus killStatus = DoesNotKill)
        : m_encodedWord(makeWord(node, useKind, proofStatus, killStatus))
    {
    }
    
    static constexpr uintptr_t shift() { return 48; }
    static constexpr uintptr_t addressMask() { return ~(0xffull << shift()); }

    Node* node() const { return std::bit_cast<Node*>(m_encodedWord & addressMask()); }

    Node& operator*() const { return *node(); }
    Node* operator->() const { return node(); }
    
    void setNode(Node* node)
    {
        m_encodedWord = makeWord(node, useKind(), proofStatus(), killStatus());
    }
    
    UseKind useKindUnchecked() const
    {
        unsigned shifted = m_encodedWord >> (2 + shift()) & 0x3f;
        ASSERT(shifted < static_cast<unsigned>(LastUseKind));
        UseKind result = static_cast<UseKind>(shifted);
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
        m_encodedWord = makeWord(node(), useKind, proofStatus(), killStatus());
    }

    ProofStatus proofStatusUnchecked() const
    {
        return proofStatusForIsProved(m_encodedWord >> shift() & 1);
    }
    ProofStatus proofStatus() const
    {
        ASSERT(node());
        return proofStatusUnchecked();
    }
    void setProofStatus(ProofStatus proofStatus)
    {
        ASSERT(node());
        m_encodedWord = makeWord(node(), useKind(), proofStatus, killStatus());
    }
    bool isProved() const
    {
        return proofStatus() == IsProved;
    }
    
    bool willNotHaveCheck() const
    {
        return isProved() || shouldNotHaveTypeCheck(useKind());
    }
    bool willHaveCheck() const
    {
        return !willNotHaveCheck();
    }
    
    KillStatus killStatusUnchecked() const
    {
        return killStatusForDoesKill(m_encodedWord >> shift() & 2);
    }
    KillStatus killStatus() const
    {
        ASSERT(node());
        return killStatusUnchecked();
    }
    void setKillStatus(KillStatus killStatus)
    {
        ASSERT(node());
        m_encodedWord = makeWord(node(), useKind(), proofStatus(), killStatus);
    }
    bool doesKill() const { return DFG::doesKill(killStatus()); }

    bool isSet() const { return !!node(); }

    Edge sanitized() const
    {
        Edge result = *this;
        result.m_encodedWord = makeWord(node(), useKindUnchecked(), NeedsCheck, DoesNotKill);
        return result;
    }

    bool operator!() const { return !isSet(); }
    explicit operator bool() const { return isSet(); }
    
    friend bool operator==(const Edge&, const Edge&) = default;

    void dump(PrintStream&) const;
    
    unsigned hash() const
    {
        return IntHash<uintptr_t>::hash(m_encodedWord);
    }

private:
    friend class AdjacencyList;
    
    static uintptr_t makeWord(Node* node, UseKind useKind, ProofStatus proofStatus, KillStatus killStatus)
    {
        ASSERT(sizeof(node) == 8);
        uintptr_t maskedPointer = std::bit_cast<uintptr_t>(node) & addressMask();
        ASSERT(maskedPointer == std::bit_cast<uintptr_t>(node));
        ASSERT(useKind < LastUseKind);
        static_assert((static_cast<uintptr_t>(LastUseKind) << 2) < (static_cast<uintptr_t>(1) << shift()), "We rely on this being true to not clobber the node pointer.");
        uintptr_t kindBits = (static_cast<uintptr_t>(useKind) << 2) | (DFG::doesKill(killStatus) << 1) | static_cast<uintptr_t>(DFG::isProved(proofStatus));
        kindBits <<= shift();
        uintptr_t result = maskedPointer | kindBits;

        if (ASSERT_ENABLED) {
            union U {
                U() { word = 0; }
                uintptr_t word;
                Edge edge;
            } u;
            u.word = result;
            ASSERT(u.edge.useKindUnchecked() == useKind);
            ASSERT(u.edge.node() == node);
            ASSERT(u.edge.proofStatusUnchecked() == proofStatus);
            ASSERT(u.edge.killStatusUnchecked() == killStatus);
        }
        return result;
    }
    
    // This holds both the pointer and the use kind. It may be hijacked by
    // AdjacencyList for storing firstChild and numChildren.
    uintptr_t m_encodedWord;
};

inline bool operator==(Edge edge, Node* node)
{
    return edge.node() == node;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
