/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "Options.h"
#include <wtf/Atomics.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

// This class manages the tier up counts for Wasm binaries. The main interesting thing about
// wasm tiering up counts is that the least significant bit indicates if the tier up has already
// started. Also, wasm code does not atomically update this count. This is because we
// don't care too much if the countdown is slightly off. The tier up trigger is atomic, however,
// so tier up will be triggered exactly once.
class TierUpCount {
    WTF_MAKE_NONCOPYABLE(TierUpCount);
public:
    TierUpCount()
        : m_count(Options::webAssemblyOMGTierUpCount())
        , m_tierUpStarted(false)
    {
    }

    TierUpCount(TierUpCount&& other)
    {
        ASSERT(other.m_count == Options::webAssemblyOMGTierUpCount());
        m_count = other.m_count;
    }

    static uint32_t loopDecrement() { return Options::webAssemblyLoopDecrement(); }
    static uint32_t functionEntryDecrement() { return Options::webAssemblyFunctionEntryDecrement(); }

    bool shouldStartTierUp()
    {
        return !m_tierUpStarted.exchange(true);
    }

    int32_t count() { return bitwise_cast<int32_t>(m_count); }

private:
    uint32_t m_count;
    Atomic<bool> m_tierUpStarted;
};
    
} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
