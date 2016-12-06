/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "DFAMinimizer.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "DFA.h"
#include "DFANode.h"
#include "MutableRangeList.h"
#include <wtf/HashMap.h>
#include <wtf/Hasher.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace ContentExtensions {

namespace {

template<typename VectorType, typename Iterable, typename Function>
static inline void iterateIntersections(const VectorType& singularTransitionsFirsts, const Iterable& iterableTransitionList, const Function& intersectionHandler)
{
    ASSERT(!singularTransitionsFirsts.isEmpty());
    auto otherIterator = iterableTransitionList.begin();
    auto otherEnd = iterableTransitionList.end();

    if (otherIterator == otherEnd)
        return;

    unsigned singularTransitionsLength = singularTransitionsFirsts.size();
    unsigned singularTransitionsFirstsIndex = 0;
    for (; otherIterator != otherEnd; ++otherIterator) {
        auto firstCharacter = otherIterator.first();
        while (singularTransitionsFirstsIndex < singularTransitionsLength
            && singularTransitionsFirsts[singularTransitionsFirstsIndex] != firstCharacter)
            ++singularTransitionsFirstsIndex;

        intersectionHandler(singularTransitionsFirstsIndex, otherIterator);
        ++singularTransitionsFirstsIndex;

        auto lastCharacter = otherIterator.last();
        while (singularTransitionsFirstsIndex < singularTransitionsLength
            && singularTransitionsFirsts[singularTransitionsFirstsIndex] <= lastCharacter) {
            intersectionHandler(singularTransitionsFirstsIndex, otherIterator);
            ++singularTransitionsFirstsIndex;
        }
    }
}

class Partition {
public:
    void initialize(unsigned size)
    {
        if (!size)
            return;

        m_sets.reserveInitialCapacity(size);
        m_partitionedElements.resize(size);
        m_elementPositionInPartitionedNodes.resize(size);
        m_elementToSetMap.resize(size);

        for (unsigned i = 0; i < size; ++i) {
            m_partitionedElements[i] = i;
            m_elementPositionInPartitionedNodes[i] = i;
            m_elementToSetMap[i] = 0;
        }
        m_sets.uncheckedAppend(SetDescriptor { 0, size, 0 });
    }

    void reserveUninitializedCapacity(unsigned elementCount)
    {
        m_partitionedElements.resize(elementCount);
        m_elementPositionInPartitionedNodes.resize(elementCount);
        m_elementToSetMap.resize(elementCount);
    }

    void addSetUnchecked(unsigned start, unsigned size)
    {
        m_sets.append(SetDescriptor { start, size, 0 });
    }

    void setElementUnchecked(unsigned elementIndex, unsigned positionInPartition, unsigned setIndex)
    {
        ASSERT(setIndex < m_sets.size());
        m_partitionedElements[positionInPartition] = elementIndex;
        m_elementPositionInPartitionedNodes[elementIndex] = positionInPartition;
        m_elementToSetMap[elementIndex] = setIndex;
    }

    unsigned startOffsetOfSet(unsigned setIndex) const
    {
        return m_sets[setIndex].start;
    }

    ALWAYS_INLINE void markElementInCurrentGeneration(unsigned elementIndex)
    {
        // Swap the node with the first unmarked node.
        unsigned setIndex = m_elementToSetMap[elementIndex];
        SetDescriptor& setDescriptor = m_sets[setIndex];

        unsigned elementPositionInPartition = m_elementPositionInPartitionedNodes[elementIndex];
        ASSERT(elementPositionInPartition >= setDescriptor.start);
        ASSERT(elementPositionInPartition < setDescriptor.end());

        unsigned firstUnmarkedElementPositionInPartition = setDescriptor.indexAfterMarkedElements();
        ASSERT(firstUnmarkedElementPositionInPartition >= setDescriptor.start && firstUnmarkedElementPositionInPartition < setDescriptor.end());
        ASSERT(firstUnmarkedElementPositionInPartition >= firstUnmarkedElementPositionInPartition);

        // Swap the nodes in the set.
        unsigned firstUnmarkedElement = m_partitionedElements[firstUnmarkedElementPositionInPartition];
        m_partitionedElements[firstUnmarkedElementPositionInPartition] = elementIndex;
        m_partitionedElements[elementPositionInPartition] = firstUnmarkedElement;

        // Update their index.
        m_elementPositionInPartitionedNodes[elementIndex] = firstUnmarkedElementPositionInPartition;
        m_elementPositionInPartitionedNodes[firstUnmarkedElement] = elementPositionInPartition;

        if (!setDescriptor.markedCount) {
            ASSERT(!m_setsMarkedInCurrentGeneration.contains(setIndex));
            m_setsMarkedInCurrentGeneration.append(setIndex);
        }
        ++setDescriptor.markedCount;
    }

