/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/ThreadSafetyAnalysis.h>

namespace WTF {

// A lock type with support for thread safety analysis.
// To annotate a member variable or a global variable with thread ownership information,
// use lock capability annotations defined in ThreadSafetyAnalysis.h.
//
// Example:
//   class MyValue : public ThreadSafeRefCounted<MyValue>
//   {
//   public:
//       void setValue(int value) { Lochker holdLock { m_lock }; m_value = value;  }
//       void maybeSetOtherValue(int value)
//       {
//           if (!m_lock.tryLock())
//              return;
//           Locker locker { AdoptLockTag { }, m_otherLock };
//           m_otherValue = value;
//       }
//   private:
//       CheckedLock m_lock;
//       int m_value WTF_GUARDED_BY_LOCK(m_lock) { 77 };
//       int m_otherValue WTF_GUARDED_BY_LOCK(m_lock) { 88 };
//   };
// FIXME: Maybe should be folded back to Lock once holdLock is not used globally.
class WTF_CAPABILITY_LOCK CheckedLock : Lock {
    WTF_MAKE_NONCOPYABLE(CheckedLock);
    WTF_MAKE_FAST_ALLOCATED;
public:
    constexpr CheckedLock() = default;
    void lock() WTF_ACQUIRES_LOCK() { Lock::lock(); }
    bool tryLock() WTF_ACQUIRES_LOCK_IF(true) { return Lock::tryLock(); }
    bool try_lock() WTF_ACQUIRES_LOCK_IF(true) { return Lock::try_lock(); } // NOLINT: Intentional deviation to support std::scoped_lock.
    void unlock() WTF_RELEASES_LOCK() { Lock::unlock(); }
    void unlockFairly() WTF_RELEASES_LOCK() { Lock::unlockFairly(); }
    void safepoint() { Lock::safepoint(); }
    bool isHeld() const { return Lock::isHeld(); }
    bool isLocked() const { return Lock::isLocked(); }
    friend class CheckedCondition;
};

using AdoptLockTag = std::adopt_lock_t;

// Locker specialization to use with CheckedLock.
// Non-movable simple scoped lock holder.
// Example: Locker locker { m_lock };
template <>
class WTF_CAPABILITY_SCOPED_LOCK Locker<CheckedLock> {
public:
    explicit Locker(CheckedLock& lock) WTF_ACQUIRES_LOCK(lock)
        : m_lock(lock)
    {
        m_lock.lock();
    }
    Locker(AdoptLockTag, CheckedLock& lock) WTF_REQUIRES_LOCK(lock)
        : m_lock(lock)
    {
    }
    ~Locker() WTF_RELEASES_LOCK()
    {
        m_lock.unlock();
    }
    Locker(const Locker<CheckedLock>&) = delete;
    Locker& operator=(const Locker<CheckedLock>&) = delete;
private:
    CheckedLock& m_lock;
};

Locker(CheckedLock&) -> Locker<CheckedLock>;
Locker(AdoptLockTag, CheckedLock&) -> Locker<CheckedLock>;

} // namespace WTF

using WTF::CheckedLock;
using WTF::AdoptLockTag;
