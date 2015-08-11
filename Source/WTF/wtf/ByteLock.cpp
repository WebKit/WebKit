/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "ByteLock.h"

#include "DataLog.h"
#include "ParkingLot.h"
#include "StringPrintStream.h"
#include <thread>

namespace WTF {

static const bool verbose = false;

void ByteLock::lockSlow()
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
        ParkingLot::compareAndPark(&m_byte, isHeldBit | hasParkedBit);

        // We have awaken, or we never parked because the byte value changed. Either way, we loop
        // around and try again.
    }
}

void ByteLock::unlockSlow()
{
    // Release the lock while finding out if someone is parked. Note that as soon as we do this,
    // someone might barge in.
    uint8_t oldByteValue;
    for (;;) {
        oldByteValue = m_byte.load();
        if (verbose)
            dataLog(toString(currentThread(), ": unlocking with ", oldByteValue, "\n"));
        ASSERT(oldByteValue & isHeldBit);
        if (m_byte.compareExchangeWeak(oldByteValue, 0))
            break;
    }

    // Note that someone could try to park right now. If that happens, they will return immediately
    // because our parking predicate is that m_byte == isHeldBit | hasParkedBit, but we've already set
    // m_byte = 0.

    // If there had been threads parked, unpark all of them. This causes a thundering herd, but while
    // that is theoretically scary, it's totally fine in WebKit because we usually don't have enough
    // threads for this to matter.
    // FIXME: We don't really need this to exhibit thundering herd. We could use unparkOne(), and if
    // that returns true, just set the parked bit again. If in the process of setting the parked bit
    // we fail the CAS, then just unpark again.
    if (oldByteValue & hasParkedBit)
        ParkingLot::unparkAll(&m_byte);
}

} // namespace WTF

