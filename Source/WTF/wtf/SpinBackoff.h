/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#include <wtf/Threading.h>
#include <wtf/simde/simde.h>

namespace WTF {

// A caller that can park uses shouldParkAfterSpinOnce() and parks when it returns true:
//     SpinBackoff backoff;
//     for (;;) {
//         if (tryLock())
//             return;
//         if (backoff.shouldParkAfterSpinOnce())
//             park();
//     }
//
// A caller with nothing to park on uses spinOnce(), which yields for as long as it is called:
//     SpinBackoff backoff;
//     while (!tryLock())
//         backoff.spinOnce();

// The balancing act here lies in speeding up the "semi-contended" case
// without too strongly disadvantaging the "heavily-contended" case
// (the uncontended case of course never hits the spinloop).
// There are a few variables to consider:
//
//   * Time-to-park: the total CPU time of a full spinloop
//     Higher is better (to a point) for semi-contended,
//     but significantly worse for heavily-contended, as we pay
//     the full cost of the spinloop on ~every attempt to acquire.
//
//   * Niceness: (vaguely) how often we yield vs. run on core
//     Higher is better for heavily-contended, as it means that high-
//     priority threads will 'make room' for other threads as they
//     spin, rather than taking up high-priority CPU time on a spinloop.
//     However, it's worse for the semi-contended case, as when we
//     do acquire the spinlock the priority depression can last
//     for some time, meaning it could take a few quanta to get
//     back to 'full speed'.
//
//   * Poll-rate: the rate at which we read the atomic lock bit
//     This affects performance in two different ways.
//     The first is that, if the lock does become available, we
//     may be in the middle of a nop-spin, and therefore have to
//     execute the remaining nops before we check again.
//     Therefore, in the semi-contended case we want a higher frequency.
//     However, the higher the frequency, the more often we hammer the
//     lock's cache line. In sparse contention regimes this is relatively
//     OK: e.g. if there's only a single waiter, then the cache-line
//     stays local. With multiple waiters, however, then the line
//     can ping between cores, hurting performance.
//     Therefore, in the heavily-contended case it's better for this
//     to be lower.
//
// In general, the gains for the semi-contended case are modest, but
// show up across the board. On the flipside, hits to the heavily-
// contended case tend to be localized to a few scenarios, but have
// a very large effect-size; heavy contention is very rare
// (by design, from how WebKit uses locks), but very sensitive
// because spinlocks are poorly-adapted for that regime. E.g. on
// Darwin, omitting thread_switch entirely can more than
// double the runtime of certain benchmarks!
//
// N.b.: there are of course more considerations than just the above three.
// Fairness suffers as time-to-park increases, while all three can have
// deleterious effects on the rest of the system (e.g. scheduler churn,
// wasting memory bandwidth, etc.) depending on the details. But since
// those factors are harder to frame neatly I'm leaving them to this
// appendix.

// Keep this up to date with bmalloc/SpinBackoff.h.
class SpinBackoff {
public:
    // Takes one step, in which case it returns true
    // to say that the caller should park now.
    bool shouldParkAfterSpinOnce()
    {
        if (m_spinCount >= s_spinLimit)
            return true;
        spinOnce();
        return false;
    }

    // Takes one spin step, and keeps yielding once the budget is spent.
    void spinOnce()
    {
        unsigned step = m_spinCount;
        if (step < s_pauseSteps) {
            m_spinCount = step + 1;
            for (unsigned remaining = 1U << step; remaining--;)
                simde_mm_pause();
            return;
        }
        if (step < s_spinLimit)
            m_spinCount = step + 1;
        Thread::yield();
    }

private:
    // These are tuned empirically.
#if CPU(X86_64)
    static constexpr unsigned s_extraPauseSteps = 0;
#else
    static constexpr unsigned s_extraPauseSteps = 2;
#endif

// Tuned empirically.
#if CPU(ARM64) && OS(MACOS)
    static constexpr unsigned s_pauseSteps = 9;
    static constexpr unsigned s_yieldSteps = 5;
#elif CPU(ARM64) && OS(IOS_FAMILY)
    static constexpr unsigned s_pauseSteps = 9;
    static constexpr unsigned s_yieldSteps = 10;
#elif OS(DARWIN) || OS(WINDOWS)
    static constexpr unsigned s_pauseSteps = 0;
    static constexpr unsigned s_yieldSteps = 40;
#else
    static constexpr unsigned s_pauseSteps = 6 + s_extraPauseSteps;
    static constexpr unsigned s_yieldSteps = 0;
#endif
    static constexpr unsigned s_spinLimit = s_pauseSteps + s_yieldSteps;

    unsigned m_spinCount { 0 };
};

} // namespace WTF

using WTF::SpinBackoff;
