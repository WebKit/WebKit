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

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

namespace ContentExtensions {

class DFAMerger {
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

        while (!m_unprocessedNodes.isEmpty()) {
            memset(m_transitionTargets, 0xff, sizeof(m_transitionTargets));

            uint64_t unprocessedNode = m_unprocessedNodes.takeLast();

            // We cannot cache the source node itself since we modify the machine. We only manipulate the IDs.
            uint32_t sourceNodeId = m_nodeMapping.get(unprocessedNode);

            // Populate the unique transitions.
            uint32_t indexA = extractIndexA(unprocessedNode);
            if (indexA != invalidNodeIndex)
                populateTransitions<WhichDFA::A>(indexA);

            uint32_t indexB = extractIndexB(unprocessedNode);
            if (indexB != invalidNodeIndex)
                populateTransitions<WhichDFA::B>(indexB);

            // Spread the fallback transitions over the unique transitions and keep a reference to the fallback transition.
            uint64_t fallbackTransitionSignature = signatureForIndices(invalidNodeIndex, invalidNodeIndex);
            if (indexA != invalidNodeIndex)
                populateFromFallbackTransitions<WhichDFA::A>(indexA, fallbackTransitionSignature);
            if (indexB != invalidNodeIndex)
                populateFromFallbackTransitions<WhichDFA::B>(indexB, fallbackTransitionSignature);

            createTransitions(sourceNodeId);
            createFallbackTransitionIfNeeded(sourceNodeId, fallbackTransitionSignature);
        }
        return m_output;
    }

private:
    uint32_t invalidNodeIndex = 0xffffffff;

    enum class WhichDFA {
        A,
        B
    };

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

        HashSet<uint64_t, DefaultHash<uint64_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> actions;
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

    template<WhichDFA whichDFA>
    void setHalfSignature(uint64_t& signature, uint32_t value)
    {
        unsigned shiftAmount = (whichDFA == WhichDFA::A) ? 32 : 0;
        uint64_t mask = static_cast<uint64_t>(0xffffffff) << (32 - shiftAmount);
        signature = (signature & mask) | static_cast<uint64_t>(value) << shiftAmount;
    }

    template<WhichDFA whichDFA>
    void populateTransitions(uint32_t nodeIndex)
    {
        const DFA& dfa = (whichDFA == WhichDFA::A) ? m_dfaA : m_dfaB;
        const DFANode& node = dfa.nodes[nodeIndex];
        uint32_t transitionsStart = node.transitionsStart();
        uint32_t transitionsEnd = transitionsStart + node.transitionsLength();

        // Extract transitions.
        for (uint32_t transitionIndex = transitionsStart; transitionIndex < transitionsEnd; ++transitionIndex) {
            uint8_t transitionCharacter = dfa.transitionCharacters[transitionIndex];
            RELEASE_ASSERT(transitionCharacter < WTF_ARRAY_LENGTH(m_transitionTargets));

            uint32_t transitionDestination = dfa.transitionDestinations[transitionIndex];
            setHalfSignature<whichDFA>(m_transitionTargets[transitionCharacter], transitionDestination);
        }
    }

    template<WhichDFA whichDFA>
    void populateFromFallbackTransitions(uint32_t nodeIndex, uint64_t& fallbackTransitionSignature)
    {
        const DFA& dfa = (whichDFA == WhichDFA::A) ? m_dfaA : m_dfaB;
        const DFANode& node = dfa.nodes[nodeIndex];
        if (!node.hasFallbackTransition())
            return;

        uint32_t fallbackTarget = node.fallbackTransitionDestination(dfa);
        setHalfSignature<whichDFA>(fallbackTransitionSignature, fallbackTarget);

        // Spread the fallback over transitions where the other has something and we don't.
        for (unsigned i = 0; i < WTF_ARRAY_LENGTH(m_transitionTargets); ++i) {
            uint64_t& targetSignature = m_transitionTargets[i];

            uint32_t otherTarget = (whichDFA == WhichDFA::A) ? extractIndexB(targetSignature) : extractIndexA(targetSignature);
            uint32_t selfTarget = (whichDFA == WhichDFA::A) ? extractIndexA(targetSignature) : extractIndexB(targetSignature);

            if (otherTarget != invalidNodeIndex && selfTarget == invalidNodeIndex)
                setHalfSignature<whichDFA>(targetSignature, fallbackTarget);
        }
    }

    void createTransitions(uint32_t sourceNodeId)
    {
        // Create transitions out of the source node, creating new nodes as needed.
        uint32_t transitionsStart = m_output.transitionCharacters.size();
        for (uint8_t i = 0; i < WTF_ARRAY_LENGTH(m_transitionTargets); ++i) {
            uint64_t signature = m_transitionTargets[i];
            if (signature == signatureForIndices(invalidNodeIndex, invalidNodeIndex))
                continue;
            uint32_t nodeId = getOrCreateCombinedNode(signature);
            m_output.transitionCharacters.append(i);
            m_output.transitionDestinations.append(nodeId);
        }
        uint32_t transitionsEnd = m_output.transitionCharacters.size();
        uint8_t transitionsLength = static_cast<uint8_t>(transitionsEnd - transitionsStart);

        m_output.nodes[sourceNodeId].setTransitions(transitionsStart, transitionsLength);
    }

    void createFallbackTransitionIfNeeded(uint32_t sourceNodeId, uint64_t fallbackTransitionSignature)
    {
        if (fallbackTransitionSignature != signatureForIndices(invalidNodeIndex, invalidNodeIndex)) {
            uint32_t nodeId = getOrCreateCombinedNode(fallbackTransitionSignature);
            m_output.nodes[sourceNodeId].addFallbackTransition(m_output, nodeId);
        }
    }

    const DFA& m_dfaA;
    const DFA& m_dfaB;
    DFA m_output;
    HashMap<uint64_t, uint32_t, DefaultHash<uint64_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_nodeMapping;
    Vector<uint64_t> m_unprocessedNodes;
    uint64_t m_transitionTargets[128];
};

void DFACombiner::combineDFAs(unsigned minimumSize, std::function<void(DFA&&)> handler)
{
    if (m_dfas.isEmpty())
        return;

    for (unsigned i = m_dfas.size(); i--;) {
        if (m_dfas[i].graphSize() > minimumSize) {
            handler(WTF::move(m_dfas[i]));
            m_dfas.remove(i);
        }
    }

    while (!m_dfas.isEmpty()) {
        if (m_dfas.size() == 1) {
            handler(WTF::move(m_dfas.first()));
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
            handler(WTF::move(c));
        else
            m_dfas.append(c);
    }
}

}

} // namespace WebCore
