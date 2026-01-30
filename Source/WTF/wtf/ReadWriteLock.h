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
#include <wtf/Noncopyable.h>

namespace WTF {

// This is a high-performance read-write lock implementation that uses ParkingLot directly,
// following the same patterns as WTF::Lock. It enables concurrency between readers while
// providing efficient fast paths for uncontended operations.
//
// Compared to the traditional Lock+Condition implementation:
// - Fast paths use a single CAS operation (no internal lock acquisition)
// - No thundering herd: uses selective wakeup via unparkOne/unparkAll
// - Writer fairness: waiting writers block new readers to prevent starvation
//
// It's easiest to read lock like this:
//     Locker locker { rwLock.read() };
//
// It's easiest to write lock like this:
//     Locker locker { rwLock.write() };

class ReadWriteLock {
    WTF_MAKE_NONCOPYABLE(ReadWriteLock);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ReadWriteLock);
public:
    constexpr ReadWriteLock() = default;

    void readLock()
    {
        uint32_t state = m_state.load(std::memory_order_relaxed);
        if (!(state & (WriterHeldBit | WriterWaitingBit))) [[likely]] {
            if (m_state.compareExchangeWeak(state, state + ReaderCountUnit, std::memory_order_acquire)) [[likely]]
                return;
        }
        readLockSlow();
    }

    void readUnlock()
    {
        uint32_t state = m_state.load(std::memory_order_relaxed);
        uint32_t readerCount = state >> ReaderCountShift;
        if (readerCount > 1 && !(state & HasParkedWritersBit)) [[likely]] {
            if (m_state.compareExchangeWeak(state, state - ReaderCountUnit, std::memory_order_release)) [[likely]]
                return;
        }
        readUnlockSlow();
    }

    void writeLock()
    {
        if (m_state.compareExchangeWeak(0u, WriterHeldBit, std::memory_order_acquire)) [[likely]]
            return;
        writeLockSlow();
    }

    void writeUnlock()
    {
        if (m_state.compareExchangeWeak(WriterHeldBit, 0u, std::memory_order_release)) [[likely]]
            return;
        writeUnlockSlow();
    }

    class ReadLock;
    class WriteLock;

    ReadLock& read();
    WriteLock& write();

private:
    // State encoding in a 32-bit word:
    // Bits 31-16 (16 bits): Reader count (up to 65535 concurrent readers)
    // Bit      3:           Writer waiting bit (blocks new readers for fairness)
    // Bit      2:           Writer held bit
    // Bit      1:           Has parked writers bit
    // Bit      0:           Has parked readers bit
    static constexpr uint32_t ReaderCountShift = 16;
    static constexpr uint32_t ReaderCountUnit = 1u << ReaderCountShift;
    static constexpr uint32_t ReaderCountMask = 0xFFFF0000;
    static constexpr uint32_t WriterWaitingBit = 1u << 3;
    static constexpr uint32_t WriterHeldBit = 1u << 2;
    static constexpr uint32_t HasParkedWritersBit = 1u << 1;
    static constexpr uint32_t HasParkedReadersBit = 1u << 0;

    // Tokens for ParkingLot handoff (matching Lock pattern)
    enum Token : intptr_t {
        BargingOpportunity = 0,
        DirectHandoff = 1
    };

    WTF_EXPORT_PRIVATE NEVER_INLINE void readLockSlow();
    WTF_EXPORT_PRIVATE NEVER_INLINE void readUnlockSlow();
    WTF_EXPORT_PRIVATE NEVER_INLINE void writeLockSlow();
    WTF_EXPORT_PRIVATE NEVER_INLINE void writeUnlockSlow();

    // Separate parking addresses for readers and writers to enable selective wakeup.
    // ParkingLot uses the address as a key, so different addresses create separate queues.
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    void* readerParkingAddress() { return std::bit_cast<void*>(std::bit_cast<uint8_t*>(this)); }
    void* writerParkingAddress() { return std::bit_cast<void*>((std::bit_cast<uint8_t*>(this) + 1)); }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    Atomic<uint32_t> m_state { 0 };
};

class ReadWriteLock::ReadLock : public ReadWriteLock {
public:
    bool tryLock() { return false; }
    void lock() { readLock(); }
    void unlock() { readUnlock(); }
};

class ReadWriteLock::WriteLock : public ReadWriteLock {
public:
    bool tryLock() { return false; }
    void lock() { writeLock(); }
    void unlock() { writeUnlock(); }
};

inline ReadWriteLock::ReadLock& ReadWriteLock::read() { return *static_cast<ReadLock*>(this); }
inline ReadWriteLock::WriteLock& ReadWriteLock::write() { return *static_cast<WriteLock*>(this); }

} // namespace WTF

using WTF::ReadWriteLock;
