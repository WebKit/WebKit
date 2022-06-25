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

#include <mutex>
#include <wtf/LockAlgorithm.h>
#include <wtf/Locker.h>
#include <wtf/Noncopyable.h>
#include <wtf/Seconds.h>
#include <wtf/ThreadSafetyAnalysis.h>

namespace TestWebKitAPI {
struct LockInspector;
}

namespace WTF {

typedef LockAlgorithm<uint8_t, 1, 2> DefaultLockAlgorithm;

// This is a fully adaptive mutex that only requires 1 byte of storage. It has fast paths that are
// competetive to a spinlock (uncontended locking is inlined and is just a CAS, microcontention is
// handled by spinning and yielding), and a slow path that is competetive to std::mutex (if a lock
// cannot be acquired in a short period of time, the thread is put to sleep until the lock is
// available again). It uses less memory than a std::mutex. This lock guarantees eventual stochastic
// fairness, even in programs that relock the lock immediately after unlocking it. Except when there
// are collisions between this lock and other locks in the ParkingLot, this lock will guarantee that
// at worst one call to unlock() per millisecond will do a direct hand-off to the thread that is at
// the head of the queue. When there are collisions, each collision increases the fair unlock delay
// by one millisecond in the worst case.
//
// This lock type supports thread safety analysis.
// To annotate a member variable or a global variable with thread ownership information,
// use lock capability annotations defined in ThreadSafetyAnalysis.h.
class WTF_CAPABILITY_LOCK Lock {
    WTF_MAKE_NONCOPYABLE(Lock);
    WTF_MAKE_FAST_ALLOCATED;
public:
    constexpr Lock() = default;

    void lock() WTF_ACQUIRES_LOCK()
    {
        if (UNLIKELY(!DefaultLockAlgorithm::lockFastAssumingZero(m_byte)))
            lockSlow();
    }

    bool tryLock() WTF_ACQUIRES_LOCK_IF(true) // NOLINT: Intentional deviation to support std::scoped_lock.
    {
        return DefaultLockAlgorithm::tryLock(m_byte);
    }

    // Need this version for std::unique_lock.
    bool try_lock() WTF_ACQUIRES_LOCK_IF(true)
    {
        return tryLock();
    }

    WTF_EXPORT_PRIVATE bool tryLockWithTimeout(Seconds timeout) WTF_ACQUIRES_LOCK_IF(true);

    // Relinquish the lock. Either one of the threads that were waiting for the lock, or some other
    // thread that happens to be running, will be able to grab the lock. This bit of unfairness is
    // called barging, and we allow it because it maximizes throughput. However, we bound how unfair
    // barging can get by ensuring that every once in a while, when there is a thread waiting on the
    // lock, we hand the lock to that thread directly. Every time unlock() finds a thread waiting,
    // we check if the last time that we did a fair unlock was more than roughly 1ms ago; if so, we
    // unlock fairly. Fairness matters most for long critical sections, and this virtually
    // guarantees that long critical sections always get a fair lock.
    void unlock() WTF_RELEASES_LOCK()
    {
        if (UNLIKELY(!DefaultLockAlgorithm::unlockFastAssumingZero(m_byte)))
            unlockSlow();
    }

    // This is like unlock() but it guarantees that we unlock the lock fairly. For short critical
    // sections, this is much slower than unlock(). For long critical sections, unlock() will learn
    // to be fair anyway. However, if you plan to relock the lock right after unlocking and you want
    // to ensure that some other thread runs in the meantime, this is probably the function you
    // want.
    void unlockFairly() WTF_RELEASES_LOCK()
    {
        if (UNLIKELY(!DefaultLockAlgorithm::unlockFastAssumingZero(m_byte)))
            unlockFairlySlow();
    }
    
    void safepoint()
    {
        if (UNLIKELY(!DefaultLockAlgorithm::safepointFast(m_byte)))
            safepointSlow();
    }

    bool isHeld() const
    {
        return DefaultLockAlgorithm::isLocked(m_byte);
    }

    bool isLocked() const
    {
        return isHeld();
    }

private:
    friend struct TestWebKitAPI::LockInspector;
    
    static constexpr uint8_t isHeldBit = 1;
    static constexpr uint8_t hasParkedBit = 2;
    
    WTF_EXPORT_PRIVATE void lockSlow();
    WTF_EXPORT_PRIVATE void unlockSlow();
    WTF_EXPORT_PRIVATE void unlockFairlySlow();
    WTF_EXPORT_PRIVATE void safepointSlow();

    // Method used for testing only.
    bool isFullyReset() const
    {
        return !m_byte.load();
    }

    Atomic<uint8_t> m_byte { 0 };
};

// Asserts that the lock is held.
// This can be used in cases where the annotations cannot be added to the function
// declaration.
inline void assertIsHeld(const Lock& lock) WTF_ASSERTS_ACQUIRED_LOCK(lock) { ASSERT_UNUSED(lock, lock.isHeld()); }

// Locker specialization to use with Lock.
// Non-movable simple scoped lock holder.
// Example: Locker locker { m_lock };
template <>
class WTF_CAPABILITY_SCOPED_LOCK Locker<Lock> : public AbstractLocker {
public:
    explicit Locker(Lock& lock) WTF_ACQUIRES_LOCK(lock)
        : m_lock(lock)
        , m_isLocked(true)
    {
        m_lock.lock();
    }
    Locker(AdoptLockTag, Lock& lock) WTF_REQUIRES_LOCK(lock)
        : m_lock(lock)
        , m_isLocked(true)
    {
    }
    ~Locker() WTF_RELEASES_LOCK()
    {
        if (m_isLocked)
            m_lock.unlock();
    }
    void unlockEarly() WTF_RELEASES_LOCK()
    {
        ASSERT(m_isLocked);
        m_isLocked = false;
        m_lock.unlock();
    }
    Locker(const Locker<Lock>&) = delete;
    Locker& operator=(const Locker<Lock>&) = delete;

private:
    // Support DropLockForScope even though it doesn't support thread safety analysis.
    template<typename>
    friend class DropLockForScope;

    void lock() WTF_ACQUIRES_LOCK(m_lock)
    {
        m_lock.lock();
        compilerFence();
    }

    void unlock() WTF_RELEASES_LOCK(m_lock)
    {
        compilerFence();
        m_lock.unlock();
    }

    Lock& m_lock;
    bool m_isLocked { false };
};

Locker(Lock&) -> Locker<Lock>;
Locker(AdoptLockTag, Lock&) -> Locker<Lock>;

using LockHolder = Locker<Lock>;

} // namespace WTF

using WTF::Lock;
using WTF::LockHolder;
