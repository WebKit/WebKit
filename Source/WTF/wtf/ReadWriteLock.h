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

#pragma once

#include <wtf/Atomics.h>
#include <wtf/Lock.h>
#include <wtf/Nonmovable.h>
#include <wtf/ThreadSafetyAnalysis.h>

namespace WTF {

class ReadLockView;
class WriteLockView;

// A phase-fair, eventually writer-fair, read-write lock, inspired by the PF-T algorithm from
// Spin-Based Reader-Writer Synchronization for Multiprocessor Real-Time Systems by Bjorn B.
// Brandenburg and James H. Anderson.
// See: https://www.cs.unc.edu/~anderson/papers/rtsj10-for-web.pdf,
//
// It's easiest to read lock like this:
//     Locker locker { rwLock.read() };
//
// It's easiest to write lock like this:
//     Locker locker { rwLock.write() };
//
// This lock is **NOT** recursive: taking a second read lock on a thread that already holds one
// deadlocks if a writer announces itself in between, and upgrading one of them deadlocks outright,
// since the upgrade then waits for a departure only the upgrading thread can make.
//
// Fairness is what we call eventually phase-fair:
//  - A reader waits only for the one writer whose phase it collided with. Writers queued behind that
//    one are waiting on m_writerLock and are invisible to readers, so reader cost does not grow with
//    the number of waiting writers.
//  - A writer waits only for the readers that were already inside when it announced itself. Readers
//    arriving afterwards queue behind it.
//  - Writers are eventually fair among themselves via the same mechanism as WTF::Lock.
//
// How does the lock work:
// Readers:
// The core of the read lock is implemented as two ticket counters. Each lock that enters the critical
// section conceptually takes an "in" ticket. These tickets are used by an active writer to ensure
// that all readers have left the critical section (discussed in the Writer section). When a reader leaves
// the critical section increments the "out" ticket count. Newly arriving readers wait for a writer before
// entering the read critical section, so there are two cohorts of readers at any time:
//  - Readers that are in the critical section already.
//  - Readers that are waiting on the current writer to leave.
//
// These cohorts are distinguished with a phase bit that alternates between each writer. This phase bit is
// embedded into the bottom of the "in" ticket along with two other bits:
//   - Phase id bit: the cohort of readers allowed into the critical section.
//   - Writer present bit: indicates that a writer is waiting for readers to exit
//   - Reader parked bit: indicates that a reader gave up spinning on the current writer and parked itself.
//
// Writers:
// Writers enter order themselves by acquiring the m_writerLock. Once they have the lock they begin the
// next phase by setting the writer present bit and flipping the phase id bit. Once they've flipped the
// bit all new incoming readers are gated behind the writer. The writer also records the "in" ticket
// count at the time it registered then waits for the "out" count to match. If readers take too long
// the writer will park, setting a bit on the out and handing-off notification responsibility to the
// last reader to exit.

class WTF_CAPABILITY_LOCK ReadWriteLock {
    WTF_MAKE_NONMOVABLE(ReadWriteLock);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ReadWriteLock);
public:
    constexpr ReadWriteLock() = default;

    void readLock() WTF_ACQUIRES_SHARED_LOCK()
    {
        uint32_t previous = m_readersIn.exchangeAdd(s_readerIncrement, std::memory_order_acquire);
        // We only have to worry about phases when there's a writer present on entry.
        if (previous & s_writerPresentBit) [[unlikely]]
            readLockSlow(previous & s_phaseFieldMask);
    }

    void readUnlock() WTF_RELEASES_SHARED_LOCK()
    {
        uint64_t previous = m_readersOut.exchangeAdd(s_outIncrement, std::memory_order_release);
        if (previous & s_writerDrainParkedBit) [[unlikely]] {
            // The parked bit and the drain target both live in the word we just incremented, so if our ticket
            // matches the drain target we're expected to wake the writer.
            uint64_t current = previous + s_outIncrement;
            if (outCount(current) == drainTarget(current))
                readUnlockSlow();
        }
    }

