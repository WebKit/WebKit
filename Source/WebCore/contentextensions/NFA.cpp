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
#include "NFA.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include <wtf/DataLog.h>
#include <wtf/HashSet.h>

namespace WebCore {

namespace ContentExtensions {

NFA::NFA()
    : m_root(createNode())
{
}

unsigned NFA::createNode()
{
    unsigned nextId = m_nodes.size();
    m_nodes.append(NFANode());
    return nextId;
}

void NFA::addTransition(unsigned from, unsigned to, char character)
{
    ASSERT(from < m_nodes.size());
    ASSERT(to < m_nodes.size());

    NFANode& fromNode = m_nodes[from];
    if (fromNode.transitionsOnAnyCharacter.contains(to))
        return;

    auto addResult = m_nodes[from].transitions.add(character, NFANodeIndexSet());
    addResult.iterator->value.add(to);
}

void NFA::addEpsilonTransition(unsigned from, unsigned to)
{
    ASSERT(from < m_nodes.size());
    ASSERT(to < m_nodes.size());

    auto addResult = m_nodes[from].transitions.add(epsilonTransitionCharacter, NFANodeIndexSet());
    addResult.iterator->value.add(to);
}

void NFA::addTransitionsOnAnyCharacter(unsigned from, unsigned to)
{
    ASSERT(from < m_nodes.size());
    ASSERT(to < m_nodes.size());

    NFANode& fromNode = m_nodes[from];
    fromNode.transitionsOnAnyCharacter.add(to);

    for (auto transitionSlot : fromNode.transitions)
        transitionSlot.value.remove(to);
}

void NFA::setFinal(unsigned node, uint64_t ruleId)
{
    if (!m_nodes[node].finalRuleIds.contains(ruleId))
        m_nodes[node].finalRuleIds.append(ruleId);
}

unsigned NFA::graphSize() const
{
    return m_nodes.size();
}

void NFA::restoreToGraphSize(unsigned size)
{
    ASSERT(size >= 1);
    ASSERT(size <= graphSize());

    m_nodes.shrink(size);
}

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING

void NFA::addRuleId(unsigned node, uint64_t ruleId)
{
    if (!m_nodes[node].ruleIds.contains(ruleId))
        m_nodes[node].ruleIds.append(ruleId);
}

static void printRange(bool firstRange, uint16_t rangeStart, uint16_t rangeEnd, uint16_t epsilonTransitionCharacter)
{
    if (!firstRange)
        dataLogF(", ");
    if (rangeStart == rangeEnd) {
        if (rangeStart == epsilonTransitionCharacter)
            dataLogF("ɛ");
        else if (rangeStart == '"' || rangeStart == '\\')
            dataLogF("\\%c", rangeStart);
        else if (rangeStart >= '!' && rangeStart <= '~')
            dataLogF("%c", rangeStart);
        else
            dataLogF("\\\\%d", rangeStart);
    } else {
        if (rangeStart == 1 && rangeEnd == 127)
            dataLogF("[any input]");
        else
            dataLogF("\\\\%d-\\\\%d", rangeStart, rangeEnd);
    }
}

static void printTransitions(const Vector<NFANode>& graph, unsigned sourceNode, uint16_t epsilonTransitionCharacter)
{
    const NFANode& node = graph[sourceNode];
    const HashMap<uint16_t, NFANodeIndexSet, DefaultHash<uint16_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint16_t>>& transitions = node.transitions;

    HashMap<unsigned, HashSet<uint16_t, DefaultHash<uint16_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint16_t>>, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> transitionsPerTarget;

    for (const auto& transition : transitions) {
        for (unsigned targetNode : transition.value) {
            transitionsPerTarget.add(targetNode, HashSet<uint16_t, DefaultHash<uint16_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint16_t>>());
            transitionsPerTarget.find(targetNode)->value.add(transition.key);
        }
    }

    for (const auto& transitionPerTarget : transitionsPerTarget) {
        dataLogF("        %d -> %d [label=\"", sourceNode, transitionPerTarget.key);

        Vector<uint16_t> incommingCharacters;
        copyToVector(transitionPerTarget.value, incommingCharacters);
        std::sort(incommingCharacters.begin(), incommingCharacters.end());

        uint16_t rangeStart = incommingCharacters.first();
        uint16_t rangeEnd = rangeStart;
        bool first = true;
        for (unsigned sortedTransitionIndex = 1; sortedTransitionIndex < incommingCharacters.size(); ++sortedTransitionIndex) {
            uint16_t nextChar = incommingCharacters[sortedTransitionIndex];
            if (nextChar == rangeEnd+1) {
                rangeEnd = nextChar;
                continue;
            }
            printRange(first, rangeStart, rangeEnd, epsilonTransitionCharacter);
            rangeStart = rangeEnd = nextChar;
            first = false;
        }
        printRange(first, rangeStart, rangeEnd, epsilonTransitionCharacter);

        dataLogF("\"];\n");
    }

    for (unsigned targetOnAnyCharacter : node.transitionsOnAnyCharacter)
        dataLogF("        %d -> %d [label=\"[any input]\"];\n", sourceNode, targetOnAnyCharacter);
}

void NFA::debugPrintDot() const
{
    dataLogF("digraph NFA_Transitions {\n");
    dataLogF("    rankdir=LR;\n");
    dataLogF("    node [shape=circle];\n");
    dataLogF("    {\n");
    for (unsigned i = 0; i < m_nodes.size(); ++i) {
        dataLogF("         %d [label=<Node %d", i, i);

        const Vector<uint64_t>& originalRules = m_nodes[i].ruleIds;
        if (!originalRules.isEmpty()) {
            dataLogF("<BR/>(Rules: ");
            for (unsigned ruleIndex = 0; ruleIndex < originalRules.size(); ++ruleIndex) {
                if (ruleIndex)
                    dataLogF(", ");
                dataLogF("%llu", originalRules[ruleIndex]);
            }
            dataLogF(")");
        }

        const Vector<uint64_t>& finalRules = m_nodes[i].finalRuleIds;
        if (!finalRules.isEmpty()) {
            dataLogF("<BR/>(Final: ");
            for (unsigned ruleIndex = 0; ruleIndex < finalRules.size(); ++ruleIndex) {
                if (ruleIndex)
                    dataLogF(", ");
                dataLogF("%llu", finalRules[ruleIndex]);
            }
            dataLogF(")");
        }

        dataLogF(">]");

        if (!finalRules.isEmpty())
            dataLogF(" [shape=doublecircle]");

        dataLogF(";\n");
    }
    dataLogF("    }\n");

    dataLogF("    {\n");
    for (unsigned i = 0; i < m_nodes.size(); ++i)
        printTransitions(m_nodes, i, epsilonTransitionCharacter);
    dataLogF("    }\n");
    dataLogF("}\n");
}
#endif

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
