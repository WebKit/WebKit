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
};

template <typename T>
class WTF_CAPABILITY_SCOPED_LOCK DataMutexLocker {
public:
    explicit DataMutexLocker(DataMutex<T>& dataMutex) WTF_ACQUIRES_LOCK(m_dataMutex.m_mutex)
        : m_dataMutex(dataMutex)
    {
        DATA_MUTEX_CHECK(!mutex().isHeld());
        mutex().lock();
        m_isLocked = true;
    }

    ~DataMutexLocker() WTF_RELEASES_LOCK(m_dataMutex.m_mutex)
    {
        if (m_isLocked)
            mutex().unlock();
    }

    T* operator->()
    {
        DATA_MUTEX_CHECK(mutex().isHeld());
        assertIsHeld(m_dataMutex.m_mutex);
        return &m_dataMutex.m_data;
    }

    T& operator*()
    {
        DATA_MUTEX_CHECK(mutex().isHeld());
        assertIsHeld(m_dataMutex.m_mutex);
        return m_dataMutex.m_data;
    }

    Lock& mutex() WTF_RETURNS_LOCK(m_dataMutex.m_mutex)
    {
        return m_dataMutex.m_mutex;
    }

    void unlockEarly() WTF_RELEASES_LOCK(m_dataMutex.m_mutex)
    {
        DATA_MUTEX_CHECK(mutex().isHeld());
        m_isLocked = false;
        mutex().unlock();
    }

    // Used to avoid excessive brace scoping when only small parts of the code need to be run unlocked.
    // Please be mindful that accessing the wrapped data from the callback is unsafe and will fail on assertions.
    // It's helpful to use a minimal lambda capture to be conscious of what data you're having access to in these sections.
    void runUnlocked(const Function<void()>& callback)
    {
        DATA_MUTEX_CHECK(mutex().isHeld());
        assertIsHeld(m_dataMutex.m_mutex);
        mutex().unlock();
        callback();
        mutex().lock();
    }

private:
    DataMutex<T>& m_dataMutex;
    bool m_isLocked { false };
};

} // namespace WTF

using WTF::DataMutex;
using WTF::DataMutexLocker;
