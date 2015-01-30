/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef CallEdgeProfileInlines_h
#define CallEdgeProfileInlines_h

#include "CallEdgeProfile.h"

namespace JSC {

inline CallEdgeProfile::CallEdgeProfile()
    : m_closuresAreDespecified(false)
    , m_numCallsToPrimary(0)
    , m_numCallsToNotCell(0)
    , m_numCallsToUnknownCell(0)
    , m_totalCount(0)
    , m_primaryCallee(nullptr)
{
}

ALWAYS_INLINE void CallEdgeProfile::add(JSValue value, CallEdgeProfileVector& mergeBackLog)
{
    unsigned newTotalCount = m_totalCount + 1;
    if (UNLIKELY(!newTotalCount)) {
        fadeByHalf(); // Tackle overflows by dividing all counts by two.
        newTotalCount = m_totalCount + 1;
    }
    ASSERT(newTotalCount);
    m_totalCount = newTotalCount;
    
    if (UNLIKELY(!value.isCell())) {
        m_numCallsToNotCell++;
        return;
    }

    CallVariant callee = CallVariant(value.asCell());
    
    if (m_closuresAreDespecified)
        callee = callee.despecifiedClosure();
    
    if (UNLIKELY(!m_primaryCallee)) {
        m_primaryCallee = callee;
        m_numCallsToPrimary = 1;
        return;
    }
        
    if (LIKELY(m_primaryCallee == callee)) {
        m_numCallsToPrimary++;
        return;
    }
        
    if (UNLIKELY(!m_otherCallees)) {
        addSlow(callee, mergeBackLog);
        return;
    }
        
    CallSpectrum* secondary = m_otherCallees->m_temporarySpectrum.get();
    if (!secondary) {
        addSlow(callee, mergeBackLog);
        return;
    }
        
    secondary->add(callee);
}

} // namespace JSC

#endif // CallEdgeProfileInlines_h

