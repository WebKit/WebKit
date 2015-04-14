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
typedef NFANodeIndexSet NodeIdSet;

static inline void epsilonClosureExcludingSelf(const Vector<NFANode>& nfaGraph, unsigned nodeId, unsigned epsilonTransitionCharacter, Vector<unsigned>& output)
{
    const auto& transitions = nfaGraph[nodeId].transitions;
    auto epsilonTransitionSlot = transitions.find(epsilonTransitionCharacter);

    if (epsilonTransitionSlot == transitions.end())
        return;

    NodeIdSet closure({ nodeId });
    Vector<unsigned, 64> unprocessedNodes;
    copyToVector(epsilonTransitionSlot->value, unprocessedNodes);
    closure.add(unprocessedNodes.begin(), unprocessedNodes.end());
    output = unprocessedNodes;

    do {
        unsigned unprocessedNodeId = unprocessedNodes.takeLast();
        const NFANode& node = nfaGraph[unprocessedNodeId];
        auto epsilonTransitionSlot = node.transitions.find(epsilonTransitionCharacter);
        if (epsilonTransitionSlot != node.transitions.end()) {
            for (unsigned targetNodeId : epsilonTransitionSlot->value) {
                auto addResult = closure.add(targetNodeId);
                if (addResult.isNewEntry) {
                    unprocessedNodes.append(targetNodeId);
                    output.append(targetNodeId);
                }
            }
        }
    } while (!unprocessedNodes.isEmpty());
}

static void resolveEpsilonClosures(Vector<NFANode>& nfaGraph, unsigned epsilonTransitionCharacter, Vector<Vector<unsigned>>& nfaNodeClosures)
{
    unsigned nfaGraphSize = nfaGraph.size();
    nfaNodeClosures.resize(nfaGraphSize);
    for (unsigned nodeId = 0; nodeId < nfaGraphSize; ++nodeId)
        epsilonClosureExcludingSelf(nfaGraph, nodeId, epsilonTransitionCharacter, nfaNodeClosures[nodeId]);

    for (unsigned nodeId = 0; nodeId < nfaGraphSize; ++nodeId) {
        if (!nfaNodeClosures[nodeId].isEmpty()) {
            bool epsilonTransitionIsRemoved = nfaGraph[nodeId].transitions.remove(epsilonTransitionCharacter);
            ASSERT_UNUSED(epsilonTransitionIsRemoved, epsilonTransitionIsRemoved);
        }
    }
}

static ALWAYS_INLINE void extendSetWithClosure(const Vector<Vector<unsigned>>& nfaNodeClosures, unsigned nodeId, NodeIdSet& set)
{
    ASSERT(set.contains(nodeId));
    const Vector<unsigned>& nodeClosure = nfaNodeClosures[nodeId];
    if (!nodeClosure.isEmpty())
        set.add(nodeClosure.begin(), nodeClosure.end());
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
            actions.add(nfaNode.finalRuleIds.begin(), nfaNode.finalRuleIds.end());
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
            newDFANode.correspondingNFANodes.append(nfaNodeId);
#endif
        }
        copyToVector(actions, newDFANode.actions);

        unsigned dfaNodeId = source.dfaGraph.size();
        source.dfaGraph.append(newDFANode);
        new (NotNull, &location) UniqueNodeIdSet(source.nodeIdSet, hash, dfaNodeId);

        ASSERT(location.impl());
    }
};

