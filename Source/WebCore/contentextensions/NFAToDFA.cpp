/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NFAToDFA.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionsDebugging.h"
#include "DFANode.h"
#include "NFA.h"
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

namespace ContentExtensions {

// FIXME: set a better initial size.
// FIXME: include the hash inside NodeIdSet.
typedef HashSet<unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> NodeIdSet;

static inline void epsilonClosure(NodeIdSet& nodeSet, const Vector<NFANode>& graph, unsigned epsilonTransitionCharacter)
{
    ASSERT(!nodeSet.isEmpty());
    ASSERT(!graph.isEmpty());

    // FIXME: fine a good inline size for unprocessedNodes.
    Vector<unsigned> unprocessedNodes;
    copyToVector(nodeSet, unprocessedNodes);

    do {
        unsigned unprocessedNodeId = unprocessedNodes.takeLast();
        const NFANode& node = graph[unprocessedNodeId];
        auto epsilonTransitionSlot = node.transitions.find(epsilonTransitionCharacter);
        if (epsilonTransitionSlot != node.transitions.end()) {
            for (unsigned targetNodeId : epsilonTransitionSlot->value) {
                auto addResult = nodeSet.add(targetNodeId);
                if (addResult.isNewEntry)
                    unprocessedNodes.append(targetNodeId);
            }
        }
    } while (!unprocessedNodes.isEmpty());
}

struct UniqueNodeIdSetImpl {
    unsigned* buffer()
    {
        return m_buffer;
    }

    const unsigned* buffer() const
    {
        return m_buffer;
    }

    unsigned m_size;
    unsigned m_hash;
    unsigned m_dfaNodeId;
private:
    unsigned m_buffer[1];
};

class UniqueNodeIdSet {
public:
    UniqueNodeIdSet() { }
    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };

    UniqueNodeIdSet(EmptyValueTag) { }
    UniqueNodeIdSet(DeletedValueTag)
        : m_uniqueNodeIdSetBuffer(reinterpret_cast<UniqueNodeIdSetImpl*>(-1))
    {
    }

    UniqueNodeIdSet(const NodeIdSet& nodeIdSet, unsigned hash, unsigned dfaNodeId)
    {
        ASSERT(nodeIdSet.size());

        unsigned size = nodeIdSet.size();
        size_t byteSize = sizeof(UniqueNodeIdSetImpl) + (size - 1) * sizeof(unsigned);
        m_uniqueNodeIdSetBuffer = static_cast<UniqueNodeIdSetImpl*>(fastMalloc(byteSize));

        m_uniqueNodeIdSetBuffer->m_size = size;
        m_uniqueNodeIdSetBuffer->m_hash = hash;
        m_uniqueNodeIdSetBuffer->m_dfaNodeId = dfaNodeId;

        unsigned* buffer = m_uniqueNodeIdSetBuffer->buffer();
        for (unsigned nodeId : nodeIdSet) {
            *buffer = nodeId;
            ++buffer;
        }
    }

    UniqueNodeIdSet(UniqueNodeIdSet&& other)
        : m_uniqueNodeIdSetBuffer(other.m_uniqueNodeIdSetBuffer)
    {
        other.m_uniqueNodeIdSetBuffer = nullptr;
    }

    UniqueNodeIdSet& operator=(UniqueNodeIdSet&& other)
    {
        m_uniqueNodeIdSetBuffer = other.m_uniqueNodeIdSetBuffer;
        other.m_uniqueNodeIdSetBuffer = nullptr;
        return *this;
    }

    ~UniqueNodeIdSet()
    {
        fastFree(m_uniqueNodeIdSetBuffer);
    }

    bool operator==(const UniqueNodeIdSet& other) const
    {
        return m_uniqueNodeIdSetBuffer == other.m_uniqueNodeIdSetBuffer;
    }

