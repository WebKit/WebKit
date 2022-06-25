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

template <typename T> class DataMutexLocker;

template<typename T>
class DataMutex {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DataMutex);
public:
    template<typename ...Args>
    explicit DataMutex(Args&&... args)
        : m_data(std::forward<Args>(args)...)
    { }

private:
    friend class DataMutexLocker<T>;

    Lock m_mutex;
    T m_data WTF_GUARDED_BY_LOCK(m_mutex);
#if ENABLE_DATA_MUTEX_CHECKS
    Thread* m_currentMutexHolder { nullptr };
#endif
};

template <typename T>
class WTF_CAPABILITY_SCOPED_LOCK DataMutexLocker {
public:
    explicit DataMutexLocker(DataMutex<T>& dataMutex) WTF_ACQUIRES_LOCK(m_dataMutex.m_mutex)
        : m_dataMutex(dataMutex)
    {
        lock();
    }

    ~DataMutexLocker() WTF_RELEASES_LOCK()
    {
        if (m_isLocked) {
            unlock();
        }
    }

    T* operator->()
    {
        DATA_MUTEX_CHECK(m_isLocked && mutex().isHeld());
        assertIsHeld(m_dataMutex.m_mutex);
        return &m_dataMutex.m_data;
    }

    T& operator*()
    {
        DATA_MUTEX_CHECK(m_isLocked && mutex().isHeld());
        assertIsHeld(m_dataMutex.m_mutex);
        return m_dataMutex.m_data;
    }

    Lock& mutex() WTF_RETURNS_LOCK(m_dataMutex.m_mutex)
    {
        return m_dataMutex.m_mutex;
    }

    // Note: DataMutexLocker shouldn't be used after this. Due to limitations of clang thread safety analysis this can't
    // currently be staticly checked (adding WTF_REQUIRES_LOCK() to operator->() doesn't work.)
    // Run-time checks are still performed if enabled.
    void unlockEarly() WTF_RELEASES_LOCK(m_dataMutex.m_mutex)
    {
        assertIsHeld(m_dataMutex.m_mutex);
        unlock();
    }

    // Used to avoid excessive brace scoping when only small parts of the code need to be run unlocked.
    // Please be mindful that accessing the wrapped data from the callback is unsafe and will fail on assertions.
    // It's helpful to use a minimal lambda capture to be conscious of what data you're having access to in these sections.
    void runUnlocked(const Function<void()>& callback) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        unlock();
        callback();
        lock();
    }

private:
    DataMutex<T>& m_dataMutex;
    bool m_isLocked { false };

    void lock() WTF_ACQUIRES_LOCK(m_dataMutex.m_mutex)
    {
        DATA_MUTEX_CHECK(m_dataMutex.m_currentMutexHolder != &Thread::current()); // Thread attempted recursive lock on non-recursive lock.
        mutex().lock();
        m_isLocked = true;
#if ENABLE_DATA_MUTEX_CHECKS
        m_dataMutex.m_currentMutexHolder = &Thread::current();
#endif
    }

    void unlock() WTF_RELEASES_LOCK(m_dataMutex.m_mutex)
    {
        DATA_MUTEX_CHECK(mutex().isHeld());
        assertIsHeld(m_dataMutex.m_mutex);
#if ENABLE_DATA_MUTEX_CHECKS
        m_dataMutex.m_currentMutexHolder = nullptr;
#endif
        m_isLocked = false;
        mutex().unlock();
    }
};

} // namespace WTF

using WTF::DataMutex;
using WTF::DataMutexLocker;
