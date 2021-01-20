/*
 * Copyright (C) 2019 Igalia, S.L.
 * Copyright (C) 2019 Metrological Group B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <wtf/Lock.h>
#include <wtf/Threading.h>

namespace WTF {

// By default invalid access checks are only done in Debug builds.
#if !defined(ENABLE_DATA_MUTEX_CHECKS)
#if defined(NDEBUG)
#define ENABLE_DATA_MUTEX_CHECKS 0
#else
#define ENABLE_DATA_MUTEX_CHECKS 1
#endif
#endif

#if ENABLE_DATA_MUTEX_CHECKS
#define DATA_MUTEX_CHECK(expr) RELEASE_ASSERT(expr)
#else
#define DATA_MUTEX_CHECK(expr)
#endif

template<typename LockType>
class OwnerAwareLockAdapter {
public:
    void lock()
    {
        DATA_MUTEX_CHECK(m_owner != &Thread::current()); // Thread attempted recursive lock (unsupported).
        m_lock.lock();
#if ENABLE_DATA_MUTEX_CHECKS
        ASSERT(!m_owner);
        m_owner = &Thread::current();
#endif
    }

    void unlock()
    {
#if ENABLE_DATA_MUTEX_CHECKS
        m_owner = nullptr;
#endif
        m_lock.unlock();
    }

    bool tryLock()
    {
        DATA_MUTEX_CHECK(m_owner != &Thread::current()); // Thread attempted recursive lock (unsupported).
        if (!m_lock.tryLock())
            return false;

#if ENABLE_DATA_MUTEX_CHECKS
        ASSERT(!m_owner);
        m_owner = &Thread::current();
#endif
        return true;
    }

    bool isLocked() const
    {
        return m_lock.isLocked();
    }

private:
#if ENABLE_DATA_MUTEX_CHECKS
    Thread* m_owner { nullptr }; // Use Thread* instead of RefPtr<Thread> since m_owner thread is always alive while m_owner is set.
#endif
    LockType m_lock;
};

using OwnerAwareLock = OwnerAwareLockAdapter<Lock>;

template<typename T, typename LockType = OwnerAwareLock>
class DataMutex {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DataMutex);
public:
    template<typename ...Args>
    explicit DataMutex(Args&&... args)
        : m_data(std::forward<Args>(args)...)
    { }

    class LockedWrapper {
    public:
        explicit LockedWrapper(DataMutex& dataMutex)
            : m_mutex(dataMutex.m_mutex)
            , m_lockHolder(dataMutex.m_mutex)
            , m_data(dataMutex.m_data)
        { }

        T* operator->()
        {
            DATA_MUTEX_CHECK(m_mutex.isLocked());
            return &m_data;
        }

        T& operator*()
        {
            DATA_MUTEX_CHECK(m_mutex.isLocked());
            return m_data;
        }

        LockType& mutex()
        {
            return m_mutex;
        }

        Locker<LockType>& lockHolder()
        {
            return m_lockHolder;
        }

        // Used to avoid excessive brace scoping when only small parts of the code need to be run unlocked.
        // Please be mindful that accessing the wrapped data from the callback is unsafe and will fail on assertions.
        // It's helpful to use a minimal lambda capture to be conscious of what data you're having access to in these sections.
        void runUnlocked(WTF::Function<void()> callback)
        {
            m_mutex.unlock();
            callback();
            m_mutex.lock();
        }

    private:
        LockType& m_mutex;
        Locker<LockType> m_lockHolder;
        T& m_data;
    };

private:
    LockType m_mutex;
    T m_data;
};

} // namespace WTF