    // The function passed as argument MUST not modify the partition.
    template<typename Function>
    void refineGeneration(const Function& function)
    {
        for (unsigned setIndex : m_setsMarkedInCurrentGeneration) {
            SetDescriptor& setDescriptor = m_sets[setIndex];
            if (setDescriptor.markedCount == setDescriptor.size) {
                // Everything is marked, there is nothing to refine.
                setDescriptor.markedCount = 0;
                continue;
            }

            SetDescriptor newSet;
            bool newSetIsMarkedSet = setDescriptor.markedCount * 2 <= setDescriptor.size;
            if (newSetIsMarkedSet) {
                // Less than half of the nodes have been marked.
                newSet = { setDescriptor.start, setDescriptor.markedCount, 0 };
                setDescriptor.start = setDescriptor.start + setDescriptor.markedCount;
            } else
                newSet = { setDescriptor.start + setDescriptor.markedCount, setDescriptor.size - setDescriptor.markedCount, 0 };
            setDescriptor.size -= newSet.size;
            setDescriptor.markedCount = 0;

            unsigned newSetIndex = m_sets.size();
            m_sets.append(newSet);

            for (unsigned i = newSet.start; i < newSet.end(); ++i)
                m_elementToSetMap[m_partitionedElements[i]] = newSetIndex;

            function(newSetIndex);
        }
        m_setsMarkedInCurrentGeneration.clear();
    }

    // Call Function() on every node of a given subset.
    template<typename Function>
    void iterateSet(unsigned setIndex, const Function& function)
    {
        SetDescriptor& setDescriptor = m_sets[setIndex];
        for (unsigned i = setDescriptor.start; i < setDescriptor.end(); ++i)
            function(m_partitionedElements[i]);
    }

    // Index of the set containing the Node.
    unsigned setIndex(unsigned elementIndex) const
    {
        return m_elementToSetMap[elementIndex];
    }

    // NodeIndex of the first element in the set.
    unsigned firstElementInSet(unsigned setIndex) const
    {
        return m_partitionedElements[m_sets[setIndex].start];
    }

    unsigned size() const
    {
        return m_sets.size();
    }

private:
    struct SetDescriptor {
        unsigned start;
        unsigned size;
        unsigned markedCount;

        unsigned indexAfterMarkedElements() const { return start + markedCount; }
        unsigned end() const { return start + size; }
    };

    // List of sets.
    Vector<SetDescriptor, 0, ContentExtensionsOverflowHandler> m_sets;

    // All the element indices such that two elements of the same set never have a element of a different set between them.
    Vector<unsigned, 0, ContentExtensionsOverflowHandler> m_partitionedElements;

    // Map elementIndex->position in the partitionedElements.
    Vector<unsigned, 0, ContentExtensionsOverflowHandler> m_elementPositionInPartitionedNodes;

    // Map elementIndex->SetIndex.
    Vector<unsigned, 0, ContentExtensionsOverflowHandler> m_elementToSetMap;

