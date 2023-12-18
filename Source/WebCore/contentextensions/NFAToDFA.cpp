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
#include "ImmutableNFA.h"
#include "MutableRangeList.h"
#include "NFA.h"
#include "SerializedNFA.h"
#include <wtf/DataLog.h>
#include <wtf/HashSet.h>

namespace WebCore {

namespace ContentExtensions {

typedef MutableRange<signed char, NFANodeIndexSet> NFANodeRange;
typedef MutableRangeList<signed char, NFANodeIndexSet> NFANodeRangeList;
typedef MutableRangeList<signed char, NFANodeIndexSet, 128> PreallocatedNFANodeRangeList;
typedef Vector<uint32_t, 0, ContentExtensionsOverflowHandler> UniqueNodeList;
typedef Vector<UniqueNodeList, 0, ContentExtensionsOverflowHandler> NFANodeClosures;

// FIXME: set a better initial size.
// FIXME: include the hash inside NodeIdSet.
typedef NFANodeIndexSet NodeIdSet;

static inline void epsilonClosureExcludingSelf(const SerializedNFA& nfa, unsigned nodeId, UniqueNodeList& output)
{
    NodeIdSet closure({ nodeId });
    Vector<unsigned, 64, ContentExtensionsOverflowHandler> unprocessedNodes({ nodeId });

    do {
        unsigned unprocessedNodeId = unprocessedNodes.takeLast();
        const auto* node = nfa.nodes().pointerAt(unprocessedNodeId);

        for (uint32_t epsilonTargetIndex = node->epsilonTransitionTargetsStart; epsilonTargetIndex < node->epsilonTransitionTargetsEnd; ++epsilonTargetIndex) {
            uint32_t targetNodeId = nfa.epsilonTransitionsTargets().valueAt(epsilonTargetIndex);
            auto addResult = closure.add(targetNodeId);
            if (addResult.isNewEntry) {
                unprocessedNodes.append(targetNodeId);
                output.append(targetNodeId);
            }
        }
    } while (!unprocessedNodes.isEmpty());

    output.shrinkToFit();
}

static NFANodeClosures resolveEpsilonClosures(const SerializedNFA& nfa)
{
    unsigned nfaGraphSize = nfa.nodes().size();
    NFANodeClosures nfaNodeClosures(nfaGraphSize);

    for (unsigned nodeId = 0; nodeId < nfaGraphSize; ++nodeId)
        epsilonClosureExcludingSelf(nfa, nodeId, nfaNodeClosures[nodeId]);

    return nfaNodeClosures;
}

static ALWAYS_INLINE void extendSetWithClosure(const NFANodeClosures& nfaNodeClosures, unsigned nodeId, NodeIdSet& set)
{
    ASSERT(set.contains(nodeId));
    const UniqueNodeList& nodeClosure = nfaNodeClosures[nodeId];
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

typedef Vector<UniqueNodeIdSetImpl*, 128, ContentExtensionsOverflowHandler> UniqueNodeQueue;

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

    friend bool operator==(const UniqueNodeIdSet&, const UniqueNodeIdSet&) = default;

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
    NodeIdSetToUniqueNodeIdSetSource(DFA& dfa, const SerializedNFA& nfa, const NodeIdSet& nodeIdSet)
        : dfa(dfa)
        , nfa(nfa)
        , nodeIdSet(nodeIdSet)
    {
        // The hashing operation must be independant of the nodeId.
        unsigned hash = 4207445155;
        for (unsigned nodeId : nodeIdSet)
            hash += nodeId;
        this->hash = DefaultHash<unsigned>::hash(hash);
    }
    DFA& dfa;
    const SerializedNFA& nfa;
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

        HashSet<uint64_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> actions;

        for (unsigned nfaNodeId : source.nodeIdSet) {
            const auto* nfaNode = source.nfa.nodes().pointerAt(nfaNodeId);
            for (unsigned actionIndex = nfaNode->actionStart; actionIndex < nfaNode->actionEnd; ++actionIndex)
                actions.add(source.nfa.actions().valueAt(actionIndex));
        }

        unsigned actionsStart = source.dfa.actions.size();
        for (uint64_t action : actions)
            source.dfa.actions.append(action);
        unsigned actionsEnd = source.dfa.actions.size();
        unsigned actionsLength = actionsEnd - actionsStart;
        RELEASE_ASSERT_WITH_MESSAGE(actionsLength <= std::numeric_limits<uint16_t>::max(), "Too many actions for the current DFANode size.");
        newDFANode.setActions(actionsStart, static_cast<uint16_t>(actionsLength));

        unsigned dfaNodeId = source.dfa.nodes.size();
        source.dfa.nodes.append(newDFANode);
        new (NotNull, &location) UniqueNodeIdSet(source.nodeIdSet, hash, dfaNodeId);

        ASSERT(location.impl());
    }
};

struct DataConverterWithEpsilonClosure {
    const NFANodeClosures& nfaNodeclosures;

