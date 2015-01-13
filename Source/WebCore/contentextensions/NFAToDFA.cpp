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

#include "DFANode.h"
#include "NFA.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

namespace ContentExtensions {

typedef HashSet<unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> NodeIdSet;

static NodeIdSet epsilonClosure(const NodeIdSet& nodeSet, const Vector<NFANode>& graph, unsigned epsilonTransitionCharacter)
{
    ASSERT(!nodeSet.isEmpty());
    ASSERT(!graph.isEmpty());

    // We go breadth-first first into our graph following all the epsilon transition. At each generation,
    // discoveredNodes contains all the new nodes we have discovered by following a single epsilon transition
    // out of the previous set of nodes.
    NodeIdSet outputNodeSet = nodeSet;
    NodeIdSet discoveredNodes = nodeSet;
    do {
        outputNodeSet.add(discoveredNodes.begin(), discoveredNodes.end());

        NodeIdSet nextGenerationDiscoveredNodes;

        for (unsigned nodeId : discoveredNodes) {
            const NFANode& node = graph[nodeId];
            auto epsilonTransitionSlot = node.transitions.find(epsilonTransitionCharacter);
            if (epsilonTransitionSlot != node.transitions.end()) {
                const HashSet<unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>& targets = epsilonTransitionSlot->value;
                for (unsigned targetNodeId : targets) {
                    if (!outputNodeSet.contains(targetNodeId))
                        nextGenerationDiscoveredNodes.add(targetNodeId);
                }
            }
        }

        discoveredNodes = nextGenerationDiscoveredNodes;
    } while (!discoveredNodes.isEmpty());

    ASSERT(!outputNodeSet.isEmpty());
    return outputNodeSet;
}

typedef HashMap<uint16_t, NodeIdSet, DefaultHash<uint16_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint16_t>> SetTransitionsExcludingEpsilon;

static SetTransitionsExcludingEpsilon setTransitionsExcludingEpsilon(const NodeIdSet& nodeSet, const Vector<NFANode>& graph, unsigned epsilonTransitionCharacter)
{
    ASSERT(!nodeSet.isEmpty());
    ASSERT(!graph.isEmpty());

    SetTransitionsExcludingEpsilon outputSetTransitionsExcludingEpsilon;

    for (unsigned nodeId : nodeSet) {
        const NFANode& node = graph[nodeId];
        for (const auto& transitionSlot : node.transitions) {
            if (transitionSlot.key != epsilonTransitionCharacter) {
                auto existingTransition = outputSetTransitionsExcludingEpsilon.find(transitionSlot.key);
                if (existingTransition != outputSetTransitionsExcludingEpsilon.end())
                    existingTransition->value.add(transitionSlot.value.begin(), transitionSlot.value.end());
                else {
                    NodeIdSet newSet;
                    newSet.add(transitionSlot.value.begin(), transitionSlot.value.end());
                    outputSetTransitionsExcludingEpsilon.add(transitionSlot.key, newSet);
                }
            }
        }
    }

    return outputSetTransitionsExcludingEpsilon;
}

class HashableNodeIdSet {
public:
    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };

    HashableNodeIdSet(EmptyValueTag) { }
    HashableNodeIdSet(DeletedValueTag)
        : m_isDeleted(true)
    {
    }

    HashableNodeIdSet(const NodeIdSet& nodeIdSet)
        : m_nodeIdSet(nodeIdSet)
    {
        ASSERT(!nodeIdSet.isEmpty());
    }

    HashableNodeIdSet(HashableNodeIdSet&& other)
        : m_nodeIdSet(other.m_nodeIdSet)
        , m_isDeleted(other.m_isDeleted)
    {
        other.m_nodeIdSet.clear();
        other.m_isDeleted = false;
    }

    HashableNodeIdSet& operator=(HashableNodeIdSet&& other)
    {
        m_nodeIdSet = other.m_nodeIdSet;
        other.m_nodeIdSet.clear();
        m_isDeleted = other.m_isDeleted;
        other.m_isDeleted = false;
        return *this;
    }

    bool isEmptyValue() const { return m_nodeIdSet.isEmpty(); }
    bool isDeletedValue() const { return m_isDeleted; }

    NodeIdSet nodeIdSet() const { return m_nodeIdSet; }

private:
    NodeIdSet m_nodeIdSet;
    bool m_isDeleted = false;
};

struct HashableNodeIdSetHash {
    static unsigned hash(const HashableNodeIdSet& p)
    {
        unsigned hash = 4207445155;
        for (unsigned nodeId : p.nodeIdSet())
            hash += DefaultHash<unsigned>::Hash::hash(nodeId);
        return hash;
    }