class SetTransitions {
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

static inline void populateTransitions(SetTransitions& setTransitions, NodeIdSet& setFallbackTransition, const UniqueNodeIdSetImpl& sourceNodeSet, const Vector<NFANode>& graph, const Vector<Vector<unsigned>>& nfaNodeclosures)
{
    ASSERT(!graph.isEmpty());
    ASSERT(setFallbackTransition.isEmpty());
#if !ASSERT_DISABLED
    for (const NodeIdSet& set : setTransitions)
        ASSERT(set.isEmpty());
#endif

    Vector<unsigned, 8> allFallbackTransitions;
    const unsigned* buffer = sourceNodeSet.buffer();
    for (unsigned i = 0; i < sourceNodeSet.m_size; ++i) {
        unsigned nodeId = buffer[i];
        const NFANode& nfaSourceNode = graph[nodeId];
        for (unsigned targetTransition : nfaSourceNode.transitionsOnAnyCharacter)
            allFallbackTransitions.append(targetTransition);
    }
    for (unsigned targetNodeId : allFallbackTransitions) {
        auto addResult = setFallbackTransition.add(targetNodeId);
        if (addResult.isNewEntry)
            extendSetWithClosure(nfaNodeclosures, targetNodeId, setFallbackTransition);
    }

    for (unsigned i = 0; i < sourceNodeSet.m_size; ++i) {
        unsigned nodeId = buffer[i];
        for (const auto& transitionSlot : graph[nodeId].transitions) {
            NodeIdSet& targetSet = setTransitions[transitionSlot.key];
            for (unsigned targetNodId : transitionSlot.value) {
                targetSet.add(targetNodId);
                extendSetWithClosure(nfaNodeclosures, targetNodId, targetSet);
            }
            if (transitionSlot.key)
                targetSet.add(setFallbackTransition.begin(), setFallbackTransition.end());
        }
    }
}

static ALWAYS_INLINE unsigned getOrCreateDFANode(const NodeIdSet& nfaNodeSet, const Vector<NFANode>& nfaGraph, Vector<DFANode>& dfaGraph, UniqueNodeIdSetTable& uniqueNodeIdSetTable, Vector<UniqueNodeIdSetImpl*>& unprocessedNodes)
{
    NodeIdSetToUniqueNodeIdSetSource nodeIdSetToUniqueNodeIdSetSource(dfaGraph, nfaGraph, nfaNodeSet);
    auto uniqueNodeIdAddResult = uniqueNodeIdSetTable.add<NodeIdSetToUniqueNodeIdSetTranslator>(nodeIdSetToUniqueNodeIdSetSource);
    if (uniqueNodeIdAddResult.isNewEntry)
        unprocessedNodes.append(uniqueNodeIdAddResult.iterator->impl());

    return uniqueNodeIdAddResult.iterator->impl()->m_dfaNodeId;
}

DFA NFAToDFA::convert(NFA& nfa)
{
    Vector<NFANode>& nfaGraph = nfa.m_nodes;

    Vector<Vector<unsigned>> nfaNodeClosures;
    resolveEpsilonClosures(nfaGraph, NFA::epsilonTransitionCharacter, nfaNodeClosures);

    NodeIdSet initialSet({ nfa.root() });
    extendSetWithClosure(nfaNodeClosures, nfa.root(), initialSet);

    UniqueNodeIdSetTable uniqueNodeIdSetTable;

    Vector<DFANode> dfaGraph;
    NodeIdSetToUniqueNodeIdSetSource initialNodeIdSetToUniqueNodeIdSetSource(dfaGraph, nfaGraph, initialSet);
    auto addResult = uniqueNodeIdSetTable.add<NodeIdSetToUniqueNodeIdSetTranslator>(initialNodeIdSetToUniqueNodeIdSetSource);

    Vector<UniqueNodeIdSetImpl*> unprocessedNodes;
    unprocessedNodes.append(addResult.iterator->impl());

    SetTransitions transitionsFromClosedSet;

    do {
        UniqueNodeIdSetImpl* uniqueNodeIdSetImpl = unprocessedNodes.takeLast();

        unsigned dfaNodeId = uniqueNodeIdSetImpl->m_dfaNodeId;
        NodeIdSet setFallbackTransition;
        populateTransitions(transitionsFromClosedSet, setFallbackTransition, *uniqueNodeIdSetImpl, nfaGraph, nfaNodeClosures);

        for (unsigned key = 0; key < transitionsFromClosedSet.size(); ++key) {
            NodeIdSet& targetNodeSet = transitionsFromClosedSet[key];

            if (targetNodeSet.isEmpty())
                continue;

            unsigned targetNodeId = getOrCreateDFANode(targetNodeSet, nfaGraph, dfaGraph, uniqueNodeIdSetTable, unprocessedNodes);
            const auto addResult = dfaGraph[dfaNodeId].transitions.add(key, targetNodeId);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);

            targetNodeSet.clear();
        }
        if (!setFallbackTransition.isEmpty()) {
            unsigned targetNodeId = getOrCreateDFANode(setFallbackTransition, nfaGraph, dfaGraph, uniqueNodeIdSetTable, unprocessedNodes);
            DFANode& dfaSourceNode = dfaGraph[dfaNodeId];
            ASSERT(!dfaSourceNode.hasFallbackTransition);
            dfaSourceNode.hasFallbackTransition = true;
            dfaSourceNode.fallbackTransition = targetNodeId;
        }
    } while (!unprocessedNodes.isEmpty());

    return DFA(WTF::move(dfaGraph), 0);
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
