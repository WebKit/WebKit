/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include <wtf/ParkingLot.h>
#include <wtf/Threading.h>

namespace WTF {

// This magic number turns out to be optimal based on past JikesRVM experiments.
// Same value used by WTF::Lock.
static constexpr unsigned spinLimit = 40;

void ReadWriteLock::readLockSlow()
{
    unsigned spinCount = 0;

    for (;;) {
        uint32_t state = m_state.load(std::memory_order_relaxed);

        // If no writer is held and no writer is waiting, try to acquire read lock.
        if (!(state & (WriterHeldBit | WriterWaitingBit))) {
            if (m_state.compareExchangeWeak(state, state + ReaderCountUnit, std::memory_order_acquire))
                return;
            continue;
        }

        // Writer is active or waiting. Spin first if no parked readers and under spin limit.
        if (!(state & HasParkedReadersBit) && spinCount < spinLimit) {
            spinCount++;
            Thread::yield();
            continue;
        }

        // Set HasParkedReadersBit before parking.
        if (!(state & HasParkedReadersBit)) {
            if (!m_state.compareExchangeWeak(state, state | HasParkedReadersBit, std::memory_order_relaxed))
                continue;
            state |= HasParkedReadersBit;
        }

        // Park on the reader queue.
        ParkingLot::parkConditionally(
            readerParkingAddress(),
            [&]() -> bool {
                // Validate: still need to wait if writer is held or waiting.
                uint32_t currentState = m_state.load(std::memory_order_relaxed);
                return currentState & (WriterHeldBit | WriterWaitingBit);
            },
            []() { },
            ParkingLot::Time::infinity());

        // After waking, loop around and try again.
    }
}

void ReadWriteLock::readUnlockSlow()
{
    for (;;) {
        uint32_t state = m_state.load(std::memory_order_relaxed);
        ASSERT(state >> ReaderCountShift); // Must have at least one reader.

        uint32_t newState = state - ReaderCountUnit;
        uint32_t newReaderCount = newState >> ReaderCountShift;

        // If this is the last reader and there are parked writers, wake one.
        if (!newReaderCount && (state & HasParkedWritersBit)) {
            if (!m_state.compareExchangeWeak(state, newState, std::memory_order_release))
                continue;

            // Wake one writer.
            ParkingLot::unparkOne(
                writerParkingAddress(),
                [&](ParkingLot::UnparkResult result) -> intptr_t {
                    if (!result.mayHaveMoreThreads) {
                        // Clear HasParkedWritersBit since there are no more parked writers.
                        m_state.transactionRelaxed([](uint32_t& value) -> bool {
                            value &= ~HasParkedWritersBit;
                            return true;
                        });
                    }
                    return 0;
                });
            return;
        }

        // Not the last reader or no waiting writers, just decrement.
        if (m_state.compareExchangeWeak(state, newState, std::memory_order_release))
            return;
    }
}

void ReadWriteLock::writeLockSlow()
{
    unsigned spinCount = 0;
    bool setWaiting = false;

    for (;;) {
        uint32_t state = m_state.load(std::memory_order_relaxed);

        // If completely free (or only WriterWaitingBit is set by us), try to acquire.
        if (state == 0 || (setWaiting && state == WriterWaitingBit)) {
            uint32_t expected = setWaiting ? WriterWaitingBit : 0;
            if (m_state.compareExchangeWeak(expected, WriterHeldBit, std::memory_order_acquire))
                return;
            continue;
        }

        // Set WriterWaitingBit to block new readers (for fairness).
        if (!setWaiting && !(state & WriterWaitingBit)) {
            if (m_state.compareExchangeWeak(state, state | WriterWaitingBit, std::memory_order_relaxed)) {
                setWaiting = true;
                state |= WriterWaitingBit;
            }
            continue;
        }

        // Spin if under limit and no parked writers.
        if (!(state & HasParkedWritersBit) && spinCount < spinLimit) {
            spinCount++;
            Thread::yield();
            continue;
        }

        // Set HasParkedWritersBit before parking.
        if (!(state & HasParkedWritersBit)) {
            if (!m_state.compareExchangeWeak(state, state | HasParkedWritersBit, std::memory_order_relaxed))
                continue;
            state |= HasParkedWritersBit;
        }

        // Park on the writer queue.
        ParkingLot::ParkResult parkResult = ParkingLot::parkConditionally(
            writerParkingAddress(),
            [&]() -> bool {
                // Validate: still blocked if readers exist or writer is held.
                uint32_t currentState = m_state.load(std::memory_order_relaxed);
                return (currentState & ReaderCountMask) || (currentState & WriterHeldBit);
            },
            []() { },
            ParkingLot::Time::infinity());

        // Check for direct handoff.
        if (parkResult.wasUnparked && parkResult.token == DirectHandoff) {
            // The lock was handed directly to us.
            ASSERT(m_state.load(std::memory_order_relaxed) & WriterHeldBit);
            return;
        }

        // Otherwise, loop around and try again (barging opportunity).
    }
}

void ReadWriteLock::writeUnlockSlow()
{
    for (;;) {
        uint32_t state = m_state.load(std::memory_order_relaxed);
        ASSERT(state & WriterHeldBit);

        // Check for parked writers first (writer preference to avoid starvation).
        if (state & HasParkedWritersBit) {
            ParkingLot::unparkOne(
                writerParkingAddress(),
                [&](ParkingLot::UnparkResult result) -> intptr_t {
                    if (result.didUnparkThread && result.timeToBeFair) {
                        // Direct handoff: keep WriterHeldBit set, transfer lock to waiting writer.
                        if (!result.mayHaveMoreThreads) {
                            m_state.transactionRelaxed([](uint32_t& value) -> bool {
                                value &= ~HasParkedWritersBit;
                                return true;
                            });
                        }
                        return DirectHandoff;
                    }

                    // Barging opportunity: release the lock, unparked thread competes.
                    m_state.transactionRelaxed([&](uint32_t& value) -> bool {
                        value &= ~WriterHeldBit;
                        if (!result.mayHaveMoreThreads)
                            value &= ~HasParkedWritersBit;
                        return true;
                    });
                    return BargingOpportunity;
                });
            return;
        }

        // Check for parked readers.
        if (state & HasParkedReadersBit) {
            // Release the writer lock.
            uint32_t newState = (state & ~WriterHeldBit) & ~HasParkedReadersBit;
            if (!m_state.compareExchangeWeak(state, newState, std::memory_order_release))
                continue;

            // Wake ALL parked readers.
            ParkingLot::unparkAll(readerParkingAddress());
            return;
        }

        // No parked threads, just release the writer lock.
        uint32_t newState = state & ~WriterHeldBit;
        if (m_state.compareExchangeWeak(state, newState, std::memory_order_release))
            return;
    }
}

} // namespace WTF
