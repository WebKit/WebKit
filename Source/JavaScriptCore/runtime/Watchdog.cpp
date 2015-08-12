/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Watchdog.h"

#include "CallFrame.h"
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>

namespace JSC {

const std::chrono::microseconds Watchdog::noTimeLimit = std::chrono::microseconds::max();

static std::chrono::microseconds currentWallClockTime()
{
    auto steadyTimeSinceEpoch = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(steadyTimeSinceEpoch);
}

Watchdog::Watchdog()
    : m_timerDidFire(false)
    , m_didFire(false)
    , m_timeLimit(noTimeLimit)
    , m_cpuDeadline(noTimeLimit)
    , m_wallClockDeadline(noTimeLimit)
    , m_callback(0)
    , m_callbackData1(0)
    , m_callbackData2(0)
    , m_timerQueue(WorkQueue::create("jsc.watchdog.queue", WorkQueue::Type::Serial, WorkQueue::QOS::Utility))
{
    m_timerHandler = [this] {
        this->m_timerDidFire = true;
        this->deref();
    };
}

inline bool Watchdog::hasStartedTimer()
{
    return m_cpuDeadline != noTimeLimit;
}

void Watchdog::setTimeLimit(VM& vm, std::chrono::microseconds limit,
    ShouldTerminateCallback callback, void* data1, void* data2)
{
    bool hadTimeLimit = hasTimeLimit();

    m_didFire = false; // Reset the watchdog.

    m_timeLimit = limit;
    m_callback = callback;
    m_callbackData1 = data1;
    m_callbackData2 = data2;

    // If this is the first time that time limit is being enabled, then any
    // previously JIT compiled code will not have the needed polling checks.
    // Hence, we need to flush all the pre-existing compiled code.
    //
    // However, if the time limit is already enabled, and we're just changing the
    // time limit value, then any existing JITted code will have the appropriate
    // polling checks. Hence, there is no need to re-do this flushing.
    if (!hadTimeLimit) {
        // And if we've previously compiled any functions, we need to revert
        // them because they don't have the needed polling checks yet.
        vm.discardAllCode();
    }

    if (m_hasEnteredVM && hasTimeLimit())
        startTimer(m_timeLimit);
}

bool Watchdog::didFireSlow(ExecState* exec)
{
    ASSERT(m_timerDidFire);
    m_timerDidFire = false;

    // A legitimate timer is the timer which woke up watchdog and can caused didFireSlow() to be
    // called after m_wallClockDeadline has expired. All other timers are considered to be stale,
    // and their wake ups are considered to be spurious and should be ignored.
    //
    // didFireSlow() will race against m_timerHandler to clear and set m_timerDidFire respectively.
    // We need to deal with a variety of scenarios where we can have stale timers and legitimate
    // timers firing in close proximity to each other.
    //
    // Consider the following scenarios:
    //
    // 1. A stale timer fires long before m_wallClockDeadline expires.
    //
    //    In this case, the m_wallClockDeadline check below will fail and the stale timer will be
    //    ignored as spurious. We just need to make sure that we clear m_timerDidFire before we
    //    check m_wallClockDeadline and return below.
    //
    // 2. A stale timer fires just before m_wallClockDeadline expires.
    //    Before the watchdog can gets to the clearing m_timerDidFire above, the legit timer also fires.
    //
    //    In this case, m_timerDidFire was set twice by the 2 timers, but we only clear need to clear
    //    it once. The m_wallClockDeadline below will pass and this invocation of didFireSlow() will
    //    be treated as the response to the legit timer. The spurious timer is effectively ignored.
    //
    // 3. A state timer fires just before m_wallClockDeadline expires.
    //    Right after the watchdog clears m_timerDidFire above, the legit timer also fires.
    //
    //    The fact that the legit timer fails means that the m_wallClockDeadline check will pass.
    //    As long as we clear m_timerDidFire before we do the check, we are safe. This is the same
    //    scenario as 2 above.
    //
    // 4. A stale timer fires just before m_wallClockDeadline expires.
    //    Right after we do the m_wallClockDeadline check below, the legit timer fires.
    //
    //    The fact that the legit timer fires after the m_wallClockDeadline check means that
    //    the m_wallClockDeadline check will have failed. This is the same scenario as 1 above.
    //
    // 5. A legit timer fires.
    //    A stale timer also fires before we clear m_timerDidFire above.
    //
    //    This is the same scenario as 2 above.
    //
    // 6. A legit timer fires.
    //    A stale timer fires right after we clear m_timerDidFire above.
    //
    //    In this case, the m_wallClockDeadline check will pass, and we fire the watchdog
    //    though m_timerDidFire remains set. This just means that didFireSlow() will be called again due
    //    to m_timerDidFire being set. The subsequent invocation of didFireSlow() will end up handling
    //    the stale timer and ignoring it. This is the same scenario as 1 above.
    //
    // 7. A legit timer fires.
    //    A stale timer fires right after we do the m_wallClockDeadline check.
    //
    //    This is the same as 6, which means it's the same as 1 above.
    //
    // In all these cases, we just need to ensure that we clear m_timerDidFire before we do the
    // m_wallClockDeadline check below. Hence, a storeLoad fence is needed to ensure that these 2
    // operations do not get re-ordered.

    WTF::storeLoadFence();

    if (currentWallClockTime() < m_wallClockDeadline)
        return false; // Just a stale timer firing. Nothing to do.

    m_wallClockDeadline = noTimeLimit;

    if (currentCPUTime() >= m_cpuDeadline) {
        // Case 1: the allowed CPU time has elapsed.

        // If m_callback is not set, then we terminate by default.
        // Else, we let m_callback decide if we should terminate or not.
        bool needsTermination = !m_callback
            || m_callback(exec, m_callbackData1, m_callbackData2);
        if (needsTermination) {
            m_didFire = true;
            return true;
        }

        // If we get here, then the callback above did not want to terminate execution. As a
        // result, the callback may have done one of the following:
        //   1. cleared the time limit (i.e. watchdog is disabled),
        //   2. set a new time limit via Watchdog::setTimeLimit(), or
        //   3. did nothing (i.e. allow another cycle of the current time limit).
        //
        // In the case of 1, we don't have to do anything.
        // In the case of 2, Watchdog::setTimeLimit() would already have started the timer.
        // In the case of 3, we need to re-start the timer here.

        ASSERT(m_hasEnteredVM);
        if (hasTimeLimit() && !hasStartedTimer())
            startTimer(m_timeLimit);

    } else {
        // Case 2: the allowed CPU time has NOT elapsed.
        auto remainingCPUTime = m_cpuDeadline - currentCPUTime();
        startTimer(remainingCPUTime);
    }

    return false;
}

bool Watchdog::hasTimeLimit()
{
    return (m_timeLimit != noTimeLimit);
}

void Watchdog::fire()
{
    m_didFire = true;
}

void Watchdog::enteredVM()
{
    m_hasEnteredVM = true;
    if (hasTimeLimit())
        startTimer(m_timeLimit);
}

void Watchdog::exitedVM()
{
    ASSERT(m_hasEnteredVM);
    stopTimer();
    m_hasEnteredVM = false;
}

void Watchdog::startTimer(std::chrono::microseconds timeLimit)
{
    ASSERT(m_hasEnteredVM);
    ASSERT(hasTimeLimit());

    m_cpuDeadline = currentCPUTime() + timeLimit;
    auto wallClockTime = currentWallClockTime();
    auto wallClockDeadline = wallClockTime + timeLimit;

    if ((wallClockTime < m_wallClockDeadline)
        && (m_wallClockDeadline <= wallClockDeadline)) {
        return; // Wait for the current active timer to expire before starting a new one.
    }

    // Else, the current active timer won't fire soon enough. So, start a new timer.
    this->ref(); // m_timerHandler will deref to match later.
    m_wallClockDeadline = wallClockDeadline;
    m_timerDidFire = false;

    // We clear m_timerDidFire because we're starting a new timer. However, we need to make sure
    // that the clearing occurs before the timer thread is started. Thereafter, only didFireSlow()
    // should clear m_timerDidFire (unless we start yet another timer). Hence, we need a storeStore
    // fence here to ensure these operations do not get re-ordered.
    WTF::storeStoreFence();

    m_timerQueue->dispatchAfter(std::chrono::nanoseconds(timeLimit), m_timerHandler);
}

void Watchdog::stopTimer()
{
    m_cpuDeadline = noTimeLimit;
}

} // namespace JSC