    // These both hold m_writerLock across the call boundary, which clang's thread-safety analysis
    // cannot express, so they opt out of it.
    void writeLock() WTF_ACQUIRES_LOCK() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        m_writerLock.lock();
        uint32_t target = beginPhase();
        if (outCount(m_readersOut.load(std::memory_order_acquire)) != target) [[unlikely]]
            writeLockSlow(target);
    }

    void writeUnlock() WTF_RELEASES_LOCK() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        endPhase();
        m_writerLock.unlock();
    }

    // WTF_RETURNS_LOCK declares that these denote the same capability as the lock itself, so that
    // data annotated WTF_GUARDED_BY_LOCK(theLock) is recognised as held inside
    // Locker { theLock.read() } as well as by theLock.readLock().
    ReadLockView& read() WTF_RETURNS_LOCK(*this);
    WriteLockView& write() WTF_RETURNS_LOCK(*this);

    bool hasWriterPresentForTesting() const
    {
        return m_readersIn.load(std::memory_order_relaxed) & s_writerPresentBit;
    }

    bool hasParkedBitsForTesting() const
    {
        return (m_readersIn.load(std::memory_order_relaxed) & s_hasParkedReadersBit)
            || (m_readersOut.load(std::memory_order_relaxed) & s_writerDrainParkedBit);
    }

    bool isQuiescentForTesting() const
    {
        uint32_t readersIn = m_readersIn.load(std::memory_order_relaxed);
        uint64_t readersOut = m_readersOut.load(std::memory_order_relaxed);
        return !(readersIn & s_writerPresentBit)
            && inCount(readersIn) == outCount(readersOut)
            && !m_writerLock.isHeld();
    }

