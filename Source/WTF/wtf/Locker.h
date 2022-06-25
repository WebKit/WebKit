/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <mutex>
#include <wtf/Assertions.h>
#include <wtf/Atomics.h>
#include <wtf/Compiler.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Noncopyable.h>
#include <wtf/ThreadSanitizerSupport.h>

namespace WTF {

enum NoLockingNecessaryTag { NoLockingNecessary };

class AbstractLocker {
    WTF_MAKE_NONCOPYABLE(AbstractLocker);
public:
    AbstractLocker(NoLockingNecessaryTag)
    {
    }
    
protected:
    AbstractLocker()
    {
    }
};

template<typename T> class DropLockForScope;

using AdoptLockTag = std::adopt_lock_t;
constexpr AdoptLockTag AdoptLock;

template<typename T>
class Locker : public AbstractLocker {
public:
    explicit Locker(T& lockable) : m_lockable(&lockable) { lock(); }
    explicit Locker(T* lockable) : m_lockable(lockable) { lock(); }
    explicit Locker(AdoptLockTag, T& lockable) : m_lockable(&lockable) { }

    // You should be wary of using this constructor. It's only applicable
    // in places where there is a locking protocol for a particular object
    // but it's not necessary to engage in that protocol yet. For example,
    // this often happens when an object is newly allocated and it can not
    // be accessed concurrently.
    Locker(NoLockingNecessaryTag) : m_lockable(nullptr) { }
    
    Locker(std::underlying_type_t<NoLockingNecessaryTag>) = delete;

    ~Locker()
    {
        unlock();
    }
    
    static Locker tryLock(T& lockable)
    {
        Locker result(NoLockingNecessary);
        if (lockable.tryLock()) {
            result.m_lockable = &lockable;
            return result;
        }
        return result;
    }

    T* lockable() { return m_lockable; }
    
    explicit operator bool() const { return !!m_lockable; }
    
    void unlockEarly()
    {
        unlock();
        m_lockable = nullptr;
    }
    
    // It's great to be able to pass lockers around. It enables custom locking adaptors like
    // JSC::LockDuringMarking.
    Locker(Locker&& other)
        : m_lockable(other.m_lockable)
    {
        ASSERT(&other != this);
        other.m_lockable = nullptr;
    }
    
    Locker& operator=(Locker&& other)
    {
        ASSERT(&other != this);
        m_lockable = other.m_lockable;
        other.m_lockable = nullptr;
        return *this;
    }
    
private:
    template<typename>
    friend class DropLockForScope;

    void unlock()
    {
        compilerFence();
        if (m_lockable) {
            TSAN_ANNOTATE_HAPPENS_BEFORE(m_lockable);
            m_lockable->unlock();
        }
    }

    void lock()
    {
        if (m_lockable) {
            m_lockable->lock();
            TSAN_ANNOTATE_HAPPENS_AFTER(m_lockable);
        }
        compilerFence();
    }
    
    T* m_lockable;
};

template<typename LockType>
class DropLockForScope : private AbstractLocker {
    WTF_FORBID_HEAP_ALLOCATION(DropLockForScope);
public:
    DropLockForScope(Locker<LockType>& lock)
        : m_lock(lock)
    {
        m_lock.unlock();
    }

    ~DropLockForScope()
    {
        m_lock.lock();
    }

private:
    Locker<LockType>& m_lock;
};

// This is a close replica of Locker, but for generic lock/unlock functions.
template<typename T, void (lockFunction)(T*), void (*unlockFunction)(T*)>
class ExternalLocker: public WTF::AbstractLocker {
public:
    explicit ExternalLocker(T* lockable)
        : m_lockable(lockable)
    {
        ASSERT(lockable);
        lock();
    }

    ~ExternalLocker()
    {
        unlock();
    }

    T* lockable() { return m_lockable; }

    explicit operator bool() const { return !!m_lockable; }

    void unlockEarly()
    {
        unlock();
        m_lockable = nullptr;
    }

    ExternalLocker(ExternalLocker&& other)
        : m_lockable(other.m_lockable)
    {
        ASSERT(&other != this);
        other.m_lockable = nullptr;
    }

    ExternalLocker& operator=(ExternalLocker&& other)
    {
        ASSERT(&other != this);
        m_lockable = other.m_lockable;
        other.m_lockable = nullptr;
        return *this;
    }

private:
    template<typename>
    friend class DropLockForScope;

    void unlock()
    {
        if (m_lockable)
            unlockFunction(m_lockable);
    }

    void lock()
    {
        if (m_lockable)
            lockFunction(m_lockable);
    }

    T* m_lockable;
};

}

using WTF::AbstractLocker;
using WTF::AdoptLock;
using WTF::Locker;
using WTF::NoLockingNecessaryTag;
using WTF::NoLockingNecessary;
using WTF::DropLockForScope;
using WTF::ExternalLocker;