    bool operator==(const NodeIdSet& other) const
    {
        if (m_uniqueNodeIdSetBuffer->m_size != static_cast<unsigned>(other.size()))
            return false;
        unsigned* buffer = m_uniqueNodeIdSetBuffer->buffer();
        for (unsigned i = 0; i < m_uniqueNodeIdSetBuffer->m_size; ++i) {
            if (!other.contains(buffer[i]))
                return false;
        }
        return true;
    }

    UniqueNodeIdSetImpl* impl() const { return m_uniqueNodeIdSetBuffer; }

    unsigned hash() const { return m_uniqueNodeIdSetBuffer->m_hash; }
    bool isEmptyValue() const { return !m_uniqueNodeIdSetBuffer; }
    bool isDeletedValue() const { return m_uniqueNodeIdSetBuffer == reinterpret_cast<UniqueNodeIdSetImpl*>(-1); }

private:
    UniqueNodeIdSetImpl* m_uniqueNodeIdSetBuffer = nullptr;
};

struct UniqueNodeIdSetHash {
    static unsigned hash(const UniqueNodeIdSet& p)
    {
        return p.hash();
    }

    static bool equal(const UniqueNodeIdSet& a, const UniqueNodeIdSet& b)
    {
        return a == b;
    }
    // It would be fine to compare empty or deleted here, but not for the HashTranslator.
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct UniqueNodeIdSetHashHashTraits : public WTF::CustomHashTraits<UniqueNodeIdSet> {
    static const bool emptyValueIsZero = true;

    // FIXME: Get a good size.
    static const int minimumTableSize = 128;
};

typedef HashSet<std::unique_ptr<UniqueNodeIdSet>, UniqueNodeIdSetHash, UniqueNodeIdSetHashHashTraits> UniqueNodeIdSetTable;

struct NodeIdSetToUniqueNodeIdSetSource {
    NodeIdSetToUniqueNodeIdSetSource(Vector<DFANode>& dfaGraph, const Vector<NFANode>& nfaGraph, const NodeIdSet& nodeIdSet)
        : dfaGraph(dfaGraph)
        , nfaGraph(nfaGraph)
        , nodeIdSet(nodeIdSet)
    {
        // The hashing operation must be independant of the nodeId.
        unsigned hash = 4207445155;
        for (unsigned nodeId : nodeIdSet)
            hash += nodeId;
        this->hash = DefaultHash<unsigned>::Hash::hash(hash);
    }
    Vector<DFANode>& dfaGraph;
    const Vector<NFANode>& nfaGraph;
    const NodeIdSet& nodeIdSet;
    unsigned hash;
};

struct NodeIdSetToUniqueNodeIdSetTranslator {
    static unsigned hash(const NodeIdSetToUniqueNodeIdSetSource& source)
    {
        return source.hash;
    }

    static inline bool equal(const UniqueNodeIdSet& a, const NodeIdSetToUniqueNodeIdSetSource& b)
    {
        return a == b.nodeIdSet;
    }

    static void translate(UniqueNodeIdSet& location, const NodeIdSetToUniqueNodeIdSetSource& source, unsigned hash)
    {
        DFANode newDFANode;

        HashSet<uint64_t, DefaultHash<uint64_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> actions;
        for (unsigned nfaNodeId : source.nodeIdSet) {
            const NFANode& nfaNode = source.nfaGraph[nfaNodeId];
            if (nfaNode.isFinal)
                actions.add(nfaNode.ruleId);
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
            newDFANode.correspondingDFANodes.append(nfaNodeId);
#endif
        }
        copyToVector(actions, newDFANode.actions);

        unsigned dfaNodeId = source.dfaGraph.size();
        source.dfaGraph.append(newDFANode);
        new (NotNull, &location) UniqueNodeIdSet(source.nodeIdSet, hash, dfaNodeId);

        ASSERT(location.impl());
    }
};

class SetTransitionsExcludingEpsilon {
public:
    NodeIdSet& operator[](unsigned index)
    {
        ASSERT(index < size());
        return m_targets[index];
    }

