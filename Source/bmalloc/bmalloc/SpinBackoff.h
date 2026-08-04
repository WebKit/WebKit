/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2026 Igalia S.L.
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

#ifdef __cplusplus

#include "BPlatform.h"

#if BOS(DARWIN)
#include <mach/mach_traps.h>
#include <mach/thread_switch.h>
#endif
#if BOS(WINDOWS)
#include <windows.h>
#endif
#if BOS(UNIX)
#include <time.h>
#endif

namespace bmalloc {

// Keep up to date with WTF::SpinBackoff
class SpinBackoff {
public:
    bool shouldParkAfterSpinOnce()
    {
        if (m_spinCount >= s_spinLimit)
            return true;
        spinOnce();
        return false;
    }

    void spinOnce()
    {
        unsigned step = m_spinCount;
        if (step < s_pauseSteps) {
            m_spinCount = step + 1;
            for (unsigned remaining = 1U << step; remaining--;)
                cpuPause();
            return;
        }
        if (step < s_spinLimit)
            m_spinCount = step + 1;
        yield();
    }

    static void yield()
    {
#if BOS(DARWIN)
        constexpr mach_msg_timeout_t timeoutInMS = 1;
        thread_switch(MACH_PORT_NULL, SWITCH_OPTION_DEPRESS, timeoutInMS);
#elif BOS(WINDOWS)
        // SwitchToThread only yields to threads on the same processor
        if (!SwitchToThread())
            Sleep(0);
#else
        struct timespec minimalSleep { 0, 1 };
        nanosleep(&minimalSleep, nullptr);
#endif
    }

private:
    static void cpuPause()
    {
#if BCPU(X86_64)
        __asm__ volatile("pause");
#elif BCPU(ARM64)
        __asm__ volatile("isb");
#endif
    }

#if BCPU(X86_64)
    static constexpr unsigned s_extraPauseSteps = 0;
#else
    static constexpr unsigned s_extraPauseSteps = 2;
#endif

#if BCPU(ARM64) && BPLATFORM(MAC)
    static constexpr unsigned s_pauseSteps = 9;
    static constexpr unsigned s_yieldSteps = 5;
#elif BCPU(ARM64) && BPLATFORM(IOS_FAMILY)
    static constexpr unsigned s_pauseSteps = 9;
    static constexpr unsigned s_yieldSteps = 10;
#elif BOS(DARWIN) || BOS(WINDOWS)
    static constexpr unsigned s_pauseSteps = 0;
    static constexpr unsigned s_yieldSteps = 40;
#else
    static constexpr unsigned s_pauseSteps = 6 + s_extraPauseSteps;
    static constexpr unsigned s_yieldSteps = 0;
#endif
    static constexpr unsigned s_spinLimit = s_pauseSteps + s_yieldSteps;

    unsigned m_spinCount { 0 };
};

} // namespace bmalloc

#endif // __cplusplus
