/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#include "DeferGC.h"
#include <wtf/Lock.h>
#include <wtf/NoLock.h>
#include <wtf/Optional.h>

namespace JSC {

using ConcurrentJSLock = Lock;

static_assert(sizeof(ConcurrentJSLock) == 1, "Regardless of status of concurrent JS flag, size of ConcurrentJSLock is always one byte.");

template<typename Lock>
class ConcurrentJSLockerBase : public AbstractLocker {
    WTF_MAKE_NONCOPYABLE(ConcurrentJSLockerBase);
public:
    explicit ConcurrentJSLockerBase(Lock& lockable)
        : m_locker(&lockable)
    {
    }
    explicit ConcurrentJSLockerBase(Lock* lockable)
        : m_locker(lockable)
    {
    }

    explicit ConcurrentJSLockerBase(NoLockingNecessaryTag)
        : m_locker(NoLockingNecessary)
    {
    }

    ~ConcurrentJSLockerBase()
    {
    }
    
    void unlockEarly()
    {
        m_locker.unlockEarly();
    }

private:
    Locker<Lock> m_locker;
};

template<typename Lock>
class GCSafeConcurrentJSLockerImpl : public ConcurrentJSLockerBase<Lock> {
public:
    GCSafeConcurrentJSLockerImpl(Lock& lockable, Heap& heap)
        : ConcurrentJSLockerBase<Lock>(lockable)
        , m_deferGC(heap)
    {
    }

    GCSafeConcurrentJSLockerImpl(Lock* lockable, Heap& heap)
        : ConcurrentJSLockerBase<Lock>(lockable)
        , m_deferGC(heap)
    {
    }

    ~GCSafeConcurrentJSLockerImpl()
    {
        // We have to unlock early due to the destruction order of base
        // vs. derived classes. If we didn't, then we would destroy the 
        // DeferGC object before unlocking the lock which could cause a GC
        // and resulting deadlock.
        ConcurrentJSLockerBase<Lock>::unlockEarly();
    }

private:
    DeferGC m_deferGC;
};

template<typename Lock>
class ConcurrentJSLockerImpl : public ConcurrentJSLockerBase<Lock> {
public:
    ConcurrentJSLockerImpl(Lock& lockable)
        : ConcurrentJSLockerBase<Lock>(lockable)
#if !defined(NDEBUG)
        , m_disallowGC(std::in_place)
#endif
    {
    }

    ConcurrentJSLockerImpl(Lock* lockable)
        : ConcurrentJSLockerBase<Lock>(lockable)
#if !defined(NDEBUG)
        , m_disallowGC(std::in_place)
#endif
    {
    }

    ConcurrentJSLockerImpl(NoLockingNecessaryTag)
        : ConcurrentJSLockerBase<Lock>(NoLockingNecessary)
#if !defined(NDEBUG)
        , m_disallowGC(WTF::nullopt)
#endif
    {
    }
    
    ConcurrentJSLockerImpl(int) = delete;

#if !defined(NDEBUG)
private:
    Optional<DisallowGC> m_disallowGC;
#endif
};

using ConcurrentJSLocker = ConcurrentJSLockerImpl<ConcurrentJSLock>;
using GCSafeConcurrentJSLocker = GCSafeConcurrentJSLockerImpl<ConcurrentJSLock>;

} // namespace JSC