    unsigned size() const
    {
        return WTF_ARRAY_LENGTH(m_targets);
    }

    NodeIdSet* begin()
    {
        return m_targets;
    }

    NodeIdSet* end()
    {
        return m_targets + size();
    }

private:
    NodeIdSet m_targets[128];
};

static inline void populateTransitionsExcludingEpsilon(SetTransitionsExcludingEpsilon& setTransitionsExcludingEpsilon, const UniqueNodeIdSetImpl& sourceNodeSet, const Vector<NFANode>& graph, unsigned epsilonTransitionCharacter)
{
    ASSERT(!graph.isEmpty());
#if !ASSERT_DISABLED
    for (const NodeIdSet& set : setTransitionsExcludingEpsilon)
        ASSERT(set.isEmpty());
#endif

    const unsigned* buffer = sourceNodeSet.buffer();
    for (unsigned i = 0; i < sourceNodeSet.m_size; ++i) {
        unsigned nodeId = buffer[i];
        const NFANode& node = graph[nodeId];
        for (const auto& transitionSlot : node.transitions) {
            if (transitionSlot.key != epsilonTransitionCharacter)
                setTransitionsExcludingEpsilon[transitionSlot.key].add(transitionSlot.value.begin(), transitionSlot.value.end());
        }
    }
}

DFA NFAToDFA::convert(const NFA& nfa)
{
    Vector<DFANode> dfaGraph;

    const Vector<NFANode>& nfaGraph = nfa.m_nodes;

    NodeIdSet initialSet({ nfa.root() });
    epsilonClosure(initialSet, nfaGraph, NFA::epsilonTransitionCharacter);

    UniqueNodeIdSetTable uniqueNodeIdSetTable;

    NodeIdSetToUniqueNodeIdSetSource initialNodeIdSetToUniqueNodeIdSetSource(dfaGraph, nfaGraph, initialSet);
    auto addResult = uniqueNodeIdSetTable.add<NodeIdSetToUniqueNodeIdSetTranslator>(initialNodeIdSetToUniqueNodeIdSetSource);

    Vector<UniqueNodeIdSetImpl*> unprocessedNodes;
    unprocessedNodes.append(addResult.iterator->impl());

    SetTransitionsExcludingEpsilon transitionsFromClosedSet;

    do {
        UniqueNodeIdSetImpl* uniqueNodeIdSetImpl = unprocessedNodes.takeLast();

        unsigned dfaNodeId = uniqueNodeIdSetImpl->m_dfaNodeId;
        populateTransitionsExcludingEpsilon(transitionsFromClosedSet, *uniqueNodeIdSetImpl, nfaGraph, NFA::epsilonTransitionCharacter);

        // FIXME: there should not be any transition on key 0.
        for (unsigned key = 0; key < transitionsFromClosedSet.size(); ++key) {
            NodeIdSet& targetNodeSet = transitionsFromClosedSet[key];

            if (targetNodeSet.isEmpty())
                continue;

            epsilonClosure(targetNodeSet, nfaGraph, NFA::epsilonTransitionCharacter);

            NodeIdSetToUniqueNodeIdSetSource nodeIdSetToUniqueNodeIdSetSource(dfaGraph, nfaGraph, targetNodeSet);
            auto uniqueNodeIdAddResult = uniqueNodeIdSetTable.add<NodeIdSetToUniqueNodeIdSetTranslator>(nodeIdSetToUniqueNodeIdSetSource);

            unsigned targetNodeId = uniqueNodeIdAddResult.iterator->impl()->m_dfaNodeId;
            const auto addResult = dfaGraph[dfaNodeId].transitions.add(key, targetNodeId);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);

            if (uniqueNodeIdAddResult.isNewEntry)
                unprocessedNodes.append(uniqueNodeIdAddResult.iterator->impl());

            targetNodeSet.clear();
        }
    } while (!unprocessedNodes.isEmpty());

    return DFA(WTF::move(dfaGraph), 0);
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