private:
    friend class WriteLockView;

    static uint32_t inCount(uint32_t readersIn) { return (readersIn & s_readerCountMask) >> s_readerCountShift; }
    static uint32_t outCount(uint64_t readersOut) { return static_cast<uint32_t>(readersOut >> s_outCountShift); }
    static uint32_t drainTarget(uint64_t readersOut) { return static_cast<uint32_t>(readersOut & s_drainTargetMask); }

    uint32_t beginPhase() WTF_REQUIRES_LOCK(m_writerLock)
    {
        // s_writerPresentBit should be zero from when the last writer did endPhase() and we want to flip the value of s_phaseIdBit.
        // So this is equivalent to doing: m_readersIn |= s_writerPresentBit; m_readersIn ^= s_phaseIdBit, atomically.
        ASSERT_WITH_MESSAGE(!(m_readersIn.loadRelaxed() & s_writerPresentBit), "endPhase should have left s_writerPresentBit zero");
        return inCount(m_readersIn.exchangeXor(s_writerPresentBit | s_phaseIdBit, std::memory_order_acquire));
    }

    void endPhase() WTF_REQUIRES_LOCK(m_writerLock)
    {
        uint32_t readersIn = m_readersIn.exchangeAnd(~s_writerPresentBit, std::memory_order_release);
        ASSERT(readersIn & s_writerPresentBit);
        if (readersIn & s_hasParkedReadersBit) [[unlikely]]
            writeUnlockSlow();
    }

    WTF_EXPORT_PRIVATE NEVER_INLINE void readLockSlow(uint32_t observedPhase);
    WTF_EXPORT_PRIVATE NEVER_INLINE void readUnlockSlow();
    WTF_EXPORT_PRIVATE NEVER_INLINE void writeLockSlow(uint32_t target);
    WTF_EXPORT_PRIVATE NEVER_INLINE void writeUnlockSlow();

    // Distinct addresses because ParkingLot keys its queues by address; each is the word whose
    // changes that queue's waiters are watching for.
    const void* readerParkingAddress() const { return &m_readersIn; }
    const void* drainParkingAddress() const { return &m_readersOut; }

    //  31              8 7      3  2   1   0
    // +-----------------+--------+---+---+---+
    // | readers entered | unused | W | I | P |   m_readersIn
    // +-----------------+--------+---+---+---+
    //
    //  63            40 39    33 32  31    24 23           0
    // +----------------+--------+---+--------+--------------+
    // | readers exited | unused | D | unused | drain target |   m_readersOut
    // +----------------+--------+---+--------+--------------+
    //
    //   W  a writer owns the current phase          I  phase id
    //   P  readers are parked on the phase field    D  a writer is parked while draining
    //
    // Some notes on this layout:
    // - The drain target is part of the same atomic word as the readers left so both are
    //   synchronized together.
    //
    // - readers entered / exited are not counts but rather a rolling ticket. So overflow
    //   is expected as long as the total number of threads in the critical section doesn't
    //   overflow the count. This is also why the count is at the top of the fields so overflow
    //   doesn't touch any of the other bits.
    //
    // FIXME: the reader counts could fit in 13 bits, which would shrink the lock. At the price of a
    // 8192-thread ceiling on concurrent readers. This would make the m_readerIn/Out fields 16/32
    // bits, respectively.
    static constexpr uint32_t s_hasParkedReadersBit = 1u << 0;
    // These two bits should only be modified under the writerLock.
    static constexpr uint32_t s_phaseIdBit = 1u << 1;
    static constexpr uint32_t s_writerPresentBit = 1u << 2;
    static constexpr uint32_t s_phaseFieldMask = s_writerPresentBit | s_phaseIdBit;
    static constexpr uint32_t s_readerCountShift = 8;
    static constexpr uint32_t s_readerIncrement = 1u << s_readerCountShift;
    static constexpr uint32_t s_readerCountMask = ~static_cast<uint32_t>(0xFF);
    static constexpr uint64_t s_outCountShift = 40;
    static constexpr uint64_t s_outIncrement = 1ull << s_outCountShift;
    static constexpr uint64_t s_outCountMask = ~(s_outIncrement - 1);
    static constexpr uint64_t s_writerDrainParkedBit = 1ull << 32;
    static constexpr uint32_t s_countBits = 32 - s_readerCountShift;
    static constexpr uint64_t s_drainTargetMask = (1ull << s_countBits) - 1;
    // The drain wait is an equality test between the two counts, so they have to wrap together.
    static_assert(64 - s_outCountShift == s_countBits, "the reader counts must share a modulus.");

    // FIXME: We should consider letting the embedder nest a class in the lock so the reader words are on
    // separate cache lines. Giving m_readersIn and m_readersOut a line each measured about 50% more reader
    // throughput and half the write acquire latency under heavy contention, because a reader's acquire and
    // release stop contending for one line. If we do make such a change we likely want m_writerLock to be
    // on the readersIn line.
    Atomic<uint64_t> m_readersOut { 0 };
    Atomic<uint32_t> m_readersIn { 0 };
    // FIXME: We should just embed these two bits into m_readersIn. It's possible that one bit could be
    // used for both the s_writerPresentBit and WTF::Lock's lock held bit too.
    Lock m_writerLock;
};

class WTF_CAPABILITY_LOCK ReadLockView : public ReadWriteLock {
public:
    void lock() WTF_ACQUIRES_SHARED_LOCK() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { readLock(); }
    void unlock() WTF_RELEASES_SHARED_LOCK() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { readUnlock(); }
};

class WTF_CAPABILITY_LOCK WriteLockView : public ReadWriteLock {
public:
    void lock() WTF_ACQUIRES_LOCK() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { writeLock(); }
    void unlock() WTF_RELEASES_LOCK() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { writeUnlock(); }
    void assertIsOwner() const { m_writerLock.assertIsOwner(); }
};

inline ReadLockView& ReadWriteLock::read() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { return *static_cast<ReadLockView*>(this); }
inline WriteLockView& ReadWriteLock::write() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { return *static_cast<WriteLockView*>(this); }

} // namespace WTF

using WTF::ReadWriteLock;
