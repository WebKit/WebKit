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
#include "DFANode.h"

#include "DFA.h"

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {

namespace ContentExtensions {

Vector<uint64_t> DFANode::actions(const DFA& dfa) const
{
    // FIXME: Use iterators instead of copying the Vector elements.
    Vector<uint64_t> vector;
    vector.reserveInitialCapacity(m_actionsLength);
    for (uint32_t i = m_actionsStart; i < m_actionsStart + m_actionsLength; ++i)
        vector.uncheckedAppend(dfa.actions[i]);
    return vector;
}

bool DFANode::containsTransition(char transition, const DFA& dfa) const
{
    // Called from DFAMinimizer, this loops though a maximum of 128 transitions, so it's not too slow.
    ASSERT(m_transitionsLength <= 128);
    for (unsigned i = m_transitionsStart; i < m_transitionsStart + m_transitionsLength; ++i) {
        if (dfa.transitionRanges[i].first <= transition
            && dfa.transitionRanges[i].last >= transition)
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
        dfa.transitionRanges[i] = { -1, -1 };
        dfa.transitionDestinations[i] = std::numeric_limits<uint32_t>::max();
    }
    for (unsigned i = m_actionsStart; i < m_actionsStart + m_actionsLength; ++i)
        dfa.actions[i] = std::numeric_limits<uint64_t>::max();

    m_actionsStart = 0;
    m_actionsLength = 0;
    m_transitionsStart = 0;
    m_transitionsLength = 0;
};

bool DFANode::canUseFallbackTransition(const DFA& dfa) const
{
    // Transitions can contain '\0' if the expression has a end-of-line marker.
    // Fallback transitions cover 1-127. We have to be careful with the first.

    IterableConstRange iterableTransitions = transitions(dfa);
    auto iterator = iterableTransitions.begin();
    auto end = iterableTransitions.end();
    if (iterator == end)
        return false;

    char lastSeenCharacter = 0;
    if (!iterator.first()) {
        lastSeenCharacter = iterator.last();
        if (lastSeenCharacter == 127)
            return true;
        ++iterator;
    }

    for (;iterator != end; ++iterator) {
        ASSERT(iterator.first() > lastSeenCharacter);
        if (iterator.first() != lastSeenCharacter + 1)
            return false;

        if (iterator.range().last == 127)
            return true;
        lastSeenCharacter = iterator.last();
    }
    return false;
}

uint32_t DFANode::bestFallbackTarget(const DFA& dfa) const
{
    ASSERT(canUseFallbackTransition(dfa));

    HashMap<uint32_t, unsigned, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> histogram;

    IterableConstRange iterableTransitions = transitions(dfa);
    auto iterator = iterableTransitions.begin();
    auto end = iterableTransitions.end();
    ASSERT_WITH_MESSAGE(iterator != end, "An empty range list cannot use a fallback transition.");

    if (!iterator.first() && !iterator.last())
        ++iterator;
    ASSERT_WITH_MESSAGE(iterator != end, "An empty range list matching only zero cannot use a fallback transition.");

    uint32_t bestTarget = iterator.target();
    unsigned bestTargetScore = !iterator.range().first ? iterator.range().size() - 1 : iterator.range().size();
    histogram.add(bestTarget, bestTargetScore);
    ++iterator;

    for (;iterator != end; ++iterator) {
        unsigned rangeSize = iterator.range().size();
        uint32_t target = iterator.target();
        auto addResult = histogram.add(target, rangeSize);
        if (!addResult.isNewEntry)
            addResult.iterator->value += rangeSize;
        if (addResult.iterator->value > bestTargetScore) {
            bestTargetScore = addResult.iterator->value;
            bestTarget = target;
        }
    }
    return bestTarget;
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
