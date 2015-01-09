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

unsigned NFA::createNode(uint64_t ruleId)
{
    unsigned nextId = m_nodes.size();
    m_nodes.append(NFANode(ruleId));
    return nextId;
}

void NFA::addTransition(unsigned from, unsigned to, char character)
{
    ASSERT(from < m_nodes.size());
    ASSERT(to < m_nodes.size());
    ASSERT(character);

    auto addResult = m_nodes[from].transitions.add(character, HashSet<unsigned>());
    addResult.iterator->value.add(to);
}

void NFA::addEpsilonTransition(unsigned from, unsigned to)
{
    ASSERT(from < m_nodes.size());
    ASSERT(to < m_nodes.size());

    auto addResult = m_nodes[from].transitions.add(epsilonTransitionCharacter, HashSet<unsigned>());
    addResult.iterator->value.add(to);
}

void NFA::setFinal(unsigned node)
{
    ASSERT(node < m_nodes.size());
    m_nodes[node].isFinal = true;
}

#ifndef NDEBUG
void NFA::debugPrintDot() const
{
    dataLogF("digraph NFA_Transitions {\n");
    dataLogF("    rankdir=LR;\n");
    dataLogF("    node [shape=circle];\n");
    dataLogF("    {\n");
    for (unsigned i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].ruleId  == std::numeric_limits<uint64_t>::max())
            dataLogF("         %d [label=\"Node %d\"]", i, i);
        else
            dataLogF("         %d [label=<Node %d<BR/>(Rule %llu)>]", i, i, m_nodes[i].ruleId);

        if (m_nodes[i].isFinal)
            dataLogF(" [shape=doublecircle]");

        dataLogF(";\n");
    }
    dataLogF("    }\n");

    dataLogF("    {\n");
    for (unsigned i = 0; i < m_nodes.size(); ++i) {
        for (const auto& slot : m_nodes[i].transitions) {
            for (unsigned nextState : slot.value) {
                if (slot.key == epsilonTransitionCharacter)
                    dataLogF("        %d -> %d [label=\"ɛ\"];\n", i, nextState);
                else
                    dataLogF("        %d -> %d [label=\"%c\"];\n", i, nextState, slot.key);
            }
        }
    }
    dataLogF("    }\n");
    dataLogF("}\n");
}
#endif

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
