#ifndef _DESEMAPHORE_HPP
#define _DESEMAPHORE_HPP
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
 * \brief deSemaphore C++ wrapper.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deSemaphore.h"

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief Semaphore
 *
 * Semaphore provides standard semaphore functionality.
 *
 * Semaphore is thread-safe counter that can be used to control access
 * to a set of resources. The count can be incremented and decremented.
 * When decrementing causes counter to reach negative value, it will
 * block until increment has been called from an another thread.
 *//*--------------------------------------------------------------------*/
class Semaphore
{
public:
    Semaphore(int initialValue, uint32_t flags = 0);
    ~Semaphore(void);

    void increment(void) throw();
    void decrement(void) throw();
    bool tryDecrement(void) throw();

private:
    Semaphore(const Semaphore &other);            // Not allowed!
    Semaphore &operator=(const Semaphore &other); // Not allowed!

    deSemaphore m_semaphore;
};

// Inline implementations.

/*--------------------------------------------------------------------*//*!
 * \brief Increment semaphore count.
 *
 * Incremeting increases semaphore value by 1. If a value is currently
 * negative (there is a thread waiting in decrement) the waiting thread
 * will be resumed.
 *
 * Incrementing semaphore will never block.
 *//*--------------------------------------------------------------------*/
inline void Semaphore::increment(void) throw()
{
    deSemaphore_increment(m_semaphore);
}

/*--------------------------------------------------------------------*//*!
 * \brief Decrement semaphore count.
 *
 * Decrementing decreases semaphore value by 1. If resulting value is negative
 * (only -1 is possible) decrement() will block until the value is again 0
 * (increment() has been called).
 *
 * If there is an another thread waiting in decrement(), the current thread
 * will block until other thread(s) have been resumed.
 *//*--------------------------------------------------------------------*/
inline void Semaphore::decrement(void) throw()
{
    deSemaphore_decrement(m_semaphore);
}

/*--------------------------------------------------------------------*//*!
 * \brief Try to decrement semaphore value.
 * \return true if decrementing was successful without blocking, false
 *           otherwise
 *
 * This function will never block, i.e. it will return false if decrementing
 * semaphore counter would result in negative value or there is already
 * one or more threads waiting for this semaphore.
 *//*--------------------------------------------------------------------*/
inline bool Semaphore::tryDecrement(void) throw()
{
    return deSemaphore_tryDecrement(m_semaphore) == true;
}

} // namespace de

#endif // _DESEMAPHORE_HPP
