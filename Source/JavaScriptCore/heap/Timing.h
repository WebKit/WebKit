/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Seconds.h>

namespace JSC {

class CPUTimingScope {
public:
    CPUTimingScope(const std::function<void(Seconds)>& notifier);
    CPUTimingScope(CPUTimingScope&& other);
    CPUTimingScope& operator=(CPUTimingScope&& other) noexcept;
    CPUTimingScope(const CPUTimingScope&) = delete;
    CPUTimingScope& operator=(const CPUTimingScope&) = delete;
    ~CPUTimingScope();
private:
    Seconds m_before;
    std::function<void(Seconds)> m_notifier;
    bool m_cancelled { false };
};

class ParallelCPUTimer {
public:
    CPUTimingScope synchronousScope();
    CPUTimingScope parallelMainScope();

    // This is the only method that should be called from parallel helper threads.
    CPUTimingScope parallelHelperScope();

    Seconds read();
    void clear();
private:
    void accumulateParallelThreads();
    void parallelDurationFetchMax(Seconds duration);

    Atomic<Seconds> m_parallelDuration { 0_s };
    Seconds m_duration { 0 };
    bool m_inSychronousThreadScope { false };
    bool m_inParrallelMainThreadScope { false };
};

} // namespace JSC
