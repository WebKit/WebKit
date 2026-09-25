/*
 * Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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
#include <wtf/ReadWriteLock.h>

#include <limits>
#include <wtf/ParkingLot.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#include <wtf/simde/simde.h>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

namespace WTF {

namespace ReadWriteLockInternal {

// See SpinBackoff.h for an explanation of these values.
// Microbenchmarking didn't detect any benefit from yielding in the common case and it seemed to
// regress performance in the oversubscribed case, so we don't bother yielding for now. We could /
// should reconsider if we ever find evidence to the contrary. Exponential backoff, like SpinBackoff
// does, also doesn't seem to help here.
constexpr unsigned readerSpinLimit = 640;
constexpr unsigned writerSpinLimit = 640;

// One iteration of a spin loop. Returns false once the caller has spun long enough and should park.
ALWAYS_INLINE bool spinStep(unsigned& spinCount, unsigned limit)
{
    if (spinCount >= limit)
        return false;
    ++spinCount;
    simde_mm_pause();
    return true;
}

} // namespace ReadWriteLockInternal

void ReadWriteLock::readLockSlow(uint32_t observedPhase)
{
    unsigned spinCount = 0;
    for (;;) {
        uint32_t readersIn = m_readersIn.load(std::memory_order_acquire);
        // Note: If we get to this line from unparking the s_phaseFieldMask bits will be different and we'll still
        // exit this loop and enter the critical section. Either there's no new incoming writer present so the phase id
        // is the same but s_writerPresentBit is different, or there is a new writer but the phase id will have flipped.
        if ((readersIn & s_phaseFieldMask) != observedPhase)
            return;

        if (ReadWriteLockInternal::spinStep(spinCount, ReadWriteLockInternal::readerSpinLimit))
            continue;

        m_readersIn.exchangeOr(s_hasParkedReadersBit, std::memory_order_relaxed);

        ParkingLot::parkConditionally(
            readerParkingAddress(),
            [&]() -> bool {
                uint32_t currentReadersIn = m_readersIn.load(std::memory_order_relaxed);
                return (currentReadersIn & s_phaseFieldMask) == observedPhase && (currentReadersIn & s_hasParkedReadersBit);
            },
            []() { },
            ParkingLot::Time::infinity());
    }
}

void ReadWriteLock::readUnlockSlow()
{
    // We were the reader the draining writer was waiting for.
    ParkingLot::unparkOne(
        drainParkingAddress(),
        [&](ParkingLot::UnparkResult) -> intptr_t {
            // Only one writer can be draining, so once the queue is empty the bit is no longer needed.
            // mayHaveMoreThreads is bucket-granular and would only leave the bit stale here.
            m_readersOut.exchangeAnd(~s_writerDrainParkedBit, std::memory_order_relaxed);
            return 0;
        });
}

void ReadWriteLock::writeLockSlow(uint32_t target)
{
    // We own the phase, so no more readers can enter the critical section; wait for the ones already counted to leave.
    unsigned spinCount = 0;
    for (;;) {
        uint64_t readersOut = m_readersOut.load(std::memory_order_acquire);
        if (outCount(readersOut) == target)
            return;

        if (ReadWriteLockInternal::spinStep(spinCount, ReadWriteLockInternal::writerSpinLimit))
            continue;

        // It's ok that we set the parked bit before actually parking. We end up synchronizing with that
        // reader under the ParkingLot's lock. If they win we'll abort the park or we'll win and actually
        // park.
        uint64_t parked = (readersOut & s_outCountMask) | s_writerDrainParkedBit | target;
        if (m_readersOut.compareExchangeStrong(readersOut, parked, std::memory_order_relaxed) != readersOut)
            continue;

        ParkingLot::parkConditionally(
            drainParkingAddress(),
            [&]() -> bool {
                uint64_t currentReadersOut = m_readersOut.loadRelaxed();
                return outCount(currentReadersOut) != target && (currentReadersOut & s_writerDrainParkedBit);
            },
            []() { },
            ParkingLot::Time::infinity());
    }
}

void ReadWriteLock::writeUnlockSlow()
{
    // Our phase is already over, so every one of these readers can proceed; none of them has to
    // re-examine anything but the phase field, which we have already changed.
    ParkingLot::unparkCount(
        readerParkingAddress(), std::numeric_limits<unsigned>::max(),
        [&](ParkingLot::UnparkResult) -> intptr_t {
            // We asked for every waiter at this address, so the queue is drained. mayHaveMoreThreads
            // is bucket-granular and would only leave the bit stale here.
            m_readersIn.exchangeAnd(~s_hasParkedReadersBit, std::memory_order_relaxed);
            return 0;
        });
}

} // namespace WTF
