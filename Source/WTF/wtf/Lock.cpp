/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "Lock.h"

#include "DataLog.h"
#include "ParkingLot.h"
#include "StringPrintStream.h"
#include "ThreadingPrimitives.h"
#include <thread>

namespace WTF {

static const bool verbose = false;

NEVER_INLINE void LockBase::lockSlow()
{
    unsigned spinCount = 0;

    // This magic number turns out to be optimal based on past JikesRVM experiments.
    const unsigned spinLimit = 40;
    
    for (;;) {
        uint8_t currentByteValue = m_byte.load();
        if (verbose)
            dataLog(toString(currentThread(), ": locking with ", currentByteValue, "\n"));

        // We allow ourselves to barge in.
        if (!(currentByteValue & isHeldBit)
            && m_byte.compareExchangeWeak(currentByteValue, currentByteValue | isHeldBit))
            return;

        // If there is nobody parked and we haven't spun too much, we can just try to spin around.
        if (!(currentByteValue & hasParkedBit) && spinCount < spinLimit) {
            spinCount++;
            std::this_thread::yield();
            continue;
        }

        // Need to park. We do this by setting the parked bit first, and then parking. We spin around
        // if the parked bit wasn't set and we failed at setting it.
        if (!(currentByteValue & hasParkedBit)
            && !m_byte.compareExchangeWeak(currentByteValue, currentByteValue | hasParkedBit))
            continue;

        // We now expect the value to be isHeld|hasParked. So long as that's the case, we can park.
        ParkingLot::ParkResult parkResult =
            ParkingLot::compareAndPark(&m_byte, isHeldBit | hasParkedBit);
        if (parkResult.wasUnparked) {
            switch (static_cast<Token>(parkResult.token)) {
            case DirectHandoff:
                // The lock was never released. It was handed to us directly by the thread that did
                // unlock(). This means we're done!
                RELEASE_ASSERT(isHeld());
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

void LockBase::unlockSlow()
{
    unlockSlowImpl(Unfair);
}

void LockBase::unlockFairlySlow()
{
    unlockSlowImpl(Fair);
}

NEVER_INLINE void LockBase::unlockSlowImpl(Fairness fairness)
{
    // We could get here because the weak CAS in unlock() failed spuriously, or because there is
    // someone parked. So, we need a CAS loop: even if right now the lock is just held, it could
    // be held and parked if someone attempts to lock just as we are unlocking.
    for (;;) {
        uint8_t oldByteValue = m_byte.load();
        RELEASE_ASSERT(oldByteValue == isHeldBit || oldByteValue == (isHeldBit | hasParkedBit));
        
        if (oldByteValue == isHeldBit) {
            if (m_byte.compareExchangeWeak(isHeldBit, 0))
                return;
            continue;
        }

        // Someone is parked. Unpark exactly one thread. We may hand the lock to that thread
        // directly, or we will unlock the lock at the same time as we unpark to allow for barging.
        // When we unlock, we may leave the parked bit set if there is a chance that there are still
        // other threads parked.
        ASSERT(oldByteValue == (isHeldBit | hasParkedBit));
        ParkingLot::unparkOne(
            &m_byte,
            [&] (ParkingLot::UnparkResult result) -> intptr_t {
                // We are the only ones that can clear either the isHeldBit or the hasParkedBit,
                // so we should still see both bits set right now.
                ASSERT(m_byte.load() == (isHeldBit | hasParkedBit));
                
                if (result.didUnparkThread && (fairness == Fair || result.timeToBeFair)) {
                    // We don't unlock anything. Instead, we hand the lock to the thread that was
                    // waiting.
                    return DirectHandoff;
                }

                if (result.mayHaveMoreThreads)
                    m_byte.store(hasParkedBit);
                else
                    m_byte.store(0);
                return BargingOpportunity;
            });
        return;
    }
}

} // namespace WTF

