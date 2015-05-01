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

#ifndef DFANode_h
#define DFANode_h

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionsDebugging.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace ContentExtensions {

struct DFA;

// A DFANode abstract the transition table out of a DFA state. If a state is accepting, the DFANode also have
// the actions for that state.
class DFANode {
public:
    // FIXME: Stop minimizing killed nodes and add ASSERT(!isKilled()) in many functions here.
    void kill(DFA&);
    Vector<uint64_t> actions(const DFA&) const;
    Vector<std::pair<uint8_t, uint32_t>> transitions(const DFA&) const;
    uint32_t fallbackTransitionDestination(const DFA&) const;
    void addFallbackTransition(DFA&, uint32_t destination);
    bool containsTransition(uint8_t, DFA&);
    void changeFallbackTransition(DFA&, uint32_t newDestination);
    
    bool isKilled() const { return m_flags & IsKilled; }
    bool hasFallbackTransition() const { return m_flags & HasFallbackTransition; }
    bool hasActions() const { return !!m_actionsLength; }
    uint8_t transitionsLength() const { return m_transitionsLength; }
    uint16_t actionsLength() const { return m_actionsLength; }
    uint32_t actionsStart() const { return m_actionsStart; }
    uint32_t transitionsStart() const { return m_transitionsStart; }
    
    void setActions(uint32_t start, uint16_t length)
    {
        ASSERT(!m_actionsStart);
        ASSERT(!m_actionsLength);
        m_actionsStart = start;
        m_actionsLength = length;
    }
    void setTransitions(uint32_t start, uint16_t length)
    {
        ASSERT(!m_transitionsStart);
        ASSERT(!m_transitionsLength);
        m_transitionsStart = start;
        m_transitionsLength = length;
    }
    void resetTransitions(uint32_t start, uint16_t length)
    {
        m_transitionsStart = start;
        m_transitionsLength = length;
    }
    void setHasFallbackTransitionWithoutChangingDFA(bool shouldHaveFallbackTransition)
    {
        if (shouldHaveFallbackTransition)
            m_flags |= HasFallbackTransition;
        else
            m_flags &= ~HasFallbackTransition;
    }
    
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
    Vector<unsigned> correspondingNFANodes;
#endif
private:
    uint32_t m_actionsStart { 0 };
    uint32_t m_transitionsStart { 0 };
    uint16_t m_actionsLength { 0 };
    uint8_t m_transitionsLength { 0 };
    
    uint8_t m_flags { 0 };
    const uint8_t HasFallbackTransition = 0x01;
    const uint8_t IsKilled = 0x02;
};

// FIXME: Pack this down to 12.
// It's probably already 12 on ARMv7.
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
COMPILE_ASSERT(sizeof(DFANode) <= 16 + sizeof(Vector<unsigned>), Keep the DFANodes small);
#else
COMPILE_ASSERT(sizeof(DFANode) <= 16, Keep the DFANodes small);
#endif

}

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)

#endif // DFANode_h