    template<typename Iterable>
    NFANodeIndexSet convert(const Iterable& iterable)
    {
        NFANodeIndexSet result;
        for (unsigned nodeId : iterable) {
            result.add(nodeId);
            const UniqueNodeList& nodeClosure = nfaNodeclosures[nodeId];
            result.add(nodeClosure.begin(), nodeClosure.end());
        }
        return result;
    }

    template<typename Iterable>
    void extend(NFANodeIndexSet& destination, const Iterable& iterable)
    {
        for (unsigned nodeId : iterable) {
            auto addResult = destination.add(nodeId);
            if (addResult.isNewEntry) {
                const UniqueNodeList& nodeClosure = nfaNodeclosures[nodeId];
                destination.add(nodeClosure.begin(), nodeClosure.end());
            }
        }
    }
};

static inline void createCombinedTransition(PreallocatedNFANodeRangeList& combinedRangeList, const UniqueNodeIdSetImpl& sourceNodeSet, const SerializedNFA& serializedNFA, const NFANodeClosures& nfaNodeclosures)
{
    combinedRangeList.clear();

    const unsigned* buffer = sourceNodeSet.buffer();

    DataConverterWithEpsilonClosure converter { nfaNodeclosures };
    for (unsigned i = 0; i < sourceNodeSet.m_size; ++i) {
        unsigned nodeId = buffer[i];
        auto transitions = serializedNFA.transitionsForNode(nodeId);
        combinedRangeList.extend(transitions.begin(), transitions.end(), converter);
    }
}

static ALWAYS_INLINE unsigned getOrCreateDFANode(const NodeIdSet& nfaNodeSet, const SerializedNFA& serializedNFA, DFA& dfa, UniqueNodeIdSetTable& uniqueNodeIdSetTable, UniqueNodeQueue& unprocessedNodes)
{
    NodeIdSetToUniqueNodeIdSetSource nodeIdSetToUniqueNodeIdSetSource(dfa, serializedNFA, nfaNodeSet);
    auto uniqueNodeIdAddResult = uniqueNodeIdSetTable.add<NodeIdSetToUniqueNodeIdSetTranslator>(nodeIdSetToUniqueNodeIdSetSource);
    if (uniqueNodeIdAddResult.isNewEntry)
        unprocessedNodes.append(uniqueNodeIdAddResult.iterator->impl());

    return uniqueNodeIdAddResult.iterator->impl()->m_dfaNodeId;
}

std::optional<DFA> NFAToDFA::convert(NFA&& nfa)
{
    auto serializedNFA = SerializedNFA::serialize(WTFMove(nfa));
    if (!serializedNFA)
        return std::nullopt;

    NFANodeClosures nfaNodeClosures = resolveEpsilonClosures(*serializedNFA);
    DFA dfa;

    NodeIdSet initialSet({ serializedNFA->root() });
    extendSetWithClosure(nfaNodeClosures, serializedNFA->root(), initialSet);

    UniqueNodeIdSetTable uniqueNodeIdSetTable;

    NodeIdSetToUniqueNodeIdSetSource initialNodeIdSetToUniqueNodeIdSetSource(dfa, *serializedNFA, initialSet);
    auto addResult = uniqueNodeIdSetTable.add<NodeIdSetToUniqueNodeIdSetTranslator>(initialNodeIdSetToUniqueNodeIdSetSource);

    UniqueNodeQueue unprocessedNodes;
    unprocessedNodes.append(addResult.iterator->impl());

    PreallocatedNFANodeRangeList combinedRangeList;
    do {
        UniqueNodeIdSetImpl* uniqueNodeIdSetImpl = unprocessedNodes.takeLast();
        createCombinedTransition(combinedRangeList, *uniqueNodeIdSetImpl, *serializedNFA, nfaNodeClosures);

        unsigned transitionsStart = dfa.transitionRanges.size();
        for (const NFANodeRange& range : combinedRangeList) {
            unsigned targetNodeId = getOrCreateDFANode(range.data, *serializedNFA, dfa, uniqueNodeIdSetTable, unprocessedNodes);
            dfa.transitionRanges.append({ range.first, range.last });
            dfa.transitionDestinations.append(targetNodeId);
        }
        unsigned transitionsEnd = dfa.transitionRanges.size();
        unsigned transitionsLength = transitionsEnd - transitionsStart;

        unsigned dfaNodeId = uniqueNodeIdSetImpl->m_dfaNodeId;
        DFANode& dfaSourceNode = dfa.nodes[dfaNodeId];
        dfaSourceNode.setTransitions(transitionsStart, static_cast<uint8_t>(transitionsLength));
    } while (!unprocessedNodes.isEmpty());

    return dfa;
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