    static bool equal(const HashableNodeIdSet& a, const HashableNodeIdSet& b)
    {
        return a.nodeIdSet() == b.nodeIdSet() && a.isDeletedValue() == b.isDeletedValue();
    }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct HashableNodeIdSetHashTraits : public WTF::CustomHashTraits<HashableNodeIdSet> { };

typedef HashMap<HashableNodeIdSet, unsigned, HashableNodeIdSetHash, HashableNodeIdSetHashTraits> NFAToDFANodeMap;

static unsigned addDFAState(Vector<DFANode>& dfaGraph, NFAToDFANodeMap& nfaToDFANodeMap, const Vector<NFANode>& nfaGraph, NodeIdSet nfaNodes, unsigned epsilonTransitionCharacter)
{
    ASSERT(!nfaToDFANodeMap.contains(nfaNodes));
    ASSERT_UNUSED(epsilonTransitionCharacter, epsilonClosure(nfaNodes, nfaGraph, epsilonTransitionCharacter) ==  nfaNodes);

    DFANode newDFANode;

    HashSet<uint64_t, DefaultHash<uint64_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> actions;
    for (unsigned nfaNodeId : nfaNodes) {
        const NFANode& nfaNode = nfaGraph[nfaNodeId];
        if (nfaNode.isFinal)
            actions.add(nfaNode.ruleId);
#ifndef NDEBUG
        newDFANode.correspondingDFANodes.append(nfaNodeId);
#endif
    }

    for (uint64_t action : actions)
        newDFANode.actions.append(action);

    unsigned dfaNodeId = dfaGraph.size();
    dfaGraph.append(newDFANode);
    nfaToDFANodeMap.add(nfaNodes, dfaNodeId);
    return dfaNodeId;
}

typedef HashSet<HashableNodeIdSet, HashableNodeIdSetHash, HashableNodeIdSetHashTraits> SetOfNodeSet;

DFA NFAToDFA::convert(const NFA& nfa)
{
    Vector<DFANode> dfaGraph;
    NFAToDFANodeMap nfaToDFANodeMap;

    SetOfNodeSet processedStateSets;
    SetOfNodeSet unprocessedStateSets;

    const Vector<NFANode>& nfaGraph = nfa.m_nodes;

    NodeIdSet initialSet({ nfa.root() });
    NodeIdSet closedInitialSet = epsilonClosure(initialSet, nfaGraph, NFA::epsilonTransitionCharacter);

    addDFAState(dfaGraph, nfaToDFANodeMap, nfaGraph, closedInitialSet, NFA::epsilonTransitionCharacter);
    unprocessedStateSets.add(closedInitialSet);

    do {
        HashableNodeIdSet stateSet = unprocessedStateSets.takeAny();

        ASSERT(!processedStateSets.contains(stateSet.nodeIdSet()));
        processedStateSets.add(stateSet.nodeIdSet());

        unsigned dfaNodeId = nfaToDFANodeMap.get(stateSet);

        SetTransitionsExcludingEpsilon transitionsFromClosedSet = setTransitionsExcludingEpsilon(stateSet.nodeIdSet(), nfaGraph, NFA::epsilonTransitionCharacter);
        for (const auto& transitionSlot : transitionsFromClosedSet) {
            NodeIdSet closedTargetNodeSet = epsilonClosure(transitionSlot.value, nfaGraph, NFA::epsilonTransitionCharacter);
            unsigned newDFANodeId;

            const auto& existingNFAToDFAAssociation = nfaToDFANodeMap.find(closedTargetNodeSet);
            if (existingNFAToDFAAssociation != nfaToDFANodeMap.end())
                newDFANodeId = existingNFAToDFAAssociation->value;
            else
                newDFANodeId = addDFAState(dfaGraph, nfaToDFANodeMap, nfaGraph, closedTargetNodeSet, NFA::epsilonTransitionCharacter);

            ASSERT(newDFANodeId < dfaGraph.size());

            const auto addResult = dfaGraph[dfaNodeId].transitions.set(transitionSlot.key, newDFANodeId);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);

            if (!processedStateSets.contains(closedTargetNodeSet))
                unprocessedStateSets.add(closedTargetNodeSet);
        }
    } while (!unprocessedStateSets.isEmpty());

    ASSERT(processedStateSets.size() == nfaToDFANodeMap.size());

    return DFA(WTF::move(dfaGraph), 0);
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
