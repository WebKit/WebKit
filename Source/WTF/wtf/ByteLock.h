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

#ifndef ByteLock_h
#define ByteLock_h

#include <wtf/Atomics.h>
#include <wtf/Locker.h>
#include <wtf/Noncopyable.h>

namespace WTF {

// This is like Lock, but only requires 1 byte instead of sizeof(void*) storage. The downside is that
// it is slower on the slow path under heavy contention involving many threads because it is
// susceptible to the "thundering herd" problem: it doesn't have enough state to track how many
// threads are waiting for the lock, so when unlock detects that any thread is waiting, then it wakes
// all of the waiting threads. All of those threads will pile onto the lock; one will succeed and take
// it while the others will go back to sleep. The slow-down is small. Ten threads repeatedly
// contending for the same ~1-10us long critical section will run 7% slower on my machine. If I mess
// with any of the parameters - change the number of threads or change the critical section duration -
// then ByteLock reaches parity with Lock, and sometimes outperforms it. Perhaps most surprisingly,
// the slow-down goes away even when you increase the number of threads. ByteLock appears to sometimes
// be faster under microcontention, perhaps because a contentious 8-bit CAS is cheaper than a
// contentious 64-bit CAS. Note that uncontended locking is equally fast with ByteLock as with Lock.
// In all, Lock and ByteLock have similar performance under controlled microbenchmarks, but Lock is
// theoretically safer because of the thundering herd issue. It's probably best to use ByteLock only
// when you really care about space and you don't have reason to believe that a large number of
// threads would ever pile onto the same lock.

class ByteLock {
    WTF_MAKE_NONCOPYABLE(ByteLock);
public:
    ByteLock()
    {
        m_byte.store(0, std::memory_order_relaxed);
    }
    
    void lock()
    {
        if (LIKELY(m_byte.compareExchangeWeak(0, isHeldBit, std::memory_order_acquire))) {
            // Lock acquired!
            return;
        }

        lockSlow();
    }

    void unlock()
    {
        if (LIKELY(m_byte.compareExchangeWeak(isHeldBit, 0, std::memory_order_release))) {
            // Lock released and nobody was waiting!
            return;
        }

        unlockSlow();
    }

    bool isHeld() const
    {
        return m_byte.load(std::memory_order_acquire) & isHeldBit;
    }

    bool isLocked() const
    {
        return isHeld();
    }

private:
    static const uint8_t isHeldBit = 1;
    static const uint8_t hasParkedBit = 2;

    WTF_EXPORT_PRIVATE void lockSlow();
    WTF_EXPORT_PRIVATE void unlockSlow();

    Atomic<uint8_t> m_byte;
};

typedef Locker<ByteLock> ByteLocker;

} // namespace WTF

using WTF::ByteLock;
using WTF::ByteLocker;

#endif // ByteLock_h

