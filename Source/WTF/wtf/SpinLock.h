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

#ifndef SpinLock_h
#define SpinLock_h

#include <thread>
#include <wtf/Atomics.h>
#include <wtf/Locker.h>

namespace WTF {

// SpinLockBase is a struct without an explicitly defined constructors so that
// it can be initialized at compile time. See StaticSpinLock below.
struct SpinLockBase {

    void lock()
    {
        while (!m_lock.compareExchangeWeak(0, 1, std::memory_order_acquire))
            std::this_thread::yield();
    }

    void unlock()
    {
        m_lock.store(0, std::memory_order_release);
    }

    bool isLocked() const
    {
        return m_lock.load(std::memory_order_acquire);
    }
    
    Atomic<unsigned> m_lock;
};

// SpinLock is for use as instance variables in structs and classes, not as
// statics and globals.
struct SpinLock : public SpinLockBase {
    SpinLock()
    {
        m_lock.store(0, std::memory_order_relaxed);
    }
};

// StaticSpinLock is for use as statics and globals, not as instance variables.
typedef SpinLockBase StaticSpinLock;
typedef Locker<SpinLockBase> SpinLockHolder;

} // namespace WTF

using WTF::StaticSpinLock;
using WTF::SpinLock;
using WTF::SpinLockHolder;

#endif // SpinLock_h
