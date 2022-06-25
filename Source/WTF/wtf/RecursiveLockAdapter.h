/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

#include <wtf/Lock.h>
#include <wtf/Threading.h>

namespace WTF {

template<typename LockType>
class RecursiveLockAdapter {
public:
    RecursiveLockAdapter() = default;

    // Use WTF_IGNORES_THREAD_SAFETY_ANALYSIS because the function does conditional locking, which is
    // not supported by analysis. Also RecursiveLockAdapter may wrap a lock type besides WTF::Lock
    // which doesn't support analysis.
    void lock() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        Thread& me = Thread::current();
        if (&me == m_owner) {
            m_recursionCount++;
            return;
        }
        
        m_lock.lock();
        ASSERT(!m_owner);
        ASSERT(!m_recursionCount);
        m_owner = &me;
        m_recursionCount = 1;
    }
    
    // Use WTF_IGNORES_THREAD_SAFETY_ANALYSIS because the function does conditional unlocking, which is
    // not supported by analysis. Also RecursiveLockAdapter may wrap a lock type besides WTF::Lock
    // which doesn't support analysis.
    void unlock() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        if (--m_recursionCount)
            return;
        m_owner = nullptr;
        m_lock.unlock();
    }
    
    // Use WTF_IGNORES_THREAD_SAFETY_ANALYSIS because the function does conditional locking, which is
    // not supported by analysis. Also RecursiveLockAdapter may wrap a lock type besides WTF::Lock
    // which doesn't support analysis.
    bool tryLock() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        Thread& me = Thread::current();
        if (&me == m_owner) {
            m_recursionCount++;
            return true;
        }
        
        if (!m_lock.tryLock())
            return false;
        
        ASSERT(!m_owner);
        ASSERT(!m_recursionCount);
        m_owner = &me;
        m_recursionCount = 1;
        return true;
    }
    
    bool isLocked() const
    {
        return m_lock.isLocked();
    }

    bool isOwner() const { return m_owner == &Thread::current(); }
    
private:
    Thread* m_owner { nullptr }; // Use Thread* instead of RefPtr<Thread> since m_owner thread is always alive while m_onwer is set.
    unsigned m_recursionCount { 0 };
    LockType m_lock;
};

using RecursiveLock = RecursiveLockAdapter<Lock>;

} // namespace WTF

using WTF::RecursiveLock;
