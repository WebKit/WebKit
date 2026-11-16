#ifndef _DEMUTEX_HPP
#define _DEMUTEX_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief deMutex C++ wrapper.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deMutex.h"

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief Mutual exclusion lock
 *
 * Mutex class provides standard mutual exclusion lock functionality.
 *//*--------------------------------------------------------------------*/
class Mutex
{
public:
    Mutex(uint32_t flags = 0);
    ~Mutex(void);

    void lock(void) throw();
    void unlock(void) throw();
    bool tryLock(void) throw();

private:
    Mutex(const Mutex &other);            // Not allowed!
    Mutex &operator=(const Mutex &other); // Not allowed!

    deMutex m_mutex;
};

/*--------------------------------------------------------------------*//*!
 * \brief Scoped mutex lock.
 *
 * ScopedLock provides helper for maintaining Mutex lock for the duration
 * of current scope. The lock is acquired in constructor and released
 * when ScopedLock goes out of scope.
 *//*--------------------------------------------------------------------*/
class ScopedLock
{
public:
    ScopedLock(Mutex &mutex);
    ~ScopedLock(void)
    {
        m_mutex.unlock();
    }

private:
    ScopedLock(const ScopedLock &other);            // Not allowed!
    ScopedLock &operator=(const ScopedLock &other); // Not allowed!

    Mutex &m_mutex;
};

// Mutex inline implementations.

/*--------------------------------------------------------------------*//*!
 * \brief Acquire mutex lock.
 * \note This method will never report failure. If an error occurs due
 *         to misuse or other reason it will lead to process termination
*         in debug build.
 *
 * If mutex is currently locked the function will block until current
 * lock is released.
 *
 * In recursive mode further calls from the thread owning the mutex will
 * succeed and increment lock count.
 *//*--------------------------------------------------------------------*/
inline void Mutex::lock(void) throw()
{
    deMutex_lock(m_mutex);
}

/*--------------------------------------------------------------------*//*!
 * \brief Release mutex lock.
 * \note This method will never report failure. If an error occurs due
 *         to misuse or other reason it will lead to process termination
*         in debug build.
 *
 * In recursive mode the mutex will be released once the lock count reaches
 * zero.
 *//*--------------------------------------------------------------------*/
inline void Mutex::unlock(void) throw()
{
    deMutex_unlock(m_mutex);
}

/*--------------------------------------------------------------------*//*!
 * \brief Try to acquire lock.
 * \return Returns true if lock was acquired and false otherwise.
 *
 * This function will never block, i.e. it will return false if mutex
 * is currently locked.
 *//*--------------------------------------------------------------------*/
inline bool Mutex::tryLock(void) throw()
{
    return deMutex_tryLock(m_mutex) == true;
}

// ScopedLock inline implementations.

/*--------------------------------------------------------------------*//*!
 * \brief Acquire scoped lock to mutex.
 * \param mutex Mutex to be locked.
 *//*--------------------------------------------------------------------*/
inline ScopedLock::ScopedLock(Mutex &mutex) : m_mutex(mutex)
{
    m_mutex.lock();
}

} // namespace de

#endif // _DEMUTEX_HPP
