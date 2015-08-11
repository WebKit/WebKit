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

#ifndef WTF_Lock_h
#define WTF_Lock_h

#include <wtf/Atomics.h>
#include <wtf/Compiler.h>
#include <wtf/Locker.h>
#include <wtf/Noncopyable.h>

namespace WTF {

// A Lock is a fully adaptive mutex that gives you the best of SpinLock and Mutex. For small critical
// sections (that take nanoseconds), it will usually perform within 2x of a SpinLock in both the
// contended and uncontended case. When using a Mutex, such critical sections take up to 100x longer
// than Lock in the contended case, or 3x longer than Lock in the uncontended case. For longer
// critical sections (that take tens of microseconds), it will perform as well as a Mutex and slightly
// better than a SpinLock. But, crucially, a SpinLock will burn up to 90x more time in the kernel for
// such critical sections than either Lock or Mutex. Hence, using Lock will make the common case of
// locking perform close to SpinLock for any critical section that does more than a few nanoseconds of
// work while being as kind to the scheduler for longer critical sections as a Mutex.
//
// Like SpinLock, Lock takes very little memory - just sizeof(void*), though see a detailed caveat
// below.
//
// Generally, you should use Lock instead of SpinLock because while it penalizes you slightly, you
// make up for it by not wasting CPU cycles in case of contention.
//
// The Lock has the following nice properties:
//
// - Uncontended fast paths for lock acquisition and lock release that are almost as fast as the
//   uncontended fast paths of a spinlock. The only overhead is that the spinlock will not CAS on
//   release, while Lock will CAS. This overhead *can* slow things down for extremely small critical
//   sections that do little or nothing - it makes them 2x slower since in that case, CAS is the most
//   expensive instruction and having two of them is twice as bad as just having one. However, this
//   lock implementation is still almost 3x faster than a platform mutex in those cases. It's unlikely
//   that you'll encounter no-op critical sections, so usually, this lock is better than a spinlock.
//
// - Contended fast path that attempts to spin and yield for some number of times. For critical
//   sections that are held only briefly, this allows Lock to perform almost as well as a SpinLock.
//   SpinLock can still be almost 2x faster than Lock if the critical section is a no-op, but this
//   advantage diminishes as the critical section grows.
//
// - Contended slow path that enqueues the contending thread and causes it to wait on a condition
//   variable until the lock is released. This is the only case in which system mutexes and condition
//   variables are used. This case is rare and self-limiting: it will only happen when a lock is held
//   for long enough that spinning some number of times doesn't acquire it. The fact that Lock does
//   this as a fallback when spinning for some number of times fails means that it will burn
//   dramatically fewer CPU cycles - for example with 10 threads on an 8 logical CPU machine acquiring
//   a critical section that takes 50 microseconds, the WTF SpinLock will cause 90x more time to be
//   spent in the kernel than Lock.
//
// - Very low memory usage. Each Lock requires only sizeof(void*) memory. When the contended slow
//   path is activated, Lock only relies on each thread having a preallocated thread-specific data
//   structure called ThreadData that, together with the Lock itself, is used to build up a thread
//   queue. So, the total memory usage of all Locks is still bounded by:
//
//       numberOfLocks * sizeof(void*) + numberOfThreads * sizeof(ThreadData)
//
//   Where ThreadData is a decently large data structure, but we will only ever have one per thread,
//   regardless of the number of Locks in memory. Another way to view this is that the worst case
//   memory usage per Lock is:
//
//       sizeof(void*) + numberOfThreads / numberOfLocks * sizeof(ThreadData)
//
//   So, unless you have a small number of Locks (or, a large number of threads, which is far less
//   likely), the memory usage per-Lock is still going to be somewhere around sizeof(void*).
//
// - Barging fast paths. The Lock is tuned for maximum throughput rather than maximum fairness. If
//   a thread releases a Lock that was contended and had a queue of waiting threads, then it will
//   wake up the head of the queue, but it will also mark the lock as being available. This means that
//   some other thread that is just now attempting to acquire the lock may get it before the thread
//   that got woken up. When a thread barges into the lock, the thread that got woken up will simply
//   go back to the end of the queue. The barging behavior ends up being probabilistic on most
//   platforms and even though it may be unfair to some thread at some moment in time, it will rarely
//   have a long streak of unfairness towards any particular thread: eventually each thread waiting on
//   the lock will get to have a turn so long as no thread just holds the lock forever. That said,
//   there *is* a chance of pathologies - users of Lock should not depend on first-in, first-out lock
//   acquisition order under contention. The same caveat is generally true of SpinLock and platform
//   mutexes on some platforms.

// FIXME: We could make this Lock more efficient by using ParkingLot and atomic add instead of CAS.
// The lock would be available if the 32-bit lock word <= 1. Locking would atomically add 2 to the
// word. The lock would be known to be held if the old value of the word was <= 1. The low bit
// just indicates if anyone is waiting. If the word was >= 2 after the atomic add, we would go to a
// slow path that repeatedly attempts to set the lock word to 3 [sic] from 0 or 1, and parks if that's
// not possible. The slow path could also perform defensive fixup that drops the lock value down to
// 3 if it was greater than 3, anytime that it needed to go to park. The unlock fast path would
// atomically subtract 2. If the decrement operation does not cause the count to zero, it would go to
// a slow path. The slow path would unpark one. If unparking returns false, the slow path would
// attempt a strong CAS from 1 to 0. It wouldn't do anything if the CAS fails, since the only goal of
// that CAS is to tell future unlock fast paths that there is possibly a thread parked. As such the
// states of the lock are:
//
//   0: lock available and nobody waiting
//   1: lock available and there may be threads waiting
//   2: lock taken and no threads waiting
// >=3: lock taken and threads waiting
//
// It may be possible to design an even better lock implementation based on atomic increments rather
// than atomic +=2/-=2.
//
// Note that because ParkingLot uses this lock internally, we would probably rename this lock
// implementation to something like BootstrapLock or even make it part of an anonymous namespace
// inside ParkingLot.
//
// https://bugs.webkit.org/show_bug.cgi?id=147841

// This is a struct without a constructor or destructor so that it can be statically initialized.
// Use Lock in instance variables.
struct LockBase {
    void lock()
    {
        if (LIKELY(m_word.compareExchangeWeak(0, isLockedBit, std::memory_order_acquire))) {
            // Lock acquired!
            return;
        }

        lockSlow();
    }

    void unlock()
    {
        if (LIKELY(m_word.compareExchangeWeak(isLockedBit, 0, std::memory_order_release))) {
            // Lock released, and nobody was waiting!
            return;
        }

        unlockSlow();
    }

    bool isHeld() const
    {
        return m_word.load(std::memory_order_acquire) & isLockedBit;
    }

    bool isLocked() const
    {
        return isHeld();
    }

protected:
    static const uintptr_t isLockedBit = 1;
    static const uintptr_t isQueueLockedBit = 2;
    static const uintptr_t queueHeadMask = 3;

    WTF_EXPORT_PRIVATE void lockSlow();
    WTF_EXPORT_PRIVATE void unlockSlow();

    Atomic<uintptr_t> m_word;
};

class Lock : public LockBase {
    WTF_MAKE_NONCOPYABLE(Lock);
public:
    Lock()
    {
        m_word.store(0, std::memory_order_relaxed);
    }
};

typedef LockBase StaticLock;
typedef Locker<LockBase> LockHolder;

} // namespace WTF

using WTF::StaticLock;
using WTF::Lock;
using WTF::LockHolder;

#endif // WTF_Lock_h

