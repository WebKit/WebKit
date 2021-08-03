/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include <wtf/Lock.h>

#include <wtf/LockAlgorithmInlines.h>
#include <wtf/StackShotProfiler.h>

#if OS(WINDOWS)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace WTF {

static constexpr bool profileLockContention = false;

void Lock::lockSlow()
{
    if (profileLockContention)
        STACK_SHOT_PROFILE(4, 2, 5);

    // Heap allocations are forbidden on certain threads (e.g. audio rendering thread) for performance reasons so we need to
    // explicitly allow the following allocation(s). In some rare cases, the lockSlow() algorithm may cause allocations.
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    DefaultLockAlgorithm::lockSlow(m_byte);
}

void Lock::unlockSlow()
{
    // Heap allocations are forbidden on certain threads (e.g. audio rendering thread) for performance reasons so we need to
    // explicitly allow the following allocation(s). In some rare cases, the unlockSlow() algorithm may cause allocations.
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    DefaultLockAlgorithm::unlockSlow(m_byte, DefaultLockAlgorithm::Unfair);
}

void Lock::unlockFairlySlow()
{
    // Heap allocations are forbidden on certain threads (e.g. audio rendering thread) for performance reasons so we need to
    // explicitly allow the following allocation(s). In some rare cases, the unlockSlow() algorithm may cause allocations.
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    DefaultLockAlgorithm::unlockSlow(m_byte, DefaultLockAlgorithm::Fair);
}

void Lock::safepointSlow()
{
    DefaultLockAlgorithm::safepointSlow(m_byte);
}

bool Lock::tryLockWithTimeout(Seconds timeout)
{
    // This function may be called from a signal handler (e.g. via visit()). Hence,
    // it should only use APIs that are safe to call from signal handlers. This is
    // why we use unistd.h's sleep() instead of its alternatives.

    // We'll be doing sleep(1) between tries below. Hence, sleepPerRetry is 1.
    unsigned maxRetries = (timeout < Seconds::infinity()) ? timeout.value() : std::numeric_limits<unsigned>::max();
    unsigned tryCount = 0;
    while (!tryLock() && tryCount++ <= maxRetries) {
#if OS(WINDOWS)
        Sleep(1000);
#else
        ::sleep(1);
#endif
    }
    return isHeld();
}

} // namespace WTF

