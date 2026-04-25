/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/VM.h>

namespace JSC {

ALWAYS_INLINE VM& VMTraps::vm() const
{
    return *std::bit_cast<VM*>(std::bit_cast<uintptr_t>(this) - VM::offsetOfTraps());
}

inline void VMTraps::deferTermination(DeferAction deferAction)
{
    auto originalCount = m_deferTerminationCount++;
    ASSERT(m_deferTerminationCount < UINT_MAX);
    // Strictly speaking, we're only interested in vm.hasPendingTerminationException() here.
    // However, vm.exception() is a necessary condition for vm.hasPendingTerminationException().
    // Since this checks is intended to be cheap, we'll just do the cheaper check of vm.exception()
    // which itself rarely returns true. We'll let the slow path do the full
    // vm.hasPendingTerminationException() check instead.
    if (!originalCount && vm().exception()) [[unlikely]]
        deferTerminationSlow(deferAction);
}

inline void VMTraps::undoDeferTermination(DeferAction deferAction)
{
    ASSERT(m_deferTerminationCount > 0);
    ASSERT(!m_suspendedTerminationException || vm().hasTerminationRequest());
    if (!--m_deferTerminationCount && vm().hasTerminationRequest()) [[unlikely]]
        undoDeferTerminationSlow(deferAction);
}

ALWAYS_INLINE DeferTraps::DeferTraps(VM& vm)
    : m_traps(vm.traps())
    , m_previousTrapsDeferred(m_traps.m_trapsDeferred)
{
    m_traps.m_trapsDeferred = true;
}

ALWAYS_INLINE DeferTraps::~DeferTraps()
{
    m_traps.m_trapsDeferred = m_previousTrapsDeferred;
}

} // namespace JSC
