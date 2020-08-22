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

DFA DFA::empty()
{
    DFA newDFA;
    newDFA.nodes.append(DFANode());
    return newDFA;
}

size_t DFA::memoryUsed() const
{
    return sizeof(DFA)
        + actions.capacity() * sizeof(uint64_t)
        + transitionRanges.capacity() * sizeof(CharRange)
        + transitionDestinations.capacity() * sizeof(uint32_t)
        + nodes.capacity() * sizeof(DFANode);
}

void DFA::shrinkToFit()
{
    nodes.shrinkToFit();
    actions.shrinkToFit();
    transitionRanges.shrinkToFit();
    transitionDestinations.shrinkToFit();
}

void DFA::minimize()
{
    DFAMinimizer::minimize(*this);
}

unsigned DFA::graphSize() const
{
    unsigned count = 0;
    for (const DFANode& node : nodes) {
        if (!node.isKilled())
            ++count;
    }
    return count;
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

    if (transitions.begin() == transitions.end())
        return;

    HashMap<unsigned, Vector<uint16_t>, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> transitionsPerTarget;

    // First, we build the list of transitions coming to each target node.
    for (const auto& transition : transitions) {
        unsigned target = transition.target();
        transitionsPerTarget.add(target, Vector<uint16_t>());

        for (unsigned offset = 0; offset < transition.range().size(); ++offset)
            transitionsPerTarget.find(target)->value.append(transition.first() + offset);
    }

    // Then we go over each one an display the ranges one by one.
    for (const auto& transitionPerTarget : transitionsPerTarget) {
        dataLogF("        %d -> %d [label=\"", sourceNodeId, transitionPerTarget.key);

        Vector<uint16_t> incomingCharacters = transitionPerTarget.value;
        std::sort(incomingCharacters.begin(), incomingCharacters.end());

        char rangeStart = incomingCharacters.first();
        char rangeEnd = rangeStart;
        bool first = true;
        for (unsigned sortedTransitionIndex = 1; sortedTransitionIndex < incomingCharacters.size(); ++sortedTransitionIndex) {
            char nextChar = incomingCharacters[sortedTransitionIndex];
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
                dataLogF("%" PRIu64, actions[actionIndex]);
            }
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