    // List of sets with any marked node. Each set can appear at most once.
    // FIXME: find a good inline size for this.
    Vector<unsigned, 128, ContentExtensionsOverflowHandler> m_setsMarkedInCurrentGeneration;
};

class FullGraphPartition {
    typedef MutableRangeList<char, uint32_t, 128> SingularTransitionsMutableRangeList;
public:
    FullGraphPartition(const DFA& dfa)
    {
        m_nodePartition.initialize(dfa.nodes.size());

        SingularTransitionsMutableRangeList singularTransitions;
        CounterConverter counterConverter;
        for (const DFANode& node : dfa.nodes) {
            if (node.isKilled())
                continue;
            auto transitions = node.transitions(dfa);
            singularTransitions.extend(transitions.begin(), transitions.end(), counterConverter);
        }

        // Count the number of transition for each singular range. This will give us the bucket size
        // for the transition partition, where transitions are partitioned by "symbol".
        unsigned rangeIndexAccumulator = 0;
        for (const auto& transition : singularTransitions) {
            m_transitionPartition.addSetUnchecked(rangeIndexAccumulator, transition.data);
            rangeIndexAccumulator += transition.data;
        }

        // Count the number of incoming transitions per node.
        m_flattenedTransitionsStartOffsetPerNode.resize(dfa.nodes.size());
        memset(m_flattenedTransitionsStartOffsetPerNode.data(), 0, m_flattenedTransitionsStartOffsetPerNode.size() * sizeof(unsigned));

        Vector<char, 0, ContentExtensionsOverflowHandler> singularTransitionsFirsts;
        singularTransitionsFirsts.reserveInitialCapacity(singularTransitions.m_ranges.size());
        for (const auto& transition : singularTransitions)
            singularTransitionsFirsts.uncheckedAppend(transition.first);

        for (const DFANode& node : dfa.nodes) {
            if (node.isKilled())
                continue;
            auto transitions = node.transitions(dfa);
            iterateIntersections(singularTransitionsFirsts, transitions, [&](unsigned, const DFANode::ConstRangeIterator& origin) {
                uint32_t targetNodeIndex = origin.target();
                ++m_flattenedTransitionsStartOffsetPerNode[targetNodeIndex];
            });
        }

        // Accumulate the offsets. This gives us the start position of each bucket.
        unsigned transitionAccumulator = 0;
        for (unsigned i = 0; i < m_flattenedTransitionsStartOffsetPerNode.size(); ++i) {
            unsigned transitionsCountForNode = m_flattenedTransitionsStartOffsetPerNode[i];
            m_flattenedTransitionsStartOffsetPerNode[i] = transitionAccumulator;
            transitionAccumulator += transitionsCountForNode;
        }
        unsigned flattenedTransitionsSize = transitionAccumulator;
        ASSERT_WITH_MESSAGE(flattenedTransitionsSize == rangeIndexAccumulator, "The number of transitions should be the same, regardless of how they are arranged in buckets.");

        m_transitionPartition.reserveUninitializedCapacity(flattenedTransitionsSize);

        // Next, let's fill the transition table and set up the size of each group at the same time.
        m_flattenedTransitionsSizePerNode.resize(dfa.nodes.size());
        for (unsigned& counter : m_flattenedTransitionsSizePerNode)
            counter = 0;
        m_flattenedTransitions.resize(flattenedTransitionsSize);

        Vector<uint32_t> transitionPerRangeOffset(m_transitionPartition.size());
        memset(transitionPerRangeOffset.data(), 0, transitionPerRangeOffset.size() * sizeof(uint32_t));

        for (unsigned i = 0; i < dfa.nodes.size(); ++i) {
            const DFANode& node = dfa.nodes[i];
            if (node.isKilled())
                continue;

            auto transitions = node.transitions(dfa);
            iterateIntersections(singularTransitionsFirsts, transitions, [&](unsigned singularTransitonIndex, const DFANode::ConstRangeIterator& origin) {
                uint32_t targetNodeIndex = origin.target();

                unsigned start = m_flattenedTransitionsStartOffsetPerNode[targetNodeIndex];
                unsigned offset = m_flattenedTransitionsSizePerNode[targetNodeIndex];
                unsigned positionInFlattenedTransitions = start + offset;
                m_flattenedTransitions[positionInFlattenedTransitions] = Transition({ i });

                uint32_t& inRangeOffset = transitionPerRangeOffset[singularTransitonIndex];
                unsigned positionInTransitionPartition = m_transitionPartition.startOffsetOfSet(singularTransitonIndex) + inRangeOffset;
                ++inRangeOffset;

                m_transitionPartition.setElementUnchecked(positionInFlattenedTransitions, positionInTransitionPartition, singularTransitonIndex);

                ++m_flattenedTransitionsSizePerNode[targetNodeIndex];
            });
        }
    }

    void markNode(unsigned nodeIndex)
    {
        m_nodePartition.markElementInCurrentGeneration(nodeIndex);
    }

    void refinePartitions()
    {
        m_nodePartition.refineGeneration([&](unsigned smallestSetIndex) {
            m_nodePartition.iterateSet(smallestSetIndex, [&](unsigned nodeIndex) {
                unsigned incomingTransitionsStartForNode = m_flattenedTransitionsStartOffsetPerNode[nodeIndex];
                unsigned incomingTransitionsSizeForNode = m_flattenedTransitionsSizePerNode[nodeIndex];

                for (unsigned i = 0; i < incomingTransitionsSizeForNode; ++i)
                    m_transitionPartition.markElementInCurrentGeneration(incomingTransitionsStartForNode + i);
            });

            // We only need to split the transitions, we handle the new sets through the main loop.
            m_transitionPartition.refineGeneration([](unsigned) { });
        });
    }

    void splitByUniqueTransitions()
    {
        for (; m_nextTransitionSetToProcess < m_transitionPartition.size(); ++m_nextTransitionSetToProcess) {
            // We use the known splitters to refine the set.
            m_transitionPartition.iterateSet(m_nextTransitionSetToProcess, [&](unsigned transitionIndex) {
                unsigned sourceNodeIndex = m_flattenedTransitions[transitionIndex].source;
                m_nodePartition.markElementInCurrentGeneration(sourceNodeIndex);
            });

            refinePartitions();
        }
    }

