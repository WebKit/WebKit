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
#include "DFA.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "DFAMinimizer.h"
#include <wtf/DataLog.h>

namespace WebCore {

namespace ContentExtensions {

size_t DFA::memoryUsed() const
{
    return sizeof(DFA)
        + actions.capacity() * sizeof(uint64_t)
        + transitionCharacters.capacity() * sizeof(uint8_t)
        + transitionDestinations.capacity() * sizeof(uint32_t)
        + nodes.capacity() * sizeof(DFANode);
}

// FIXME: Make DFANode.cpp.
Vector<uint64_t> DFANode::actions(const DFA& dfa) const
{
    // FIXME: Use iterators instead of copying the Vector elements.
    Vector<uint64_t> vector;
    vector.reserveInitialCapacity(m_actionsLength);
    for (uint32_t i = m_actionsStart; i < m_actionsStart + m_actionsLength; ++i)
        vector.uncheckedAppend(dfa.actions[i]);
    return vector;
}
    
Vector<std::pair<uint8_t, uint32_t>> DFANode::transitions(const DFA& dfa) const
{
    // FIXME: Use iterators instead of copying the Vector elements.
    Vector<std::pair<uint8_t, uint32_t>> vector;
    vector.reserveInitialCapacity(transitionsLength());
    for (uint32_t i = m_transitionsStart; i < m_transitionsStart + m_transitionsLength; ++i)
        vector.uncheckedAppend(std::make_pair(dfa.transitionCharacters[i], dfa.transitionDestinations[i]));
    return vector;
}

uint32_t DFANode::fallbackTransitionDestination(const DFA& dfa) const
{
    RELEASE_ASSERT(hasFallbackTransition());

    // If there is a fallback transition, it is just after the other transitions and has an invalid ASCII character to mark it as a fallback transition.
    ASSERT(dfa.transitionCharacters[m_transitionsStart + m_transitionsLength] == std::numeric_limits<uint8_t>::max());
    return dfa.transitionDestinations[m_transitionsStart + m_transitionsLength];
}

void DFANode::changeFallbackTransition(DFA& dfa, uint32_t newDestination)
{
    RELEASE_ASSERT(hasFallbackTransition());
    ASSERT_WITH_MESSAGE(dfa.transitionCharacters[m_transitionsStart + m_transitionsLength] == std::numeric_limits<uint8_t>::max(), "When changing a fallback transition, the fallback transition should already be marked as such");
    dfa.transitionCharacters[m_transitionsStart + m_transitionsLength] = std::numeric_limits<uint8_t>::max();
    dfa.transitionDestinations[m_transitionsStart + m_transitionsLength] = newDestination;
}

void DFANode::addFallbackTransition(DFA& dfa, uint32_t destination)
{
    RELEASE_ASSERT(dfa.transitionCharacters.size() == dfa.transitionDestinations.size());
    RELEASE_ASSERT_WITH_MESSAGE(dfa.transitionCharacters.size() == m_transitionsStart + m_transitionsLength, "Adding a fallback transition should only happen if the node is at the end");
    dfa.transitionCharacters.append(std::numeric_limits<uint8_t>::max());
    dfa.transitionDestinations.append(destination);
    ASSERT(!(m_flags & HasFallbackTransition));
    m_flags |= HasFallbackTransition;
}

bool DFANode::containsTransition(uint8_t transition, DFA& dfa)
{
    // Called from DFAMinimizer, this loops though a maximum of 128 transitions, so it's not too slow.
    ASSERT(m_transitionsLength <= 128);
    for (unsigned i = m_transitionsStart; i < m_transitionsStart + m_transitionsLength; ++i) {
        if (dfa.transitionCharacters[i] == transition)
            return true;
    }
    return false;
}
    
void DFANode::kill(DFA& dfa)
{
    ASSERT(m_flags != IsKilled);
    m_flags = IsKilled; // Killed nodes don't have any other flags.
    
    // Invalidate the now-unused memory in the DFA to make finding bugs easier.
    for (unsigned i = m_transitionsStart; i < m_transitionsStart + m_transitionsLength; ++i) {
        dfa.transitionCharacters[i] = std::numeric_limits<uint8_t>::max();
        dfa.transitionDestinations[i] = std::numeric_limits<uint32_t>::max();
    }
    for (unsigned i = m_actionsStart; i < m_actionsStart + m_actionsLength; ++i)
        dfa.actions[i] = std::numeric_limits<uint64_t>::max();

    m_actionsStart = 0;
    m_actionsLength = 0;
    m_transitionsStart = 0;
    m_transitionsLength = 0;
};

void DFA::minimize()
{
    DFAMinimizer::minimize(*this);
}

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
static void printRange(bool firstRange, char rangeStart, char rangeEnd)
{
    if (!firstRange)
        dataLogF(", ");
    if (rangeStart == rangeEnd) {
        if (rangeStart == '"' || rangeStart == '\\')
            dataLogF("\\%c", rangeStart);
        else if (rangeStart >= '!' && rangeStart <= '~')
            dataLogF("%c", rangeStart);
        else
            dataLogF("\\\\%d", rangeStart);
    } else
        dataLogF("\\\\%d-\\\\%d", rangeStart, rangeEnd);
}

static void printTransitions(const DFA& dfa, unsigned sourceNodeId)
{
    const DFANode& sourceNode = dfa.nodes[sourceNodeId];
    auto transitions = sourceNode.transitions(dfa);

    if (transitions.isEmpty() && !sourceNode.hasFallbackTransition())
        return;

    HashMap<unsigned, Vector<uint16_t>, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> transitionsPerTarget;

    // First, we build the list of transitions coming to each target node.
    for (const auto& transition : transitions) {
        unsigned target = transition.second;
        transitionsPerTarget.add(target, Vector<uint16_t>());

        transitionsPerTarget.find(target)->value.append(transition.first);
    }

    // Then we go over each one an display the ranges one by one.
    for (const auto& transitionPerTarget : transitionsPerTarget) {
        dataLogF("        %d -> %d [label=\"", sourceNodeId, transitionPerTarget.key);

        Vector<uint16_t> incommingCharacters = transitionPerTarget.value;
        std::sort(incommingCharacters.begin(), incommingCharacters.end());

        char rangeStart = incommingCharacters.first();
        char rangeEnd = rangeStart;
        bool first = true;
        for (unsigned sortedTransitionIndex = 1; sortedTransitionIndex < incommingCharacters.size(); ++sortedTransitionIndex) {
            char nextChar = incommingCharacters[sortedTransitionIndex];
            if (nextChar == rangeEnd+1) {
                rangeEnd = nextChar;
                continue;
            }
            printRange(first, rangeStart, rangeEnd);
            rangeStart = rangeEnd = nextChar;
            first = false;
        }
        printRange(first, rangeStart, rangeEnd);

        dataLogF("\"];\n");
    }

    if (sourceNode.hasFallbackTransition())
        dataLogF("        %d -> %d [label=\"[fallback]\"];\n", sourceNodeId, sourceNode.fallbackTransitionDestination(dfa));
}

void DFA::debugPrintDot() const
{
    dataLogF("digraph DFA_Transitions {\n");
    dataLogF("    rankdir=LR;\n");
    dataLogF("    node [shape=circle];\n");
    dataLogF("    {\n");
    for (unsigned i = 0; i < nodes.size(); ++i) {
        if (nodes[i].isKilled())
            continue;

        dataLogF("         %d [label=<Node %d", i, i);
        const Vector<uint64_t>& actions = nodes[i].actions(*this);
        if (!actions.isEmpty()) {
            dataLogF("<BR/>Actions: ");
            for (unsigned actionIndex = 0; actionIndex < actions.size(); ++actionIndex) {
                if (actionIndex)
                    dataLogF(", ");
                dataLogF("%llu", actions[actionIndex]);
            }
        }

        Vector<unsigned> correspondingNFANodes = nodes[i].correspondingNFANodes;
        ASSERT(!correspondingNFANodes.isEmpty());
        dataLogF("<BR/>NFA Nodes: ");
        for (unsigned correspondingDFANodeIndex = 0; correspondingDFANodeIndex < correspondingNFANodes.size(); ++correspondingDFANodeIndex) {
            if (correspondingDFANodeIndex)
                dataLogF(", ");
            dataLogF("%d", correspondingNFANodes[correspondingDFANodeIndex]);
        }

        dataLogF(">]");

        if (!actions.isEmpty())
            dataLogF(" [shape=doublecircle]");

        dataLogF(";\n");
    }
    dataLogF("    }\n");

    dataLogF("    {\n");
    for (unsigned i = 0; i < nodes.size(); ++i)
        printTransitions(*this, i);

    dataLogF("    }\n");
    dataLogF("}\n");
}
#endif

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
