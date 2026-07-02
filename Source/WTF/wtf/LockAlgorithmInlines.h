/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#include <wtf/DataLog.h>
#include <wtf/LockAlgorithm.h>
#include <wtf/ParkingLot.h>
#include <wtf/Platform.h>
#include <wtf/Threading.h>
#include <wtf/simde/simde.h>

// It's a good idea to avoid including this header in too many places, so that it's possible to change
// the lock algorithm slow path without recompiling the world. Right now this should be included in two
// places (Lock.cpp and JSCell.cpp).

namespace WTF {

template<typename LockType, LockType isHeldBit, LockType hasParkedBit, typename Hooks>
void LockAlgorithm<LockType, isHeldBit, hasParkedBit, Hooks>::lockSlow(Atomic<LockType>& lock)
{
    // These values were selected empirically.
    // The balancing act here lies in speeding up the "semi-contended" case
    // without too strongly disadvantaging the "heavily-contended" case
    // (the uncontended case of course never hits the spinloop).
    // There are a few variables to consider:
    //
    //   * Time-to-park: the total CPU time of a full spinloop
    //     ~= spinLimit * (nopCount + 1/yieldInterval)
    //     Increases with spinLimit, nopCount; decreases with yieldInterval.
    //     Higher is better (to a point) for semi-contended,
    //     but significantly worse for heavily-contended, as we pay
    //     the full cost of the spinloop on ~every attempt to acquire.
    //
    //   * Niceness: (vaguely) how often we yield vs. run on core
    //     ~= 1 / (yieldInterval * nopCount)
    //     Higher is better for heavily-contended, as it means that high-
    //     priority threads will 'make room' for other threads as they
    //     spin, rather than taking up high-priority CPU time on a spinloop.
    //     However, it's worse for the semi-contended case, as when we
    //     do acquire the spinlock the priority depression can last
    //     for some time, meaning it could take a few quanta to get
    //     back to 'full speed'.
    //
    //   * Poll-rate: the rate at which we read the atomic lock bit
    //     ~= 1 / (nopCount + 1/yieldInterval)
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
    // because spinlocks are poorly-adapted for that regime. E.g.
    // omitting sched-yield entirely can more than double the runtime
    // of certain benchmarks!
    //
    // N.b.: there are of course more considerations than just the above three.
    // Fairness suffers as time-to-park increases, while all three can have
    // deleterious effects on the rest of the system (e.g. scheduler churn,
    // wasting memory bandwidth, etc.) depending on the details. But since
    // those factors are harder to frame neatly I'm leaving them to this
    // appendix.
#if CPU(ARM64) && OS(MACOS)
    static constexpr unsigned spinLimit = 80;
    static constexpr unsigned nopCount = 8;
    static constexpr unsigned yieldInterval = 16;
#elif CPU(ARM64) && OS(IOS_FAMILY)
    static constexpr unsigned spinLimit = 40;
    static constexpr unsigned nopCount = 16;
    static constexpr unsigned yieldInterval = 4;
#else
    static constexpr unsigned spinLimit = 40;
    // The tuning necessary to determine the optimal values
    // for other platforms has not yet been done, so we
    // retain the old sched-yield loop to avoid
    // possible regressions.
    static constexpr unsigned nopCount = 0;
    static constexpr unsigned yieldInterval = 1;
#endif
    
    unsigned spinCount = 0;
    
    for (;;) {
        LockType currentValue = lock.load();
        
        // We allow ourselves to barge in.
        if (!(currentValue & isHeldBit)) {
            if (lock.compareExchangeWeak(currentValue, Hooks::lockHook(currentValue | isHeldBit)))
                return;
            continue;
        }

        // If there is nobody parked and we haven't spun too much, we can just try to spin around.
        if (!(currentValue & hasParkedBit) && spinCount < spinLimit) {
            spinCount++;
            // It's important that we check this after incrementing,
            // as we want to avoid yielding for the first few spins.
            // This makes it more likely that we can acquire the lock
            // without having depressed our own priority beforehand.
            if (!(spinCount % yieldInterval))
                Thread::yield();
            for (unsigned i = 0; i < nopCount; i++)
                simde_mm_pause();
            continue;
        }

        // Need to park. We do this by setting the parked bit first, and then parking. We spin around
        // if the parked bit wasn't set and we failed at setting it.
        if (!(currentValue & hasParkedBit)) {
            LockType newValue = Hooks::parkHook(currentValue | hasParkedBit);
            if (!lock.compareExchangeWeak(currentValue, newValue))
                continue;
            currentValue = newValue;
        }
        
        if (!(currentValue & isHeldBit)) {
            dataLog("Lock not held!\n");
            CRASH_WITH_INFO(currentValue);
        }
        if (!(currentValue & hasParkedBit)) {
            dataLog("Lock not parked!\n");
            CRASH_WITH_INFO(currentValue);
        }

        // We now expect the value to be isHeld|hasParked. So long as that's the case, we can park.
        ParkingLot::ParkResult parkResult =
            ParkingLot::compareAndPark(&lock, currentValue);
        if (parkResult.wasUnparked) {
            switch (static_cast<Token>(parkResult.token)) {
            case DirectHandoff:
                // The lock was never released. It was handed to us directly by the thread that did
                // unlock(). This means we're done!
                RELEASE_ASSERT(isLocked(lock));
                return;
            case BargingOpportunity:
                // This is the common case. The thread that called unlock() has released the lock,
                // and we have been woken up so that we may get an opportunity to grab the lock. But
                // other threads may barge, so the best that we can do is loop around and try again.
                break;
            }
        }

        // We have awoken, or we never parked because the byte value changed. Either way, we loop
        // around and try again.
    }
}

template<typename LockType, LockType isHeldBit, LockType hasParkedBit, typename Hooks>
void LockAlgorithm<LockType, isHeldBit, hasParkedBit, Hooks>::unlockSlow(Atomic<LockType>& lock, Fairness fairness)
{
    // We could get here because the weak CAS in unlock() failed spuriously, or because there is
    // someone parked. So, we need a CAS loop: even if right now the lock is just held, it could
    // be held and parked if someone attempts to lock just as we are unlocking.
    for (;;) {
        uint8_t oldByteValue = lock.load();
        if ((oldByteValue & mask) != isHeldBit
            && (oldByteValue & mask) != (isHeldBit | hasParkedBit)) {
            dataLog("Invalid value for lock: ", oldByteValue, "\n");
            CRASH_WITH_INFO(oldByteValue);
        }
        
        if ((oldByteValue & mask) == isHeldBit) {
            if (lock.compareExchangeWeak(oldByteValue, Hooks::unlockHook(oldByteValue & ~isHeldBit)))
                return;
            continue;
        }

        // Someone is parked. Unpark exactly one thread. We may hand the lock to that thread
        // directly, or we will unlock the lock at the same time as we unpark to allow for barging.
        // When we unlock, we may leave the parked bit set if there is a chance that there are still
        // other threads parked.
        ASSERT((oldByteValue & mask) == (isHeldBit | hasParkedBit));
        ParkingLot::unparkOne(
            &lock,
            [&] (ParkingLot::UnparkResult result) -> intptr_t {
                // We are the only ones that can clear either the isHeldBit or the hasParkedBit,
                // so we should still see both bits set right now.
                ASSERT((lock.load() & mask) == (isHeldBit | hasParkedBit));
                
                if (result.didUnparkThread && (fairness == Fair || result.timeToBeFair)) {
                    // We don't unlock anything. Instead, we hand the lock to the thread that was
                    // waiting.
                    lock.transaction(
                        [&] (LockType& value) -> bool {
                            LockType newValue = Hooks::handoffHook(value);
                            if (newValue == value)
                                return false;
                            value = newValue;
                            return true;
                        });
                    return DirectHandoff;
                }
                
                lock.transaction(
                    [&] (LockType& value) -> bool {
                        value &= ~mask;
                        value = Hooks::unlockHook(value);
                        if (result.mayHaveMoreThreads)
                            value |= hasParkedBit;
                        return true;
                    });
                return BargingOpportunity;
            });
        return;
    }
}

} // namespace WTF

