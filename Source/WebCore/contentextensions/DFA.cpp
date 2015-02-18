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

#include <wtf/DataLog.h>

namespace WebCore {

namespace ContentExtensions {

DFA::DFA()
    : m_root(0)
{
}

DFA::DFA(Vector<DFANode>&& nodes, unsigned rootIndex)
    : m_nodes(WTF::move(nodes))
    , m_root(rootIndex)
{
    ASSERT(rootIndex < m_nodes.size());
}

DFA::DFA(const DFA& dfa)
    : m_nodes(dfa.m_nodes)
    , m_root(dfa.m_root)
{
}

DFA& DFA::operator=(const DFA& dfa)
{
    m_nodes = dfa.m_nodes;
    m_root = dfa.m_root;
    return *this;
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

static void printTransitions(const Vector<DFANode>& graph, unsigned sourceNodeId)
{
    const DFANode& sourceNode = graph[sourceNodeId];
    const HashMap<uint16_t, unsigned>& transitions = sourceNode.transitions;

    if (transitions.isEmpty() && !sourceNode.hasFallbackTransition)
        return;

    HashMap<unsigned, Vector<uint16_t>, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> transitionsPerTarget;

    // First, we build the list of transitions coming to each target node.
    for (const auto& transition : transitions) {
        unsigned target = transition.value;
        transitionsPerTarget.add(target, Vector<uint16_t>());

        transitionsPerTarget.find(target)->value.append(transition.key);
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

    if (sourceNode.hasFallbackTransition)
        dataLogF("        %d -> %d [label=\"[fallback]\"];\n", sourceNodeId, sourceNode.fallbackTransition);
}

void DFA::debugPrintDot() const
{
    dataLogF("digraph DFA_Transitions {\n");
    dataLogF("    rankdir=LR;\n");
    dataLogF("    node [shape=circle];\n");
    dataLogF("    {\n");
    for (unsigned i = 0; i < m_nodes.size(); ++i) {
        dataLogF("         %d [label=<Node %d", i, i);
        const Vector<uint64_t>& actions = m_nodes[i].actions;
        if (!actions.isEmpty()) {
            dataLogF("<BR/>Actions: ");
            for (unsigned actionIndex = 0; actionIndex < actions.size(); ++actionIndex) {
                if (actionIndex)
                    dataLogF(", ");
                dataLogF("%llu", actions[actionIndex]);
            }
        }

        Vector<unsigned> correspondingNFANodes = m_nodes[i].correspondingNFANodes;
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
    for (unsigned i = 0; i < m_nodes.size(); ++i)
        printTransitions(m_nodes, i);

    dataLogF("    }\n");
    dataLogF("}\n");
}
#endif

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
