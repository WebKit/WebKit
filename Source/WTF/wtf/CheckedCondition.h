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

#include <wtf/CheckedLock.h>
#include <wtf/Condition.h>

namespace WTF {

// A condition variable type for CheckedLock.
class CheckedCondition final {
    WTF_MAKE_NONCOPYABLE(CheckedCondition);
    WTF_MAKE_FAST_ALLOCATED;
public:
    constexpr CheckedCondition() = default;

    bool waitUntil(CheckedLock& lock, const TimeWithDynamicClockType& timeout) WTF_REQUIRES_LOCK(lock)
    {
        return m_condition.waitUntil(uncheckedCast(lock), timeout);
    }
    template<typename Functor>
    bool waitUntil(CheckedLock& lock, const TimeWithDynamicClockType& timeout, const Functor& predicate) WTF_REQUIRES_LOCK(lock)
    {
        return m_condition.waitUntil(uncheckedCast(lock), timeout, predicate);
    }
    template<typename Functor>
    bool waitFor(CheckedLock& lock, Seconds relativeTimeout, const Functor& predicate) WTF_REQUIRES_LOCK(lock)
    {
        return m_condition.waitFor(uncheckedCast(lock), relativeTimeout, predicate);
    }
    bool waitFor(CheckedLock& lock, Seconds relativeTimeout) WTF_REQUIRES_LOCK(lock)
    {
        return m_condition.waitFor(uncheckedCast(lock), relativeTimeout);
    }
    void wait(CheckedLock& lock) WTF_REQUIRES_LOCK(lock)
    {
        return m_condition.wait(uncheckedCast(lock));
    }
    template<typename Functor>
    void wait(CheckedLock& lock, const Functor& predicate) WTF_REQUIRES_LOCK(lock)
    {
        m_condition.wait(uncheckedCast(lock), predicate);
    }
    bool notifyOne()
    {
        return m_condition.notifyOne();
    }
    void notifyAll()
    {
        m_condition.notifyAll();
    }
private:
    static Lock& uncheckedCast(CheckedLock& lock) { return static_cast<Lock&>(lock); }
    Condition m_condition;
};

} // namespace WTF

using WTF::CheckedCondition;