    unsigned nodeReplacement(unsigned nodeIndex)
    {
        unsigned setIndex = m_nodePartition.setIndex(nodeIndex);
        return m_nodePartition.firstElementInSet(setIndex);
    }

private:
    struct Transition {
        unsigned source;
    };

    struct CounterConverter {
        uint32_t convert(uint32_t)
        {
            return 1;
        }

        void extend(uint32_t& destination, uint32_t)
        {
            ++destination;
        }
    };

    Vector<unsigned, 0, ContentExtensionsOverflowHandler> m_flattenedTransitionsStartOffsetPerNode;
    Vector<unsigned, 0, ContentExtensionsOverflowHandler> m_flattenedTransitionsSizePerNode;
    Vector<Transition, 0, ContentExtensionsOverflowHandler> m_flattenedTransitions;

    Partition m_nodePartition;
    Partition m_transitionPartition;

    unsigned m_nextTransitionSetToProcess { 0 };
};

struct ActionKey {
    enum DeletedValueTag { DeletedValue };
    explicit ActionKey(DeletedValueTag) { state = Deleted; }

    enum EmptyValueTag { EmptyValue };
    explicit ActionKey(EmptyValueTag) { state = Empty; }

    explicit ActionKey(const DFA* dfa, uint32_t actionsStart, uint16_t actionsLength)
        : dfa(dfa)
        , actionsStart(actionsStart)
        , actionsLength(actionsLength)
        , state(Valid)
    {
        StringHasher hasher;
        hasher.addCharactersAssumingAligned(reinterpret_cast<const UChar*>(&dfa->actions[actionsStart]), actionsLength * sizeof(uint64_t) / sizeof(UChar));
        hash = hasher.hash();
    }

    bool isEmptyValue() const { return state == Empty; }
    bool isDeletedValue() const { return state == Deleted; }

    unsigned hash;
    
    const DFA* dfa;
    uint32_t actionsStart;
    uint16_t actionsLength;
    
    enum {
        Valid,
        Empty,
        Deleted
    } state;
};

struct ActionKeyHash {
    static unsigned hash(const ActionKey& actionKey)
    {
        return actionKey.hash;
    }

    static bool equal(const ActionKey& a, const ActionKey& b)
    {
        if (a.state != b.state
            || a.dfa != b.dfa
            || a.actionsLength != b.actionsLength)
            return false;
        for (uint16_t i = 0; i < a.actionsLength; ++i) {
            if (a.dfa->actions[a.actionsStart + i] != a.dfa->actions[b.actionsStart + i])
                return false;
        }
        return true;
    }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct ActionKeyHashTraits : public WTF::CustomHashTraits<ActionKey> {
    static const bool emptyValueIsZero = true;
};

} // anonymous namespace.

void DFAMinimizer::minimize(DFA& dfa)
{
    FullGraphPartition fullGraphPartition(dfa);

    // Unlike traditional minimization final states can be differentiated by their action.
    // Instead of creating a single set for the final state, we partition by actions from
    // the start.
    HashMap<ActionKey, Vector<unsigned>, ActionKeyHash, ActionKeyHashTraits> finalStates;
    for (unsigned i = 0; i < dfa.nodes.size(); ++i) {
        const DFANode& node = dfa.nodes[i];
        if (node.hasActions()) {
            // FIXME: Sort the actions in the dfa to make nodes that have the same actions in different order equal.
            auto addResult = finalStates.add(ActionKey(&dfa, node.actionsStart(), node.actionsLength()), Vector<unsigned>());
            addResult.iterator->value.append(i);
        }
    }

    for (const auto& slot : finalStates) {
        for (unsigned finalStateIndex : slot.value)
            fullGraphPartition.markNode(finalStateIndex);
        fullGraphPartition.refinePartitions();
    }

    // Use every splitter to refine the node partitions.
    fullGraphPartition.splitByUniqueTransitions();

    Vector<unsigned> relocationVector;
    relocationVector.reserveInitialCapacity(dfa.nodes.size());
    for (unsigned i = 0; i < dfa.nodes.size(); ++i)
        relocationVector.uncheckedAppend(i);

    // Update all the transitions.
    for (unsigned i = 0; i < dfa.nodes.size(); ++i) {
        unsigned replacement = fullGraphPartition.nodeReplacement(i);
        if (i != replacement) {
            relocationVector[i] = replacement;
            dfa.nodes[i].kill(dfa);
        }
    }

    dfa.root = relocationVector[dfa.root];
    for (DFANode& node : dfa.nodes) {
        if (node.isKilled())
            continue;

        for (auto& transition : node.transitions(dfa)) {
            uint32_t target = transition.target();
            uint32_t relocatedTarget = relocationVector[target];
            if (target != relocatedTarget)
                transition.resetTarget(relocatedTarget);
        }
    }
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
