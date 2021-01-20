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
#include "DFACombiner.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "MutableRangeList.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

namespace ContentExtensions {

class DFAMerger {
    typedef MutableRangeList<signed char, uint64_t, 128> CombinedTransitionsMutableRangeList;

    enum class WhichDFA {
        A,
        B
    };

    template<WhichDFA whichDFA>
    struct TargetConverter {
        uint64_t convert(uint32_t target)
        {
            uint64_t value = 0xffffffffffffffff;
            extend(value, target);
            return value;
        }

        void extend(uint64_t& destination, uint32_t target)
        {
            setHalfSignature(destination, target);
        }
    private:
        void setHalfSignature(uint64_t& signature, uint32_t value)
        {
            unsigned shiftAmount = (whichDFA == WhichDFA::A) ? 32 : 0;
            uint64_t mask = static_cast<uint64_t>(0xffffffff) << (32 - shiftAmount);
            signature = (signature & mask) | static_cast<uint64_t>(value) << shiftAmount;
        }
    };

public:
    DFAMerger(const DFA& a, const DFA& b)
        : m_dfaA(a)
        , m_dfaB(b)
    {
    }

    DFA merge()
    {
        if (!m_nodeMapping.isEmpty())
            return m_output;

        uint64_t rootSignature = signatureForIndices(m_dfaA.root, m_dfaB.root);
        getOrCreateCombinedNode(rootSignature);

        CombinedTransitionsMutableRangeList combinedTransitions;
        while (!m_unprocessedNodes.isEmpty()) {
            combinedTransitions.clear();

            uint64_t unprocessedNode = m_unprocessedNodes.takeLast();

            uint32_t indexA = extractIndexA(unprocessedNode);
            if (indexA != invalidNodeIndex) {
                const DFANode& nodeA = m_dfaA.nodes[indexA];
                auto transitionsA = nodeA.transitions(m_dfaA);
                TargetConverter<WhichDFA::A> converterA;
                combinedTransitions.extend(transitionsA.begin(), transitionsA.end(), converterA);
            }

            uint32_t indexB = extractIndexB(unprocessedNode);
            if (indexB != invalidNodeIndex) {
                const DFANode& nodeB = m_dfaB.nodes[indexB];
                auto transitionsB = nodeB.transitions(m_dfaB);
                TargetConverter<WhichDFA::B> converterB;
                combinedTransitions.extend(transitionsB.begin(), transitionsB.end(), converterB);
            }

            unsigned transitionsStart = m_output.transitionRanges.size();
            for (const auto& range : combinedTransitions) {
                unsigned targetNodeId = getOrCreateCombinedNode(range.data);
                m_output.transitionRanges.append({ range.first, range.last });
                m_output.transitionDestinations.append(targetNodeId);
            }
            unsigned transitionsEnd = m_output.transitionRanges.size();
            unsigned transitionsLength = transitionsEnd - transitionsStart;

            uint32_t sourceNodeId = m_nodeMapping.get(unprocessedNode);
            DFANode& dfaSourceNode = m_output.nodes[sourceNodeId];
            dfaSourceNode.setTransitions(transitionsStart, static_cast<uint8_t>(transitionsLength));
        }
        return m_output;
    }

private:
    uint32_t invalidNodeIndex = 0xffffffff;

    static uint64_t signatureForIndices(uint32_t aIndex, uint32_t bIndex)
    {
        return static_cast<uint64_t>(aIndex) << 32 | bIndex;
    }

    static uint32_t extractIndexA(uint64_t signature)
    {
        return static_cast<uint32_t>(signature >> 32);
    }

    static uint32_t extractIndexB(uint64_t signature)
    {
        return static_cast<uint32_t>(signature);
    }

    uint32_t getOrCreateCombinedNode(uint64_t newNodeSignature)
    {
        auto addResult = m_nodeMapping.add(newNodeSignature, invalidNodeIndex);
        if (!addResult.isNewEntry)
            return addResult.iterator->value;

        m_output.nodes.append(DFANode());
        uint32_t newNodeIndex = m_output.nodes.size() - 1;
        addResult.iterator->value = newNodeIndex;
        m_unprocessedNodes.append(newNodeSignature);

        uint32_t indexA = extractIndexA(newNodeSignature);
        uint32_t indexB = extractIndexB(newNodeSignature);

        HashSet<uint64_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> actions;
        if (indexA != invalidNodeIndex) {
            const DFANode& node = m_dfaA.nodes[indexA];
            uint32_t actionsStart = node.actionsStart();
            uint32_t actionsEnd = actionsStart + node.actionsLength();
            for (uint32_t i = actionsStart; i < actionsEnd; ++i)
                actions.add(m_dfaA.actions[i]);
        }
        if (indexB != invalidNodeIndex) {
            const DFANode& node = m_dfaB.nodes[indexB];
            uint32_t actionsStart = node.actionsStart();
            uint32_t actionsEnd = actionsStart + node.actionsLength();
            for (uint32_t i = actionsStart; i < actionsEnd; ++i)
                actions.add(m_dfaB.actions[i]);
        }

        uint32_t actionsStart = m_output.actions.size();
        for (uint64_t action : actions)
            m_output.actions.append(action);
        uint32_t actionsEnd = m_output.actions.size();
        uint16_t actionsLength = static_cast<uint16_t>(actionsEnd - actionsStart);

        m_output.nodes.last().setActions(actionsStart, actionsLength);
        return newNodeIndex;
    }

    const DFA& m_dfaA;
    const DFA& m_dfaB;
    DFA m_output;
    HashMap<uint64_t, uint32_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_nodeMapping;
    Vector<uint64_t, 0, ContentExtensionsOverflowHandler> m_unprocessedNodes;
};

void DFACombiner::combineDFAs(unsigned minimumSize, const WTF::Function<void(DFA&&)>& handler)
{
    if (m_dfas.isEmpty())
        return;

    for (unsigned i = m_dfas.size(); i--;) {
        if (m_dfas[i].graphSize() > minimumSize) {
            handler(WTFMove(m_dfas[i]));
            m_dfas.remove(i);
        }
    }

    while (!m_dfas.isEmpty()) {
        if (m_dfas.size() == 1) {
            handler(WTFMove(m_dfas.first()));
            return;
        }

        DFA a = m_dfas.takeLast();
        DFA b = m_dfas.takeLast();
        DFAMerger dfaMerger(a, b);
        DFA c = dfaMerger.merge();

        if (c.graphSize() > minimumSize || m_dfas.isEmpty()) {
            // Minimizing is somewhat expensive. We only do it in bulk when we reach the threshold
            // to reduce the load.
            c.minimize();
        }

        if (c.graphSize() > minimumSize)
            handler(WTFMove(c));
        else
            m_dfas.append(c);
    }
}

}

} // namespace WebCore

#endif
